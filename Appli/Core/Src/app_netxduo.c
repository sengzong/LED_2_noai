/* app_netxduo.c -- NetXDuo OTA(TCP 客户端): 连 PC 后接收 appli 固件
 * 板子作 TCP 客户端主动连 PC 监听端, 按 [len u32 LE][payload] 接收新的 appli.bin,
 * 存入内部 SRAM 的 OTA 缓冲区 ota_rx_buffer/ota_rx_len(后续由加载函数写入外部 NOR)。
 */
#include "app_netxduo.h"
#include "main.h"
#include "nx_stm32_eth_driver.h"
#include "nx_stm32_phy_driver.h"
#include "nxd_dhcp_client.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"

#define PC_SERVER_IP       IP_ADDRESS(192, 168, 2, 10)   /* PC 上 OTA 服务端监听 IP */
#define PC_SERVER_PORT     8080

/* IP 方式: 0=静态IP(PC 直连), 1=DHCP */
#define APP_USE_DHCP         0
#define APP_STATIC_IP        IP_ADDRESS(192, 168, 2, 11)
#define APP_NETMASK          IP_ADDRESS(255, 255, 255, 0)

#define PAYLOAD_SIZE         1536
#define PACKET_POOL_SIZE     ((PAYLOAD_SIZE + sizeof(NX_PACKET)) * 10)
#define IP_THREAD_STACK_SIZE (2 * 1024)
#define IP_THREAD_PRIORITY   10
#define LINK_THREAD_STACK_SIZE (2 * 1024)
#define LINK_THREAD_PRIORITY 11
#define APP_THREAD_STACK_SIZE (4 * 1024)
#define APP_THREAD_PRIORITY  10

/* OTA 接收缓冲(内部 SRAM, 512KB) */
#define OTA_RX_BUFFER_SIZE   (512 * 1024)

static uint8_t  ota_rx_buffer[OTA_RX_BUFFER_SIZE] __attribute__((aligned(32)));
static volatile uint32_t ota_rx_len = 0;

static NX_PACKET_POOL packet_pool;
static NX_IP          ip_instance;
static TX_THREAD      link_thread;
static TX_THREAD      app_thread;
#if (APP_USE_DHCP == 1)
static NX_DHCP        dhcp_client;
static TX_SEMAPHORE   dhcp_semaphore;
static ULONG          ip_address = 0;
static ULONG          network_mask = 0;
#endif

static VOID link_thread_entry(ULONG id);
static VOID app_thread_entry(ULONG id);
#if (APP_USE_DHCP == 1)
static VOID ip_address_change_notify_callback(NX_IP *ip_instance, VOID *additional_info);
#endif

/* ======================= OTA 写 NOR 代理 =======================
 * 硬约束: XSPI2 切 IO 模式瞬间, 0x70000000 的 XIP 取指断开, 代理必须整体在内部 SRAM 跑。
 * 本文件实现两层:
 *   ota_agent_main()  —— 自包含代理(直写 XSPI2 寄存器, 零 HAL/printf/memcpy 外部依赖),
 *                        独立 section .ota_agent, OTA 时刻被 memcpy 进 SRAM 跳转执行。
 *   ota_run_agent()    —— NOR 侧装载器: 拷贝代理机器码矩形窗进 SRAM, 刷 I-cache, 跳转。
 * OPI-DTR 常量取自通用 FSBL 的 MX25UM25645G 驱动: 8D-8D-8D, 16bit 指令 DTR, 32bit 地址 DTR。
 * NOR 在 XSPI2(非安全): 寄存器基址 0x4000A000。 */
#include "app_ota_flash.h"

/* ---- 扇区/其它 ---- */
#define OTA_APP_FLASH_BASE   0x70000000u      /* XSPI2 XIP 基址 */
#define OTA_APP_OFFSET       0x100000u        /* appli 在 NOR 的偏移(= FSBL EXTMEM_XIP_IMAGE_OFFSET) */
#define OTA_APP_HEADER       0x400u           /* FSBL 头部(偏移 0x100000) */
#define OTA_APP_TARGET       (OTA_APP_FLASH_BASE + OTA_APP_OFFSET)  /* 0x70100000: 新固件起点 */
#define OTA_APP_IMAGE_START  (OTA_APP_FLASH_BASE + OTA_APP_OFFSET + OTA_APP_HEADER) /* 0x70100400 当前运行点 */
#define OTA_SECTOR           0x1000u
#define OTA_PAGE             0x100u

/* 代理结果落盘: 固定 SRAM 0x341B0000(0x34143270..0x341F8000 之间全为空洞, startup 不碰, 跨复位保留)。
 * 布局 4 words: [0]=本轮 flags  [1]=rc  [2]=status_byte  [3]=OTA_HANDOFF_MAGIC */
#define OTA_HANDOFF_ADDR   0x341B0000u
#define OTA_HANDOFF_MAGIC  0x31525453u      /* "STR1"  */

/* ---- XSPI 寄存器偏移(见 stm32n647xx.h XSPI_TypeDef) ---- */
#define R_CR    0x000u
#define R_DCR1  0x008u
#define R_DCR2  0x00Cu
#define R_SR    0x020u
#define R_FCR   0x024u
#define R_DLR   0x040u
#define R_AR    0x048u
#define R_DR    0x050u
#define R_CCR   0x100u
#define R_TCR   0x108u
#define R_IR    0x110u
#define R_WCCR  0x180u
#define R_WTCR  0x188u
#define R_WIR   0x190u

/* CR 域 */
#define CR_FMODE     0x30000000u   /* FMODE[1:0] 掩码; 0x30000000 = memory-mapped */
#define CR_FMODE_IR  0x10000000u   /* indirect read */
#define CR_ABORT     0x00000002u
#define CR_MSEL      0xC0000000u   /* bit31:30 */
/* CCR 域(8D-8D-8D) */
#define CCR_BASE  0x20000000u  /* DQSE */
#define CCR_INST_ADDR_DATA (CCR_BASE | 0x00000004u /*IMODE2*/ | 0x00000008u /*IDTR*/ | 0x00000010u /*ISZ16*/ \
        | 0x00000400u /*ADMODE2*/ | 0x00000800u /*ADDTR*/ | 0x00003000u /*ADSZ32*/ \
        | 0x04000000u /*DMODE2*/ | 0x08000000u /*DDTR*/)
#define CCR_INST_ADDR      (CCR_BASE | 0x00000004u | 0x00000008u | 0x00000010u \
        | 0x00000400u | 0x00000800u | 0x00003000u)
#define CCR_INST_ONLY      (CCR_BASE | 0x00000004u | 0x00000008u | 0x00000010u)
#define CCR_DDTR           0x08000000u
/* TCR 域 */
#define TCR_DCYC    0x0000001Fu
#define TCR_SSHIFT  0x40000000u
#define TCR_DHQC    0x10000000u
/* SR 域 */
#define SR_BUSY  0x00000020u
#define SR_TCF   0x00000002u
#define SR_FTF   0x00000004u
#define SR_TEF   0x00000001u
#define SR_FLEV  0x00007F00u
/* FCR */
#define FC_CTCF  0x00000002u
#define FC_CTEF  0x00000001u

/* OPI-DTR 16bit 指令: MX25UM25645G 在 DTR 下第二字节为反码 */
#define OPI_INSTR16(c)  (uint16_t)((((uint16_t)(c)) << 8) | (uint16_t)((uint8_t)(~(c) & 0xFFu)))


/* ============ 自包含代理(仅直写寄存器, 位置无关, 在 SRAM 运行) ============ */

#if defined(__GNUC__)
#  define OAAI static __attribute__((always_inline)) inline
#else
#  define OAAI static inline
#endif

OAAI uint32_t oa_rd(const OtaJob *j, uint32_t off) { return *(volatile uint32_t *)(j->reg_base + off); }
OAAI void     oa_wr(const OtaJob *j, uint32_t off, uint32_t v) { *(volatile uint32_t *)(j->reg_base + off) = v; }

/* 等 SR 某组位达到状态; want_set=1 任一置位, 0 全部清; 超时返回 -1 */
OAAI int oa_wait(const OtaJob *j, uint32_t msk, int want_set, uint32_t tries)
{
    while (tries--) {
        uint32_t s = oa_rd(j, R_SR) & msk;
        if (want_set ? (s != 0u) : (s == 0u)) return 0;
    }
    return -1;
}

/* LED 打点: 代理自包含诊断。XIP 断后 fault handler 取指也会 fault(锁死), 因此进度只能用
 * SRAM 直写 GPIO 的方式标记——挂机后看 LED 组合即知崩在哪个阶段。
 * LED0=PG10(GPIOG_NS ODR 0x46020814)、LED1=PE10(GPIOE_NS ODR 0x46021014), 低电平点亮。
 *   LED1(g0=0)：代理进入                 = (亮,亮)
 *   abort 通过                          = (灭,亮)
 *   状态读通过                           = (亮,灭)
 *   恢复映射/准备返回                     = (灭,灭) */
#define OPI_LED0_ODR (0x46020814u)
#define OPI_LED1_ODR (0x46021014u)
OAAI void oa_led(int g0, int e0)
{
    uint32_t v;
    v = *(volatile uint32_t *)OPI_LED0_ODR; v &= ~(1u << 10u); v |= (g0 ? 0u : (1u << 10u));
    *(volatile uint32_t *)OPI_LED0_ODR = v;
    v = *(volatile uint32_t *)OPI_LED1_ODR; v &= ~(1u << 10u); v |= (e0 ? 0u : (1u << 10u));
    *(volatile uint32_t *)OPI_LED1_ODR = v;
}

/* ---- 自包含 USART1 直报(代理专用, 不依赖 HAL/printf/返回路径) ----
 * 恢复映射崩 / 复位失效时, 代理也能把进度打到串口(轮询 TXE)。
 * USART1_NS=0x42001000; ISR@0x1C(TXE bit7), TDR@0x28。 */
#define UA_UART (0x42001000u)
#define UA_ISR  (UA_UART + 0x1Cu)
#define UA_TDR  (UA_UART + 0x28u)
OAAI void oa_c(char c)
{
    while (((*(volatile uint32_t *)UA_ISR) & 0x80u) == 0u) { }
    *(volatile uint32_t *)UA_TDR = (uint32_t)(uint8_t)c;
}
OAAI void oa_hex(uint32_t v)
{
    int i;
    oa_c('0'); oa_c('x');
    for (i = 28; i >= 0; i -= 4) {
        uint32_t d = (v >> i) & 0xFu;
        oa_c((char)((d < 10u) ? ('0' + (int)d) : ('A' + (int)(d - 10u))));
    }
}
/* 打一行: [OTA <tag> rc=0x.. st=0x..] */
OAAI void oa_report(char tag, uint32_t rc, uint32_t st)
{
    oa_c('['); oa_c('O'); oa_c('T'); oa_c('A'); oa_c(']'); oa_c(' ');
    oa_c(tag); oa_c(' '); oa_c('r'); oa_c('c'); oa_c('=');
    oa_hex(rc); oa_c(' '); oa_c('s'); oa_c('t'); oa_c('=');
    oa_hex(st); oa_c('\r'); oa_c('\n');
}

OAAI void oa_flush_fifo(const OtaJob *j);   /* 前置声明(定义在 oa_abort_mm 附近) */

/* 完成一条间接命令(带/不带数据)
 * has_addr / has_data / dlen / dummy (dlen=dummy 仅 data 时有效) */
OAAI int oa_cfg(const OtaJob *j, uint32_t instr16, uint32_t addr, uint32_t dlen,
                int has_addr, int has_data, uint32_t dummy)
{
    uint32_t cr, ccr, tcr;

    oa_flush_fifo(j);               /* 每个命令前清 FIFO: 否则之前 DTR 读残留的字节会让
                                       无数据命令(如 WEN/擦除)误以为要发数据而卡 BUSY(上一次
                                       t 行 SR=0x124: BUSY+FLEVEL=1 正是这) */
    tcr = oa_rd(j, R_TCR);

    cr = oa_rd(j, R_CR);
    cr &= ~(CR_FMODE | CR_MSEL);          /* FMODE=0(indirect write), MSEL=0(IO7-0) */
    oa_wr(j, R_CR, cr);

    ccr = CCR_BASE;
    if (has_data) {
        ccr |= CCR_INST_ADDR_DATA;
        oa_wr(j, R_DLR, dlen - 1u);
        tcr &= ~TCR_SSHIFT;               /* DTR 收数须清 sample shift */
    } else if (has_addr) {
        ccr |= CCR_INST_ADDR;
        if ((tcr & TCR_DHQC) != 0u) ccr |= CCR_DDTR;   /* 与 HAL ConfigCmd 的 DHQC/DDTR 联动一致 */
    } else {
        ccr |= CCR_INST_ONLY;
        if ((tcr & TCR_DHQC) != 0u) ccr |= CCR_DDTR;
    }
    oa_wr(j, R_CCR, ccr);

    tcr &= ~(TCR_DCYC | TCR_SSHIFT);
    tcr |= (dummy & TCR_DCYC);
    oa_wr(j, R_TCR, tcr);

    oa_wr(j, R_IR, instr16);
    if (has_addr || has_data) oa_wr(j, R_AR, addr);

    if (!has_data) {
        if (oa_wait(j, SR_TCF, 1, 3000000u)) {
            oa_report('t', (uint32_t)instr16, oa_rd(j, R_SR));   /* 无数据命令没完成, 报 SR 看 TEF/BUSY */
            return -1;
        }
        oa_wr(j, R_FCR, FC_CTCF);
    }
    return 0;
}

/* 间接写数据(已 oa_cfg 为 data-write 后调用) */
OAAI int oa_tx(const OtaJob *j, const uint8_t *p, uint32_t n)
{
    while (n--) {
        if (oa_wait(j, SR_FTF, 1, 2000000u)) return -1;
        *(volatile uint8_t *)(j->reg_base + R_DR) = *p++;
    }
    if (oa_wait(j, SR_TCF, 1, 3000000u)) return -1;
    oa_wr(j, R_FCR, FC_CTCF);
    return 0;
}

/* 间接读 dlen 字节(已 oa_cfg 为 data-read 后调用; 触发 = 重写 AR) */
OAAI int oa_rx(const OtaJob *j, uint32_t addr, uint32_t dlen, uint8_t *out)
{
    uint32_t cr = oa_rd(j, R_CR);
    cr = (cr & ~CR_FMODE) | CR_FMODE_IR;  /* indirect read */
    cr &= ~CR_MSEL;
    oa_wr(j, R_CR, cr);
    oa_wr(j, R_AR, addr);                 /* 触发 */
    while (dlen--) {
        if (oa_wait(j, SR_FTF | SR_TCF, 1, 3000000u)) return -1;
        *out++ = *(volatile uint8_t *)(j->reg_base + R_DR);
    }
    if (oa_wait(j, SR_TCF, 1, 3000000u)) return -1;
    oa_wr(j, R_FCR, FC_CTCF);
    return 0;
}

/* 一条读命令(input/status): instr16 + 地址 + dlen 字节 + dummy */
OAAI int oa_read(const OtaJob *j, uint32_t instr16, uint32_t addr, uint32_t dlen, uint32_t dummy, uint8_t *out)
{
    if (oa_cfg(j, instr16, addr, dlen, 1, 1, dummy)) return -1;
    return oa_rx(j, addr, dlen, out);
}

/* 轮询 WIP(bit0=1 忙); 每次空转降频, tries 设大点覆盖扇区擦除 400ms */
OAAI int oa_wip(const OtaJob *j, uint32_t tries)
{
    uint32_t t;
    for (t = 0; t < tries; t++) {
        uint8_t st;
        if (oa_read(j, OPI_INSTR16(0x05), 0u, 1u, 4u, &st)) return -1;
        if ((st & 1u) == 0u) return 0;
        { volatile uint32_t d = 60000u; while (d) d--; }
    }
    return -2;
}

/* 写使能 0x06 + 等 WEL(bit1)置位(诊断期验证过, 现净链) */
OAAI int oa_wen(const OtaJob *j)
{
    uint32_t t;
    uint8_t st = 0u;
    if (oa_cfg(j, OPI_INSTR16(0x06), 0u, 0u, 0, 0, 0u)) return -1;
    for (t = 0; t < 400000u; t++) {
        if (oa_read(j, OPI_INSTR16(0x05), 0u, 1u, 4u, &st)) return -1;
        if ((st & 2u) != 0u) return 0;                    /* WEL */
    }
    return -2;
}

/* 4KB 扇区擦除 [start, end) */
OAAI int oa_erase_sectors(const OtaJob *j, uint32_t start, uint32_t end)
{
    uint32_t a;
    for (a = start; a < end; a += OTA_SECTOR) {
        if (oa_wip(j, 2000u)) return -10;
        if (oa_wen(j))        return -11;
        if (oa_cfg(j, OPI_INSTR16(0x21), a, 0u, 1, 0, 0u)) return -12;
        if (oa_wip(j, 4000000u)) return -13;   /* 等扇区擦除完成 */
    }
    return 0;
}

/* 页编程, 自动按 256B 页切开 */
OAAI int oa_program(const OtaJob *j, uint32_t addr, const uint8_t *src, uint32_t n)
{
    while (n) {
        uint32_t chunk = OTA_PAGE - (addr & (OTA_PAGE - 1u));
        if (chunk > n) chunk = n;
        if (oa_wip(j, 2000u))  return -20;
        if (oa_wen(j))         return -21;
        if (oa_cfg(j, OPI_INSTR16(0x12), addr, chunk, 1, 1, 0u)) return -22;
        if (oa_tx(j, src, chunk)) return -23;
        src += chunk; addr += chunk; n -= chunk;
    }
    return oa_wip(j, 2000u);
}

/* OPI 读回校验(0xEE 读, 20 dummy) */
OAAI int oa_verify(const OtaJob *j, uint32_t addr, const uint8_t *src, uint32_t n)
{
    uint8_t buf[OTA_PAGE];
    while (n) {
        uint32_t chunk = (n < OTA_PAGE) ? n : OTA_PAGE;
        uint32_t i;
        if (oa_read(j, OPI_INSTR16(0xEE), addr, chunk, 20u, buf)) return -31;
        for (i = 0; i < chunk; i++) {
            if (buf[i] != src[i]) return -1000 - (int)(addr + i);
        }
        addr += chunk; src += chunk; n -= chunk;
    }
    return 0;
}

/* 清空 XSPI FIFO(换向/恢复映射前必须, 否则残留旧读数据污染恢复后的首个映射读) */
OAAI void oa_flush_fifo(const OtaJob *j)
{
    uint32_t tries = 128u;
    while (((oa_rd(j, R_SR) & SR_FLEV) != 0u) && (tries-- != 0u)) {
        (void)oa_rd(j, R_DR);            /* 每读一个字节弹出一个 */
    }
    oa_wr(j, R_FCR, FC_CTCF | FC_CTEF);  /* 顺带清 TC/TE 标志 */
}

/* 退出内存映射 -> 命令模式。
 * 注意: 清 FMODE 后 XIP 立即断, fault 处理函数(在NOR)取指也会 fault -> 锁死无声。
 * 因此本函数前后由代理自带 LED/UART 打点标记进度(在 SRAM 里跑, 不受 XIP 影响)。 */
OAAI int oa_abort_mm(const OtaJob *j)
{
    __DSB(); __DMB();
    /* 先等 XIP 在途读排空(CPU 已全在 SRAM, 无新取指; 让最后一条 NOR 取指完成) */
    (void)oa_wait(j, SR_BUSY, 0, 2000000u);
    __DSB();
    if ((oa_rd(j, R_SR) & SR_BUSY) != 0u) {
        oa_wr(j, R_CR, oa_rd(j, R_CR) | CR_ABORT);
        if (oa_wait(j, SR_TCF, 1, 3000000u)) return -1;
        oa_wr(j, R_FCR, FC_CTCF);
        if (oa_wait(j, SR_BUSY, 0, 3000000u)) return -2;
    }
    oa_wr(j, R_CR, oa_rd(j, R_CR) & ~(CR_FMODE | CR_MSEL));
    oa_flush_fifo(j);                   /* 排掉 XIP 中断前的残留读数据 */
    __DSB();
    return 0;
}

/* 恢复内存映射: 只需读配置(0xEE/20dummy)+FMODE=MM 即可让 XIP 读回来。
 * appli 只从 NOR 读, 从不经 MM 写, 故无需回写 WCCR/WTCR/WIR。
 * 先排空 FIFO + 清 FMODE, 再配读配置 + 进 MM; 每步用自包含 UART 上报('1''2''3')。
 * 若恢复后首个映射取指还崩, 串口会显示最后到哪一步。 */
OAAI void oa_restore_mm(const OtaJob *j)
{
    uint32_t cr, tcr;

    oa_flush_fifo(j);                                   /* 清残留数据 + TC/TE */
    cr = oa_rd(j, R_CR) & ~(CR_FMODE | CR_MSEL);        /* FMODE=0, MSEL=0 */
    oa_wr(j, R_CR, cr);
    oa_report('1', 0u, oa_rd(j, R_SR));

    oa_wr(j, R_CCR, CCR_INST_ADDR_DATA);
    tcr = oa_rd(j, R_TCR);
    tcr &= ~(TCR_DCYC | TCR_SSHIFT);
    oa_wr(j, R_TCR, tcr | 20u);
    oa_wr(j, R_IR, OPI_INSTR16(0xEE));
    oa_wr(j, R_AR, 0u);
    oa_report('2', 0u, oa_rd(j, R_CR));

    cr = oa_rd(j, R_CR) & ~CR_FMODE;
    cr |= CR_FMODE;                     /* memory-mapped */
    oa_wr(j, R_CR, cr);
    oa_wr(j, R_FCR, FC_CTCF | FC_CTEF);
    __DSB(); __ISB();
    oa_report('3', 0u, oa_rd(j, R_CR));
}

/* 复位(直写 SCS AIRCR; 本 app 为非安全域, 必须写**非安全** SCS 别名 0xE002ED0C,
 * 硬编码的安全别名 0xE000ED0C 会 bus fault -> LOCKUP)。
 * 双别名都写: NS 别名从任何域都合法, 能触发系统复位。 */
OAAI void oa_reset(void)
{
    const uint32_t KEY = (0x5FAu << 16) | (1u << 2);   /* VECTKEY + SYSRESETREQ */
    __DSB();
    *(volatile uint32_t *)0xE002ED0Cu = KEY;   /* NS SCS AIRCR(本 app 所在域) */
    __DSB();
    for (;;) { }
}

/* ---- 代理入口(整个函数体 + 其内嵌字面池会整体拷进 SRAM) ----
 * 说明: 编译带 -ffunction-sections, 每个函数独占 .text.ota_agent_main 段, 字面池随函数
 * 生成在段尾, 因此 ota_run_agent 拷贝一个包含它的固定矩形窗即可完整搬走(无需自定义 .ld 段)。 */
__attribute__((used, noinline))
int ota_agent_main(const OtaJob *j)
{
    int rc = 0;
    uint32_t sbyte = 0u;

    __disable_irq();
    __DSB();
    oa_led(1, 1);                        /* 打点A: 已进代理, 且未退出内存映射 */

    if ((j->flags & OTA_F_PROBE) != 0u) {
        /* 只读寄存器快照(不切 IO / 不下 flash 命令): 供启动探针校验访问权与继承配置 */
        if (j->probe_out != 0u) {
            OtaProbeInfo *p = (OtaProbeInfo *)(uintptr_t)j->probe_out;
            p->cr   = oa_rd(j, R_CR);
            p->sr   = oa_rd(j, R_SR);
            p->dcr1 = oa_rd(j, R_DCR1);
            p->dcr2 = oa_rd(j, R_DCR2);
            p->ccr  = oa_rd(j, R_CCR);
            p->tcr  = oa_rd(j, R_TCR);
        }
        __DSB();
        __enable_irq();
        return 0;
    }

    rc = oa_abort_mm(j);
    oa_report('a', (uint32_t)rc, oa_rd(j, R_SR));   /* 串口自报: abort 结果 */
    if (rc != 0) goto done;
    oa_led(0, 1);                        /* 打点B: 已退出内存映射(此后 NOR 取指断裂) */

    if ((j->flags & OTA_F_STAT) != 0u) {
        uint8_t st = 0u;
        rc = oa_read(j, OPI_INSTR16(0x05), 0u, 1u, 4u, &st);
        sbyte = st;
        if (j->probe_out != 0u)
            ((OtaProbeInfo *)(uintptr_t)j->probe_out)->status_byte = st;
        if (rc != 0) rc = -3;
        oa_led(1, 0);                    /* 打点C: 状态读命令已执行 */
        oa_report('s', (uint32_t)rc, sbyte);        /* 串口自报: 状态读结果 */
    }

    if ((j->flags & OTA_F_ID) != 0u) {
        /* 读 JEDEC ID(0x9F, 6 字节, 0 dummy): 验证 OPI 读路径对 flash 回话是否真实有效 */
        uint8_t id[6], i;
        rc = oa_read(j, OPI_INSTR16(0x9F), 0u, 6u, 0u, id);
        sbyte = 0u;
        for (i = 0; i < 4u; i++) sbyte = (sbyte << 8) | (uint32_t)id[i];   /* 前 4 字节 */
        oa_report('j', (uint32_t)rc, sbyte);
    }

    if (((j->flags & OTA_F_PROGRAM) != 0u) && (rc == 0)) {
        uint32_t start = j->target & ~(OTA_SECTOR - 1u);
        uint32_t end   = ((j->target + j->len + OTA_SECTOR - 1u) & ~(OTA_SECTOR - 1u));
        rc = oa_erase_sectors(j, start, end);
        if (rc == 0) rc = oa_program(j, j->target, j->data, j->len);
        if ((rc == 0) && ((j->flags & OTA_F_VERIFY) != 0u))
            rc = oa_verify(j, j->target, j->data, j->len);
        oa_report('p', (uint32_t)rc, 0u);   /* 串口自报: 擦+写+读回结果 */
    }

done:
    if (j->probe_out != 0u)
        ((OtaProbeInfo *)(uintptr_t)j->probe_out)->result = rc;
    if (j->res_addr != 0u) {
        volatile uint32_t *r = (volatile uint32_t *)(uintptr_t)j->res_addr;
        r[0] = j->flags;
        r[1] = (uint32_t)rc;
        r[2] = (j->probe_out != 0u) ? ((OtaProbeInfo *)(uintptr_t)j->probe_out)->status_byte : 0u;
        r[3] = OTA_HANDOFF_MAGIC;
        __DSB();
    }
    oa_report('d', (uint32_t)rc, sbyte);   /* 串口自报: 最终结果(崩前必达) */
    if ((j->flags & OTA_F_RESET) != 0u) {
        oa_led(1, 1);                    /* 生产 OTA 完成, 复位前亮点双灯 */
        oa_reset();     /* 不复用(生产 OTA: 已擦过自身运行分区, 必须复位) */
    } else {
        oa_restore_mm(j);
        oa_led(0, 0);                    /* 打点D: 已恢复内存映射(到此才允许返回 NOR) */
        __enable_irq();
    }
    return rc;
}


/* ============ NOR 侧装载器与探针 ============ */

#define OTA_AGENT_FRAME_SIZE 16384u
static uint8_t ota_agent_sram[OTA_AGENT_FRAME_SIZE + 32u] __attribute__((aligned(32)));
static OtaProbeInfo ota_boot_probe;

const OtaProbeInfo *ota_get_boot_probe(void) { return &ota_boot_probe; }

/* 拷贝代理帧(含函数 + 其内嵌字面池)进 SRAM, 刷 I-cache 后跳转执行 */
int ota_run_agent(const OtaJob *job)
{
    uint32_t fn    = (uint32_t)(uintptr_t)&ota_agent_main;
    uint32_t delta = fn & 31u;
    uint32_t frame = fn & ~31u;
    int (*fnp)(const OtaJob *);

    memcpy(ota_agent_sram, (const void *)(uintptr_t)frame, OTA_AGENT_FRAME_SIZE);
    __DSB();
    SCB_InvalidateICache();
    __ISB();
    fnp = (int (*)(const OtaJob *))(uintptr_t)((uint32_t)(uintptr_t)ota_agent_sram + delta);
    return fnp(job);
}

/* 启动探针: 代理拷入 SRAM 执行(仅寄存器快照), 成功后返回 'OTA1' 魔数 */
uint32_t ota_probe_run(void)
{
    OtaJob job;
    int rc;

    job.reg_base  = (uint32_t)XSPI2;   /* 非安全 XSPI2 寄存器基址 0x4000A000 */
    job.target    = 0u;
    job.len       = 0u;
    job.data      = 0u;
    job.flags     = OTA_F_PROBE;
    job.probe_out = (uint32_t *)&ota_boot_probe;

    rc = ota_run_agent(&job);
    if (rc == 0) {
        ota_boot_probe.flash_map_addr = OTA_APP_FLASH_BASE;
        /* 探针每次上电都往固定 SRAM 写 '1BRP' 标记:
         * 用来证明(1)该地址真实可写 (2)startup/FSBL 不会清它 (3)软复位保留。 */
        ((volatile uint32_t *)OTA_HANDOFF_ADDR)[4] = 0x50425231u;   /* "1BRP" */
        return OTA_F_IMAGE_OK;
    }
    return OTA_F_IMAGE_OK ^ (uint32_t)(rc & 0xFFu);   /* 出错时返回非魔数 */
}

/* ============ NOR 侧辅助: sanity 校验 / 写盘 / 分步测试阶梯 ============ */

/* 真机分步测试阶梯级别(诊断/验收用, 生产设 0):
 * 0 = 启动不跑;  1 = 阶段1(OPI 状态读+JEDEC, 只读);  2 = 阶段1 + scratch 擦+写+读回 */
#ifndef OTA_BOOT_LADDER_LEVEL
#define OTA_BOOT_LADDER_LEVEL 0
#endif

/* 测试用的 scratch 扇区(距运行代码几 MB, 可安全擦写) */
#define OTA_SCRATCH_OFFSET 0x400000u

/* 简易保险: 新 appli 顶部 8 字节应像向量表(初始 MSP 在内部 SRAM, Reset 向量在 NOR XIP 区) */
static int ota_image_sane(const uint8_t *img, uint32_t len)
{
    uint32_t sp, rv;
    if (len < 8u) return 0;
    if (len > (OTA_RX_BUFFER_SIZE - OTA_APP_HEADER)) return 0;
    sp = (uint32_t)img[0] | ((uint32_t)img[1] << 8) | ((uint32_t)img[2] << 16) | ((uint32_t)img[3] << 24);
    rv = (uint32_t)img[4] | ((uint32_t)img[5] << 8) | ((uint32_t)img[6] << 16) | ((uint32_t)img[7] << 24);
    if ((sp < 0x34000000u) || (sp > 0x34200000u)) return 0;   /* SP=_estack=0x34200000 是合法顶值 */
    if ((rv < OTA_APP_FLASH_BASE) || (rv >= 0x7C000000u)) return 0;
    return 1;
}

/* 生产 OTA: ota_rx_buffer[0x400..]=收到的 appli, 组装成 [FSBL头|新appli] 写 NOR@0x100000, 校验后复位 */
static int ota_flash_write_image(void)
{
    OtaJob j;

    if (!ota_image_sane(&ota_rx_buffer[OTA_APP_HEADER], ota_rx_len))
    {
        printf("OTA: image sanity check FAILED - abort, keep running old app\r\n");
        return -99;
    }
    /* 保留现 FSBL 头部字节到 payload 前部(0x400 恰好页对齐) */
    memcpy(ota_rx_buffer, (const void *)OTA_APP_TARGET, OTA_APP_HEADER);

    j.reg_base  = (uint32_t)XSPI2;
    j.target    = OTA_APP_OFFSET;          /* NOR 偏移(非 XIP 地址) */
    j.len       = OTA_APP_HEADER + ota_rx_len;
    j.data      = ota_rx_buffer;
    j.flags     = OTA_F_PROGRAM | OTA_F_VERIFY | OTA_F_RESET;
    j.probe_out = 0;
    j.res_addr  = OTA_HANDOFF_ADDR;          /* 复位前把结果写固定 SRAM, 下次启动可打 */
    return ota_run_agent(&j);              /* 成功后代理内部复位, 不返回 */
}

/* ============ 多槽位: CRC / 槽位表 / 存储 / 切换 ============ */

volatile uint8_t  g_ota_receiving = 0;
volatile uint32_t g_ota_progress  = 0;
volatile int8_t   g_ota_last_slot = -1;
OtaSlotView g_slots[OTA_SLOT_COUNT];

/* slot NOR 偏移 -> XIP 可读地址(0x70000000) */
static uint32_t ota_slot_xip(uint32_t slot)
{
    return OTA_APP_FLASH_BASE + OTA_SLOT_BASE + slot * OTA_SLOT_SPACING;
}

uint32_t ota_crc32(const uint8_t *p, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFu, i;
    for (i = 0; i < len; i++) {
        uint32_t b = p[i], k;
        crc ^= b;
        for (k = 0; k < 8; k++)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

/* 经 XIP 重读各槽位头 -> g_slots(UI 用) */
void ota_slot_refresh(void)
{
    uint8_t i;
    for (i = 0; i < OTA_SLOT_COUNT; i++) {
        const OtaSlotHeader *h = (const OtaSlotHeader *)ota_slot_xip(i);
        g_slots[i].off = OTA_SLOT_BASE + (uint32_t)i * OTA_SLOT_SPACING;
        g_slots[i].ready = 0;
        g_slots[i].img_len = 0;
        g_slots[i].desc[0] = 0;
        if (h->magic == OTA_SLOT_MAGIC &&
            (h->img_len >= 8u) && (h->img_len <= (OTA_RX_BUFFER_SIZE - OTA_SLOT_HEADER))) {
            uint8_t dl = (h->desc_len > OTA_SLOT_DESC_MAX) ? OTA_SLOT_DESC_MAX : (uint8_t)h->desc_len;
            g_slots[i].ready = 1;
            g_slots[i].img_len = h->img_len;
            memset(g_slots[i].desc, 0, sizeof(g_slots[i].desc));
            memcpy(g_slots[i].desc, h->desc, dl);
        }
    }
}

/* 把 [img] 连同描述存进下一个空槽(不动引导区, 不复位) */
int ota_store_to_next_slot(const uint8_t *img, uint32_t img_len, const char *desc, uint32_t desc_len)
{
    OtaSlotHeader *h;
    OtaJob j;
    uint32_t slot, off, len;
    int rc;
    uint8_t i;
    /* desc 可能与 ota_rx_buffer 重叠(等于 OtaSlotHeader.desc 的位置), 若先 memset 会把源清掉,
     * 故先拷到本地再组装 */
    uint8_t desc_save[OTA_SLOT_DESC_MAX];

    if ((img == 0u) || (img_len < 8u) || (img_len > (OTA_RX_BUFFER_SIZE - OTA_SLOT_HEADER))) return -1;
    if (!ota_image_sane(img, img_len)) return -3;   /* 向量表 SP/RV 不合法 -> 拒收(防坏镜像进槽) */
    ota_slot_refresh();
    slot = OTA_SLOT_COUNT;
    for (i = 0; i < OTA_SLOT_COUNT; i++) { if (!g_slots[i].ready) { slot = i; break; } }
    if (slot >= OTA_SLOT_COUNT)
    {
        /* 全满: 轮询覆盖下一个槽(自然清掉旧槽), 避免无法继续存 */
        slot = (uint32_t)((g_ota_last_slot >= 0) ? ((uint8_t)g_ota_last_slot + 1u) : 0u) % OTA_SLOT_COUNT;
        printf("OTA: all slots full, overwrite Slot-%lu\r\n", (unsigned long)slot);
    }

    memset(desc_save, 0, sizeof(desc_save));
    if (desc_len > OTA_SLOT_DESC_MAX) desc_len = OTA_SLOT_DESC_MAX;
    memcpy(desc_save, desc, desc_len);

    off = OTA_SLOT_BASE + slot * OTA_SLOT_SPACING;
    len = OTA_SLOT_HEADER + img_len;

    /* 组装 [64B 槽头 | 镜像] 复用 OTA 收包缓冲。
     * 关键顺序(曾踩坑): 镜像原址 img 在 buf[16+desc_len], 与槽头 region [0..64) 重叠。
     * 必须先算 CRC(源未坏)、再 memmove 把镜像挪到 buf+64、最后才 memset/写槽头 ——
     * 若先 memset(h->desc) 会把镜像开头的几十字节清零, 存进 flash 的镜像永远无法启动。 */
    h = (OtaSlotHeader *)ota_rx_buffer;
    h->crc32 = ota_crc32(img, img_len);        /* memmove 前算(源还没被挪坏) */
    memmove(ota_rx_buffer + OTA_SLOT_HEADER, img, img_len);   /* 先挪镜像到镜像位 */
    h->magic    = OTA_SLOT_MAGIC;              /* 再组槽头: 此时 [0..64) 不再与镜像重叠 */
    h->img_len  = img_len;
    h->desc_len = desc_len;
    memset(h->desc, 0, sizeof(h->desc));
    memcpy(h->desc, desc_save, desc_len);

    j.reg_base   = (uint32_t)XSPI2;
    j.target     = off;                    /* NOR 偏移 */
    j.len        = len;
    j.data       = ota_rx_buffer;
    j.flags      = OTA_F_PROGRAM | OTA_F_VERIFY;   /* 不动引导区, 不 RESET */
    j.probe_out  = (uint32_t *)OTA_HANDOFF_ADDR;
    j.res_addr   = OTA_HANDOFF_ADDR;
    rc = ota_run_agent(&j);
    if (rc != 0) return -100 - rc;

    g_ota_last_slot = (int8_t)slot;
    ota_slot_refresh();
    return (int)slot;
}

/* 切换: 把槽内镜像拷到引导区(0x100000)覆盖当前固件, 校验后复位启动 */
int ota_switch_slot(uint32_t slot)
{
    const OtaSlotHeader *h;
    OtaJob j;
    uint32_t il;

    ota_slot_refresh();
    if (slot >= OTA_SLOT_COUNT) return -1;
    if (!g_slots[slot].ready)    return -2;
    h = (const OtaSlotHeader *)ota_slot_xip(slot);
    il = h->img_len;
    if ((il < 8u) || (il > (OTA_RX_BUFFER_SIZE - OTA_APP_HEADER))) return -3;
    /* 切换会把槽镜像盖到引导区(当前固件) -> 覆盖前必须确认它是可启动的 appli */
    if (!ota_image_sane((const uint8_t *)(ota_slot_xip(slot) + OTA_SLOT_HEADER), il)) return -4;

    /* XIP 读出槽镜像, 拼上引导区 FSBL 头, 组装 [0x400 头|固件] */
    memcpy(ota_rx_buffer, (const void *)OTA_APP_TARGET, OTA_APP_HEADER);
    memcpy(ota_rx_buffer + OTA_APP_HEADER,
           (const void *)(ota_slot_xip(slot) + OTA_SLOT_HEADER), il);
    printf("switch: agent now writing boot area + reset\r\n");

    j.reg_base   = (uint32_t)XSPI2;
    j.target     = OTA_APP_OFFSET;
    j.len        = OTA_APP_HEADER + il;
    j.data       = ota_rx_buffer;
    j.flags      = OTA_F_PROGRAM | OTA_F_VERIFY | OTA_F_RESET;
    j.probe_out  = 0;
    j.res_addr   = OTA_HANDOFF_ADDR;
    return ota_run_agent(&j);              /* 成功则代理复位, 不返回 */
}

/* 代理崩前结果落盘到固定 SRAM 的跨复位手递手
 *   宽带: [0]=本轮 flags  [1]=rc  [2]=status  [3]=MAGIC('1STR')  4 字 */
static void handoff_show(void)
{
    volatile uint32_t *hh = (volatile uint32_t *)OTA_HANDOFF_ADDR;
    if (hh[3] == OTA_HANDOFF_MAGIC) {
        printf(" prev-run: flags=0x%08lX rc=%ld status=0x%02lX  %s\r\n",
               (unsigned long)hh[0], (long)(int32_t)hh[1], (unsigned long)(hh[2] & 0xFFu),
               ((int32_t)hh[1] == 0) ? "OK" : "FAIL");
    }
}

/* 单次启动内的返回式阶梯测试: 每阶段代理执行 -> 恢复映射 -> 返回 -> 直接打印。
 * 若恢复映射仍崩, 代理会在崩前用自包含 UART 打 [OTA x rc=.. st=..] 行 + 把结果写
 * 固定 SRAM(0x341B0000), 所以照样看得见结果。 */
void ota_dbg_ladder(void)
{
    OtaJob j;
    volatile uint32_t *hh = (volatile uint32_t *)OTA_HANDOFF_ADDR;
    int rc;
#if (OTA_BOOT_LADDER_LEVEL >= 2)
    static uint8_t pat[OTA_SECTOR];
    uint32_t i;
#endif

    printf("\r\n=== OTA ladder (XSPI2 OPI-DTR agent in SRAM) ===\r\n");
    handoff_show();
    printf("L1 regs:  XSPI2=0x%08lX CR=0x%08lX SR=0x%08lX\r\n"
           "          DCR1=0x%08lX DCR2=0x%08lX CCR=0x%08lX TCR=0x%08lX\r\n",
           (unsigned long)(uintptr_t)XSPI2,
           (unsigned long)ota_boot_probe.cr, (unsigned long)ota_boot_probe.sr,
           (unsigned long)ota_boot_probe.dcr1, (unsigned long)ota_boot_probe.dcr2,
           (unsigned long)ota_boot_probe.ccr, (unsigned long)ota_boot_probe.tcr);

    j.reg_base   = (uint32_t)XSPI2;
    j.probe_out  = (uint32_t *)OTA_HANDOFF_ADDR;
    j.res_addr   = OTA_HANDOFF_ADDR;
    j.target     = 0u; j.len = 0u; j.data = 0u;
    hh[3] = 0u;

    /* L2: 状态读(返回式) */
    j.flags = OTA_F_STAT;
    rc = ota_run_agent(&j);
    printf("L2 status-read: rc=%d status=0x%02lX (WIP=%lu WEL=%lu)\r\n",
           rc, (unsigned long)hh[2],
           (unsigned long)((hh[2] >> 0) & 1u), (unsigned long)((hh[2] >> 1) & 1u));

    /* L2.5: JEDEC ID(验证读路径; 代理自报 [OTA] j 行含 ID) */
    j.flags = OTA_F_ID;
    (void)ota_run_agent(&j);

#if (OTA_BOOT_LADDER_LEVEL >= 2)
    /* L3: scratch 扇区 擦+编程+读回(真正写 NOR 的链路) */
    for (i = 0; i < sizeof(pat); i++) pat[i] = (uint8_t)(0x5Au ^ (uint8_t)(i & 0x3Fu));
    j.flags   = OTA_F_PROGRAM | OTA_F_VERIFY;
    j.target  = OTA_SCRATCH_OFFSET;
    j.len     = OTA_SECTOR;
    j.data    = pat;
    rc = ota_run_agent(&j);
    printf("L3 scratch RW: rc=%d target=0x%08lX len=0x%lX (erase+program+readback)\r\n",
           rc, (unsigned long)OTA_SCRATCH_OFFSET, (unsigned long)OTA_SECTOR);
#endif
    printf("=== ladder end ===\r\n");
}

UINT app_netxduo_init(VOID *memory_ptr)
{
    TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL *)memory_ptr;
    CHAR *pointer;

    printf("NetXDuo OTA client config\r\n");
    printf("PC server %lu.%lu.%lu.%lu:%u\r\n",
           (PC_SERVER_IP >> 24) & 0xFF, (PC_SERVER_IP >> 16) & 0xFF,
           (PC_SERVER_IP >> 8) & 0xFF, PC_SERVER_IP & 0xFF, (uint32_t)PC_SERVER_PORT);
    printf("\r\n");

    nx_system_initialize();

    if (tx_byte_allocate(byte_pool, (VOID **)&pointer, PACKET_POOL_SIZE, TX_NO_WAIT) != TX_SUCCESS)
        return TX_POOL_ERROR;
    if (nx_packet_pool_create(&packet_pool, "Packet Pool", PAYLOAD_SIZE, pointer, PACKET_POOL_SIZE) != NX_SUCCESS)
        return NX_POOL_ERROR;

    if (tx_byte_allocate(byte_pool, (VOID **)&pointer, IP_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
        return TX_POOL_ERROR;
    if (nx_ip_create(&ip_instance, "IP Instance", 0, 0, &packet_pool, nx_stm32_eth_driver, pointer,
                     IP_THREAD_STACK_SIZE, IP_THREAD_PRIORITY) != NX_SUCCESS)
        return NX_NOT_SUCCESSFUL;

#if (APP_USE_DHCP == 0)
    if (nx_ip_address_set(&ip_instance, APP_STATIC_IP, APP_NETMASK) != NX_SUCCESS)
        return NX_NOT_SUCCESSFUL;
#endif

    if (tx_byte_allocate(byte_pool, (VOID **)&pointer, 1024, TX_NO_WAIT) != TX_SUCCESS)
        return TX_POOL_ERROR;
    if (nx_arp_enable(&ip_instance, (VOID *)pointer, 1024) != NX_SUCCESS)
        return NX_NOT_SUCCESSFUL;
    if (nx_icmp_enable(&ip_instance) != NX_SUCCESS)
        return NX_NOT_SUCCESSFUL;
    if (nx_tcp_enable(&ip_instance) != NX_SUCCESS)
        return NX_NOT_SUCCESSFUL;
    if (nx_udp_enable(&ip_instance) != NX_SUCCESS)
        return NX_NOT_SUCCESSFUL;

    #if (APP_USE_DHCP == 1)
    if (nx_dhcp_create(&dhcp_client, &ip_instance, "DHCP Client") != NX_SUCCESS)
        return NX_DHCP_ERROR;
    if (tx_semaphore_create(&dhcp_semaphore, "DHCP Semaphore", 0) != TX_SUCCESS)
        return TX_SEMAPHORE_ERROR;
    #endif

    if (tx_byte_allocate(byte_pool, (VOID **)&pointer, LINK_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
        return TX_POOL_ERROR;
    if (tx_thread_create(&link_thread, "Link Thread", link_thread_entry, 0, pointer, LINK_THREAD_STACK_SIZE,
                         LINK_THREAD_PRIORITY, LINK_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS)
        return TX_THREAD_ERROR;

    if (tx_byte_allocate(byte_pool, (VOID **)&pointer, APP_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
        return TX_POOL_ERROR;
    if (tx_thread_create(&app_thread, "OTA Thread", app_thread_entry, 0, pointer, APP_THREAD_STACK_SIZE,
                         APP_THREAD_PRIORITY, APP_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_DONT_START) != TX_SUCCESS)
        return TX_THREAD_ERROR;

    return TX_SUCCESS;
}

/* OTA 线程：连 PC，裸流接收 appli 固件进 ota_rx_buffer; 连接断开即收完 */
static VOID app_thread_entry(ULONG id)
{
    NX_TCP_SOCKET ota_socket;
    UINT ret;

    (void)id;

#if (APP_USE_DHCP == 1)
    if (nx_ip_address_change_notify(&ip_instance, ip_address_change_notify_callback, NULL) != NX_SUCCESS)
        Error_Handler();
    if (nx_dhcp_start(&dhcp_client) != NX_SUCCESS)
        Error_Handler();

    printf("Looking for DHCP server...\r\n");
    if (tx_semaphore_get(&dhcp_semaphore, TX_WAIT_FOREVER) != TX_SUCCESS)
        Error_Handler();
    printf("IP: %lu.%lu.%lu.%lu\r\n",
           (ip_address >> 24) & 0xFF, (ip_address >> 16) & 0xFF,
           (ip_address >> 8) & 0xFF, ip_address & 0xFF);
#else
    printf("Static IP: %lu.%lu.%lu.%lu\r\n",
           (APP_STATIC_IP >> 24) & 0xFF, (APP_STATIC_IP >> 16) & 0xFF,
           (APP_STATIC_IP >> 8) & 0xFF,  APP_STATIC_IP & 0xFF);
#endif

    printf("Connecting to PC %lu.%lu.%lu.%lu:%u ...\r\n",
           (PC_SERVER_IP >> 24) & 0xFF, (PC_SERVER_IP >> 16) & 0xFF,
           (PC_SERVER_IP >> 8) & 0xFF, PC_SERVER_IP & 0xFF, (uint32_t)PC_SERVER_PORT);

    for (;;)
    {
        if (nx_tcp_socket_create(&ip_instance, &ota_socket, "OTA Socket",
                                 NX_IP_NORMAL, NX_FRAGMENT_OKAY, NX_IP_TIME_TO_LIVE,
                                 512, NX_NULL, NX_NULL) != NX_SUCCESS)
            Error_Handler();
        if (nx_tcp_client_socket_bind(&ota_socket, 0, NX_WAIT_FOREVER) != NX_SUCCESS)
            Error_Handler();

        ret = nx_tcp_client_socket_connect(&ota_socket, PC_SERVER_IP, PC_SERVER_PORT,
                                           10 * NX_IP_PERIODIC_RATE);
        if (ret != NX_SUCCESS)
        {
            nx_tcp_client_socket_unbind(&ota_socket);
            nx_tcp_socket_delete(&ota_socket);
            printf(".");
            tx_thread_sleep(2 * NX_IP_PERIODIC_RATE);
            continue;
        }
        printf("\r\nOTA: connected, waiting for firmware (PC sends then closes)...\r\n");

        /* 接收整流到 ota_rx_buffer; 前 4 字节决定是"容器(带描述)"还是"旧裸流"。UI 据
         * g_ota_receiving/g_ota_progress 画进度页。 */
        ota_rx_len = 0;
        g_ota_receiving = 1;
        g_ota_progress  = 0;
        {
            UINT running = TX_TRUE;
            while (running && (ota_rx_len < OTA_RX_BUFFER_SIZE))
            {
                NX_PACKET *pkt = NX_NULL;
                ULONG plen = 0;
                ret = nx_tcp_socket_receive(&ota_socket, &pkt, NX_IP_PERIODIC_RATE);
                if (ret != NX_SUCCESS) { running = TX_FALSE; break; }
                (void)nx_packet_length_get(pkt, &plen);
                if (plen == 0u) { running = TX_FALSE; }
                else if (ota_rx_len + plen <= OTA_RX_BUFFER_SIZE)
                {
                    memcpy(&ota_rx_buffer[ota_rx_len], pkt->nx_packet_prepend_ptr, (size_t)plen);
                    ota_rx_len += plen;

                    if ((ota_rx_len >= 16u) &&
                        (*(const uint32_t *)ota_rx_buffer == OTA_CTR_MAGIC))
                    {
                        uint32_t total = (uint32_t)ota_rx_len;
                        uint32_t dl = *(const uint32_t *)(&ota_rx_buffer[8]);
                        uint32_t il = *(const uint32_t *)(&ota_rx_buffer[12]);
                        if (dl <= 2000u) total = OTA_CTR_HDRZ + dl + il;
                        g_ota_progress = (ota_rx_len >= total) ? 1000u
                            : (uint32_t)(((uint64_t)ota_rx_len * 1000u) / ((total) ? total : 1u));
                        if ((ota_rx_len % 10240) < plen)
                            printf("OTA: %lu KB/%lu KB\r\n", (unsigned long)(ota_rx_len / 1024),
                                   (unsigned long)(total / 1024));
                    }
                    else if ((ota_rx_len % 10240) < plen)
                    {
                        printf("OTA: %lu KB\r\n", (unsigned long)(ota_rx_len / 1024));
                    }
                }
                nx_packet_release(pkt);
            }
        }
        g_ota_progress = 1000u;

        printf("OTA: session end, received %lu bytes.\r\n", (unsigned long)ota_rx_len);

        if (ota_rx_len > OTA_CTR_HDRZ && (*(const uint32_t *)ota_rx_buffer == OTA_CTR_MAGIC))
        {
            /* --- 容器模式(PC ota_gui.py): 解析描述+镜像 -> 存进下一个空槽 --- */
            uint32_t ver = *(const uint32_t *)(&ota_rx_buffer[4]);
            uint32_t dl  = *(const uint32_t *)(&ota_rx_buffer[8]);
            uint32_t il  = *(const uint32_t *)(&ota_rx_buffer[12]);
            printf("OTA: container ver=%lu desc_len=%lu img_len=%lu\r\n",
                   (unsigned long)ver, (unsigned long)dl, (unsigned long)il);
            if ((ver == OTA_CTR_VER) && (dl <= 2000u) && (il >= 8u) &&
                ((OTA_CTR_HDRZ + dl + il) == ota_rx_len) &&
                ((OTA_CTR_HDRZ + dl + il) <= OTA_RX_BUFFER_SIZE))
            {
                int slot = ota_store_to_next_slot(&ota_rx_buffer[OTA_CTR_HDRZ + dl], il,
                                                  (const char *)&ota_rx_buffer[OTA_CTR_HDRZ], dl);
                if (slot >= 0)
                    printf("OTA: stored Slot-%d \"%.*s\"\r\n", slot, (int)((dl > 24u) ? 24u : dl),
                           &ota_rx_buffer[OTA_CTR_HDRZ]);
                else
                    printf("OTA: store slot FAIL rc=%d\r\n", slot);
            }
            else
            {
                printf("OTA: bad container (ver=%lu dl=%lu il=%lu rx=%lu)\r\n",
                       (unsigned long)ver, (unsigned long)dl, (unsigned long)il,
                       (unsigned long)ota_rx_len);
            }
        }
        else if (ota_rx_len > 0u)
        {
            /* --- 旧裸流: 单发覆盖引导区 + 复位(向后兼容) --- */
            if (ota_rx_len < OTA_RX_BUFFER_SIZE - OTA_APP_HEADER)
            {
                memmove(&ota_rx_buffer[OTA_APP_HEADER], &ota_rx_buffer[0], ota_rx_len);
                printf("OTA: Flash programming...\r\n");
                int rc = ota_flash_write_image();   /* 内部复位 */
                if (rc != 0)
                    printf("OTA: FLASH FAIL rc=%d\r\n", rc);
            }
        }
        g_ota_receiving = 0;   /* 收完, UI 回到槽位列表 */

        nx_tcp_socket_disconnect(&ota_socket, 10 * NX_IP_PERIODIC_RATE);
        nx_tcp_client_socket_unbind(&ota_socket);
        nx_tcp_socket_delete(&ota_socket);
        tx_thread_sleep(2 * NX_IP_PERIODIC_RATE);
    }
}

/* 链接线程：监测网线插拔，link up 后使能接口并启动 OTA 线程 */
static VOID link_thread_entry(ULONG id)
{
    UINT status;
    ULONG actual_status;
    UINT linkdown = 2;
    int32_t link_state;

    (void)id;

    while (1)
    {
        status = nx_ip_interface_status_check(&ip_instance, 0, NX_IP_LINK_ENABLED, &actual_status, 10);
        if (status == NX_SUCCESS)
        {
            if ((linkdown == 1) || (linkdown == 2))
            {
                linkdown = 0;
                printf("Network cable connected.\r\n");
                link_state = nx_eth_phy_get_link_state();
                if (link_state == ETH_PHY_STATUS_100MBITS_FULLDUPLEX)
                    printf("100Mbps full-duplex.\r\n");
                else if (link_state == ETH_PHY_STATUS_100MBITS_HALFDUPLEX)
                    printf("100Mbps half-duplex.\r\n");
                else if (link_state == ETH_PHY_STATUS_10MBITS_FULLDUPLEX)
                    printf("10Mbps full-duplex.\r\n");
                else
                    printf("10Mbps half-duplex.\r\n");

                nx_ip_driver_direct_command(&ip_instance, NX_LINK_ENABLE, &actual_status);
                tx_thread_resume(&app_thread);
                tx_thread_relinquish();
            }
        }
        else
        {
            if ((linkdown == 0) || (linkdown == 2))
            {
                linkdown = 1;
                printf("Network cable not connected.\r\n");
                nx_ip_driver_direct_command(&ip_instance, NX_LINK_DISABLE, &actual_status);
            }
        }
        tx_thread_sleep(NX_IP_PERIODIC_RATE);
    }
}

#if (APP_USE_DHCP == 1)
static VOID ip_address_change_notify_callback(NX_IP *nx_ip_ptr, VOID *additional_info)
{
    if (nx_ip_address_get(nx_ip_ptr, &ip_address, &network_mask) != NX_SUCCESS)
        Error_Handler();
    if (ip_address != 0)
        tx_semaphore_put(&dhcp_semaphore);
}
#endif