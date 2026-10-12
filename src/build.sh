#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE"
cc -m32 -c -Os -fno-pic -fno-asynchronous-unwind-tables -fno-stack-protector -ffreestanding -nostdlib radar.c -o radar.o
as --32 hook.s -o hook.o
ld -m elf_i386 -T link.ld -nostdlib -o radar.elf hook.o radar.o
objcopy -O binary radar.elf radar.bin
wc -c radar.bin
