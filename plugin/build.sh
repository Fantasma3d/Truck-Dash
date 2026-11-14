#!/bin/sh
set -e
cd "$(dirname "$0")"
O=../out
llvm-dlltool -m i386:x86-64 -d ws2_32.def -l $O/ws2_32.lib
llvm-dlltool -m i386:x86-64 -d kernel32.def -l $O/kernel32.lib
clang --target=x86_64-pc-windows-msvc -O2 -ffreestanding -fno-stack-protector -mno-stack-arg-probe \
  -fms-extensions -Wall -Wno-unknown-pragmas -c td_plugin.c -o $O/td_plugin.obj
lld-link /DLL /NODEFAULTLIB /ENTRY:DllMain /SUBSYSTEM:WINDOWS /MACHINE:X64 \
  /OUT:$O/TruckDash.dll $O/td_plugin.obj $O/ws2_32.lib $O/kernel32.lib
