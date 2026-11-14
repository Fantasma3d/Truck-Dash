#!/usr/bin/env python3
"""resources.arsc minimo (um drawable: o icone) + leitor para conferencia."""
import struct
from axml import encode_pool, decode_pool

RES_TABLE, RES_PACKAGE, RES_TYPE, RES_TYPE_SPEC = 0x0002, 0x0200, 0x0201, 0x0202
CONFIG_SIZE = 36


def encode_arsc(pkg_name, drawables):
    """drawables: lista de (nome, caminho no apk). IDs: 0x7f010000 + i"""
    gpool = encode_pool([p for _, p in drawables], utf8=True)
    tpool = encode_pool(["drawable"])
    kpool = encode_pool([n for n, _ in drawables])
    n = len(drawables)
    spec = struct.pack("<HHIBBHI", RES_TYPE_SPEC, 16, 16 + 4 * n, 1, 0, 0, n) + b"\0\0\0\0" * n
    config = struct.pack("<I", CONFIG_SIZE) + b"\0" * (CONFIG_SIZE - 4)
    hsz = 20 + CONFIG_SIZE
    entries = b""
    offs = []
    for i in range(n):
        offs.append(len(entries))
        entries += struct.pack("<HHI", 8, 0, i) + struct.pack("<HBBI", 8, 0, 0x03, i)
    entries_start = hsz + 4 * n
    typ = struct.pack("<HHIBBHII", RES_TYPE, hsz, entries_start + len(entries), 1, 0, 0, n, entries_start) + config
    typ += struct.pack("<%dI" % n, *offs) + entries
    name = pkg_name.encode("utf-16-le")[:254].ljust(256, b"\0")
    phsz = 284
    body = tpool + kpool + spec + typ
    pkg = struct.pack("<HHII", RES_PACKAGE, phsz, phsz + len(body), 0x7f) + name
    pkg += struct.pack("<IIII", phsz, 1, phsz + len(tpool), n) + body
    total = 12 + len(gpool) + len(pkg)
    return struct.pack("<HHII", RES_TABLE, 12, total, 1) + gpool + pkg


def decode_arsc(buf, max_types=4):
    typ, hsz, size, npkg = struct.unpack_from("<HHII", buf, 0)
    assert typ == RES_TABLE
    off = hsz
    out = []
    gstr = []
    while off < size:
        ct, ch, cs = struct.unpack_from("<HHI", buf, off)
        if ct == 0x0001:
            gstr, _ = decode_pool(buf, off)
            out.append("global pool: %d strings" % len(gstr))
        elif ct == RES_PACKAGE:
            pid = struct.unpack_from("<I", buf, off + 8)[0]
            pname = buf[off + 12:off + 268].decode("utf-16-le").rstrip("\0")
            ts, lpt, ks, lpk = struct.unpack_from("<IIII", buf, off + 268)
            types, _ = decode_pool(buf, off + ts)
            keys, _ = decode_pool(buf, off + ks)
            out.append("package 0x%x %s hdr=%d types=%s keys=%d" % (pid, pname, ch, types[:6], len(keys)))
            p = off + ch
            seen = 0
            while p < off + cs:
                t2, h2, s2 = struct.unpack_from("<HHI", buf, p)
                if t2 == RES_TYPE and seen < max_types:
                    tid, _, _, cnt, est = struct.unpack_from("<BBHII", buf, p + 8)
                    csz = struct.unpack_from("<I", buf, p + 20)[0]
                    eo = struct.unpack_from("<%dI" % cnt, buf, p + h2)
                    shown = []
                    for i, e in enumerate(eo[:3]):
                        if e == 0xFFFFFFFF: continue
                        esz, efl, key = struct.unpack_from("<HHI", buf, p + est + e)
                        if not efl & 1:
                            vsz, _, vt, vd = struct.unpack_from("<HBBI", buf, p + est + e + esz)
                            v = gstr[vd] if vt == 3 else hex(vd)
                            shown.append("0x7f%02x%04x %s=%s" % (tid, i, keys[key], v))
                    out.append("  type %s hdr=%d cfgsize=%d entries=%d %s" % (types[tid - 1], h2, csz, cnt, shown))
                    seen += 1
                p += s2
        off += cs
    return out


if __name__ == "__main__":
    import sys, zipfile
    z = zipfile.ZipFile(sys.argv[1])
    print("\n".join(decode_arsc(z.read("resources.arsc"))))
