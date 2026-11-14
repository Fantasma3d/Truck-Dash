#!/bin/sh
# Compila libmain.so para armeabi-v7a e x86 usando so clang + lld (sem NDK).
set -e
cd "$(dirname "$0")"
OUT=../out/android
mkdir -p $OUT
SRC="main_android.c app.c net.c panel.c render.c aeabi.c"
for ABI in armeabi-v7a x86; do
  case $ABI in
    armeabi-v7a) T="--target=armv7a-linux-androideabi16 -march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16 -mthumb" ;;
    x86)         T="--target=i686-linux-android16 -march=i686 -mssse3 -mfpmath=sse -mstackrealign" ;;
  esac
  B=$OUT/$ABI; mkdir -p $B/stubs
  CF="$T -O2 -fPIC -nostdlibinc -fno-stack-protector -fvisibility=hidden -fno-math-errno -ffast-math \
      -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-builtin-printf -Wall -Wno-unused-function"
  OBJS=""
  for f in $SRC; do
    o=$B/$(basename $f .c).o
    clang $CF -c $f -o $o
    OBJS="$OBJS $o"
  done
  # bibliotecas "stub" so para o linker saber de qual .so vem cada simbolo
  llvm-nm -u --format=just-symbols $OBJS 2>/dev/null | grep -v -e ':$' -e '^$' | sort -u > $B/u_all.txt
  llvm-nm --defined-only --format=just-symbols $OBJS 2>/dev/null | grep -v -e ':$' -e '^$' | sort -u > $B/defined.txt
  comm -23 $B/u_all.txt $B/defined.txt > $B/undef.txt
  : > $B/stubs/c.c; : > $B/stubs/android.c; : > $B/stubs/log.c
  for s in $(cat $B/undef.txt); do
    case $s in
      __aeabi_*|_GLOBAL_OFFSET_TABLE_) ;;   # aeabi.c / linker
      ANativeWindow_*|AInputQueue_*|ALooper_*|AInputEvent_*|AMotionEvent_*|AKeyEvent_*|ANativeActivity_*) echo "void $s(void){}" >> $B/stubs/android.c ;;
      __android_log_*) echo "void $s(void){}" >> $B/stubs/log.c ;;
      *) echo "void $s(void){}" >> $B/stubs/c.c ;;
    esac
  done
  for l in c android log; do
    clang $T -w -fno-builtin -shared -nostdlib -fuse-ld=lld -Wl,-soname,lib$l.so -o $B/stubs/lib$l.so $B/stubs/$l.c
  done
  clang $T -shared -nostdlib -fuse-ld=lld -Wl,-soname,libmain.so -Wl,--hash-style=sysv \
    -Wl,-z,noexecstack -Wl,-z,max-page-size=4096 -Wl,--no-rosegment -Wl,-z,norelro -Wl,--no-undefined -Wl,--gc-sections \
    -o $B/libmain.so $OBJS -L$B/stubs -landroid -llog -lc
  llvm-strip --strip-unneeded $B/libmain.so
  echo "== $ABI: $(stat -c %s $B/libmain.so) bytes; libc: $(tr '\n' ' ' < $B/undef.txt | head -c 600)"
done
