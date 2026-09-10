# STM32N647 OTA 调试踩坑记录

> 工程：`LED_2_noai`（正点原子 ATK-CNN647B，Cortex-M55）
> 目标：把收到的固件写进外部 NOR + 复位切新固件；并做成"多槽位 + 描述 + 切换"系统。
> 记录人：Claude Code + 用户联合调试。所有条目都是真机验证过的经验。

---

## 0. 一句话背景

代码在**外部 NOR flash（0x70000000，XIP）**里直接跑。要"擦掉正在跑自己的 flash 写新固件"——但 XSPI 一切到 IO 模式，CPU 立刻无法再从 flash 取指，程序当场崩。所以写 flash 的**代理必须整体搬到内部 SRAM 里执行**，擦改完再复位。

NOR 挂在 **XSPI2**（XIP 区 0x70000000，寄存器 NS 基址 0x5802A000）。0x90000000 是 XSPI1 上的 PSRAM（**已坏**，读 0xAAAA/0x5555）。

---

## 1. 构建/环境问题

| 问题 | 根因 | 解决 |
|---|---|---|
| 构建报 `-fcyclomatic-complexity` 不支持 | 系统 GCC10 太旧 | PATH 前置 CubeIDE 自带 **GCC12.3** |
| 链接报 `SECURE_*` 多重定义 | 手改的 `objects.list` 误收了 `secure_nsclib.o`（`--out-implib` 产物，git 基线里没有） | 从 objects.list 删掉该行 |
| `allocated section not in segment` 致命 | 自定义 `.ld` 段（`.ota_agent` / `.ota_ram AT>RAM>ROM`） | **不要自定义段**。靠 `-ffunction-sections` 让函数+字面池独占子段 |
| `ota_gui.py` 语法错 | 函数里 `global PORT` 但之前已读过 PORT | 把端口改成实例属性传入 |
| `ota_gui.py` 运行报 `No module named 'tkinter'` | 本机 Python（MS Store/精简版）无 tkinter，且 tkinter 无法 pip 装 | **改用浏览器界面**（`http.server` + 内嵌 HTML，纯标准库，支持拖拽 .bin/.hex） |

**构建命令（无 CubeIDE GUI 也能编）**：
```bash
export PATH="/f/stm32CubeIde/STM32CubeIDE_1.17.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.12.3.rel1.win32_1.1.0.202410251130/tools/bin:..."
make -j8 --no-print-directory all   # 在 Appli/Release 下
```
出 hex：
```bash
arm-none-eabi-objcopy -I binary LED_2_Appli.bin --change-addresses 0x70100400 -O ihex appli.hex
```

---

## 2. SRAM 代理（位置无关代码）—— 核心

**问题**：擦写代码必须进 SRAM 跑，且**拷到哪都能执行**（位置无关）。

**要点**：
- 代理 `ota_agent_main(const OtaJob*)` **纯寄存器直写**，零 HAL/printf/memcpy。
- 拷贝靠 `-ffunction-sections` + 固定**16KB 矩形窗** `memcpy` 进 SRAM（8KB 时被函数体撑爆过，改 16KB）。
- 跳转后必须 `SCB_InvalidateICache()` + `__ISB()`（I-cache 开着，D-cache 关着 → 只需刷 I）。
- **反汇编验证代理体内 0 个 bl/blx** = 自包含确实成立。

---

## 3. 真机调试遇到的坑（按顺序）

### 3.1 第一次发命令就静默死机
**现象**：探针（寄存器只读）OK，但第一次切命令模式就黑屏 + 串口全哑。
**根因**：清 FMODE 退出内存映射后 **XIP 断了**，而 **fault handler 本身在 NOR flash 里** → 它去取指也 fault → LOCKUP，UART 全哑。
**解法**：诊断不用 flash 里的 fault handler，改**代理自己在 SRAM 直写 UART** 上报进度（`[OTA] a/s/p/d/...`），不依赖 XIP/返回/复位。

### 3.2 非安全域写复位寄存器选错别名
**现象**：`oa_reset()` 复位不生效，板子卡死/闪一下。
**根因**：硬编码**安全域 SCS** `0xE000ED0C`，从非安全域写 → bus fault。本 app 是非安全跑（XSPI2=0x5802A000、USART1=0x42001000 都是 NS 基址）。
**解法**：改**非安全别名 `0xE002ED0C`**。

### 3.3 XSPI FIFO 残留字节卡住命令
**现象**：WEN（0x06，无数据命令）等 TCF 超时；`t` 行 SR=0x124 = **BUSY+FLEVEL=1（FIFO 残留 1 字节）**。
**根因**：DTR 读后 FIFO 留了 1 字节，下一条无数据命令被误以为"还有数据要发"。
**解法**：`oa_cfg` **每条命令前先排空 FIFO**（`oa_flush_fifo`：按 SR_FLEV 读 DR 弹空 + 清 TC/TE）。→ 这是擦写最终打通的临门一脚。

### 3.4 sanity（镜像预检）字节序看反
**现象**：真 OTA 被 `sanity check FAILED` 拦下。
**根因**：`xxd` 显示 `00 00 20 34`，小端其实是 **SP=0x34200000（=RAM 顶 `_estack`）**，我检查写 `sp >= 0x34200000` 判非法 → 正好把合法顶值拒掉。
**解法**：上限改 `sp > 0x34200000`。**新固件首 8 字节务必先 `xxd -l8` 核 SP/RV。**

### 3.5 恢复内存映射崩溃（早期）
**现象**：状态读 OK 后，restore-MM（回到 XIP）崩。
**解法**：去掉 app 用不到的 `WCCR/WTCR/WIR` 写 + FIFO 排空 + `__DSB/__ISB`，恢复正常返回。

### 3.6 跨复位 SRAM 结果不可靠
**现象**：想靠固定 SRAM 地址跨复位传"阶段结果"，读全 0。
**根因**：一次完整复位后 ROM/FSBL 阶段会重排/清 SRAM，不可靠。
**解法**：放弃跨复位传递，改**单次启动内跑完 + 直接串口上报**。生产 OTA 反正要复位，借用复位后下一版打印结果。

---

## 4. 内存相关（含"2MB 够不够"）

- **唯一可用内存 = 内部 2MB**（0x34000000~0x34200000）。PSRAM（0x90000000）坏了，等于没有外扩。
- **代码不占 RAM**（在 NOR 里），RAM 只放变量/缓冲/栈。当前约用 1.4MB，剩 ~600KB，OTA 需要的代理 16KB + 收包缓冲 512KB 足够。
- SRAM 有两个别名（S=0x34 / NS=0x24），本工程用 0x34。
- 二进制 `.bss 结束≈0x3414326c`，以太网描述符在 0x341F8000 附近，中间约 700KB 空洞可用。

---

## 5. 多槽位系统 + 两个就地组包 bug

**架构**：
```
引导区(0x100000)   : 当前固件(常驻, 切换时被覆盖)
Slot-0/1/2         : 0x400000 / 0x800000 / 0xC00000, 每槽=[64B槽头|描述|镜像]
容器(PC发)         : [magic OFW2|ver|desc_len|img_len|描述|镜像]
切换               : 把槽镜像拷到引导区 + 校验 + 复位
```

### 5.1 描述存储为空
**现象**：`stored Slot-N ""`，描述没存进去。
**根因**：`desc` 源在 `ota_rx_buffer[16]`，与槽头 `h->desc` **同一位置**；先 `memset` 清掉源再 memcpy → 拷进去全 0。**源和目的地重叠**。
**解法**：先拷到本地 `desc_save[]`，再组槽头。

### 5.2 槽镜像开头 29 字节被清 0（导致切换永不可启动）
**现象**：`store` 看镜像头正常，`switch` 读 flash 镜像头全 0 → `sanity` 拒切换 → 每次切换都黑屏。
**根因**：就地组装时，槽头 `memset(h->desc,0,32)` 清 `buf[16..48)`，而镜像在 `buf[16+desc_len]=buf[19]`——**两区重叠**，memset 把镜像开头 29B 清成 0 后搬进槽。
**解法**：**先算 CRC → 先 memmove 镜像到 buf+64 → 最后组槽头**（让槽头区和镜像区物理分开）。

**经验总结**：在共享缓冲里就地组装"头 + 正文"时，若正文源与头区重叠，会互相踩——**先挪正文、再写头**。

### 5.3 切换卡死 / 不响应
**现象**：确认切换后画面停 "Switching..."，串口无声。
**根因**（早期版）：UI 在切换失败后 `for(;;)` 死循环 + 无失败码。
**解法**：`ota_switch_slot` 每个失败分支打串口码（-1~-4 带原因），UI 失败后回列表不卡死。

---

## 6. 版本文件清理

- 只留 `Binary/fsbl_universal.hex` + `Binary/appli.hex`（最新）+ `network-data.hex`。
- 其余版本 hex / `LED_2_Appli_v2_fix.bin` 等已删。
- 槽位、描述、bin 都从 `Appli/Release/LED_2_Appli.bin`（原始镜像）出，构建一次收口。

---

## 7. 待办 / 未来

- 诊断打印（`store: img[0..8)=...`、`switch: hdr/flash-img=...`）收尾清掉（保留 `stored Slot-N`、错误码）。
- `ota_send.py` 旧裸流统一成带描述容器，只留 `ota_gui.py`。
- **Boot Dispatcher（引导区常驻管理器 + 板载按键返回）**：让"运行任意固件都能一键回菜单"（当前机制切到无外设的裸固件后只能 ST-Link 重刷）。
- `ota_gui.py` 发送前可加 CRC 校验；分隔符/校验可选项。

---

## 8. 金句清单

- **代码在 flash 里跑，要擦它就得先把"擦它的程序"搬进 RAM。**
- **不用自定义 .ld 段；-ffunction-sections + 拷贝窗口就能搬函数。**
- **fault handler 在 NOR 里，XIP 断了它自己也取不了指 → 锁死无声；调试要走"代理自报"。**
- **非安全域写系统寄存器要用 NS 别名。**
- **XSPI 每条命令前排空 FIFO。**
- **就地组包头/正文重叠会互相踩：先挪正文再写头。**
- **xxd 的小端要看清；SP 上限别把 `_estack` 拒了。**