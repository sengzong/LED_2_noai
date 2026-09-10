#ifndef __APP_OTA_FLASH_H
#define __APP_OTA_FLASH_H

#include <stdint.h>

/* ========================= OTA 写 NOR 代理(OPI 直写 XSPI2) =========================
 * 硬约束: XSPI2 切 IO 模式的瞬间, 0x70000000 XIP 取指断开 -> 代理必须整体在内部 SRAM 执行。
 * 自包含: 代理(ota_agent_main)绝不调用外部函数/HAL/printf, 全部直写寄存器常量,
 * 位置无关(Thumb PC-relative 字面量)。编译带 -ffunction-sections, 函数与其字面池
 * 独占一个 .text 子段, 由 ota_run_agent 在 OTA 时刻把包含它的 8KB 矩形窗 memcpy 进
 * SRAM 再按函数首地址跳转执行。
 * OPI-DTR 常量(8D-8D-8D / 16bit 指令 DTR / 32bit 地址 DTR / 8 线数据 DTR+DQS)
 * 取自通用 FSBL 的 MX25UM25645G 驱动。NOR 在 XSPI2: XIP 区 0x70000000,
 * 外设寄存器 NS 基址 0x4000A000(若被安全门控改 S 基址 0x5000A000)。
 */

/* 任务描述: 全部字段由 NOR 侧装载器填写, 代理(SRAM)只读 */
typedef struct {
    uint32_t         reg_base;  /* XSPI 外设寄存器基址(本板 NS=0x5802A000) */
    uint32_t         target;    /* NOR 绝对地址偏移(擦除/编程起点, 建议 4KB 对齐, 如 0x100000) */
    uint32_t         len;       /* payload 字节数, data[0] 对应 target 处字节 */
    const uint8_t   *data;      /* SRAM 内 payload(编程模式); PROBE 模式未用 */
    uint32_t         flags;     /* OTA_F_* */
    uint32_t        *probe_out; /* 指向 OtaProbeInfo(SRAM), 代理回填 */
    uint32_t         res_addr;  /* 结果回写地址(固定 SRAM, 跨复位保留); 代理崩前写 {flags,rc,status,MAGIC} */
} OtaJob;

#define OTA_F_PROBE   0x00000001u  /* 仅读 XSPI 寄存器快照回填(不切 IO、不下命令), 供启动探针 */
#define OTA_F_STAT    0x00000002u  /* 切命令模式后发 OPI 0x05 状态读, status_byte 回填, 随即恢复 MM */
#define OTA_F_PROGRAM 0x00000004u  /* 擦除 [target, target+len) 扇区并页编程 payload */
#define OTA_F_VERIFY  0x00000008u  /* 编程后 OPI 读回校验 */
#define OTA_F_RESET   0x00000010u  /* 完成后 CPU 复位(生产 OTA); 未置则恢复内存映射后返回 */
#define OTA_F_ID      0x00000020u  /* 读 JEDEC ID(0x9F, 6 字节), 用于验证 OPI 读路径真伪 */

/* ---- 多槽位固件: 引导区(0x100000)常驻当前固件; 新固件先入槽位, 切换=拷槽位覆盖引导区 ----
 * 槽位布局: [64B 描述头 | 固件镜像], 等宽间距排在 NOR 高区(避开 FSBL/引导区) */
#define OTA_SLOT_BASE    0x400000u
#define OTA_SLOT_SPACING 0x400000u
#define OTA_SLOT_COUNT   3u
#define OTA_SLOT_HEADER  64u
#define OTA_SLOT_DESC_MAX 32u

#define OTA_SLOT_MAGIC   0x31544C53u   /* "SLT1" */

/* 槽位头(64B, 放槽位首地址): magic/img_len/desc_len/crc32(对镜像)/desc[..]/rsv */
typedef struct {
    uint32_t magic;
    uint32_t img_len;
    uint32_t desc_len;
    uint32_t crc32;
    uint8_t  desc[OTA_SLOT_DESC_MAX];
    uint8_t  rsv[64u - 16u - OTA_SLOT_DESC_MAX];
} OtaSlotHeader;

/* 容器(PC ota_gui.py 发): [magic u32 "OFW2" | ver u32 | desc_len u32 | img_len u32 | desc | img] */
#define OTA_CTR_MAGIC  0x3257464Fu   /* "OFW2" */
#define OTA_CTR_VER    2u
#define OTA_CTR_HDRZ   16u           /* 容器头字节数 */

#define OTA_F_IMAGE_OK 0x314F5441u /* ota_probe_run 成功魔数 (= 'OTA1') */

typedef struct {
    uint32_t cr;   /* XSPI CR    */
    uint32_t sr;   /* XSPI SR    */
    uint32_t dcr1; /* XSPI DCR1  */
    uint32_t dcr2; /* XSPI DCR2  */
    uint32_t ccr;  /* XSPI CCR   */
    uint32_t tcr;  /* XSPI TCR   */
    uint32_t status_byte; /* OTA_F_STAT: OPI 状态寄存器(bit0=WIP, bit1=WEL) */
    uint32_t flash_map_addr; /* NOR 内存映射基址(XSPI2 AXI) */
    int32_t  result;   /* 代理回写的 return code(0=成功); 崩溃前写定, 可跨复位读 */
} OtaProbeInfo;

/* ---- NOR 侧(由 app_netxduo.c 实现) ---- */
uint32_t ota_probe_run(void);               /* 启动探针: 代理拷入 SRAM 执行 OTA_F_PROBE; 成功返回 OTA_F_IMAGE_OK */
int      ota_run_agent(const OtaJob *job);  /* 装载器: memcpy 代理帧进 SRAM -> 刷 I-cache -> 跳转, 返回代理 return code */
int      ota_agent_main(const OtaJob *job); /* SRAM 自包含 OPI 代理(绝不外部调用) */
void     ota_dbg_ladder(void);              /* 真机分步测试阶梯(见 OTA_BOOT_LADDER_LEVEL) */

/* ---- 多槽位 OTA(NOR 侧辅助, 供 UI/网络线程) ---- */
extern volatile uint8_t  g_ota_receiving;   /* 1=正在接收, UI 画进度页 */
extern volatile uint32_t g_ota_progress;    /* 0..1000 千分比 */
extern volatile int8_t   g_ota_last_slot;   /* 最近写入的槽位(-1=无) */

typedef struct {
    uint32_t off;               /* NOR 槽位偏移 */
    uint32_t img_len;
    uint16_t ready;             /* 1=槽内有合法固件 */
    char     desc[OTA_SLOT_DESC_MAX + 1];
} OtaSlotView;
extern OtaSlotView g_slots[OTA_SLOT_COUNT];

uint32_t ota_crc32(const uint8_t *p, uint32_t len);
void     ota_slot_refresh(void);            /* 经 XIP 重读各槽位头 -> g_slots */
int      ota_store_to_next_slot(const uint8_t *img, uint32_t img_len, const char *desc, uint32_t desc_len); /* 存下一空槽, 返回槽号/-1 */
int      ota_switch_slot(uint32_t slot);    /* 拷槽镜像覆盖引导区 + 复位(不返回成功, 复位即切) */

#endif /* __APP_OTA_FLASH_H */