#!/usr/bin/env python3
"""Gera o painel "Scania Smart Dash" do TruckDash: PNGs em camadas + painel.txt.

Arte desenhada do zero (formas e icones proprios), seguindo a disposicao do painel
digital da Scania como aparece no ETS2: escala de velocidade a esquerda, conta-giros a
direita, informacoes no centro em 5 telas (carrossel) e faixa inferior fixa.
Nao inclui o logotipo da Scania: para usar um, coloque logo.png na pasta e descomente
a linha no painel.txt.

Uso: python3 tools/gen_scania_smart_dash.py <pasta-de-saida>
Todas as camadas tem o tamanho do painel (1024 x 600); o app corta as bordas vazias.
"""
import math, os, sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter

W, H, SS = 1024, 600, 3
OUT = sys.argv[1] if len(sys.argv) > 1 else "scania-smart-dash"
os.makedirs(OUT, exist_ok=True)

TEAL = (88, 230, 212)
TEAL_D = (44, 150, 128)
GREY = (150, 158, 168)
WHITE = (236, 240, 244)
DIM = (58, 64, 74)
RED = (255, 64, 72)
GREEN = (61, 220, 106)
BLUE = (70, 140, 255)
AMBER = (255, 176, 32)
FONT_DIR = "/usr/share/fonts/opentype/inter/"

_fc = {}
def font(px, w="SemiBold"):
    k = (px, w)
    if k not in _fc:
        _fc[k] = ImageFont.truetype(FONT_DIR + "Inter-%s.otf" % w, int(px * SS))
    return _fc[k]

def layer():
    im = Image.new("RGBA", (W * SS, H * SS), (0, 0, 0, 0))
    return im, ImageDraw.Draw(im)

def save(im, name):
    im.resize((W, H), Image.LANCZOS).save(os.path.join(OUT, name), optimize=True)
    return name

def P(*v):
    return [x * SS for x in v]

def line(d, pts, col, w):
    d.line([(x * SS, y * SS) for x, y in pts], fill=col, width=max(1, int(w * SS)), joint="curve")

def text(d, x, y, s, px, col, anchor="ls", w="SemiBold"):
    d.text((x * SS, y * SS), s, font=font(px, w), fill=col, anchor=anchor)

# ------------------------------------------------------------------ geometria das escalas
VEL_MAX, RPM_MAX = 100, 2500
Y0, Y1 = 515, 92                     # base (0) e topo da escala
def lx(y): return 352 - (Y0 - y) * (52.0 / (Y0 - Y1))   # linha da velocidade
def rx(y): return 672 + (Y0 - y) * (52.0 / (Y0 - Y1))   # linha do conta-giros
def vy(v): return Y0 - (Y0 - Y1) * v / VEL_MAX
def ry(r): return Y0 - (Y0 - Y1) * r / RPM_MAX

# ------------------------------------------------------------------ icones (desenho proprio)
def ic_fuel(d, x, y, c, s=1.0):
    d.rounded_rectangle(P(x - 8 * s, y - 11 * s, x + 3 * s, y + 11 * s), radius=2 * SS, outline=c, width=int(2.2 * SS))
    d.rectangle(P(x - 6 * s, y - 8 * s, x + 1 * s, y - 2 * s), fill=c)
    line(d, [(x + 3 * s, y - 4 * s), (x + 8 * s, y - 1 * s), (x + 8 * s, y + 8 * s), (x + 11 * s, y + 8 * s), (x + 11 * s, y - 6 * s)], c, 2 * s)

def ic_avg(d, x, y, c):
    ic_fuel(d, x, y - 2, c, 0.9)
    text(d, x + 10, y + 11, "AVG", 9, c, w="Bold")

def ic_pump_lh(d, x, y, c):
    d.rounded_rectangle(P(x - 9, y - 11, x + 9, y + 11), radius=3 * SS, outline=c, width=int(2 * SS))
    ic_fuel(d, x, y, c, 0.55)

def ic_clock_note(d, x, y, c):
    d.rounded_rectangle(P(x - 11, y - 12, x + 11, y + 12), radius=2 * SS, outline=c, width=int(2.2 * SS))
    for i in range(3):
        line(d, [(x - 6, y - 6 + i * 5), (x + 6, y - 6 + i * 5)], c, 1.6)

def ic_speed_avg(d, x, y, c):
    d.arc(P(x - 12, y - 12, x + 12, y + 12), 150, 390, fill=c, width=int(2.2 * SS))
    line(d, [(x, y), (x + 7, y - 7)], c, 2.2)
    text(d, x + 9, y + 12, "AVG", 9, c, w="Bold")

def ic_t1(d, x, y, c, px=26):
    text(d, x, y, "T", px, c, w="Bold")
    text(d, x + px * 0.58, y + 2, "1", px * 0.55, c, w="Bold")

def ic_adblue(d, x, y, c):
    text(d, x, y - 2, "Ad", 11, c, w="Bold")
    text(d, x, y + 9, "Blue", 11, c, w="Bold")
    d.ellipse(P(x + 15, y - 12, x + 21, y - 5), fill=(80, 170, 255))

def ic_water(d, x, y, c):
    line(d, [(x, y - 14), (x, y + 4)], c, 2.4)
    for i in range(3):
        line(d, [(x, y - 10 + i * 5), (x + 7, y - 10 + i * 5)], c, 1.6)
    for k in (8, 13):
        pts = [(x - 12 + i * 3, y + k + (1.6 if i % 2 else -1.6)) for i in range(9)]
        line(d, pts, c, 1.8)

def ic_oil(d, x, y, c):
    d.polygon([((x - 14) * SS, (y - 2) * SS), ((x + 6) * SS, (y - 2) * SS), ((x + 14) * SS, (y - 7) * SS),
               ((x + 6) * SS, (y + 6) * SS), ((x - 14) * SS, (y + 6) * SS)], outline=c, width=int(2 * SS))
    line(d, [(x - 4, y - 2), (x - 4, y - 7), (x - 10, y - 7)], c, 2)
    d.ellipse(P(x + 13, y + 3, x + 17, y + 8), fill=c)

def ic_circle_txt(d, x, y, t, c, r=12, px=15):
    d.ellipse(P(x - r, y - r, x + r, y + r), outline=c, width=int(2.2 * SS))
    text(d, x, y, t, px, c, anchor="mm", w="Bold")

def ic_trailer(d, x, y, c):
    d.rounded_rectangle(P(x - 12, y - 10, x + 2, y + 3), radius=2 * SS, outline=c, width=int(2 * SS))
    line(d, [(x + 2, y - 3), (x + 9, y - 3), (x + 12, y + 3)], c, 2)
    d.ellipse(P(x - 9, y + 3, x - 3, y + 9), outline=c, width=int(1.6 * SS))
    d.ellipse(P(x + 5, y + 3, x + 11, y + 9), outline=c, width=int(1.6 * SS))
    line(d, [(x - 14, y + 13), (x + 14, y + 13)], c, 1.6)

def ic_arrow(d, x, y, c, sgn):
    pts = [(-16, 0), (0, -13), (0, -6), (14, -6), (14, 6), (0, 6), (0, 13)]
    d.polygon([((x + px * sgn) * SS, (y + py) * SS) for px, py in pts], fill=c)

def ic_lamp(d, x, y, c, high):
    d.chord(P(x - 2, y - 11, x + 16, y + 11), 270, 90, outline=c, width=int(2.2 * SS))
    line(d, [(x + 3, y - 11), (x + 3, y + 11)], c, 2.2)
    for i in range(4):
        yy = y - 8 + i * 5.5
        line(d, [(x - 13, yy + (0 if high else 4)), (x - 3, yy)], c, 2)

def ic_park(d, x, y, c):
    for sg in (-1, 1):
        d.arc(P(x - 7 + sg * 7, y - 8, x + 7 + sg * 7, y + 8), 270 if sg > 0 else 90, 90 if sg > 0 else 270, fill=c, width=int(2 * SS))
        for i in range(3):
            line(d, [(x + sg * 16, y - 6 + i * 6), (x + sg * 21, y - 7 + i * 6)], c, 1.6)

def ic_brake(d, x, y, c, t="P"):
    d.ellipse(P(x - 10, y - 10, x + 10, y + 10), outline=c, width=int(2.2 * SS))
    d.arc(P(x - 16, y - 14, x + 16, y + 14), 120, 240, fill=c, width=int(2 * SS))
    d.arc(P(x - 16, y - 14, x + 16, y + 14), -60, 60, fill=c, width=int(2 * SS))
    text(d, x, y + 1, t, 13, c, anchor="mm", w="Bold")

def ic_cruise(d, x, y, c):
    d.arc(P(x - 13, y - 13, x + 13, y + 13), 135, 405, fill=c, width=int(2.2 * SS))
    line(d, [(x, y + 2), (x + 8, y - 8)], c, 2.4)
    line(d, [(x - 6, y + 13), (x + 6, y + 13)], c, 2)

def ic_box(d, x, y, t, c, px=13):
    d.rounded_rectangle(P(x - 16, y - 11, x + 16, y + 11), radius=4 * SS, outline=c, width=int(2 * SS))
    text(d, x, y + 1, t, px, c, anchor="mm", w="Bold")

def ic_battery(d, x, y, c):
    d.rounded_rectangle(P(x - 14, y - 8, x + 14, y + 10), radius=2 * SS, outline=c, width=int(2 * SS))
    line(d, [(x - 9, y - 8), (x - 9, y - 12)], c, 3); line(d, [(x + 9, y - 8), (x + 9, y - 12)], c, 3)
    text(d, x - 8, y + 2, "-", 14, c, anchor="mm", w="Bold"); text(d, x + 8, y + 2, "+", 14, c, anchor="mm", w="Bold")

# ------------------------------------------------------------------ fundo (todas as telas)
def make_fundo():
    im = Image.new("RGBA", (W * SS, H * SS), (6, 8, 11, 255))
    glow, g = layer()
    for cx, cy, r, col in ((60, 600, 330, (30, 60, 120, 150)), (964, 600, 330, (30, 60, 120, 150)), (512, 640, 300, (25, 45, 90, 110))):
        g.ellipse(P(cx - r, cy - r, cx + r, cy + r), fill=col)
    im = Image.alpha_composite(im, glow.filter(ImageFilter.GaussianBlur(90 * SS)))
    d = ImageDraw.Draw(im)
    # paineis laterais levemente mais claros
    side, sd = layer()
    sd.polygon([(0, 70 * SS), (lx(92) * SS - 40 * SS, 70 * SS), (lx(515) * SS - 30 * SS, 600 * SS), (0, 600 * SS)], fill=(20, 32, 56, 70))
    sd.polygon([(W * SS, 70 * SS), (rx(92) * SS + 40 * SS, 70 * SS), (rx(515) * SS + 30 * SS, 600 * SS), (W * SS, 600 * SS)], fill=(20, 32, 56, 70))
    im = Image.alpha_composite(im, side.filter(ImageFilter.GaussianBlur(6 * SS)))
    d = ImageDraw.Draw(im)
    # linhas de perspectiva bem fracas
    for x in (-400, -150, 120, 330, 694, 904, 1174, 1424):
        line(d, [(512, 300), (x, 600)], (26, 34, 48, 255), 1)
    # faixa inferior
    line(d, [(20, 548), (1004, 548)], (34, 42, 58, 255), 1)
    # linha do indicador de tela (o pedaco verde e de cada tela)
    line(d, [(402, 18), (622, 18)], (52, 60, 70, 255), 2)
    # combustivel: trilho
    for x0, x1 in ((70, 112), (116, 158), (162, 204)):
        line(d, [(x0, 582), (x1, 582)], GREY + (255,), 2)
    text(d, 218, 584, "→|", 20, WHITE)
    ic_fuel(d, 44, 574, WHITE)
    # faixa central: icone de pista
    for i in range(5):
        yy = 552 + i * 9; half = 14 + i * 3
        line(d, [(512 - half, yy), (512 + half, yy)], (150, 158, 170, 255), 3)
    line(d, [(489, 552), (478, 594)], (150, 158, 170, 255), 3); line(d, [(535, 552), (546, 594)], (150, 158, 170, 255), 3)
    text(d, 586, 74, "km/h", 14, WHITE)
    ic_water(d, 634, 574, WHITE)
    ic_t1(d, 750, 586, WHITE, 24)
    ic_adblue(d, 882, 580, WHITE)
    for x0, x1 in ((912, 950), (954, 1000)):
        line(d, [(x0, 582), (x1, 582)], GREY + (255,), 2)
    return save(im, "fundo.png")

# ------------------------------------------------------------------ escalas (telas 1 a 4)
def make_escalas():
    im, d = layer()
    # velocidade
    line(d, [(lx(Y1 - 8), Y1 - 8), (lx(Y0), Y0)], GREY + (255,), 2)
    for v in range(0, VEL_MAX + 1, 5):
        y = vy(v); ln = 12 if v % 20 == 0 else (8 if v % 10 == 0 else 5)
        if v <= 30 or v % 10 == 0:
            line(d, [(lx(y), y), (lx(y) + ln, y)], GREY + (255,), 2)
        if v % 20 == 0 and v <= 80:
            text(d, lx(y) + 18, y + 9, str(v), 24, WHITE)
    text(d, lx(Y0) - 12, Y0 + 30, "km/h", 14, GREY)
    line(d, [(lx(Y0) - 8, Y0), (lx(Y0) + 26, Y0)], TEAL + (255,), 4)
    # conta-giros
    line(d, [(rx(Y1 - 8), Y1 - 8), (rx(Y0), Y0)], GREY + (255,), 2)
    for r in range(0, RPM_MAX + 1, 250):
        y = ry(r); ln = 12 if r % 500 == 0 else 6
        if r <= 750 or r % 500 == 0:
            line(d, [(rx(y) - ln, y), (rx(y), y)], GREY + (255,), 2)
        if r % 500 == 0 and r <= 2000:
            text(d, rx(y) - 18, y + 9, str(r // 100), 24, WHITE, anchor="rs")
    line(d, [(rx(ry(2250)), ry(2250)), (rx(Y1 - 8), Y1 - 8)], RED + (255,), 4)
    text(d, rx(Y0) - 30, Y0 + 30, "rpm x100", 14, GREY)
    line(d, [(rx(Y0) - 26, Y0), (rx(Y0) + 8, Y0)], TEAL + (255,), 4)
    # circuitos de ar (esquerda)
    for i, t in enumerate(("1", "2", "P")):
        y = 250 + i * 50
        ic_circle_txt(d, 92, y, t, WHITE, 11, 13)
        line(d, [(118, y + 4), (190, y + 4)], GREY + (255,), 2)
        line(d, [(198, y + 4), (242, y + 4)], TEAL_D + (255,), 2)
    # consumo (direita)
    ic_avg(d, 800, 248, WHITE)
    ic_pump_lh(d, 802, 300, WHITE)
    line(d, [(790, 330), (990, 330)], (110, 118, 128, 255), 2)
    return save(im, "escalas.png")

def make_bars():
    out = {}
    im, d = layer(); line(d, [(lx(Y0), Y0), (lx(Y1), Y1)], TEAL + (255,), 5); out["vel"] = save(im, "velocidade_cheio.png")
    im, d = layer(); line(d, [(rx(Y0), Y0), (rx(ry(2250)), ry(2250))], TEAL + (255,), 5)
    line(d, [(rx(ry(2250)), ry(2250)), (rx(Y1), Y1)], RED + (255,), 5); out["rpm"] = save(im, "rpm_cheio.png")
    im, d = layer()
    for i in range(3): line(d, [(118, 250 + i * 50 - 3), (242, 250 + i * 50 - 3)], TEAL + (255,), 4)
    out["ar"] = save(im, "ar_cheio.png")
    im, d = layer(); line(d, [(790, 330), (990, 330)], TEAL + (255,), 3); out["lh"] = save(im, "consumo_hora_cheio.png")
    im, d = layer(); line(d, [(70, 575), (204, 575)], TEAL + (255,), 4); out["comb"] = save(im, "combustivel_cheio.png")
    im, d = layer(); line(d, [(912, 575), (1000, 575)], TEAL + (255,), 4); out["arla"] = save(im, "arla_cheio.png")
    # barras grandes das telas de combustivel e temperatura
    CX0, CX1 = 410, 614
    im, d = layer(); line(d, [(CX0, 280), (CX1, 280)], TEAL + (255,), 5); out["comb_g"] = save(im, "combustivel_grande_cheio.png")
    im, d = layer(); line(d, [(CX0, 372), (CX1, 372)], TEAL + (255,), 5); out["arla_g"] = save(im, "arla_grande_cheio.png")
    im, d = layer(); line(d, [(CX0, 268), (CX1, 268)], TEAL + (255,), 5); out["agua_g"] = save(im, "agua_cheio.png")
    im, d = layer(); line(d, [(CX0, 338), (CX1, 338)], TEAL + (255,), 5); out["oleo_g"] = save(im, "oleo_cheio.png")
    return out

def make_indicador(i, n):
    im, d = layer()
    seg = 220.0 / n
    line(d, [(402 + seg * i + 4, 18), (402 + seg * (i + 1) - 4, 18)], TEAL_D + (255,), 3)
    return save(im, "indicador_%d.png" % (i + 1))

def make_tela_viagem():
    im, d = layer()
    text(d, 512, 196, "Viagem", 20, WHITE, anchor="ms", w="Bold")
    ic_avg(d, 414, 250, WHITE); ic_clock_note(d, 548, 248, WHITE)
    ic_speed_avg(d, 414, 312, WHITE); ic_t1(d, 538, 324, WHITE, 26)
    return save(im, "tela_viagem.png")

def make_tela_combustivel():
    im, d = layer()
    text(d, 512, 196, "Nível de combustível", 20, WHITE, anchor="ms", w="Bold")
    ic_fuel(d, 420, 250, WHITE)
    for y in (288, 380):
        for k in range(4):
            x0 = 410 + k * 51
            line(d, [(x0 + 2, y), (x0 + 49, y)], GREY + (255,), 2)
    ic_adblue(d, 412, 348, WHITE)
    return save(im, "tela_combustivel.png")

def make_tela_temperatura():
    im, d = layer()
    text(d, 512, 196, "Temperatura", 20, WHITE, anchor="ms", w="Bold")
    ic_water(d, 420, 244, WHITE); ic_oil(d, 422, 318, WHITE)
    for y in (276, 346):
        line(d, [(410, y), (580, y)], GREY + (255,), 2)
        line(d, [(588, y), (614, y)], (190, 196, 204, 255), 2)
    text(d, 410, 384, "0", 15, WHITE); text(d, 614, 384, "130", 15, WHITE, anchor="rs")
    return save(im, "tela_temperatura.png")

def make_estrada(big):
    im, d = layer()
    if big:
        road, rd = layer()
        rd.polygon([(462 * SS, 300 * SS), (562 * SS, 300 * SS), (700 * SS, 548 * SS), (324 * SS, 548 * SS)], fill=(26, 32, 44, 200))
        im = Image.alpha_composite(im, road.filter(ImageFilter.GaussianBlur(3 * SS))); d = ImageDraw.Draw(im)
    a, b = ((484, 300), (390, 548)) if big else ((488, 380), (430, 548))
    line(d, [a, b], (205, 210, 218, 255), 7 if big else 6)
    line(d, [(1024 - a[0], a[1]), (1024 - b[0], b[1])], (205, 210, 218, 255), 7 if big else 6)
    return save(im, "estrada%s.png" % ("_grande" if big else ""))

def make_limit(x, y, name):
    im, d = layer()
    d.ellipse(P(x - 24, y - 24, x + 24, y + 24), outline=RED + (255,), width=int(3.5 * SS))
    return save(im, name)

# ------------------------------------------------------------------ luzes (aceso e apagado)
LUZES = [
    # dado, x, y, cor, desenho
    ("seta_esq", 52, 52, GREEN, lambda d, x, y, c: ic_arrow(d, x, y, c, 1)),
    ("farol_baixo", 100, 52, GREEN, lambda d, x, y, c: ic_lamp(d, x, y, c, False)),
    ("farol_alto", 148, 52, BLUE, lambda d, x, y, c: ic_lamp(d, x, y, c, True)),
    ("lanterna", 196, 52, GREEN, ic_park),
    ("aviso_combustivel", 244, 52, AMBER, lambda d, x, y, c: ic_fuel(d, x, y, c)),
    ("diferencial", 76, 100, AMBER, lambda d, x, y, c: ic_box(d, x, y, "DIF", c, 11)),
    ("eixo", 124, 100, AMBER, lambda d, x, y, c: ic_box(d, x, y, "EIXO", c, 10)),
    ("aviso_ar", 172, 100, RED, lambda d, x, y, c: ic_circle_txt(d, x, y, "AR", c, 13, 11)),
    ("aviso_bateria", 220, 100, RED, ic_battery),
    ("freio_mao", 780, 52, RED, ic_brake),
    ("freio_motor", 828, 52, AMBER, lambda d, x, y, c: ic_box(d, x, y, "FM", c)),
    ("retarder", 876, 52, AMBER, lambda d, x, y, c: ic_circle_txt(d, x, y, "R", c, 12, 14)),
    ("cruzeiro", 924, 52, GREEN, ic_cruise),
    ("seta_dir", 972, 52, GREEN, lambda d, x, y, c: ic_arrow(d, x, y, c, -1)),
    ("aviso_oleo", 804, 100, RED, ic_oil),
    ("aviso_agua", 852, 100, RED, ic_water),
    ("giroflex", 900, 100, AMBER, lambda d, x, y, c: ic_circle_txt(d, x, y, "!", c, 12, 16)),
    ("aviso_arla", 948, 100, AMBER, ic_adblue),
]

def make_luz(name, x, y, col, fn):
    for st in ("on", "off"):
        im, d = layer()
        if st == "on":
            g, gd = layer(); fn(gd, x, y, col + (255,))
            im = Image.alpha_composite(im, g.filter(ImageFilter.GaussianBlur(5 * SS)))
            d = ImageDraw.Draw(im)
            fn(d, x, y, col + (255,))
        else:
            fn(d, x, y, DIM + (255,))
        save(im, "%s_%s.png" % (name, st))

def make_trailer():
    for st, c in (("on", WHITE), ("off", DIM)):
        im, d = layer(); ic_trailer(d, 700, 66, c + (255,)); save(im, "carreta_%s.png" % st)

# ------------------------------------------------------------------ gera tudo
make_fundo(); make_escalas(); make_bars()
for i in range(5): make_indicador(i, 5)
make_tela_viagem(); make_tela_combustivel(); make_tela_temperatura()
make_estrada(False); make_estrada(True)
make_limit(600, 385, "limite_1.png"); make_limit(512, 268, "limite_5.png")
for l in LUZES: make_luz(*l)
make_trailer()

luzes_txt = "\n".join("luz %-18s on=%s_on.png off=%s_off.png" % (n, n, n) for n, *_ in LUZES)
txt = """# Scania Smart Dash - TruckDash
# Gerado por tools/gen_scania_smart_dash.py. Pode editar a vontade.
# Posicoes em pixels do fundo.png (1024 x 600). Linhas com # sao comentarios.
# Toque longo na metade ESQUERDA da tela recarrega este arquivo e os PNGs.
#
# Telas (carrossel): toque na direita = proxima, na esquerda = anterior.
#   "tela todas" ou "tela 1 2 3": as linhas seguintes aparecem so nessas telas.
#   1 = principal   2 = viagem   3 = combustivel   4 = temperatura   5 = so velocidade

fundo fundo.png

# ================================================================ todas as telas
tela todas
{luzes}
luz carreta on=carreta_on.png off=carreta_off.png
texto marcha x=650 y=92 altura=24 cor=FFFFFF alinha=centro fonte=negrito

# faixa de baixo
barra combustivel img=combustivel_cheio.png de=0 ate=100 sentido=direita
texto autonomia   x=248 y=586 altura=22 cor=FFFFFF alinha=esquerda fonte=negrito sufixo=" km"
texto hora        x=380 y=586 altura=22 cor=FFFFFF alinha=centro fonte=negrito
texto agua        x=648 y=586 altura=22 cor=FFFFFF alinha=esquerda fonte=negrito sufixo=" °C"
texto viagem_km   x=860 y=586 altura=20 cor=FFFFFF alinha=direita fonte=negrito casas=1 sufixo=" km"
barra arla img=arla_cheio.png de=0 ate=100 sentido=direita

# ================================================================ telas 1 a 4: escalas e laterais
tela 1 2 3 4
imagem escalas.png
barra velocidade img=velocidade_cheio.png de=0 ate=100 sentido=cima
barra rpm        img=rpm_cheio.png        de=0 ate=2500 sentido=cima
texto velocidade x=512 y=150 altura=96 cor=FFFFFF alinha=centro fonte=normal
barra ar img=ar_cheio.png de=0 ate=10 sentido=direita
texto consumo      x=832 y=257 altura=19 cor=FFFFFF alinha=esquerda fonte=negrito casas=1 sufixo=" l/100km"
texto consumo_hora x=832 y=307 altura=19 cor=FFFFFF alinha=esquerda fonte=negrito casas=1 sufixo=" l/h"
barra consumo_hora img=consumo_hora_cheio.png de=0 ate=100 sentido=direita

# ================================================================ tela 1: principal
tela 1
imagem indicador_1.png
imagem estrada.png
# imagem logo.png        (coloque aqui o seu logo, se quiser)
imagem limite_1.png
texto limite x=600 y=393 altura=22 cor=FFFFFF alinha=centro fonte=negrito vazio="--"

# ================================================================ tela 2: viagem
tela 2
imagem indicador_2.png
imagem tela_viagem.png
texto consumo      x=436 y=257 altura=15 cor=FFFFFF alinha=esquerda fonte=negrito casas=1 sufixo=" l/100km"
texto viagem_tempo x=566 y=257 altura=17 cor=FFFFFF alinha=esquerda fonte=negrito
texto viagem_vel   x=436 y=319 altura=15 cor=FFFFFF alinha=esquerda fonte=negrito sufixo=" km/h"
texto viagem_km    x=566 y=319 altura=17 cor=FFFFFF alinha=esquerda fonte=negrito casas=0 sufixo=" km"

# ================================================================ tela 3: combustivel
tela 3
imagem indicador_3.png
imagem tela_combustivel.png
texto litros      x=440 y=258 altura=20 cor=FFFFFF alinha=esquerda fonte=negrito sufixo=" l"
barra combustivel img=combustivel_grande_cheio.png de=0 ate=100 sentido=direita
texto combustivel x=410 y=316 altura=18 cor=FFFFFF alinha=esquerda fonte=negrito sufixo=" %"
barra arla        img=arla_grande_cheio.png de=0 ate=100 sentido=direita
texto arla        x=410 y=408 altura=18 cor=FFFFFF alinha=esquerda fonte=negrito sufixo=" %"

# ================================================================ tela 4: temperatura
tela 4
imagem indicador_4.png
imagem tela_temperatura.png
barra agua img=agua_cheio.png de=0 ate=130 sentido=direita
barra oleo img=oleo_cheio.png de=0 ate=130 sentido=direita
texto agua x=614 y=252 altura=16 cor=9AA2AB alinha=direita sufixo=" °C"
texto oleo x=614 y=322 altura=16 cor=9AA2AB alinha=direita sufixo=" °C"

# ================================================================ tela 5: so velocidade
tela 5
imagem indicador_5.png
imagem estrada_grande.png
texto velocidade x=512 y=210 altura=120 cor=FFFFFF alinha=centro fonte=normal
imagem limite_5.png
texto limite x=512 y=276 altura=22 cor=FFFFFF alinha=centro fonte=negrito vazio="--"
""".format(luzes=luzes_txt)
with open(os.path.join(OUT, "painel.txt"), "w", encoding="utf-8", newline="\r\n") as f:
    f.write(txt)
print("ok:", OUT, len(os.listdir(OUT)), "arquivos")
