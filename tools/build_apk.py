#!/usr/bin/env python3
"""Monta o APK do TruckDash sem Android SDK: manifest binario + resources.arsc +
libmain.so (NativeActivity, sem classes.dex) e assina (v1/JAR, SHA1 p/ Android 4)."""
import os, sys, struct, subprocess, zipfile, io
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from axml import Node, encode_xml, decode_xml, T_STRING, T_INT_DEC, T_INT_HEX, T_BOOL, T_REF
from arsc import encode_arsc, decode_arsc
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT = os.path.join(ROOT, "out")
PKG = "com.fantasma.truckdash"

IDS = {
    "theme": 0x01010000, "label": 0x01010001, "icon": 0x01010002, "name": 0x01010003,
    "hasCode": 0x0101000c, "debuggable": 0x0101000f, "exported": 0x01010010,
    "launchMode": 0x0101001d, "screenOrientation": 0x0101001e, "configChanges": 0x0101001f,
    "value": 0x01010024, "minSdkVersion": 0x0101020c, "versionCode": 0x0101021b,
    "versionName": 0x0101021c, "targetSdkVersion": 0x01010270,
}
A = lambda n, t, v: (True, n, t, v)


def manifest(with_icon):
    app_attrs = [A("label", T_STRING, "TruckDash"), A("hasCode", T_BOOL, False),
                 A("theme", T_REF, 0x01030007)]   # @android:style/Theme.NoTitleBar.Fullscreen
    if with_icon:
        app_attrs.append(A("icon", T_REF, 0x7f010000))
    act = Node("activity", [
        A("name", T_STRING, "android.app.NativeActivity"),
        A("label", T_STRING, "TruckDash"),
        A("screenOrientation", T_INT_DEC, 6),          # sensorLandscape
        A("configChanges", T_INT_HEX, 0xff0),          # nao recria ao girar/teclado
        A("launchMode", T_INT_DEC, 2),                 # singleTask
        A("exported", T_BOOL, True),
    ], [
        Node("meta-data", [A("name", T_STRING, "android.app.lib_name"), A("value", T_STRING, "main")]),
        Node("intent-filter", [], [
            Node("action", [A("name", T_STRING, "android.intent.action.MAIN")]),
            Node("category", [A("name", T_STRING, "android.intent.category.LAUNCHER")]),
        ]),
    ])
    root = Node("manifest", [
        A("versionCode", T_INT_DEC, 1), A("versionName", T_STRING, "1.0"), (False, "package", T_STRING, PKG),
    ], [
        Node("uses-sdk", [A("minSdkVersion", T_INT_DEC, 9), A("targetSdkVersion", T_INT_DEC, 19)]),
        Node("uses-permission", [A("name", T_STRING, "android.permission.INTERNET")]),
        Node("uses-permission", [A("name", T_STRING, "android.permission.ACCESS_NETWORK_STATE")]),
        Node("uses-permission", [A("name", T_STRING, "android.permission.READ_EXTERNAL_STORAGE")]),
        Node("application", app_attrs, [act]),
    ])
    return encode_xml(root, IDS)


def icon_png(size=144):
    s = size * 4
    im = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((8, 8, s - 8, s - 8), radius=s // 5, fill=(16, 18, 22, 255))
    c, r = s // 2, int(s * 0.36)
    d.ellipse((c - r, c - r, c + r, c + r), outline=(220, 224, 230, 255), width=s // 28)
    import math
    for i in range(11):
        a = math.radians(135 + 27 * i)
        r0, r1 = r * 0.78, r * 0.92
        d.line((c + r0 * math.cos(a), c + r0 * math.sin(a), c + r1 * math.cos(a), c + r1 * math.sin(a)),
               fill=(220, 224, 230, 255), width=s // 60)
    a = math.radians(135 + 270 * 0.62)
    d.line((c, c, c + r * 0.8 * math.cos(a), c + r * 0.8 * math.sin(a)), fill=(255, 70, 30, 255), width=s // 26)
    d.ellipse((c - s // 18, c - s // 18, c + s // 18, c + s // 18), fill=(255, 70, 30, 255))
    f = ImageFont.truetype("/usr/share/fonts/opentype/inter/InterDisplay-Bold.otf", s // 7)
    d.text((c, c + r * 0.62), "TD", font=f, fill=(255, 255, 255, 255), anchor="mm")
    im = im.resize((size, size), Image.LANCZOS)
    b = io.BytesIO(); im.save(b, "PNG", optimize=True)
    return b.getvalue()


def keystore():
    ks = os.path.join(OUT, "truckdash.keystore")
    if not os.path.exists(ks):
        subprocess.check_call(["keytool", "-genkeypair", "-keystore", ks, "-storepass", "truckdash",
                               "-keypass", "truckdash", "-alias", "truckdash", "-keyalg", "RSA", "-keysize", "2048",
                               "-sigalg", "SHA256withRSA", "-validity", "36500",
                               "-dname", "CN=TruckDash, O=Uso pessoal, C=BR"], stdout=subprocess.DEVNULL)
    return ks


def build(name, with_icon):
    unsigned = os.path.join(OUT, name + ".unsigned.apk")
    final = os.path.join(OUT, name + ".apk")
    man = manifest(with_icon)
    with zipfile.ZipFile(unsigned, "w") as z:
        z.writestr(zipfile.ZipInfo("AndroidManifest.xml", (2026, 1, 1, 0, 0, 0)), man, zipfile.ZIP_DEFLATED)
        if with_icon:
            ar = encode_arsc(PKG, [("icon", "res/drawable/icon.png")])
            z.writestr(zipfile.ZipInfo("resources.arsc", (2026, 1, 1, 0, 0, 0)), ar, zipfile.ZIP_STORED)
            z.writestr(zipfile.ZipInfo("res/drawable/icon.png", (2026, 1, 1, 0, 0, 0)), icon_png(), zipfile.ZIP_STORED)
        for abi in ("armeabi-v7a", "x86"):
            so = os.path.join(OUT, "android", abi, "libmain.so")
            z.write(so, "lib/%s/libmain.so" % abi, zipfile.ZIP_DEFLATED)
    # assinatura v1 com SHA1: Android < 4.3 nao aceita SHA-256 em APK
    sec = os.path.join(OUT, "java.security.override")
    open(sec, "w").write("jdk.jar.disabledAlgorithms=\njdk.certpath.disabledAlgorithms=\n")
    subprocess.check_call(["jarsigner", "-J-Djava.security.properties=" + sec, "-keystore", keystore(),
                           "-storepass", "truckdash", "-keypass", "truckdash", "-digestalg", "SHA1",
                           "-sigalg", "SHA1withRSA", "-signedjar", final, unsigned, "truckdash"],
                          stdout=subprocess.DEVNULL)
    os.remove(unsigned)
    return final


if __name__ == "__main__":
    for name, ic in (("TruckDash", True), ("TruckDash-sem-icone", False)):
        p = build(name, ic)
        z = zipfile.ZipFile(p)
        print("==", p, os.path.getsize(p), "bytes")
        print("  ", [i.filename for i in z.infolist()])
        print("\n".join(decode_xml(z.read("AndroidManifest.xml"))[0]))
        if ic:
            print("\n".join(decode_arsc(z.read("resources.arsc"))))
