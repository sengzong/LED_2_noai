# LED_2_noai — STM32N647 ThreadX/NetXDuo + RGB LCD 触摸 UI + 以太网多槽位 OTA

基于 STM32N6 系列的完整可运行工程:官方 FSBL 引导 + 应用 XIP 执行,ThreadX 实时操作系统,
NetXDuo TCP/IP 协议栈,RGB LCD + 电容触摸的槽位选择 UI,以及通过网口完成的多槽位固件 OTA 升级。

## 功能概览

- **引导**:ST 官方 FSBL(`FSBL/`)完成外部存储初始化后跳转应用;应用代码在 XSPI2 NOR Flash 上 XIP 原地执行
- **系统**:ThreadX 实时操作系统 + NetXDuo TCP/IP 协议栈(裸机 → RTOS 移植)
- **显示**:800x480 RGB LCD(LTDC),帧缓冲放在外部 PSRAM;触摸选槽 UI(FT5206 / GT9xxx,I2C)
- **网络**:ETH + YT8512C PHY,TCP 客户端方式接收固件
- **OTA**:3 个 4MB 槽位固件管理,带描述/CRC32 校验,触摸选择槽位覆盖引导区后复位切换
- **上位机**:纯 Python 标准库的发送工具(命令行 + 网页拖拽两种)

## 硬件

| 部件 | 型号 / 说明 |
|---|---|
| MCU | STM32N647X0HxQ(Cortex-M55) |
| NOR Flash | MX25UM25645G,XSPI2,OPI-DTR,XIP 基址 0x70000000 |
| PSRAM | W958D8NBYA5I,XSPI1,基址 0x90000000(帧缓冲) |
| 显示 | 800x480 RGB LCD(LTDC) |
| 触摸 | FT5206 / GT9xxx(I2C) |
| 网口 | ETH + YT8512C PHY |

## 目录结构

```
FSBL/            第一阶段引导(官方例程改,AXISRAM2 运行)
Appli/           应用工程(ThreadX + NetXDuo + UI + OTA)
Drivers/         CMSIS + STM32N6 HAL + 板级 BSP(LED/RGBLCD/TOUCH/UART/EEPROM/YT8512C...)
Middlewares/     Azure RTOS(ThreadX / NetXDuo)+ ExtMem_Manager
Secure_nsclib/   安全非安全调用接口头文件
Binary/          烧录产物(appli.hex / fsbl_universal.hex)
ota_gui.py       PC 端 OTA 发送工具(网页版,带描述,多槽位)
ota_send.py      PC 端 OTA 发送工具(命令行,裸流,向后兼容)
OTA_DEBUG_NOTES.md   OTA 调试记录
```

## 构建

1. 安装 **STM32CubeIDE 1.17+**(内置 GCC 12.3)
2. 导入仓库根目录(包含 `FSBL`、`Appli` 两个工程)
3. 构建 `FSBL` 与 `Appli` 的 **Release** 配置
4. Appli 的 postbuild 脚本会自动生成 `Binary/appli.hex`

> 注意:Appli 必须使用 Release(XIP)配置运行,链接脚本 `STM32N647X0HXQ_ROMxspi2_RAMxspi1.ld`;
> Debug(LRUN)配置仅用于 SRAM 调试,帧缓冲放不下会黑屏。CubeMX 重新生成代码后
> 链接脚本/postbuild 会被重置,需要恢复(详见工程内 `.cproject` 与 OTA_DEBUG_NOTES.md)。

## 烧录

使用 STM32CubeProgrammer 烧录 `Binary/` 下的两个 hex:

- `fsbl_universal.hex` — 官方 FSBL(NOR 0x00000000 区域)
- `appli.hex` — 应用(XIP,入口 0x70100400)

BOOT0=0 上电即由 FSBL 引导应用。

## 内存布局

| 区域 | 地址 | 大小 | 用途 |
|---|---|---|---|
| AXISRAM2 | 0x34180000 | 512KB | FSBL 代码+数据(引导后让出) |
| AXISRAM1+2 | 0x34000000 | 2MB | 应用数据(ThreadX/NetX/OTA 缓冲/栈) |
| XSPI2 NOR XIP | 0x70000000 | 32MB | 引导区(0x100000)+ 3 个固件槽位(0x400000 起,间距 4MB) |
| XSPI1 PSRAM | 0x90000000 | 32MB | 帧缓冲 / 大块数据 |

## OTA 使用

板子是 TCP **客户端**,上电后主动连 PC 的 `192.168.2.10:8080`(可在
`Appli/Core/Src/app_netxduo.c` 顶部修改,支持 DHCP / 静态 IP 两种配置)。

**网页版(推荐,带描述)**:

```
python ota_gui.py          # 浏览器打开 http://127.0.0.1:17800
```

在页面拖入 `appli.bin`、填描述、发送。容器协议:

```
[ magic u32 "OFW2" | ver u32 | desc_len u32 | img_len u32 | desc | img ]
```

板子校验后存入下一个空槽(NOR 0x400000 起),槽位格式 `[64B 头(magic "SLT1"/长度/描述/CRC32) | 镜像]`。

**命令行版(裸流,直接覆盖引导区并复位)**:

```
python ota_send.py --file appli.bin
```

**切换固件**:板上触摸 UI 选中槽位 → 槽镜像 + FSBL 头组装后覆盖引导区(NOR 0x100000)→ 校验 → 复位启动。

安全机制:镜像写入前校验向量表合法性(`ota_image_sane`:SP 必须落在内部 SRAM、Reset 向量必须在 NOR 区),
槽位头带 CRC32;写 NOR 由自包含的 SRAM 代理(`ota_agent`)完成——因为 XSPI2 切换 IO 模式的瞬间
XIP 取指会断开,代理必须整体在内部 SRAM 执行,结果通过跨复位保留的 SRAM 区(0x341B0000)回传。

## License

[MIT](LICENSE)
