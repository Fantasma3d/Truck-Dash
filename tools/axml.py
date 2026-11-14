#!/usr/bin/env python3
"""Codificador/decodificador de XML binario do Android (AndroidManifest.xml)."""
import struct

RES_STRING_POOL = 0x0001
RES_XML = 0x0003
RES_XML_START_NS = 0x0100
RES_XML_END_NS = 0x0101
RES_XML_START_EL = 0x0102
RES_XML_END_EL = 0x0103
RES_XML_RESMAP = 0x0180
NO = 0xFFFFFFFF
ANDROID_NS = "http://schemas.android.com/apk/res/android"

T_REF, T_STRING, T_INT_DEC, T_INT_HEX, T_BOOL = 0x01, 0x03, 0x10, 0x11, 0x12


# ------------------------------------------------------------------ string pool
def decode_pool(buf, off):
    typ, hsz, size, count, scount, flags, sstart, ystart = struct.unpack_from("<HHIIIIII", buf, off)
    assert typ == RES_STRING_POOL
    utf8 = bool(flags & (1 << 8))
    offs = struct.unpack_from("<%dI" % count, buf, off + hsz)
    out = []
    for o in offs:
        p = off + sstart + o
        if utf8:
            n = buf[p]; p += 1
            if n & 0x80: p += 1
            n = buf[p]; p += 1
            if n & 0x80: n = ((n & 0x7f) << 8) | buf[p]; p += 1
            out.append(buf[p:p + n].decode("utf-8", "replace"))
        else:
            n = struct.unpack_from("<H", buf, p)[0]; p += 2
            if n & 0x8000:
                n = ((n & 0x7fff) << 16) | struct.unpack_from("<H", buf, p)[0]; p += 2
            out.append(buf[p:p + 2 * n].decode("utf-16-le", "replace"))
    return out, size


def encode_pool(strings, utf8=False):
    data = b""
    offs = []
    for s in strings:
        offs.append(len(data))
        if utf8:
            b = s.encode("utf-8")
            assert len(s) < 128 and len(b) < 128
            data += bytes([len(s), len(b)]) + b + b"\0"
        else:
            u = s.encode("utf-16-le")
            assert len(s) < 0x8000
            data += struct.pack("<H", len(s)) + u + b"\0\0"
    while len(data) % 4:
        data += b"\0"
    hsz = 28
    sstart = hsz + 4 * len(strings)
    body = struct.pack("<%dI" % len(strings), *offs) + data
    flags = (1 << 8) if utf8 else 0
    return struct.pack("<HHIIIIII", RES_STRING_POOL, hsz, hsz + len(body), len(strings), 0, flags, sstart, 0) + body


# ------------------------------------------------------------------ XML
def decode_xml(buf):
    typ, hsz, size = struct.unpack_from("<HHI", buf, 0)
    assert typ == RES_XML, hex(typ)
    off = hsz
    strings, resmap, lines, depth = [], [], [], 0
    S = lambda i: strings[i] if i != NO else None
    while off < size:
        ctype, chsz, csize = struct.unpack_from("<HHI", buf, off)
        if ctype == RES_STRING_POOL:
            strings, _ = decode_pool(buf, off)
        elif ctype == RES_XML_RESMAP:
            resmap = list(struct.unpack_from("<%dI" % ((csize - chsz) // 4), buf, off + chsz))
        elif ctype == RES_XML_START_EL:
            ns, name, astart, asize, acount = struct.unpack_from("<IIHHH", buf, off + chsz)
            attrs = []
            for i in range(acount):
                a = off + chsz + astart + i * asize
                ans, aname, raw, vsz, _, dt, data = struct.unpack_from("<IIIHBBI", buf, a)
                rid = resmap[aname] if aname < len(resmap) else 0
                if dt == T_STRING: v = repr(S(data))
                elif dt == T_REF: v = "@0x%08x" % data
                elif dt == T_BOOL: v = "true" if data else "false"
                elif dt == T_INT_HEX: v = "0x%x" % data
                else: v = "%d(t%x)" % (data, dt)
                attrs.append("%s%s=%s [id 0x%08x]" % ("android:" if ans != NO else "", S(aname), v, rid))
            lines.append("  " * depth + "<" + S(name) + (" " if attrs else "") + " ".join(attrs) + ">")
            depth += 1
        elif ctype == RES_XML_END_EL:
            depth -= 1
        off += csize
    return lines, strings, resmap


class Node:
    def __init__(self, tag, attrs=None, children=None):
        self.tag, self.attrs, self.children = tag, attrs or [], children or []


def encode_xml(root, attr_ids):
    """attrs: lista de (android?, nome, tipo, valor). attr_ids: nome -> id de recurso."""
    # nomes de atributos com id vem primeiro no pool, na ordem do resource map
    used = []
    def walk(n):
        for (andr, name, t, v) in n.attrs:
            if andr and name not in used: used.append(name)
        for c in n.children: walk(c)
    walk(root)
    used.sort(key=lambda k: attr_ids[k])
    strings = list(used)
    def idx(s):
        if s not in strings: strings.append(s)
        return strings.index(s)
    idx("android"); idx(ANDROID_NS)
    def collect(n):
        idx(n.tag)
        for (andr, name, t, v) in n.attrs:
            if not andr: idx(name)
            if t == T_STRING: idx(v)
        for c in n.children: collect(c)
    collect(root)

    chunks = []
    def node(typ, ext):
        return struct.pack("<HHIII", typ, 16, 16 + len(ext), 1, NO) + ext
    chunks.append(node(RES_XML_START_NS, struct.pack("<II", idx("android"), idx(ANDROID_NS))))
    def emit(n):
        attrs = sorted(n.attrs, key=lambda a: (0, attr_ids[a[1]]) if a[0] else (1, 0))
        ab = b""
        for (andr, name, t, v) in attrs:
            ns = idx(ANDROID_NS) if andr else NO
            if t == T_STRING:
                raw, data = idx(v), idx(v)
            elif t == T_BOOL:
                raw, data = NO, (0xFFFFFFFF if v else 0)
            else:
                raw, data = NO, v
            ab += struct.pack("<IIIHBBI", ns, idx(name), raw, 8, 0, t, data)
        ext = struct.pack("<IIHHHHHH", NO, idx(n.tag), 20, 20, len(attrs), 0, 0, 0) + ab
        chunks.append(node(RES_XML_START_EL, ext))
        for c in n.children: emit(c)
        chunks.append(node(RES_XML_END_EL, struct.pack("<II", NO, idx(n.tag))))
    emit(root)
    chunks.append(node(RES_XML_END_NS, struct.pack("<II", idx("android"), idx(ANDROID_NS))))

    pool = encode_pool(strings)
    rm = struct.pack("<%dI" % len(used), *[attr_ids[k] for k in used])
    resmap = struct.pack("<HHI", RES_XML_RESMAP, 8, 8 + len(rm)) + rm
    body = pool + resmap + b"".join(chunks)
    return struct.pack("<HHI", RES_XML, 8, 8 + len(body)) + body


if __name__ == "__main__":
    import sys, zipfile
    z = zipfile.ZipFile(sys.argv[1])
    lines, strings, resmap = decode_xml(z.read("AndroidManifest.xml"))
    print("\n".join(lines[:int(sys.argv[2]) if len(sys.argv) > 2 else 60]))
