#!/usr/bin/env python3
"""Converte um painel do ETS2/ATS (ui/dashboard/<nome>.sii + texturas + fontes extraidos do jogo)
num pacote .tdp que o app TruckDash le do armazenamento do tablet.

Uso: tdpack.py <pasta_extraida> <nome_do_painel> <saida.tdp> [previa.png]
A pasta extraida deve conter ui/, material/ e font/ (extractor -p=/ui/dashboard,/ui/template,/material/ui/dashboard,/font).
Os pacotes gerados contem arte do jogo: sao para uso pessoal e NAO devem ser publicados."""
import os, re, sys, struct, math
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

# ------------------------------------------------------------------ SII
def parse_sii(text):
    text = text.replace("\r", "")
    units, order = {}, []
    for m in re.finditer(r'^\s*([a-z_:]+)\s*:\s*([^\s{]+)\s*\{(.*?)^\s*\}', text, re.S | re.M):
        typ, name, body = m.group(1), m.group(2), m.group(3)
        a = {}
        for line in body.split("\n"):
            line = line.strip()
            if not line or line.startswith("#") or ":" not in line:
                continue
            k, v = line.split(":", 1)
            k, v = k.strip(), v.strip()
            if v.startswith('"'):
                v = v[1:v.rfind('"')].replace("\\r", "").replace("\\n", "\n").replace('\\"', '"')
            if "[" in k:
                a.setdefault(k[:k.index("[")] + "[]", []).append(v)
            else:
                a[k] = v
        units[name] = (typ, a)
        order.append(name)
    return units, order


def sii_float(v, default=0.0):
    if v is None: return default
    v = v.strip()
    if v.startswith("&"):
        return struct.unpack(">f", bytes.fromhex(v[1:]))[0]
    try: return float(v)
    except ValueError: return default


# ------------------------------------------------------------------ texturas
def load_dds(path):
    b = open(path, "rb").read()
    h, w = struct.unpack_from("<II", b, 12)
    if b[84:88] == b"DX10":
        fmt = struct.unpack_from("<I", b, 128)[0]; off = 148
    else:
        fmt = 0; off = 128
    n = w * h * 4
    if fmt in (87, 88, 91, 93):
        return Image.frombytes("RGBA", (w, h), b[off:off + n], "raw", "BGRA")
    if fmt in (28, 29):
        return Image.frombytes("RGBA", (w, h), b[off:off + n])
    try:
        return Image.open(path).convert("RGBA")
    except Exception:
        raise SystemExit("formato de textura nao suportado (%d): %s" % (fmt, path))


class Assets:
    def __init__(self, root):
        self.root = root
        self.tex = {}

    def path(self, p):
        return os.path.join(self.root, p.lstrip("/").replace("/", os.sep))

    def material_image(self, mat):
        if mat in self.tex: return self.tex[mat]
        img = None
        if mat.endswith("/white.mat"):
            img = Image.new("RGBA", (4, 4), (255, 255, 255, 255))
        else:
            mp = self.path(mat)
            dds = None
            if os.path.exists(mp):
                mt = open(mp, errors="replace").read()
                m = re.search(r'source\s*:\s*"([^"]+)"', mt) or re.search(r'texture\s*:\s*"([^"]+\.tobj)"', mt)
                if m:
                    t = m.group(1)
                    t = t if t.startswith("/") else os.path.dirname(mat) + "/" + t
                    dds = self.path(t[:-5] + ".dds" if t.endswith(".tobj") else t)
            if not dds or not os.path.exists(dds):
                dds = self.path(mat[:-4] + ".dds")
            if os.path.exists(dds):
                img = load_dds(dds)
            else:
                print("  aviso: textura nao encontrada:", mat, file=sys.stderr)
                img = Image.new("RGBA", (4, 4), (20, 20, 20, 255))
        self.tex[mat] = img
        return img


class Font:
    def __init__(self, assets, path):
        txt = open(assets.path(path), encoding="utf-8", errors="replace").read().replace("\r", "")
        self.images, self.glyphs = [], {}
        self.line = 20.0
        for line in txt.split("\n"):
            s = line.split("#")[0].strip()
            if not s: continue
            if s.startswith("image:"):
                parts = [p.strip() for p in s[6:].split(",")]
                img = assets.material_image(parts[0])
                self.images.append((img, float(parts[3]), float(parts[4]), float(parts[5]) if len(parts) > 5 else 4.0))
            elif s.startswith("vert_span:"):
                self.line = float(s.split(":")[1])
            elif s.startswith("x"):
                p = [q.strip() for q in s.split(",")]
                code = int(p[0][1:], 16)
                self.glyphs[code] = [float(v) for v in p[1:8]] + [int(p[8])]
        img, sx, sy, pad = self.images[0]
        self.scale = sx
        self.pad = pad
        # MSDF: a distancia e a mediana dos canais R, G, B
        import numpy as np
        a = np.asarray(img.convert("RGB")).astype(np.int16)
        med = np.maximum(np.minimum(a[..., 0], a[..., 1]), np.minimum(np.maximum(a[..., 0], a[..., 1]), a[..., 2]))
        self.sdf = Image.fromarray(med.astype(np.uint8), "L")

    def glyph(self, ch):
        return self.glyphs.get(ord(ch)) or self.glyphs.get(ord("?")) or self.glyphs.get(32)


# ------------------------------------------------------------------ markup -> ops
OP_END, OP_COLOR, OP_FONT, OP_OFFSET, OP_RET, OP_ALIGN, OP_ALIGN_END, OP_IMG, OP_TEXT, OP_MARGIN, OP_MARGIN_END = range(11)


def num(v):
    v = str(v or 0).replace("px", "")
    try: return float(v)
    except ValueError: return 0.0


def uv(v, size):
    """coordenada de textura: fracao (0.5) ou pixel (p256)"""
    v = str(v)
    if v.startswith("p"): return num(v[1:]) / max(size, 1)
    return num(v)


def parse_markup(text, pack):
    ops = []
    pos = 0
    cur = 0xFFFFFFFF
    text = text.replace("\r", "").replace("\n", "")
    for m in re.finditer(r"<(/?)([a-z_]+)([^>]*)>", text):
        if m.start() > pos:
            ops.append((OP_TEXT, text[pos:m.start()]))
        pos = m.end()
        close, tag, attrs = m.group(1), m.group(2), m.group(3)
        at = dict(re.findall(r"([a-z_]+)=([^\s>]+)", attrs))
        if close:
            if tag == "align": ops.append((OP_ALIGN_END,))
            elif tag == "margin": ops.append((OP_MARGIN_END,))
            continue
        if tag == "color":
            try:
                cur = int(at.get("value", "FFFFFFFF"), 16)
                ops.append((OP_COLOR, cur))
            except ValueError:
                pass   # cor vinda do valor (%0): mantem a cor atual
        elif tag == "font":
            ops.append((OP_FONT, pack.font_index(at.get("face", "")), num(at.get("xscale", 1)) or 1.0, num(at.get("yscale", 1)) or 1.0))
        elif tag == "offset":
            ops.append((OP_OFFSET, num(at.get("hshift", 0)), num(at.get("vshift", 0))))
        elif tag == "margin":
            ops.append((OP_MARGIN, num(at.get("left", 0)), num(at.get("right", 0))))
        elif tag == "ret":
            ops.append((OP_RET,))
        elif tag == "align":
            h = {"left": 0, "center": 1, "right": 2}.get(at.get("hstyle", "left"), 0)
            v = {"top": 0, "center": 1, "bottom": 2}.get(at.get("vstyle", "top"), 0)
            ops.append((OP_ALIGN, h, v, num(at.get("left", 0)), num(at.get("right", 0))))
        elif tag == "img":
            src = at.get("src", "")
            tw, th = pack.a.material_image(src).size
            u0, u1 = uv(at.get("left", 0), tw), uv(at.get("right", 1), tw)
            v0, v1 = uv(at.get("top", 0), th), uv(at.get("bottom", 1), th)
            idx, (iw, ih) = pack.image_crop(src, u0, v0, u1, v1)
            w = -1.0 if at.get("xscale") == "stretch" else num(at.get("width", 0))
            h = -1.0 if at.get("yscale") == "stretch" else num(at.get("height", 0))
            if w == 0 and h > 0: w = h * iw / max(ih, 1)
            if h == 0 and w > 0: h = w * ih / max(iw, 1)
            if w == 0 and h == 0: w, h = iw, ih
            ic = None
            if "color" in at:
                try: ic = int(at["color"], 16)
                except ValueError: ic = None
            if ic is not None: ops.append((OP_COLOR, ic))
            ops.append((OP_IMG, idx, w, h))
            if ic is not None: ops.append((OP_COLOR, cur))
    if pos < len(text):
        ops.append((OP_TEXT, text[pos:]))
    return [o for o in ops if not (o[0] == OP_TEXT and o[1] == "")]


# ------------------------------------------------------------------ pacote
VIS_IDS = {1190, 1200, 1230, 1240, 1250, 1260, 1270, 1290, 1410, 1420, 1470, 1480, 1490, 1530, 1540,
           1570, 1580, 1590, 1610, 1620, 1630, 1640, 1650, 1660, 1670}
PAGE_IDS = {100: 1, 200: 2, 300: 3, 400: 4, 500: 5}
K_RUNS, K_VALUE, K_BAR, K_GAUGE = 1, 2, 3, 4


class Pack:
    def __init__(self, assets):
        self.a = assets
        self.images, self.img_key = [], {}
        self.fonts, self.font_key = [], {}
        self.elems = []

    def image_crop(self, src, u0, v0, u1, v1):
        img = self.a.material_image(src)
        W, H = img.size
        x0, x1 = sorted((u0 * W, u1 * W)); y0, y1 = sorted((v0 * H, v1 * H))
        flip_x, flip_y = u0 > u1, v0 > v1
        box = (int(math.floor(x0 + 1e-6)), int(math.floor(y0 + 1e-6)), max(int(math.ceil(x1 - 1e-6)), int(x0) + 1), max(int(math.ceil(y1 - 1e-6)), int(y0) + 1))
        key = (src, box, flip_x, flip_y)
        if key not in self.img_key:
            im = img.crop(box)
            if flip_x: im = im.transpose(Image.FLIP_LEFT_RIGHT)
            if flip_y: im = im.transpose(Image.FLIP_TOP_BOTTOM)
            self.img_key[key] = len(self.images)
            self.images.append(im)
        idx = self.img_key[key]
        return idx, self.images[idx].size

    def image_full(self, src, maxsize=512):
        key = (src, "full", maxsize)
        if key not in self.img_key:
            im = self.a.material_image(src)
            s = 1.0
            if max(im.size) > maxsize:
                s = maxsize / max(im.size)
                im = im.resize((max(1, round(im.size[0] * s)), max(1, round(im.size[1] * s))), Image.LANCZOS)
            self.img_key[key] = len(self.images)
            self.images.append(im)
            self.img_scale = getattr(self, "img_scale", {})
            self.img_scale[self.img_key[key]] = s
        return self.img_key[key]

    def gauge_image_from(self, src, bbox, maxsize=512):
        im = src.crop(bbox)
        s = 1.0
        if max(im.size) > maxsize:
            s = maxsize / max(im.size)
            im = im.resize((max(1, round(im.size[0] * s)), max(1, round(im.size[1] * s))), Image.LANCZOS)
        self.images.append(im)
        return len(self.images) - 1, s

    def font_index(self, face):
        if face not in self.font_key:
            try:
                f = Font(self.a, face)
            except Exception as e:
                print("  aviso: fonte", face, e, file=sys.stderr)
                if self.fonts: return 0
                raise
            self.font_key[face] = len(self.fonts)
            self.fonts.append(f)
        return self.font_key[face]


def is_full_bg(pk, e, W, H):
    """elemento fixo que pinta a tela inteira com imagens opacas"""
    if e["kind"] != K_RUNS or e["vis"]: return False
    x, y, w, h = e["rect"]
    if w < W * 0.98 or h < H * 0.85: return False
    area = 0.0
    color_a = 255
    for o in e["ops"]:
        if o[0] == OP_COLOR: color_a = (o[1] >> 24) & 255
        if o[0] == OP_TEXT and "%" in o[1]: return False
        if o[0] == OP_IMG and color_a >= 250:
            im = pk.images[o[1]]
            ow = w if o[2] < 0 else o[2]; oh = h if o[3] < 0 else o[3]
            alpha = im.split()[3]
            mean = sum(i * c for i, c in enumerate(alpha.histogram())) / max(1, im.size[0] * im.size[1])
            if mean > 250: area += ow * oh
    return area >= W * H * 0.85


def build(root, name):
    a = Assets(root)
    pk = Pack(a)
    units, order = parse_sii(open(a.path("/ui/dashboard/%s.sii" % name), encoding="utf-8", errors="replace").read())
    templates = {}
    tdir = a.path("/ui/template")
    for fn in os.listdir(tdir):
        if fn.startswith("dashboard_text") and fn.endswith(".sii"):
            u, _ = parse_sii(open(os.path.join(tdir, fn), encoding="utf-8", errors="replace").read())
            for k, (t, at) in u.items():
                if "text" in at: templates[k] = at["text"]
    # fonte padrao: a primeira usada nos templates
    pk.font_index("/font/db_normal.font") if os.path.exists(a.path("/font/db_normal.font")) else None

    root_name = next(n for n in order if units[n][0] == "ui::window")
    W = sii_float(units[root_name][1].get("coords_r")) - sii_float(units[root_name][1].get("coords_l"))
    H = sii_float(units[root_name][1].get("coords_t")) - sii_float(units[root_name][1].get("coords_b"))
    pages = set()

    def ctx(n):
        """pagina e ids de visibilidade herdados dos pais"""
        page, vis = 0, []
        cur = n
        while cur and cur in units:
            t, at = units[cur]
            i = int(sii_float(at.get("id"), 0))
            if i in PAGE_IDS: page = PAGE_IDS[i]
            elif i in VIS_IDS and cur != n: vis.append(i)
            cur = at.get("my_parent")
            if cur == "null": break
        return page, vis

    items = []
    for idx, n in enumerate(order):
        t, at = units[n]
        if t in ("ui::window", "ui::group"):
            continue
        page, vis = ctx(n)
        if page: pages.add(page)
        eid = int(sii_float(at.get("id"), 0))
        l, r = sii_float(at.get("coords_l")), sii_float(at.get("coords_r"))
        top, bot = sii_float(at.get("coords_t")), sii_float(at.get("coords_b"))
        rect = (l, H - top, r - l, top - bot)
        layer = int(sii_float(at.get("layer"), 0))
        e = {"page": page, "vis": vis[:3], "rect": rect, "id": 0, "layer": layer, "order": idx, "name": n}
        if t == "ui::text":
            e["kind"] = K_RUNS
            if eid in VIS_IDS: e["vis"] = ([eid] + e["vis"])[:3]
            e["ops"] = parse_markup(at.get("text", ""), pk)
        elif t == "ui::text_common":
            tpl = templates.get(at.get("look_template", ""), "%0")
            e["kind"] = K_VALUE
            e["id"] = eid
            if eid in VIS_IDS: e["vis"] = ([eid] + e["vis"])[:3]
            e["ops"] = parse_markup(tpl, pk)
            e["sample"] = at.get("value", "")
        elif t == "ui_text_bar":
            e["kind"] = K_BAR
            e["id"] = eid
            e["bar"] = (1 if at.get("vertical") == "true" else 0, sii_float(at.get("min_size")), sii_float(at.get("max_size")),
                        sii_float(at.get("min_value")), sii_float(at.get("max_value"), 1))
            e["ops"] = parse_markup(at.get("text", ""), pk)
        elif t == "ui_gauge":
            e["kind"] = K_GAUGE
            e["id"] = eid
            mat = at.get("material", "")
            src = a.material_image(mat)
            rx, ry = sii_float(at.get("xref_pos")), sii_float(at.get("yref_pos"))
            needle = src   # o anel tem uma 'janela' que acompanha o ponteiro: gira a textura inteira
            bbox = needle.split()[3].point(lambda v: 255 if v > 6 else 0).getbbox() or (0, 0, 1, 1)
            img, s = pk.gauge_image_from(needle, bbox)
            e["gauge"] = (img, (rx - bbox[0]) * s, (ry - bbox[1]) * s,
                          1.0 / s, sii_float(at.get("min")), sii_float(at.get("max")),
                          sii_float(at.get("value_min")), sii_float(at.get("value_max"), 1))
        else:
            continue
        items.append(e)
    items.sort(key=lambda e: (e["layer"], e["order"]))
    # paginas como mascara de bits; elementos totalmente cobertos por um fundo opaco
    # posterior (na mesma pagina) sao descartados daquela pagina
    npages = max(pages) if pages else 0
    allmask = (1 << npages) - 1 if npages else 1
    for e in items:
        e["mask"] = allmask if e["page"] == 0 else (1 << (e["page"] - 1))
    for pg in range(max(npages, 1)):
        bit = 1 << pg
        last_full = -1
        for i, e in enumerate(items):
            if e["mask"] & bit and is_full_bg(pk, e, W, H):
                last_full = i
        for i in range(last_full):
            items[i]["mask"] &= ~bit
    items = [e for e in items if e["mask"]]
    pk.elems = items
    pk.W, pk.H, pk.pages = W, H, (max(pages) if pages else 0)
    return pk


# ------------------------------------------------------------------ escrita
def write_pack(pk, out):
    b = bytearray()
    b += b"TDP1" + struct.pack("<HHHB", int(pk.W), int(pk.H), 1, pk.pages)
    b += struct.pack("<H", len(pk.images))
    for im in pk.images:
        im = im.convert("RGBA")
        b += struct.pack("<HH", im.size[0], im.size[1]) + im.tobytes()
    b += struct.pack("<B", len(pk.fonts))
    for f in pk.fonts:
        sdf = f.sdf
        b += struct.pack("<HHfff", sdf.size[0], sdf.size[1], f.scale, f.pad, f.line)
        gl = sorted(f.glyphs.items())
        b += struct.pack("<H", len(gl))
        for code, g in gl:
            b += struct.pack("<H7f", code, *g[:7])
        b += sdf.tobytes()
    b += struct.pack("<H", len(pk.elems))
    for e in pk.elems:
        vis = (e["vis"] + [0, 0, 0])[:3]
        b += struct.pack("<BB3HH4f", e["kind"], e["mask"], *vis, e["id"], *e["rect"])
        if e["kind"] == K_GAUGE:
            b += struct.pack("<H7f", *e["gauge"])
        else:
            if e["kind"] == K_BAR:
                b += struct.pack("<B4f", *e["bar"])
            ob = encode_ops(e["ops"])
            b += struct.pack("<H", len(ob)) + ob
    open(out, "wb").write(bytes(b))
    return len(b)


def encode_ops(ops):
    b = bytearray()
    for o in ops:
        k = o[0]
        if k == OP_COLOR: b += struct.pack("<BI", k, o[1])
        elif k == OP_FONT: b += struct.pack("<BBff", k, o[1], o[2], o[3])
        elif k == OP_OFFSET: b += struct.pack("<Bff", k, o[1], o[2])
        elif k in (OP_RET, OP_ALIGN_END, OP_MARGIN_END): b += struct.pack("<B", k)
        elif k == OP_MARGIN: b += struct.pack("<Bff", k, o[1], o[2])
        elif k == OP_ALIGN: b += struct.pack("<BBBff", k, o[1], o[2], o[3], o[4])
        elif k == OP_IMG: b += struct.pack("<BHff", k, o[1], o[2], o[3])
        elif k == OP_TEXT:
            s = o[1].encode("utf-8")
            b += struct.pack("<BH", k, len(s)) + s
    b += b"\0"
    return bytes(b)


if __name__ == "__main__":
    root, name, out = sys.argv[1], sys.argv[2], sys.argv[3]
    pk = build(root, name)
    n = write_pack(pk, out)
    print("%s: %dx%d, %d paginas, %d elementos, %d imagens, %d fontes, %.1f MB" % (
        name, pk.W, pk.H, pk.pages, len(pk.elems), len(pk.images), len(pk.fonts), n / 1e6))
