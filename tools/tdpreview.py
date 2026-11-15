#!/usr/bin/env python3
"""Le um pacote .tdp e desenha uma previa (mesmo algoritmo de layout do app)."""
import sys, struct, math
import numpy as np
from PIL import Image

OP_END, OP_COLOR, OP_FONT, OP_OFFSET, OP_RET, OP_ALIGN, OP_ALIGN_END, OP_IMG, OP_TEXT, OP_MARGIN, OP_MARGIN_END = range(11)
K_RUNS, K_VALUE, K_BAR, K_GAUGE = 1, 2, 3, 4


class R:
    def __init__(self, b): self.b, self.p = b, 0
    def u(self, fmt):
        v = struct.unpack_from("<" + fmt, self.b, self.p); self.p += struct.calcsize("<" + fmt)
        return v if len(v) > 1 else v[0]
    def raw(self, n):
        v = self.b[self.p:self.p + n]; self.p += n; return v


def load(path):
    r = R(open(path, "rb").read())
    assert r.raw(4) == b"TDP1"
    W, H, ver, pages = r.u("HHHB")
    imgs = []
    for _ in range(r.u("H")):
        w, h = r.u("HH")
        imgs.append(Image.frombytes("RGBA", (w, h), r.raw(w * h * 4)))
    fonts = []
    for _ in range(r.u("B")):
        aw, ah, sc, pad, line = r.u("HHfff")
        gl = {}
        for _ in range(r.u("H")):
            v = r.u("H7f"); gl[v[0]] = v[1:]
        sdf = np.frombuffer(r.raw(aw * ah), np.uint8).reshape(ah, aw)
        fonts.append(dict(sc=sc, pad=pad, line=line, g=gl, sdf=sdf))
    elems = []
    for _ in range(r.u("H")):
        kind, page, v0, v1, v2, eid, x, y, w, h = r.u("BB3HH4f")
        e = dict(kind=kind, page=page, vis=[v for v in (v0, v1, v2) if v], id=eid, rect=(x, y, w, h))
        if kind == K_GAUGE:
            e["gauge"] = r.u("H7f")
        else:
            if kind == K_BAR: e["bar"] = r.u("B4f")
            n = r.u("H"); e["ops"] = decode_ops(r.raw(n))
        elems.append(e)
    return dict(W=W, H=H, pages=pages, imgs=imgs, fonts=fonts, elems=elems)


def decode_ops(b):
    r = R(b); ops = []
    while True:
        k = r.u("B")
        if k == OP_END: break
        if k == OP_COLOR: ops.append((k, r.u("I")))
        elif k == OP_FONT: ops.append((k,) + r.u("Bff"))
        elif k == OP_OFFSET: ops.append((k,) + r.u("ff"))
        elif k in (OP_RET, OP_ALIGN_END, OP_MARGIN_END): ops.append((k,))
        elif k == OP_MARGIN: ops.append((k,) + r.u("ff"))
        elif k == OP_ALIGN: ops.append((k,) + r.u("BBff"))
        elif k == OP_IMG: ops.append((k,) + r.u("Hff"))
        elif k == OP_TEXT: n = r.u("H"); ops.append((k, r.raw(n).decode("utf-8")))
    return ops


def layout(pk, ops, rect, vals):
    """devolve itens: ('img', idx, x, y, w, h, argb) / ('glyph', font, glyph, x, y, w, h, argb)"""
    x0, y0, rw, rh = rect
    px, py = x0, y0
    line_y, line_h = y0, 0.0
    color, font, fsx, fsy = 0xFFFFFFFF, 0, 1.0, 1.0
    items, stack, mstack = [], [], []
    ml_, mr_ = 0.0, 0.0
    for op in ops:
        k = op[0]
        if k == OP_COLOR: color = op[1]
        elif k == OP_FONT: font, fsx, fsy = op[1], op[2], op[3]
        elif k == OP_OFFSET: px += op[1]; py += op[2]
        elif k == OP_RET:
            px = x0; py = y0   # <ret> volta ao canto do elemento; posicoes vem dos <offset>
        elif k == OP_ALIGN: stack.append((len(items), op))
        elif k == OP_ALIGN_END and stack:
            start, a = stack.pop()
            blk = items[start:]
            if blk:
                xs = [it[-5] for it in blk] + [it[-5] + it[-3] for it in blk]
                ys = [it[-4] for it in blk] + [it[-4] + it[-2] for it in blk]
                mnx, mxx, mny, mxy = min(xs), max(xs), min(ys), max(ys)
                h, v, ml, mr = a[1], a[2], a[3], a[4]
                dx = dy = 0.0
                if h == 1: dx = x0 + (rw - (mxx - mnx)) / 2 - mnx
                elif h == 2: dx = x0 + rw - mr - mxx
                elif ml: dx = x0 + ml - mnx
                if v == 1: dy = y0 + (rh - (mxy - mny)) / 2 - mny
                elif v == 2: dy = y0 + rh - mxy
                for i in range(start, len(items)):
                    it = list(items[i]); it[-5] += dx; it[-4] += dy; items[i] = tuple(it)
                px += dx; py += dy
        elif k == OP_MARGIN:
            mstack.append((ml_, mr_)); ml_, mr_ = op[1], op[2]; px += ml_
        elif k == OP_MARGIN_END:
            if mstack: ml_, mr_ = mstack.pop()
        elif k == OP_IMG:
            w = (rw - ml_ - mr_) if op[2] < 0 else op[2]
            h = rh if op[3] < 0 else op[3]
            items.append(("img", op[1], px, py, w, h, color))
            px += w
        elif k == OP_TEXT:
            s = op[1]
            if vals is not None:
                s = s.replace("%0", vals[0] if len(vals) > 0 else "").replace("%1", vals[1] if len(vals) > 1 else "")
            f = pk["fonts"][font]
            sc = f["sc"]
            for ch in s:
                g = f["g"].get(ord(ch)) or f["g"].get(63)
                if not g: continue
                ax, ay, aw, ah, plx, ply, adv = g
                if aw > 0:
                    items.append(("glyph", font, g, px + plx * sc * fsx, py + ply * sc * fsy, aw * sc * fsx, ah * sc * fsy, color))
                px += adv * sc * fsx
            line_h = max(line_h, f["line"] * fsy)
    return items


def argb(c): return ((c >> 16) & 255, (c >> 8) & 255, c & 255, (c >> 24) & 255)


def draw(pk, canvas, items, scale, clip=None):
    for it in items:
        if it[0] == "img":
            _, idx, x, y, w, h, c = it
            if w < 0.5 or h < 0.5: continue
            im = pk["imgs"][idx].resize((max(1, round(w * scale)), max(1, round(h * scale))), Image.BILINEAR)
            cr, cg, cb, ca = argb(c)
            if (cr, cg, cb, ca) != (255, 255, 255, 255):
                a = np.asarray(im).astype(np.float32)
                a[..., 0] *= cr / 255; a[..., 1] *= cg / 255; a[..., 2] *= cb / 255; a[..., 3] *= ca / 255
                im = Image.fromarray(a.astype(np.uint8), "RGBA")
            paste(canvas, im, x * scale, y * scale, clip)
        else:
            _, fi, g, x, y, w, h, c = it
            f = pk["fonts"][fi]
            ax, ay, aw, ah = g[:4]
            pad = f["pad"]
            # recorta com margem de padding e amostra o SDF
            sx0, sy0 = ax - pad, ay - pad
            sw, sh = aw + 2 * pad, ah + 2 * pad
            dw, dh = w * (sw / aw) * scale, h * (sh / ah) * scale
            if dw < 1 or dh < 1: continue
            sdf = Image.fromarray(f["sdf"]).crop((int(sx0), int(sy0), int(math.ceil(sx0 + sw)), int(math.ceil(sy0 + sh))))
            sdf = np.asarray(sdf.resize((max(1, round(dw)), max(1, round(dh))), Image.BILINEAR)).astype(np.float32)
            k = dw / sw   # px de tela por px do atlas
            alpha = np.clip((sdf - 127.5) / 255.0 * (2 * pad) * k + 0.5, 0, 1)
            cr, cg, cb, ca = argb(c)
            arr = np.zeros(alpha.shape + (4,), np.uint8)
            arr[..., 0], arr[..., 1], arr[..., 2] = cr, cg, cb
            arr[..., 3] = (alpha * ca).astype(np.uint8)
            paste(canvas, Image.fromarray(arr, "RGBA"), (x - pad * w / aw) * scale, (y - pad * h / ah) * scale, clip)


def paste(canvas, im, x, y, clip):
    x, y = int(round(x)), int(round(y))
    if clip:
        cx0, cy0, cx1, cy1 = [int(round(v)) for v in clip]
        l, t = max(0, cx0 - x), max(0, cy0 - y)
        r, b = min(im.size[0], cx1 - x), min(im.size[1], cy1 - y)
        if r <= l or b <= t: return
        im = im.crop((l, t, r, b)); x += l; y += t
    layer = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    layer.paste(im, (x, y))
    canvas.alpha_composite(layer)


def render(pk, values, on, page=1, scale=1.0):
    W, H = int(pk["W"] * scale), int(pk["H"] * scale)
    canvas = Image.new("RGBA", (W, H), (0, 0, 0, 255))
    for e in pk["elems"]:
        if not (e["page"] >> (page - 1)) & 1: continue   # campo = mascara de paginas
        if any(v not in on for v in e["vis"]): continue
        x, y, w, h = e["rect"]
        if e["kind"] == K_GAUGE:
            img, rx, ry, sc, amin, amax, vmin, vmax = e["gauge"]
            v = values.get(e["id"], vmin)
            t = min(1, max(0, (float(v) - vmin) / (vmax - vmin)))
            ang = amin + (amax - amin) * t
            im = pk["imgs"][img]
            # gira em torno do pivo, que vai para o centro do retangulo
            big = Image.new("RGBA", (im.size[0] * 3, im.size[1] * 3), (0, 0, 0, 0))
            big.paste(im, (im.size[0], im.size[1]))
            cx, cy = rx + im.size[0], ry + im.size[1]
            rot = big.rotate(-ang, resample=Image.BICUBIC, center=(cx, cy))
            s = sc * scale
            rot = rot.resize((max(1, round(rot.size[0] * s)), max(1, round(rot.size[1] * s))), Image.BILINEAR)
            paste(canvas, rot, (x + w / 2) * scale - cx * s, (y + h / 2) * scale - cy * s, None)
            continue
        if e["kind"] == K_BAR:
            vert, mn, mx, vmin, vmax = e["bar"]
            v = float(values.get(e["id"], vmin))
            t = min(1, max(0, (v - vmin) / (vmax - vmin) if vmax != vmin else 0))
            size = mn + (mx - mn) * t
            rect = (x, y + h - size, w, size) if vert else (x, y, size, h)
            items = layout(pk, e["ops"], rect, None)
            r = rect
            draw(pk, canvas, items, scale, (r[0] * scale, r[1] * scale, (r[0] + r[2]) * scale, (r[1] + r[3]) * scale))
            continue
        vals = None
        if e["kind"] == K_VALUE:
            v = values.get(e["id"])
            if v is None: continue
            vals = str(v).split("|")
        draw(pk, canvas, layout(pk, e["ops"], e["rect"], vals), scale)
    return canvas


if __name__ == "__main__":
    pk = load(sys.argv[1])
    page = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    vals = {1000: "22|C", 1010: "92|C", 1020: "78|km/h", 1030: "123456|km", 1050: "14:37", 1070: 0.55, 1080: "820|km",
            1090: "86|C", 1100: "80", 1120: 9.5, 1130: 0.4, 1140: 0.8, 1150: 86, 1160: "31.2|l/100km",
            1170: "28.4|l/100km", 1210: "245|km", 1300: "12", 1310: "A", 1390: "02:15", 1430: 78, 1450: 80,
            1520: "80", 1530: "18:20", 1540: "06:10", 1600: 92, 1610: "80", 1700: 1350}
    on = {1240, 1470, 1570, 1610, 1530, 1540}
    img = render(pk, vals, on, page, float(sys.argv[4]) if len(sys.argv) > 4 else 1.0)
    img.convert("RGB").save(sys.argv[2])
    print("ok", img.size)
