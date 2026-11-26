// Central NFC de Filamentos - caixa v0.2 (cabo escondido, saindo por tras)
// Medidas em mm. Sistema: X = largura, Y = profundidade (+Y = fundos), Z = altura.
// A frente (lado da tela) fica em Y = -120.
//
// Gerar um STL:  openscad -D 'part="bandeja"' -o bandeja.stl central.scad
// Partes: bandeja, tampa, pod, prato, cone, montagem (visualizacao)
//
// v0: feita com as medidas padrao das pecas. A fixacao usa folgas e fita
// dupla-face onde a medida exata ainda nao foi conferida.

part = "montagem";
$fn = 64;

// ---------- caixa ----------
W = 200;            // largura
D = 240;            // profundidade
R = 12;             // raio dos cantos
wall = 3;           // parede da bandeja
floor_t = 3;        // fundo da bandeja
tray_h = 29;        // altura da bandeja (topo das paredes)
lid_t = 3;          // espessura da tampa
lid_z = tray_h;     // a tampa apoia no topo das paredes

// ---------- parafusos ----------
m3_pilot = 2.8;     // furo para parafuso M3 autoatarraxante
m3_clear = 3.4;
m3_csk = 6.6;       // cabeca escareada M3
cell_screw = 5.5;   // passa M4 e M5 (folga)
cell_csk = 10.5;    // cabeca escareada M5
cell_cbore = 10;    // cabeca cilindrica M4/M5 (embaixo)
slot = 2;           // rasgo dos furos da celula (+-2 mm) para compensar medida

// ---------- celula de carga 10 kg (barra) ----------
cell_L = 80;
cell_S = 12.7;
cell_hole_end = 5;      // centro do 1o furo ate a ponta
cell_hole_step = 15;    // distancia entre os 2 furos da mesma ponta
cell_y = 40;            // linha da celula e centro do prato
cell_z = floor_t + 6;   // base da celula (fica 6 mm acima do fundo)
fixed_x = -cell_L/2;    // ponta fixa (esquerda)

// ---------- prato ----------
plat_d = 160;
plat_t = 3;
plat_z = 36;            // face de baixo do prato (4 mm acima da tampa)
col_w = 28;             // coluna que apoia na ponta livre da celula
col_dp = 14;
col_x = cell_L/2 - cell_hole_end - cell_hole_step/2;   // 27.5
nfc_off = 50;           // marca NFC: 50 mm a frente do centro
nfc_thin = 1.2;           // espessura do prato sobre a marca NFC

// ---------- cone ----------
cone_d0 = 50; cone_d1 = 32; cone_h = 16; peg_d = 10; peg_h = 3;

// ---------- PN532 ----------
pn_w = 45; pn_d = 43;   // bolso na tampa (placa 42.7 x 40.4 + folga)
pn_y = cell_y - nfc_off;  // -10
pn_depth = 2;           // sobra 1 mm de tampa sobre a antena

// ---------- pod da tela ----------
pod_w = 120;            // ~14 mm de cada lado da CYD: use cabo USB-C em L (90 graus)
pod_t = 2.4;
// perfil lateral (Y, Z): frente baixa -> topo traseiro
pod_prof = [[-118, lid_z+lid_t], [-118, 42], [-68, 74], [-65, 74], [-65, lid_z+lid_t]];
win_w = 66; win_h = 48;          // janela da tela (area visivel 57.6 x 43.2 + folga)
pocket_w = 72; pocket_h = 54;    // rebaixo interno que posiciona o vidro da tela
slope_a = atan2(74-42, 118-68);  // ~32.6 graus
face_c = [(-118 + -68)/2, (42 + 74)/2];
// ---------- cabo de alimentacao ----------
// O cabo desce do pod para dentro da bandeja e sai pela parede de tras (lado do rolo).
cable_x = -70;          // posicao da saida na parede de tras
cable_w = 8;            // largura do rasgo (cabo ate ~6 mm)
cable_h = 10;           // profundidade do rasgo a partir do topo da parede
front_notch = [-7, 14, 9];   // rasgo da frente da bandeja v0 (x0, largura, altura) - a tampa o fecha
pod_tab_x = pod_w/2 - pod_t - 4;          // 53.6
pod_tabs = [[ pod_tab_x, -69], [-pod_tab_x, -69], [ pod_tab_x, -106], [-pod_tab_x, -106]];
corner_screws = [for (sx=[-1,1], sy=[-1,1]) [sx*(W/2-9), sy*(D/2-9)]];

module rrect(w, d, r) {
  offset(r) square([w-2*r, d-2*r], center=true);
}

// ================= BANDEJA =================
module bandeja() {
  difference() {
    union() {
      linear_extrude(floor_t) rrect(W, D, R);
      linear_extrude(tray_h) difference() { rrect(W, D, R); rrect(W-2*wall, D-2*wall, R-wall); }
      // torres dos parafusos da tampa
      for (p = corner_screws) translate([p[0], p[1], 0]) cylinder(d=9, h=tray_h);
      // apoio da ponta fixa da celula
      translate([fixed_x-1, cell_y-8, 0]) cube([27, 16, cell_z]);
      // moldura do HX711 (fita dupla-face)
      translate([66, cell_y, floor_t]) linear_extrude(2) difference() { square([39.2, 25.2], center=true); square([36, 22], center=true); }
    }
    // furos dos parafusos da tampa
    for (p = corner_screws) translate([p[0], p[1], tray_h-16]) cylinder(d=m3_pilot, h=17);
    // furos da ponta fixa (parafuso de baixo para cima) + rebaixo da cabeca
    for (i = [0, 1]) {
      x = fixed_x + cell_hole_end + i*cell_hole_step;
      hull() for (dx = [-slot, slot]) translate([x+dx, cell_y, -1]) cylinder(d=cell_screw, h=cell_z+2);
      hull() for (dx = [-slot, slot]) translate([x+dx, cell_y, -1]) cylinder(d=cell_cbore, h=1+2.5);
    }
    // rasgo da frente (existia na v0; a tampa tem um tampao que o fecha)
    translate([front_notch[0], -D/2-1, tray_h-front_notch[2]+0.01]) cube([front_notch[1], wall+2, front_notch[2]]);
    // saida do cabo por tras
    translate([cable_x - cable_w/2, D/2-wall-1, tray_h-cable_h]) cube([cable_w, wall+2, cable_h+1]);
    // rebaixos para pes de borracha adesivos
    for (sx=[-1,1], sy=[-1,1]) translate([sx*82, sy*102, -1]) cylinder(d=12.5, h=1.6);
  }
}

// ================= TAMPA =================
module tampa() {
  translate([0, 0, lid_z]) {
    // tampao que fecha o rasgo da frente da bandeja (folga de 0.3 mm)
    translate([front_notch[0]+0.3, -D/2+0.3, -front_notch[2]+0.3]) cube([front_notch[1]-0.6, wall-0.3, front_notch[2]-0.3+0.01]);
    // trava do cabo: o cabo passa apertado entre os dois pinos antes de sair
    for (dx = [-5, 5]) translate([cable_x + dx, D/2 - wall - 10, -10]) cylinder(d=6, h=10.01);
    tampa_placa();
  }
}
module tampa_placa() {
  difference() {
    linear_extrude(lid_t) rrect(W, D, R);
    // parafusos da tampa (escareados por cima)
    for (p = corner_screws) translate([p[0], p[1], 0]) {
      translate([0,0,-1]) cylinder(d=m3_clear, h=lid_t+2);
      translate([0,0,lid_t-1.6]) cylinder(d1=m3_clear, d2=m3_csk, h=1.61);
    }
    // parafusos do pod (de baixo para cima, escareados por baixo)
    for (p = pod_tabs) translate([p[0], p[1], 0]) {
      translate([0,0,-1]) cylinder(d=m3_clear, h=lid_t+2);
      translate([0,0,-0.01]) cylinder(d1=m3_csk, d2=m3_clear, h=1.6);
    }
    // passagem dos cabos para o pod (larga: o plugue USB pode estar dos dois lados);
    // fica por dentro das paredes do pod, escondida embaixo dele
    translate([-(pod_w/2 - pod_t - 1), -100, -1]) cube([pod_w - 2*pod_t - 2, 22, lid_t+2]);
    // folga da coluna do prato
    translate([col_x, cell_y, -1]) linear_extrude(lid_t+2) rrect(col_w+6, col_dp+6, 4);
    // bolso do PN532 por baixo (sobra 1 mm sobre a antena)
    translate([0, pn_y, -1]) linear_extrude(pn_depth+1) square([pn_w, pn_d], center=true);
  }
}

// ================= POD DA TELA =================
module pod_solid() {
  translate([-pod_w/2, 0, 0]) rotate([90, 0, 90]) linear_extrude(pod_w) polygon(pod_prof);
}
module pod_inner() {
  translate([-pod_w/2 + pod_t, 0, 0]) rotate([90, 0, 90]) linear_extrude(pod_w - 2*pod_t)
    // fechado atras (esconde a placa); aberto so embaixo, por onde entram a CYD e os cabos
    hull() for (t = [[0,0], [0,-30]]) translate(t) offset(delta=-pod_t) polygon(pod_prof);
}
module on_face() {  // coloca o filho centrado na face inclinada (X largura, Y ao longo da rampa, Z normal)
  translate([0, face_c[0], face_c[1]]) rotate([slope_a, 0, 0]) children();
}
module pod() {
  difference() {
    union() {
      difference() { pod_solid(); pod_inner(); }
      // abas de fixacao na tampa
      for (p = pod_tabs) {
        sx = sign(p[0]);
        x_wall = sx * (pod_w/2 - pod_t);
        if (p[1] > -80) {
          // abas de tras: vao ate a face de tras (imprime apoiada na mesa)
          translate([min(x_wall, x_wall - sx*8), -73, lid_z+lid_t]) cube([8, 8, 3]);
        } else {
          // abas da frente: com reforco a 45 graus para imprimir sem suporte
          hull() {
            translate([min(x_wall, x_wall - sx*8), p[1]-4, lid_z+lid_t]) cube([8, 8, 3]);
            translate([min(x_wall, x_wall - sx*0.2), p[1]-4, lid_z+lid_t]) cube([0.2, 16, 3]);
          }
        }
      }
    }
    // janela da tela
    on_face() cube([win_w, win_h, 20], center=true);
    // rebaixo interno que encaixa o vidro do display (deixa 1.2 mm de moldura)
    on_face() translate([0, 0, -pod_t]) cube([pocket_w, pocket_h, 2.4], center=true);
    // furos das abas
    for (p = pod_tabs) translate([p[0], p[1], lid_z+lid_t-1]) cylinder(d=m3_pilot, h=5);
  }
}

// ================= PRATO =================
// Coordenadas do prato: centro em (0,0), face de cima em z = plat_t.
module prato() {
  col_h = plat_z - (cell_z + cell_S);   // da face de baixo do prato ate o topo da celula
  difference() {
    union() {
      cylinder(d=plat_d, h=plat_t, $fn=128);
      translate([col_x, 0, -col_h]) linear_extrude(col_h + 0.01) rrect(col_w, col_dp, 3);
    }
    // furos da ponta livre (escareados por cima, cabeca fica rente)
    for (i = [0, 1]) {
      x = cell_L/2 - cell_hole_end - i*cell_hole_step;
      hull() for (dx = [-slot, slot]) translate([x+dx, 0, -col_h-1]) cylinder(d=cell_screw, h=col_h+plat_t+2);
      hull() for (dx = [-slot, slot]) translate([x+dx, 0, plat_t-2.8]) cylinder(d1=cell_screw, d2=cell_csk, h=2.81);
    }
    // furo do cone
    translate([0, 0, -1]) cylinder(d=peg_d+0.4, h=plat_t+2);
    // area fina sobre o PN532 (por baixo)
    translate([0, -nfc_off, -1]) cylinder(d=40, h=1 + plat_t - nfc_thin);
    // marca NFC gravada em cima
    translate([0, -nfc_off, plat_t-0.4]) difference() { cylinder(d=36, h=1); translate([0,0,-1]) cylinder(d=33, h=3); }
    translate([0, -nfc_off, plat_t-0.4]) linear_extrude(1) text("NFC", size=7, halign="center", valign="center", font="Liberation Sans:style=Bold");
    // seta do centro ate a marca
    translate([0, -24, plat_t-0.4]) linear_extrude(1) polygon([[-1,0],[1,0],[1,-3],[3,-3],[0,-6],[-3,-3],[-1,-3]]);
  }
}

// ================= CONE =================
module cone() {
  cylinder(d1=cone_d0, d2=cone_d1, h=cone_h);
  translate([0, 0, -peg_h]) cylinder(d=peg_d, h=peg_h+0.01);
}

// ================= VISUALIZACAO =================
module celula() {
  color("silver") translate([-cell_L/2, cell_y - cell_S/2, cell_z]) cube([cell_L, cell_S, cell_S]);
}
module montagem() {
  color("DimGray") bandeja();
  color("Gray") tampa();
  color("Gray") pod();
  celula();
  color("Gainsboro") translate([0, cell_y, plat_z]) prato();
  color("OrangeRed") translate([0, cell_y, plat_z + plat_t]) cone();
  color("Red") translate([0, pn_y, lid_z + pn_depth - 1.6]) cube([42.7, 40.4, 1.6], center=true);
  color("Gold") on_face() translate([0, 0, -pod_t - 6]) cube([86, 50, 1.6], center=true);
}

// ================= SAIDA (ja na orientacao de impressao) =================
if (part == "bandeja") bandeja();
if (part == "tampa") translate([0, 0, lid_z + lid_t]) rotate([180, 0, 0]) tampa();       // face de cima na mesa
if (part == "pod") translate([0, 0, -65]) rotate([-90, 0, 0]) pod();   // apoiado na face de tras
if (part == "prato") translate([0, 0, plat_t]) rotate([180, 0, 0]) prato();            // face de cima na mesa
if (part == "cone") translate([0, 0, cone_h]) rotate([180, 0, 0]) cone();              // ponta fina na mesa
if (part == "montagem") montagem();
