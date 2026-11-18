# 1. Teste do hardware (antes de soldar e imprimir)

Objetivo: ligar a CYD, o leitor NFC e a balança **soltos na bancada** e confirmar que tudo
funciona. Só depois disso a gente fixa as medidas e imprime a caixa.

Tempo estimado: 30–60 min.

## O que você precisa

- CYD (ESP32-2432S028R) + cabo USB de dados (não serve cabo só de carga)
- PN532 (placa vermelha) + cabo JST 1.25 de 4 pinos
- HX711 + célula de carga de 10 kg
- Algumas tags NTAG213
- Computador com **Google Chrome ou Edge** (para gravar o firmware pelo navegador)
- Um objeto de peso conhecido para calibrar (ex.: pacote fechado de 1 kg de arroz/açúcar,
  ou qualquer coisa pesada numa balança de cozinha)

---

## Passo 1 — Teste só a tela

1. Ligue a CYD no computador pelo USB.
2. Ela deve acender com a demonstração de fábrica. Se acendeu, a placa está ok.

## Passo 2 — Prepare o PN532

1. No PN532 há **duas chavinhas** (SEL0 e SEL1). Coloque no modo **I2C**:
   - **SEL0 = ON (1)** e **SEL1 = OFF (0)** — confira a tabelinha impressa na própria placa.
2. Se o PN532 veio com os pinos soltos (não soldados), solde a fileira de 4 pinos
   **GND, VCC, SDA, SCL**.

## Passo 3 — Ligue os fios

> Faça com a CYD **desligada do USB**.

A CYD tem três conectores pequenos (JST 1.25). Os nomes estão impressos na placa, ao lado de cada um.
**Confira os nomes impressos** — a cor dos fios do cabo não é padronizada.

### Leitor NFC → conector **CN1** da CYD

| CN1 (CYD) | PN532 |
|---|---|
| 3.3V | VCC |
| GND | GND |
| IO27 | SDA |
| IO22 | SCL |

### Balança

Célula de carga → HX711 (fios que saem da barra de alumínio):

| Fio da célula | HX711 |
|---|---|
| Vermelho | E+ |
| Preto | E− |
| Branco | A− |
| Verde | A+ |

HX711 → CYD:

| HX711 | Onde ligar na CYD |
|---|---|
| VCC | **3.3V** (junto com o VCC do PN532 — emende os dois fios) |
| GND | GND (qualquer conector) |
| DT | **IO35** — conector **P3** |
| SCK | **TX** (IO1) — conector **P1** (o de 4 pinos ao lado do USB, marcado VIN/TX/RX/GND) |

> **Não ligue o HX711 no 5V/VIN.** Com 5V ele mandaria 5V para o pino da CYD, que só aguenta 3.3V.
> **Não use o pino IO21 do P3** — ele controla a luz da tela.

Para o teste, a célula pode ficar solta na mesa: prenda uma ponta com fita dupla-face/grampo
na beira da mesa e deixe a outra ponta livre para apoiar o peso. A **seta** gravada na barra aponta
para baixo.

## Passo 4 — Grave o firmware de teste (pelo navegador)

1. Baixe o arquivo **`firmware/teste-hardware/bin/teste-hardware-cyd.bin`** deste repositório
   (no GitHub: abra o arquivo → botão de download).
2. Abra no Chrome/Edge: **https://espressif.github.io/esptool-js/**
3. Ligue a CYD no USB e clique em **Connect**. Escolha a porta que aparecer
   (geralmente "USB-SERIAL CH340").
   - Se nenhuma porta aparecer, instale o driver **CH340** (busque "driver CH340" + seu sistema).
4. Em **Flash Address** deixe **0x0** e escolha o arquivo `.bin` baixado.
5. Clique em **Program** e espere chegar a 100%.
6. Desconecte e ligue o USB de novo (ou aperte o botão **RST** da placa).

### Se a tela ficar branca, embaralhada ou espelhada

Algumas CYD com USB-C usam outro controlador de tela. Repita o passo 4 com
**`teste-hardware-cyd-st7789.bin`**.

### Se a tela funcionar mas as cores estiverem estranhas (negativo)

Toque no botão **Cores**. A escolha fica salva.

## Passo 5 — Confira cada parte

Na tela aparece **"Teste da Central"** com dois indicadores no topo:

| Indicador | Verde | Vermelho — o que conferir |
|---|---|---|
| **NFC** | `PN532 ok` | chavinhas em I2C, SDA↔IO27, SCL↔IO22, VCC no 3.3V |
| **BALANÇA** | `HX711 ok` | DT↔IO35, SCK↔TX, VCC no 3.3V, GND |

### NFC

1. Encoste uma tag no PN532 (a antena é o desenho de cobre na placa).
2. Deve aparecer o **UID**, o tipo (**NTAG213 (144 B)**) e o conteúdo, se houver.
3. **Teste de gravação**: toque **Gravar**, encoste uma tag e espere.
   Ela é gravada com um PLA laranja de teste no formato OpenSpool e lida de volta.
   Deve aparecer `application/json: {"protocol":"openspool",...}`.

> Teste também a **distância**: com a tag em cima de 2–3 mm de plástico (um pedaço de peça
> impressa), ela ainda lê? Anote até quantos mm funciona. Isso define a espessura do prato.

### Balança

1. Sem nada em cima, toque **Calibrar** → **Próximo**.
2. Coloque o objeto de peso conhecido, ajuste o valor com **−100 / −10 / +10 / +100** e toque **Salvar**.
3. Tire e coloque o objeto: o peso deve voltar ao mesmo valor (±1–2 g).
4. Se o peso aparecer **negativo**, inverta os fios **branco e verde** no HX711.
5. **Tara** zera a balança a qualquer momento.

## Passo 6 — Me mande

- Foto da tela funcionando (ou do erro)
- Qual arquivo funcionou: `cyd` ou `cyd-st7789`
- Até quantos mm de plástico a tag ainda lê
- As medidas da lista abaixo

### Medidas para os arquivos de impressão

Com paquímetro (ou régua, para os tamanhos maiores):

| Peça | Medir |
|---|---|
| **CYD** | distância entre os centros dos 4 furos (horizontal e vertical), diâmetro dos furos, altura total do componente mais alto atrás da placa |
| **Célula de carga** | comprimento; distância entre os 2 furos de cada ponta; rosca (M4 ou M5) |
| **PN532** | tamanho da placa; distância entre os furos |
| **HX711** | tamanho da placa |
| **Seus rolos** | diâmetro do furo central e diâmetro externo de cada marca que você usa |

Uma foto de cada peça ao lado de uma régua também ajuda.
