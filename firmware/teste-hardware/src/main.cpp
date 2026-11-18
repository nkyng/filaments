// Teste do hardware da Central NFC de Filamentos
//
// Mostra, numa tela so, se cada parte esta funcionando:
//   - Tela e touch (se voce esta vendo e os botoes respondem, ok)
//   - Leitor NFC PN532: UID, tipo da tag (NTAG213/215/216) e conteudo NDEF
//   - Balanca HX711 + celula de carga: peso em gramas, tara e calibracao
//   - Gravacao de uma tag de teste no formato OpenSpool (botao "Gravar")
//
// Observacao: GPIO1 (TX) e usado como SCK do HX711, entao o monitor serial
// fica desligado neste firmware.

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <Adafruit_PN532.h>
#include <HX711.h>

// ---------- pinos fixos da CYD ----------
static const int TOUCH_CLK = 25, TOUCH_MISO = 39, TOUCH_MOSI = 32, TOUCH_CS = 33, TOUCH_IRQ = 36;

// ---------- cores ----------
static const uint16_t C_BG = 0x10A2;      // quase preto
static const uint16_t C_PANEL = 0x2124;
static const uint16_t C_LINE = 0x39C7;
static const uint16_t C_TEXT = 0xEF7D;
static const uint16_t C_MUTED = 0x9CF3;
static const uint16_t C_ACCENT = 0xFBC6;  // laranja
static const uint16_t C_OK = 0x3E89;      // verde
static const uint16_t C_BAD = 0xF208;     // vermelho

TFT_eSPI tft;
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
Adafruit_PN532 nfc(255, 255, &Wire);  // sem IRQ/RESET: modo polling por I2C
HX711 scale;
Preferences prefs;

// ---------- estado ----------
enum Mode { MODE_MAIN, MODE_CAL_EMPTY, MODE_CAL_WEIGHT, MODE_WRITE };
Mode mode = MODE_MAIN;

bool nfcOk = false;
uint32_t nfcVersion = 0;
uint8_t lastUid[10];
uint8_t lastUidLen = 0;
uint32_t lastTagSeen = 0;
String tagLine1 = "Encoste uma tag";
String tagLine2 = "";
String tagLine3 = "";
String tagMsg = "";

bool hxSeen = false;
uint32_t hxLastReady = 0;
long rawBuf[8];
int rawCount = 0, rawIdx = 0;
long rawAvg = 0;
long offsetRaw = 0;
float factor = 0;          // contagens por grama; 0 = nao calibrado
int calRefGrams = 1000;
bool invertColors = false;

uint32_t lastTouch = 0;
uint32_t lastDraw = 0;

struct Button { int x, y, w, h; const char *label; };

// ---------- utilitarios de desenho ----------
void drawButton(const Button &b, bool accent = false) {
  tft.fillRoundRect(b.x, b.y, b.w, b.h, 8, accent ? C_ACCENT : C_PANEL);
  tft.drawRoundRect(b.x, b.y, b.w, b.h, 8, accent ? C_ACCENT : C_LINE);
  tft.setTextColor(accent ? TFT_BLACK : C_TEXT);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(b.label, b.x + b.w / 2, b.y + b.h / 2 + 1, 2);
}

bool hit(const Button &b, int x, int y) {
  return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h;
}

void chip(int x, int y, const char *name, bool ok, const char *okText, const char *badText) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(C_MUTED, C_BG);
  tft.drawString(name, x, y, 1);
  tft.fillCircle(x + 3, y + 15, 3, ok ? C_OK : C_BAD);
  tft.setTextColor(ok ? C_OK : C_BAD, C_BG);
  tft.drawString(ok ? okText : badText, x + 10, y + 11, 1);
}

// ---------- botoes ----------
const Button btnTara = {4, 200, 74, 36, "Tara"};
const Button btnCal = {84, 200, 74, 36, "Calibrar"};
const Button btnWrite = {164, 200, 74, 36, "Gravar"};
const Button btnColors = {244, 200, 72, 36, "Cores"};

const Button btnNext = {200, 200, 116, 36, "Proximo"};
const Button btnCancel = {4, 200, 116, 36, "Cancelar"};
const Button btnM100 = {4, 140, 72, 40, "-100"};
const Button btnM10 = {82, 140, 72, 40, "-10"};
const Button btnP10 = {166, 140, 72, 40, "+10"};
const Button btnP100 = {244, 140, 72, 40, "+100"};
const Button btnOkCal = {200, 200, 116, 36, "Salvar"};

// ---------- balanca ----------
void loadPrefs() {
  prefs.begin("central", true);
  factor = prefs.getFloat("factor", 0);
  offsetRaw = prefs.getLong("offset", 0);
  invertColors = prefs.getBool("invert", false);
  prefs.end();
}

void savePrefs() {
  prefs.begin("central", false);
  prefs.putFloat("factor", factor);
  prefs.putLong("offset", offsetRaw);
  prefs.putBool("invert", invertColors);
  prefs.end();
}

void pollScale() {
  if (!scale.is_ready()) return;
  long v = scale.read();
  hxSeen = true;
  hxLastReady = millis();
  rawBuf[rawIdx] = v;
  rawIdx = (rawIdx + 1) % 8;
  if (rawCount < 8) rawCount++;
  long long sum = 0;
  for (int i = 0; i < rawCount; i++) sum += rawBuf[i];
  rawAvg = (long)(sum / rawCount);
}

bool scaleAlive() { return hxSeen && millis() - hxLastReady < 2000; }

float grams() {
  if (factor == 0) return NAN;
  return (rawAvg - offsetRaw) / factor;
}

// ---------- NFC / NDEF ----------
const char *tagTypeName(uint8_t ccSize) {
  switch (ccSize) {
    case 0x12: return "NTAG213 (144 B)";
    case 0x3E: return "NTAG215 (504 B)";
    case 0x6D: return "NTAG216 (888 B)";
    default: return "tag desconhecida";
  }
}

String uidHex(const uint8_t *uid, uint8_t len) {
  String s;
  for (uint8_t i = 0; i < len; i++) {
    if (uid[i] < 0x10) s += '0';
    s += String(uid[i], HEX);
    if (i + 1 < len) s += ':';
  }
  s.toUpperCase();
  return s;
}

// Le a area de dados do usuario (a partir da pagina 4) para buf. Retorna bytes lidos.
int readUserArea(uint8_t *buf, int maxBytes) {
  int n = 0;
  for (uint8_t page = 4; n + 4 <= maxBytes; page++) {
    uint8_t p[16];
    if (!nfc.ntag2xx_ReadPage(page, p)) break;
    memcpy(buf + n, p, 4);
    n += 4;
  }
  return n;
}

// Interpreta o primeiro registro NDEF e preenche tagLine3 com tipo + conteudo.
void parseNdef(const uint8_t *d, int len) {
  int i = 0;
  while (i < len) {
    uint8_t t = d[i];
    if (t == 0x00) { i++; continue; }
    if (t == 0xFE) break;
    if (i + 1 >= len) break;
    int l = d[i + 1];
    int hdr = 2;
    if (l == 0xFF) { if (i + 3 >= len) break; l = (d[i + 2] << 8) | d[i + 3]; hdr = 4; }
    if (t != 0x03) { i += hdr + l; continue; }
    const uint8_t *r = d + i + hdr;
    if (l < 3) { tagLine3 = "NDEF vazio"; return; }
    uint8_t h = r[0];
    uint8_t tnf = h & 0x07;
    bool sr = h & 0x10, il = h & 0x08;
    int p = 1;
    uint8_t typeLen = r[p++];
    uint32_t payLen = sr ? r[p++] : ((uint32_t)r[p] << 24 | (uint32_t)r[p + 1] << 16 | (uint32_t)r[p + 2] << 8 | r[p + 3]);
    if (!sr) p += 4;
    uint8_t idLen = il ? r[p++] : 0;
    String type;
    for (int k = 0; k < typeLen && p + k < l; k++) type += (char)r[p + k];
    p += typeLen + idLen;
    String payload;
    int start = p;
    if (tnf == 1 && type == "T" && payLen > 0) start += 1 + (r[p] & 0x3F);  // pula idioma
    for (int k = start; k < p + (int)payLen && k < l; k++) payload += (char)r[k];
    tagLine3 = type + ": " + payload;
    return;
  }
  tagLine3 = "Sem dados NDEF";
}

void readTagDetails() {
  uint8_t cc[16];
  tagLine2 = "";
  tagLine3 = "";
  if (!nfc.ntag2xx_ReadPage(3, cc)) { tagLine2 = "Nao e NTAG (Mifare/Bambu?)"; return; }
  if (cc[0] != 0xE1) { tagLine2 = "NTAG sem formato NDEF"; return; }
  tagLine2 = tagTypeName(cc[2]);
  int size = cc[2] * 8;
  static uint8_t data[888];
  int n = readUserArea(data, min(size, (int)sizeof(data)));
  parseNdef(data, n);
}

// Grava um registro OpenSpool de teste (cabe na NTAG213).
bool writeTestTag(String &err) {
  uint8_t cc[16];
  if (!nfc.ntag2xx_ReadPage(3, cc) || cc[0] != 0xE1) { err = "Tag nao e NTAG com NDEF"; return false; }
  const char *mime = "application/json";
  const char *json = "{\"protocol\":\"openspool\",\"version\":\"1.0\",\"type\":\"PLA\",\"color_hex\":\"#FF5500\",\"brand\":\"Teste\"}";
  uint8_t mimeLen = strlen(mime), jsonLen = strlen(json);
  uint8_t rec[200];
  int n = 0;
  rec[n++] = 0x03;                       // TLV NDEF
  rec[n++] = 3 + mimeLen + jsonLen;      // tamanho do registro
  rec[n++] = 0xD2;                       // MB ME SR, TNF=2 (MIME)
  rec[n++] = mimeLen;
  rec[n++] = jsonLen;
  memcpy(rec + n, mime, mimeLen); n += mimeLen;
  memcpy(rec + n, json, jsonLen); n += jsonLen;
  rec[n++] = 0xFE;                       // fim
  while (n % 4) rec[n++] = 0x00;
  if (n > cc[2] * 8) { err = "Nao cabe na tag"; return false; }
  for (int i = 0; i < n; i += 4) {
    if (!nfc.ntag2xx_WritePage(4 + i / 4, rec + i)) { err = "Falha na pagina " + String(4 + i / 4); return false; }
  }
  return true;
}

void pollNfc() {
  if (!nfcOk) return;
  uint8_t uid[10];
  uint8_t len = 0;
  bool found = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &len, 60);
  if (!found) {
    if (lastUidLen && millis() - lastTagSeen > 1200) {
      lastUidLen = 0;
      tagLine1 = "Encoste uma tag";
      tagLine2 = tagLine3 = "";
    }
    return;
  }
  lastTagSeen = millis();
  bool same = len == lastUidLen && memcmp(uid, lastUid, len) == 0;
  if (same && mode != MODE_WRITE) return;
  memcpy(lastUid, uid, len);
  lastUidLen = len;
  tagLine1 = "UID " + uidHex(uid, len);
  if (mode == MODE_WRITE) {
    String err;
    if (writeTestTag(err)) tagMsg = "Gravada! Lendo de volta...";
    else tagMsg = "Erro: " + err;
    mode = MODE_MAIN;
  }
  readTagDetails();
}

// ---------- telas ----------
void drawHeader() {
  tft.fillRect(0, 0, 320, 30, C_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(C_TEXT, C_BG);
  tft.drawString("Teste da Central", 6, 7, 2);
  chip(150, 2, "NFC", nfcOk, "PN532 ok", "nao achou");
  chip(232, 2, "BALANCA", scaleAlive(), "HX711 ok", "sem sinal");
  tft.drawFastHLine(0, 30, 320, C_LINE);
}

void wrapText(const String &s, int x, int y, int w, int maxLines, uint16_t color) {
  tft.setTextColor(color, C_PANEL);
  tft.setTextDatum(TL_DATUM);
  int perLine = w / 6;
  for (int line = 0, pos = 0; line < maxLines && pos < (int)s.length(); line++, pos += perLine) {
    tft.drawString(s.substring(pos, min((int)s.length(), pos + perLine)), x, y + line * 10, 1);
  }
}

void drawMain() {
  drawHeader();
  // painel NFC
  tft.fillRoundRect(4, 36, 180, 158, 8, C_PANEL);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(C_ACCENT, C_PANEL);
  tft.drawString("TAG NFC", 12, 42, 1);
  tft.setTextColor(C_TEXT, C_PANEL);
  tft.drawString(tagLine1, 12, 56, 2);
  tft.setTextColor(C_MUTED, C_PANEL);
  tft.drawString(tagLine2, 12, 76, 2);
  wrapText(tagLine3, 12, 98, 166, 7, C_TEXT);
  if (tagMsg.length()) { tft.setTextColor(C_ACCENT, C_PANEL); tft.drawString(tagMsg, 12, 180, 1); }
  // painel balanca
  tft.fillRoundRect(190, 36, 126, 158, 8, C_PANEL);
  tft.setTextColor(C_ACCENT, C_PANEL);
  tft.drawString("BALANCA", 198, 42, 1);
  tft.setTextDatum(TR_DATUM);
  if (!scaleAlive()) {
    tft.setTextColor(C_BAD, C_PANEL);
    tft.drawString("sem sinal", 308, 80, 2);
    tft.setTextColor(C_MUTED, C_PANEL);
    tft.drawString("confira DT/SCK", 308, 100, 1);
  } else if (factor == 0) {
    tft.setTextColor(C_TEXT, C_PANEL);
    tft.drawString("calibrar", 308, 74, 4);
    tft.setTextColor(C_MUTED, C_PANEL);
    tft.drawString("toque Calibrar", 308, 104, 1);
  } else {
    float g = grams();
    tft.setTextColor(C_TEXT, C_PANEL);
    tft.drawString(String((long)lroundf(g)), 308, 66, 7);
    tft.setTextColor(C_MUTED, C_PANEL);
    tft.drawString("gramas", 308, 122, 2);
  }
  tft.setTextColor(C_MUTED, C_PANEL);
  tft.drawString("bruto " + String(rawAvg), 308, 176, 1);
  tft.setTextDatum(TL_DATUM);
  drawButton(btnTara);
  drawButton(btnCal);
  drawButton(btnWrite);
  drawButton(btnColors);
}

void drawCalEmpty() {
  tft.fillScreen(C_BG);
  drawHeader();
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(C_ACCENT, C_BG);
  tft.drawString("Calibrar 1/2", 10, 44, 4);
  tft.setTextColor(C_TEXT, C_BG);
  tft.drawString("Tire tudo de cima da balanca", 10, 84, 2);
  tft.drawString("e toque Proximo.", 10, 104, 2);
  drawButton(btnCancel);
  drawButton(btnNext, true);
}

void drawCalWeight() {
  tft.fillRect(0, 31, 320, 209, C_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(C_ACCENT, C_BG);
  tft.drawString("Calibrar 2/2", 10, 40, 4);
  tft.setTextColor(C_TEXT, C_BG);
  tft.drawString("Coloque um objeto de peso conhecido", 10, 72, 2);
  tft.drawString("e ajuste o valor:", 10, 90, 2);
  tft.fillRect(180, 86, 140, 30, C_BG);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(C_ACCENT, C_BG);
  tft.drawString(String(calRefGrams) + " g", 312, 88, 4);
  drawButton(btnM100); drawButton(btnM10); drawButton(btnP10); drawButton(btnP100);
  drawButton(btnCancel);
  drawButton(btnOkCal, true);
}

void drawWrite() {
  tft.fillScreen(C_BG);
  drawHeader();
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(C_ACCENT, C_BG);
  tft.drawString("Gravar tag de teste", 10, 44, 4);
  tft.setTextColor(C_TEXT, C_BG);
  tft.drawString("Encoste uma tag NTAG.", 10, 84, 2);
  tft.drawString("Ela sera gravada com um PLA", 10, 104, 2);
  tft.drawString("laranja OpenSpool de teste.", 10, 124, 2);
  tft.setTextColor(C_MUTED, C_BG);
  tft.drawString("O conteudo atual da tag sera apagado.", 10, 150, 1);
  drawButton(btnCancel);
}

void enter(Mode m) {
  mode = m;
  if (m == MODE_MAIN) { tft.fillScreen(C_BG); drawMain(); }
  if (m == MODE_CAL_EMPTY) drawCalEmpty();
  if (m == MODE_CAL_WEIGHT) drawCalWeight();
  if (m == MODE_WRITE) { lastUidLen = 0; drawWrite(); }
}

// ---------- toque ----------
void handleTouch() {
  if (!ts.touched() || millis() - lastTouch < 300) return;
  TS_Point p = ts.getPoint();
  lastTouch = millis();
  int x = constrain(map(p.x, 200, 3700, 0, 320), 0, 319);
  int y = constrain(map(p.y, 240, 3800, 0, 240), 0, 239);

  if (mode == MODE_MAIN) {
    if (hit(btnTara, x, y)) { offsetRaw = rawAvg; savePrefs(); tagMsg = ""; drawMain(); }
    else if (hit(btnCal, x, y)) enter(MODE_CAL_EMPTY);
    else if (hit(btnWrite, x, y)) { tagMsg = ""; enter(MODE_WRITE); }
    else if (hit(btnColors, x, y)) { invertColors = !invertColors; tft.invertDisplay(invertColors); savePrefs(); }
  } else if (mode == MODE_CAL_EMPTY) {
    if (hit(btnCancel, x, y)) enter(MODE_MAIN);
    else if (hit(btnNext, x, y)) { offsetRaw = rawAvg; enter(MODE_CAL_WEIGHT); }
  } else if (mode == MODE_CAL_WEIGHT) {
    int d = 0;
    if (hit(btnM100, x, y)) d = -100;
    if (hit(btnM10, x, y)) d = -10;
    if (hit(btnP10, x, y)) d = 10;
    if (hit(btnP100, x, y)) d = 100;
    if (d) { calRefGrams = constrain(calRefGrams + d, 10, 9000); drawCalWeight(); }
    else if (hit(btnCancel, x, y)) { loadPrefs(); enter(MODE_MAIN); }
    else if (hit(btnOkCal, x, y)) {
      long delta = rawAvg - offsetRaw;
      if (labs(delta) > 50) { factor = (float)delta / calRefGrams; savePrefs(); }
      enter(MODE_MAIN);
    }
  } else if (mode == MODE_WRITE) {
    if (hit(btnCancel, x, y)) enter(MODE_MAIN);
  }
}

// ---------- setup / loop ----------
void setup() {
  loadPrefs();

  tft.init();
  tft.setRotation(1);
  tft.invertDisplay(invertColors);
  tft.fillScreen(C_BG);

  touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);

  Wire.begin(PIN_NFC_SDA, PIN_NFC_SCL);
  nfc.begin();
  nfcVersion = nfc.getFirmwareVersion();
  nfcOk = nfcVersion != 0;
  if (nfcOk) nfc.SAMConfig();
  else tagLine1 = "PN532 nao encontrado";
  if (!nfcOk) tagLine2 = "Confira fios e chave I2C";

  scale.begin(PIN_HX_DT, PIN_HX_SCK);

  enter(MODE_MAIN);
}

void loop() {
  pollScale();
  pollNfc();
  handleTouch();
  // redesenha so quando algo mudou, para a tela nao piscar
  if (millis() - lastDraw > 250) {
    lastDraw = millis();
    static String lastSig;
    String sig = String(nfcOk) + scaleAlive() + tagLine1 + tagLine2 + tagLine3 + tagMsg +
                 (factor == 0 ? String(rawAvg / 1000) : String(lroundf(grams())));
    if (sig != lastSig) {
      lastSig = sig;
      if (mode == MODE_MAIN) drawMain();
      else drawHeader();
    }
  }
}
