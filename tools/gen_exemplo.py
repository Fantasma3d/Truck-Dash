#!/usr/bin/env python3
"""Gera um painel de EXEMPLO (arte provisoria) no formato de camadas PNG, para testar o motor.
Todas as camadas sao exportadas no tamanho do painel inteiro: o app corta as bordas transparentes sozinho."""
import math, os, sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter

W, H, SS = 1280, 480, 3
out = sys.argv[1] if len(sys.argv) > 1 else "painel-exemplo"
os.makedirs(out, exist_ok=True)

def font(px):
    for f in ("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",):
        if os.path.exists(f): return ImageFont.truetype(f, px * SS)
    return ImageFont.load_default()

def layer(): 
    im = Image.new("RGBA", (W * SS, H * SS), (0, 0, 0, 0)); return im, ImageDraw.Draw(im)
def save(im, name): im.resize((W, H), Image.LANCZOS).save(os.path.join(out, name))
def P(*v): return [x * SS for x in v]
def pol(cx, cy, r, deg):   # 0 = para cima, horario
    a = math.radians(deg); return ((cx + r * math.sin(a)) * SS, (cy - r * math.cos(a)) * SS)
def arc(d, cx, cy, r, w, a0, a1, col):   # PIL: 0 = direita, horario
    d.arc(P(cx - r, cy - r, cx + r, cy + r), a0 - 90, a1 - 90, fill=col, width=w * SS)

LX, RX, CY, R = 250, 1030, 250, 180
# ---- fundo
im, d = layer()
for y in range(H * SS):
    v = int(18 - 14 * y / (H * SS)); d.line([(0, y), (W * SS, y)], fill=(v, v + 2, v + 6, 255))
for cx in (LX, RX):
    d.ellipse(P(cx - R - 14, CY - R - 14, cx + R + 14, CY + R + 14), fill=(8, 9, 12, 255), outline=(40, 44, 52, 255), width=2 * SS)
arc(d, LX, CY, R, 16, -135, 135, (38, 42, 50, 255))
for i in range(6):
    a = -135 + 270 * i / 5; d.text(pol(LX, CY, R - 44, a), str(i * 5), font=font(22), fill=(210, 215, 222, 255), anchor="mm")
d.text(P(LX, CY + 70), "rpm x100", font=font(14), fill=(110, 118, 130, 255), anchor="mm")
for v in range(0, 121, 10):
    a = -135 + 270 * v / 120
    d.line([pol(RX, CY, R - (22 if v % 20 == 0 else 12), a), pol(RX, CY, R, a)], fill=(220, 224, 230, 255), width=3 * SS)
    if v % 20 == 0: d.text(pol(RX, CY, R - 46, a), str(v), font=font(22), fill=(210, 215, 222, 255), anchor="mm")
d.text(P(RX, CY + 70), "km/h", font=font(14), fill=(110, 118, 130, 255), anchor="mm")
d.rounded_rectangle(P(470, 400, 810, 428), radius=8 * SS, fill=(26, 29, 35, 255))
d.text(P(455, 414), "E", font=font(16), fill=(200, 80, 60, 255), anchor="mm"); d.text(P(825, 414), "F", font=font(16), fill=(210, 215, 222, 255), anchor="mm")
d.text(P(640, 462), "EXEMPLO - troque estes PNGs pela arte do Scania Smart Dash", font=font(13), fill=(90, 96, 108, 255), anchor="mm")
save(im, "fundo.png")
# ---- arco do conta-giros e barra de combustivel (imagens "cheias"; o app revela so o pedaco certo)
im, d = layer(); arc(d, LX, CY, R, 16, -135, 135, (255, 110, 40, 255)); save(im, "rpm_cheio.png")
im, d = layer(); d.rounded_rectangle(P(474, 404, 806, 424), radius=6 * SS, fill=(70, 200, 110, 255)); save(im, "combustivel_cheio.png")
# ---- ponteiro (imagem pequena, apontando para cima; o pivo e o meio da base)
n = Image.new("RGBA", (20 * SS, 170 * SS), (0, 0, 0, 0)); nd = ImageDraw.Draw(n)
nd.polygon([(10 * SS, 0), (15 * SS, 170 * SS), (5 * SS, 170 * SS)], fill=(255, 70, 40, 255))
n.resize((20, 170), Image.LANCZOS).save(os.path.join(out, "ponteiro.png"))
# ---- luzes: um PNG apagado e um aceso para cada
def lamp(name, x, y, col, draw):
    for st in ("off", "on"):
        im, d = layer()
        c = col if st == "on" else (46, 50, 58)
        if st == "on":
            g, gd = layer(); gd.ellipse(P(x - 34, y - 34, x + 34, y + 34), fill=col + (90,))
            im = Image.alpha_composite(im, g.filter(ImageFilter.GaussianBlur(10 * SS))); d = ImageDraw.Draw(im)
        draw(d, x, y, c + (255,)); save(im, "%s_%s.png" % (name, st))
def seta(sig):
    return lambda d, x, y, c: d.polygon([((x - 24 * sig) * SS, y * SS), (x * SS, (y - 20) * SS), (x * SS, (y - 9) * SS), ((x + 22 * sig) * SS, (y - 9) * SS),
                                         ((x + 22 * sig) * SS, (y + 9) * SS), (x * SS, (y + 9) * SS), (x * SS, (y + 20) * SS)], fill=c)
def letra(t): return lambda d, x, y, c: (d.ellipse(P(x - 22, y - 22, x + 22, y + 22), outline=c, width=4 * SS), d.text(P(x, y), t, font=font(24), fill=c, anchor="mm"))
lamp("seta_esq", 520, 60, (60, 220, 90), seta(1)); lamp("seta_dir", 760, 60, (60, 220, 90), seta(-1))
lamp("farol_alto", 600, 60, (70, 140, 255), letra("A")); lamp("farol_baixo", 680, 60, (60, 220, 90), letra("B"))
lamp("freio_mao", 560, 350, (255, 60, 50), letra("P")); lamp("aviso_combustivel", 720, 350, (255, 190, 40), letra("C"))
lamp("cruzeiro", 640, 350, (60, 220, 90), letra("K"))

open(os.path.join(out, "painel.txt"), "w", newline="\r\n").write("""# Scania Smart Dash - painel por camadas PNG (arte de EXEMPLO)
# As posicoes sao em pixels do fundo.png. Linhas com # sao comentarios.
# Depois de mexer aqui ou trocar um PNG, toque na tela do tablet para recarregar.

fundo fundo.png

# luz <dado> on=<PNG aceso> off=<PNG apagado>
#   Se o PNG tiver o tamanho do fundo (camada exportada inteira), nao precisa de x/y.
#   Se for so o recorte do botao, informe x= e y= do canto de cima/esquerda.
luz seta_esq           on=seta_esq_on.png           off=seta_esq_off.png
luz seta_dir           on=seta_dir_on.png           off=seta_dir_off.png
luz farol_alto         on=farol_alto_on.png         off=farol_alto_off.png
luz farol_baixo        on=farol_baixo_on.png        off=farol_baixo_off.png
luz freio_mao          on=freio_mao_on.png          off=freio_mao_off.png
luz cruzeiro           on=cruzeiro_on.png           off=cruzeiro_off.png
luz aviso_combustivel  on=aviso_combustivel_on.png  off=aviso_combustivel_off.png

# arco: imagem do arco "cheio"; o app mostra so ate o valor atual. Angulos em graus, 0 = para cima, horario.
arco rpm img=rpm_cheio.png cx=250 cy=250 de=0 ang_de=-135 ate=2500 ang_ate=135

# barra: imagem da barra "cheia", revelada no sentido direita / esquerda / cima / baixo
barra combustivel img=combustivel_cheio.png de=0 ate=100 sentido=direita

# ponteiro: imagem apontando para cima; cx/cy = centro do mostrador no fundo
ponteiro velocidade img=ponteiro.png cx=1030 cy=250 de=0 ang_de=-135 ate=120 ang_ate=135

# texto <dado> x= y=(linha de base) altura= cor=RRGGBB alinha=esquerda|centro|direita fonte=normal|negrito|estreita
texto velocidade x=640 y=250 altura=130 cor=FFFFFF alinha=centro fonte=negrito
texto marcha     x=250 y=275 altura=80  cor=FFFFFF alinha=centro fonte=negrito
texto hora       x=640 y=140 altura=28  cor=9AA2AB alinha=centro
texto autonomia  x=640 y=300 altura=24  cor=9AA2AB alinha=centro sufixo=" km"
""")
print("ok", out, len(os.listdir(out)), "arquivos")
