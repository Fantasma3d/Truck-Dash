#!/bin/sh
# Recompila tudo (Linux com clang/lld, python3 + Pillow + numpy, JDK). Nao precisa de Android SDK/NDK.
set -e
cd "$(dirname "$0")"
mkdir -p out
python3 tools/gen_font.py android/font_data.h
sh plugin/build.sh
rm -f out/android/*/libmain.so
sh android/build_so.sh
python3 tools/build_apk.py
