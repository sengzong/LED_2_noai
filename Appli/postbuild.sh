#!/bin/bash
# LED_2 Appli postbuild: 把 Release 的 *_Appli.bin 变成 0x70100400(XIP/NOR)的 ihex
ProjectDir="$(dirname "$(readlink -f "$0")")"
Bin="$ProjectDir/Release/LED_2_Appli.bin"
[ -f "$Bin" ] || { echo "postbuild: 找不到 $Bin"; exit 1; }
mkdir -p "$ProjectDir/../Binary"
arm-none-eabi-objcopy -I binary "$Bin" --change-addresses 0x70100400 -O ihex "$ProjectDir/../Binary/appli.hex"
ls -la "$ProjectDir/../Binary/appli.hex"
echo "postbuild: appli.hex 已生成 (0x70100400)"