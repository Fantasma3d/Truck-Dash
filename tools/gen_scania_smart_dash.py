#!/usr/bin/env python3
"""Gera o painel "Scania Smart Dash" do TruckDash: PNGs em camadas + painel.txt.

Tamanho 1440 x 540 (8:3), o mesmo do painel do jogo: no tablet sobram faixas pretas em
cima e embaixo, como na tela do caminhao ("cinema").
Disposicao tirada das fotos de referencia (pasta "Painel smart dash") e das posicoes do
painel scania_2025 do jogo. Desenhos proprios; sem o logotipo da Scania (coloque logo.png
e descomente a linha no painel.txt se quiser).

Uso: python3 tools/gen_scania_smart_dash.py <pasta-de-saida>
"""
import os, sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter

W, H, SS = 1440, 540, 2
OUT = sys.argv[1] if len(sys.argv) > 1 else "scania-smart-dash"
os.makedirs(OUT, exist_ok=True)

TEAL = (64, 224, 208)
GREY = (150, 156, 166)
LANE = (150, 156, 168)
WHITE = (238, 241, 245)
DIM = (46, 50, 58)            # luz apagada (como nas fotos)
GREEN = (0, 178, 159)         # cores do painel do jogo
BLUE = (73, 136, 243)
AMBER = (241, 194, 27)
RED = (235, 10, 55)
BLIND = (40, 95, 200)
FONT_DIR = "/usr/share/fonts/opentype/inter/"

_fc = {}
def font(px, w="Medium"):
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

def P(*v): return [x * SS for x in v]
def Q(pts): return [(x * SS, y * SS) for x, y in pts]
def line(d, pts, col, w): d.line(Q(pts), fill=col, width=max(1, int(w * SS)), joint="curve")
def text(d, x, y, s, px, col, anchor="ls", w="Medium"): d.text((x * SS, y * SS), s, font=font(px, w), fill=col, anchor=anchor)
def lerp(a, b, t): return a + (b - a) * t
def stroke(c, w=2.2): return dict(outline=c, width=int(w * SS))

# ------------------------------------------------------------------ geometria (fotos -> 1440x540)
VEL_MAX, RPM_MAX = 120, 2500
YB, YT = 455, 185                         # posicao do 0 e do maximo nas escalas
L_TOP, L_BOT = (275, 165), (352, 462)     # linha da velocidade
R_TOP, R_BOT = (1165, 165), (1088, 462)   # linha do conta-giros
def lx(y): return lerp(L_BOT[0], L_TOP[0], (L_BOT[1] - y) / (L_BOT[1] - L_TOP[1]))
def rx(y): return lerp(R_BOT[0], R_TOP[0], (R_BOT[1] - y) / (R_BOT[1] - R_TOP[1]))
def vy(v): return YB - (YB - YT) * v / VEL_MAX
def ry(r): return YB - (YB - YT) * r / RPM_MAX
LL = ((557, 469), (714, 361))             # faixas da estrada (foto sem logo)
LR = ((987, 469), (827, 361))

# ------------------------------------------------------------------ icones (desenho proprio, no estilo das fotos)
def ic_lowbeam(d, x, y, c):
    d.chord(P(x - 2, y - 11, x + 16, y + 11), 270, 90, **stroke(c))
    line(d, [(x + 3, y - 11), (x + 3, y + 11)], c, 2.2)
    for i in range(4):
        yy = y - 8 + i * 5.5; line(d, [(x - 13, yy + 4), (x - 3, yy)], c, 2)

def ic_highbeam(d, x, y, c):
    d.chord(P(x - 2, y - 11, x + 16, y + 11), 270, 90, **stroke(c))
    line(d, [(x + 3, y - 11), (x + 3, y + 11)], c, 2.2)
    for i in range(4):
        yy = y - 8 + i * 5.5; line(d, [(x - 13, yy), (x - 3, yy)], c, 2)

def ic_beacon(d, x, y, c):
    d.rounded_rectangle(P(x - 7, y - 6, x + 7, y + 8), radius=6 * SS, fill=c)
    d.rectangle(P(x - 11, y + 8, x + 11, y + 11), fill=c)
    for a, b in (((x, y - 15), (x, y - 11)), ((x - 12, y - 9), (x - 9, y - 6)), ((x + 12, y - 9), (x + 9, y - 6)),
                 ((x - 15, y + 1), (x - 11, y + 1)), ((x + 15, y + 1), (x + 11, y + 1))):
        line(d, [a, b], c, 2)

def ic_ring(d, x, y, c):
    d.arc(P(x - 15, y - 13, x + 15, y + 13), 115, 245, fill=c, width=int(2 * SS))
    d.arc(P(x - 15, y - 13, x + 15, y + 13), -65, 65, fill=c, width=int(2 * SS))
    d.ellipse(P(x - 10, y - 10, x + 10, y + 10), **stroke(c))

def ic_brake_warn(d, x, y, c):
    ic_ring(d, x, y, c); line(d, [(x, y - 6), (x, y + 2)], c, 2.4); d.ellipse(P(x - 1.5, y + 4, x + 1.5, y + 7), fill=c)

def ic_parking(d, x, y, c):
    ic_ring(d, x, y, c); text(d, x, y + 1, "P", 13, c, "mm", "Bold")

def ic_engine_brake(d, x, y, c):
    ic_ring(d, x, y, c); line(d, [(x - 6, y), (x + 4, y)], c, 2)
    d.polygon(Q([(x + 7, y), (x + 2, y - 4), (x + 2, y + 4)]), fill=c)

def ic_retarder(d, x, y, c):
    ic_ring(d, x, y, c)
    d.ellipse(P(x - 7, y - 3.5, x, y + 3.5), **stroke(c, 1.8)); d.ellipse(P(x, y - 3.5, x + 7, y + 3.5), **stroke(c, 1.8))

def ic_battery(d, x, y, c):
    d.rounded_rectangle(P(x - 14, y - 8, x + 14, y + 10), radius=2 * SS, **stroke(c, 2))
    line(d, [(x - 9, y - 8), (x - 9, y - 12)], c, 3); line(d, [(x + 9, y - 8), (x + 9, y - 12)], c, 3)
    line(d, [(x - 11, y + 1), (x - 5, y + 1)], c, 1.8); line(d, [(x + 5, y + 1), (x + 11, y + 1)], c, 1.8)
    line(d, [(x + 8, y - 2), (x + 8, y + 4)], c, 1.8)

def ic_lift(d, x, y, c, trailer):
    if trailer: d.rectangle(P(x - 13, y - 11, x + 13, y - 8), fill=c)
    else: line(d, [(x - 13, y - 9), (x + 13, y - 9)], c, 2)
    for k in (-6, 6): d.ellipse(P(x + k - 5, y - 4, x + k + 5, y + 6), **stroke(c, 2))
    line(d, [(x + 6, y + 8), (x + 6, y + 14)], c, 2)
    d.polygon(Q([(x + 6, y + 6), (x + 3, y + 10), (x + 9, y + 10)]), fill=c)

def ic_arrow(d, x, y, c, sgn):
    pts = [(-20, 0), (-6, -12), (-6, -5), (8, -5), (8, 5), (-6, 5), (-6, 12)]
    d.polygon(Q([(x + px * sgn, y + py) for px, py in pts]), fill=c)
    a, b = sorted((x + 12 * sgn, x + 18 * sgn))
    d.rectangle(P(a, y - 4, b, y + 4), fill=c)

def ic_cruise(d, x, y, c):
    d.arc(P(x - 12, y - 10, x + 12, y + 14), 140, 400, fill=c, width=int(2 * SS))
    line(d, [(x, y + 2), (x - 5, y - 6)], c, 2.2)
    d.ellipse(P(x - 9, y - 15, x - 5, y - 11), fill=c)

def ic_hpattern(d, x, y, c):
    for xx in (x - 8, x, x + 8): line(d, [(xx, y - 10), (xx, y + 10)], c, 2.6)
    line(d, [(x - 8, y), (x + 8, y)], c, 2.6)

def ic_oilcan(d, x, y, c, waves):
    d.polygon(Q([(x - 14, y - 4), (x + 4, y - 4), (x + 14, y - 9), (x + 6, y + 5), (x - 14, y + 5)]), **stroke(c, 2))
    line(d, [(x - 5, y - 4), (x - 5, y - 9)], c, 2); line(d, [(x - 10, y - 9), (x, y - 9)], c, 2)
    d.ellipse(P(x + 13, y - 3, x + 16, y + 2), fill=c)
    if waves:
        for k in (10, 14):
            line(d, [(x - 15 + i * 3, y + k + (1.3 if i % 2 else -1.3)) for i in range(11)], c, 1.6)

def ic_water(d, x, y, c):
    line(d, [(x, y - 14), (x, y + 4)], c, 2.6)
    for i in range(3): line(d, [(x, y - 11 + i * 5), (x + 7, y - 11 + i * 5)], c, 1.6)
    d.ellipse(P(x - 3.5, y + 1, x + 3.5, y + 8), fill=c)
    for k in (9, 13): line(d, [(x - 14 + i * 3, y + k + (1.3 if i % 2 else -1.3)) for i in range(10)], c, 1.6)

def ic_fuel(d, x, y, c):
    d.rounded_rectangle(P(x - 9, y - 12, x + 4, y + 12), radius=2 * SS, fill=c)
    d.rectangle(P(x - 6, y - 9, x + 1, y - 3), fill=(12, 15, 22, 255))
    line(d, [(x + 4, y - 4), (x + 9, y - 1), (x + 9, y + 8), (x + 12, y + 8), (x + 12, y - 7)], c, 2)

def ic_fuel_out(d, x, y, c):
    d.rounded_rectangle(P(x - 9, y - 12, x + 4, y + 12), radius=2 * SS, **stroke(c, 2))
    d.rectangle(P(x - 6, y - 9, x + 1, y - 3), fill=c)
    line(d, [(x + 4, y - 4), (x + 9, y - 1), (x + 9, y + 8), (x + 12, y + 8), (x + 12, y - 7)], c, 2)

def ic_range(d, x, y, c):
    d.ellipse(P(x - 22, y - 3, x - 16, y + 3), fill=c)
    line(d, [(x - 18, y), (x + 4, y)], c, 2.4); d.polygon(Q([(x + 8, y), (x + 1, y - 5), (x + 1, y + 5)]), fill=c)
    line(d, [(x + 13, y - 12), (x + 13, y + 12)], c, 2.6)

def ic_adblue(d, x, y, c):
    text(d, x - 10, y - 2, "Ad", 11, c, w="Bold"); text(d, x - 10, y + 9, "Blue", 11, c, w="Bold")
    d.ellipse(P(x + 5, y - 13, x + 10, y - 6), fill=(80, 170, 255))

def ic_clock_note(d, x, y, c):
    d.rounded_rectangle(P(x - 11, y - 12, x + 11, y + 12), radius=2 * SS, **stroke(c))
    for i in range(3): line(d, [(x - 6, y - 6 + i * 5), (x + 6, y - 6 + i * 5)], c, 1.6)

def ic_avg(d, x, y, c, speed=False):
    if speed:
        d.arc(P(x - 12, y - 12, x + 12, y + 12), 150, 390, fill=c, width=int(2.2 * SS)); line(d, [(x, y), (x + 7, y - 7)], c, 2.2)
    else:
        ic_fuel_out(d, x, y - 2, c)
    text(d, x + 10, y + 12, "AVG", 9, c, w="Bold")

# ------------------------------------------------------------------ fundo (todas as telas)
def make_fundo():
    base, bd = layer()
    bd.rounded_rectangle(P(0, 0, W, H), radius=26 * SS, fill=(9, 11, 16, 255))
    glow, g = layer()
    for cx, cy, r, col in ((110, 600, 420, (28, 52, 105, 190)), (1330, 600, 420, (28, 52, 105, 190)), (720, 620, 380, (22, 38, 80, 120))):
        g.ellipse(P(cx - r, cy - r, cx + r, cy + r), fill=col)
    glow = glow.filter(ImageFilter.GaussianBlur(110 * SS))
    tun, td = layer()   # centro mais escuro entre as escalas
    td.polygon(Q([(L_TOP[0] + 10, 60), (R_TOP[0] - 10, 60), (R_BOT[0] - 40, 520), (L_BOT[0] + 40, 520)]), fill=(0, 0, 0, 120))
    tun = tun.filter(ImageFilter.GaussianBlur(25 * SS))
    im = Image.alpha_composite(Image.alpha_composite(base, glow), tun)
    im.putalpha(base.split()[3])
    d = ImageDraw.Draw(im)
    for x in (-500, -120, 300, 560, 880, 1140, 1560, 1940):
        line(d, [(720, 300), (x, 540)], (30, 40, 58, 255), 1)
    line(d, [(480, 30), (960, 30)], (44, 50, 60, 255), 3)
    # faixa de baixo
    ic_fuel(d, 65, 520, WHITE)
    ic_range(d, 292, 520, WHITE)
    text(d, 1102, 533, "T1", 26, WHITE)
    ic_adblue(d, 1319, 520, WHITE)
    # topo
    ic_cruise(d, 430, 180, WHITE)
    text(d, 845, 184, "km/h", 15, GREY)
    ic_hpattern(d, 929, 182, GREY)
    # lateral esquerda: pressao do oleo, temperatura do oleo, agua
    for y, fn in ((302, lambda d, x, y, c: ic_oilcan(d, x, y, c, True)), (347, lambda d, x, y, c: ic_oilcan(d, x, y, c, False)), (389, ic_water)):
        fn(d, 72, y - 4, WHITE)
        line(d, [(108, y + 3), (270, y + 3)], (36, 42, 52, 255), 5)
    # lateral direita: consumo
    ic_fuel_out(d, 1199, 322, WHITE)
    line(d, [(1184, 342), (1385, 342)], (36, 42, 52, 255), 5)
    return save(im, "fundo.png")

def make_escalas():
    im, d = layer()
    line(d, [L_TOP, L_BOT], GREY + (255,), 2)
    line(d, [R_BOT, (rx(ry(2250)), ry(2250))], GREY + (255,), 2)
    line(d, [(rx(ry(2250)), ry(2250)), R_TOP], RED + (255,), 3)
    for v in range(0, VEL_MAX + 1, 10):
        y = vy(v); line(d, [(lx(y) + 4, y), (lx(y) + (11 if v % 20 == 0 else 7), y)], (90, 98, 110, 255), 1.5)
        if v % 20 == 0: text(d, lx(y) + 42, y + 8, str(v), 21, WHITE, "ms", "Regular")
    for r in range(0, RPM_MAX + 1, 250):
        y = ry(r); line(d, [(rx(y) - 4, y), (rx(y) - (11 if r % 500 == 0 else 7), y)], (90, 98, 110, 255), 1.5)
        if r % 500 == 0: text(d, rx(y) - 42, y + 8, str(r // 100), 21, WHITE, "ms", "Regular")
    return save(im, "escalas.png")

def wedge(fn_x, segs, inward, name):
    """faixa preenchida por dentro da linha (do 0 ao maximo); o app revela de baixo para cima"""
    im, d = layer()
    steps = 24
    for i in range(steps):
        t0, t1 = i / steps, (i + 1) / steps
        a = int(235 * (1 - t0) ** 1.6)
        for ys, ye, col in segs:
            d.polygon(Q([(fn_x(ys) + inward * 24 * t0, ys), (fn_x(ys) + inward * 24 * t1, ys),
                         (fn_x(ye) + inward * 24 * t1, ye), (fn_x(ye) + inward * 24 * t0, ye)]), fill=col + (a,))
    return save(im, name)

def make_bars():
    wedge(lx, [(vy(0), vy(VEL_MAX), TEAL)], +1, "velocidade_cheio.png")
    wedge(rx, [(ry(0), ry(2250), TEAL), (ry(2250), ry(RPM_MAX), RED)], -1, "rpm_cheio.png")
    for name, y in (("pressao_oleo_cheio.png", 302), ("oleo_cheio.png", 347), ("agua_cheio.png", 389)):
        im, d = layer(); line(d, [(108, y + 3), (270, y + 3)], TEAL + (255,), 5); save(im, name)
    im, d = layer(); line(d, [(1184, 342), (1385, 342)], TEAL + (255,), 5); save(im, "consumo_cheio.png")
    for name, y in (("combustivel_grande_cheio.png", 330), ("arla_grande_cheio.png", 410),
                    ("agua_grande_cheio.png", 318), ("oleo_grande_cheio.png", 392)):
        im, d = layer(); line(d, [(560, y), (880, y)], TEAL + (255,), 6); save(im, name)

def make_indicador(i, n):
    im, d = layer()
    seg = 480.0 / n
    line(d, [(480 + seg * i + 6, 30), (480 + seg * (i + 1) - 6, 30)], TEAL + (255,), 4)
    return save(im, "indicador_%d.png" % (i + 1))

def title(d, s): text(d, 720, 275, s, 20, WHITE, "ms", "SemiBold")

def make_tela_viagem():
    im, d = layer(); title(d, "Viagem")
    ic_avg(d, 575, 318, WHITE); ic_clock_note(d, 735, 316, WHITE)
    ic_avg(d, 575, 388, WHITE, True); text(d, 722, 400, "T1", 24, WHITE)
    return save(im, "tela_viagem.png")

def make_tela_combustivel():
    im, d = layer(); title(d, "Nível de combustível")
    ic_fuel_out(d, 572, 303, WHITE); ic_adblue(d, 572, 382, WHITE)
    for y in (338, 418):
        for k in range(4):
            x0 = 560 + k * 80; line(d, [(x0 + 3, y), (x0 + 77, y)], GREY + (255,), 2)
    return save(im, "tela_combustivel.png")

def make_tela_temperatura():
    im, d = layer(); title(d, "Temperatura")
    ic_water(d, 572, 298, WHITE); ic_oilcan(d, 574, 372, WHITE, False)
    for y in (326, 400):
        line(d, [(560, y), (840, y)], GREY + (255,), 2); line(d, [(848, y), (880, y)], (200, 205, 212, 255), 2)
    text(d, 560, 436, "0", 15, WHITE); text(d, 880, 436, "130", 15, WHITE, "rs")
    return save(im, "tela_temperatura.png")

# ------------------------------------------------------------------ estrada, faixas e ponto cego
def scale_pts(a, b, k, cx=720, cy=361):   # estrada maior (tela 5)
    f = lambda p: (cx + (p[0] - cx) * k, cy + (p[1] - cy) * k * 0.9 + (k - 1) * 40)
    return f(a), f(b)

def make_estrada(k, suf):
    ll, lr = scale_pts(*LL, k), scale_pts(*LR, k)
    im, d = layer()
    road, rd = layer()
    rd.polygon(Q([ll[1], lr[1], lr[0], ll[0]]), fill=(22, 25, 32, 230))
    im = Image.alpha_composite(im, road.filter(ImageFilter.GaussianBlur(4 * SS))); d = ImageDraw.Draw(im)
    line(d, [((ll[1][0] + lr[1][0]) / 2, ll[1][1]), ((ll[0][0] + lr[0][0]) / 2, ll[0][1])], (40, 44, 54, 255), 2)
    save(im, "estrada%s.png" % suf)
    for side, (a, b) in (("esq", ll), ("dir", lr)):
        for st, col in (("", LANE), ("_aviso", AMBER)):
            im, d = layer()
            d.polygon(Q([(a[0] - 5 * k, a[1]), (a[0] + 5 * k, a[1]), (b[0] + 2 * k, b[1]), (b[0] - 2 * k, b[1])]), fill=col + (255,))
            if st: im = Image.alpha_composite(im.filter(ImageFilter.GaussianBlur(6 * SS)), im)
            save(im, "faixa_%s%s%s.png" % (side, st, suf))
        im, d = layer(); sg = -1 if side == "esq" else 1          # ponto cego: brilho azul por fora da faixa
        for i in range(18):
            t0, t1 = i / 18, (i + 1) / 18
            o0, o1 = 10 + 90 * t0, 10 + 90 * t1
            d.polygon(Q([(a[0] + sg * o0 * k, a[1]), (a[0] + sg * o1 * k, a[1]), (b[0] + sg * o1 * k * 0.45, b[1]), (b[0] + sg * o0 * k * 0.45, b[1])]),
                      fill=BLIND + (int(200 * (1 - t0) ** 1.4),))
        save(im.filter(ImageFilter.GaussianBlur(3 * SS)), "ponto_cego_%s%s.png" % (side, suf))

def make_limite(cx, cy, r, suf):
    im, d = layer()
    d.ellipse(P(cx - r, cy - r, cx + r, cy + r), fill=(255, 255, 255, 255), outline=RED + (255,), width=int(r * 0.22 * SS))
    save(im, "limite%s.png" % suf)
    im, d = layer()   # placa de fim das restricoes (quando nao ha limite), como na foto
    d.ellipse(P(cx - r, cy - r, cx + r, cy + r), fill=(250, 250, 250, 255), outline=(30, 30, 30, 255), width=int(2 * SS))
    for o in (-9, -4.5, 0, 4.5, 9):
        line(d, [(cx - r * 0.62 + o, cy + r * 0.62 + o), (cx + r * 0.62 + o, cy - r * 0.62 + o)], (40, 40, 40, 255), 1.6)
    save(im, "sem_limite%s.png" % suf)

# ------------------------------------------------------------------ luzes: so as que aparecem nas fotos
LUZES = [
    ("farol_baixo", 60, 136, GREEN, ic_lowbeam),
    ("farol_alto", 110, 136, BLUE, ic_highbeam),
    ("giroflex", 162, 136, AMBER, ic_beacon),
    ("aviso_ar", 209, 136, RED, ic_brake_warn),
    ("freio_mao", 148, 168, RED, ic_parking),
    ("freio_motor", 195, 168, GREEN, ic_engine_brake),
    ("retarder", 241, 168, GREEN, ic_retarder),
    ("seta_esq", 395, 141, GREEN, lambda d, x, y, c: ic_arrow(d, x, y, c, 1)),
    ("seta_dir", 1039, 141, GREEN, lambda d, x, y, c: ic_arrow(d, x, y, c, -1)),
    ("aviso_bateria", 1268, 131, RED, ic_battery),
    ("eixo", 1327, 163, AMBER, lambda d, x, y, c: ic_lift(d, x, y, c, False)),
    ("eixo_carreta", 1382, 163, AMBER, lambda d, x, y, c: ic_lift(d, x, y, c, True)),
]

def make_luz(name, x, y, col, fn):
    for st in ("on", "off"):
        im, d = layer()
        if st == "on":
            g, gd = layer(); fn(gd, x, y, col + (255,))
            im = Image.alpha_composite(im, g.filter(ImageFilter.GaussianBlur(4 * SS))); d = ImageDraw.Draw(im)
            fn(d, x, y, col + (255,))
        else:
            fn(d, x, y, DIM + (255,))
        save(im, "%s_%s.png" % (name, st))

# ------------------------------------------------------------------ gera tudo
for f in os.listdir(OUT):
    if f.endswith(".png") or f == "painel.txt": os.remove(os.path.join(OUT, f))
make_fundo(); make_escalas(); make_bars()
for i in range(5): make_indicador(i, 5)
make_tela_viagem(); make_tela_combustivel(); make_tela_temperatura()
make_estrada(1.0, ""); make_estrada(1.35, "_grande")
make_limite(854, 330, 28, ""); make_limite(720, 262, 26, "_5")
for l in LUZES: make_luz(*l)

luzes_txt = "\n".join("luz %-14s on=%s_on.png off=%s_off.png" % (n, n, n) for n, *_ in LUZES)
txt = """# Scania Smart Dash - TruckDash  (gerado por tools/gen_scania_smart_dash.py)
# Painel 1440 x 540 (8:3, igual ao do jogo). Posicoes em pixels do fundo.png.
# Toque longo na metade ESQUERDA recarrega este arquivo e os PNGs.
# Telas: 1 principal  2 viagem  3 combustivel  4 temperatura  5 so velocidade
#   "tela todas" / "tela 1 2 3": as linhas seguintes aparecem so nessas telas.

fundo fundo.png

# ================================================================ todas as telas
tela todas
{luzes}

texto retarder     x=238 y=222 altura=26 cor=40E0D0 alinha=centro
texto cruzeiro     x=430 y=226 altura=24 cor=40E0D0 alinha=centro vazio="-"
texto marcha       x=929 y=230 altura=28 cor=C8CDD4 alinha=centro

barra pressao_oleo img=pressao_oleo_cheio.png de=0 ate=6 sentido=direita
barra oleo         img=oleo_cheio.png         de=40 ate=130 sentido=direita
barra agua         img=agua_cheio.png         de=40 ate=130 sentido=direita
texto consumo_inst x=1225 y=329 altura=17 cor=C8CDD4 alinha=esquerda casas=1 sufixo=" l/100km"
barra consumo_inst img=consumo_cheio.png de=0 ate=80 sentido=direita

# faixa de baixo
texto autonomia    x=330  y=533 altura=22 cor=FFFFFF alinha=esquerda sufixo=" km"
texto nav_tempo    x=495  y=533 altura=22 cor=FFFFFF alinha=centro
texto hora         x=717  y=533 altura=22 cor=FFFFFF alinha=centro
texto viagem_km    x=1145 y=533 altura=22 cor=FFFFFF alinha=esquerda casas=1 sufixo=" km"
texto arla         x=1346 y=533 altura=22 cor=40E0D0 alinha=esquerda sufixo=" %"

# ================================================================ telas 1 a 4: escalas e velocidade
tela 1 2 3 4
imagem escalas.png
barra velocidade img=velocidade_cheio.png de=0 ate=120  sentido=cima
barra rpm        img=rpm_cheio.png        de=0 ate=2500 sentido=cima
texto velocidade x=716 y=233 altura=92 cor=FFFFFF alinha=centro

# ================================================================ tela 1: principal
tela 1
imagem indicador_1.png
imagem estrada.png
# imagem logo.png cx=716 cy=300 altura=104 largura=190   (seu logo, no lugar do emblema)
luz faixa_esq      on=faixa_esq_aviso.png off=faixa_esq.png
luz faixa_dir      on=faixa_dir_aviso.png off=faixa_dir.png
luz ponto_cego_esq on=ponto_cego_esq.png
luz ponto_cego_dir on=ponto_cego_dir.png
luz limite on=sem_limite.png inverte=sim
luz limite on=limite.png
texto limite x=854 y=339 altura=22 cor=000000 alinha=centro fonte=negrito

# ================================================================ tela 2: viagem
tela 2
imagem indicador_2.png
imagem tela_viagem.png
texto consumo      x=596 y=326 altura=19 cor=FFFFFF alinha=esquerda casas=1 sufixo=" l/100km"
texto viagem_tempo x=756 y=326 altura=19 cor=FFFFFF alinha=esquerda
texto viagem_vel   x=596 y=396 altura=19 cor=FFFFFF alinha=esquerda sufixo=" km/h"
texto viagem_km    x=756 y=396 altura=19 cor=FFFFFF alinha=esquerda casas=1 sufixo=" km"

# ================================================================ tela 3: combustivel
tela 3
imagem indicador_3.png
imagem tela_combustivel.png
texto litros      x=592 y=311 altura=19 cor=FFFFFF alinha=esquerda sufixo=" l"
barra combustivel img=combustivel_grande_cheio.png de=0 ate=100 sentido=direita
texto combustivel x=560 y=365 altura=17 cor=FFFFFF alinha=esquerda sufixo=" %"
barra arla        img=arla_grande_cheio.png de=0 ate=100 sentido=direita
texto arla        x=560 y=445 altura=17 cor=FFFFFF alinha=esquerda sufixo=" %"

# ================================================================ tela 4: temperatura
tela 4
imagem indicador_4.png
imagem tela_temperatura.png
barra agua img=agua_grande_cheio.png de=0 ate=130 sentido=direita
barra oleo img=oleo_grande_cheio.png de=0 ate=130 sentido=direita
texto agua x=880 y=305 altura=16 cor=9AA2AB alinha=direita sufixo=" °C"
texto oleo x=880 y=379 altura=16 cor=9AA2AB alinha=direita sufixo=" °C"

# ================================================================ tela 5: so velocidade
tela 5
imagem indicador_5.png
imagem estrada_grande.png
luz faixa_esq      on=faixa_esq_aviso_grande.png off=faixa_esq_grande.png
luz faixa_dir      on=faixa_dir_aviso_grande.png off=faixa_dir_grande.png
luz ponto_cego_esq on=ponto_cego_esq_grande.png
luz ponto_cego_dir on=ponto_cego_dir_grande.png
texto velocidade x=720 y=215 altura=120 cor=FFFFFF alinha=centro
luz limite on=sem_limite_5.png inverte=sim
luz limite on=limite_5.png
texto limite x=720 y=270 altura=20 cor=000000 alinha=centro fonte=negrito
""".format(luzes=luzes_txt)
with open(os.path.join(OUT, "painel.txt"), "w", encoding="utf-8", newline="\r\n") as f:
    f.write(txt)
print("ok:", OUT, len(os.listdir(OUT)), "arquivos")
