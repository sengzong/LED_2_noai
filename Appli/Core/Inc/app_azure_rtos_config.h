#ifndef __APP_AZURE_RTOS_CONFIG_H
#define __APP_AZURE_RTOS_CONFIG_H

#define USE_STATIC_ALLOCATION   1
#define TX_APP_MEM_POOL_SIZE    (16 * 1024)     /* ThreadX app 内存池:UI 线程栈 8192 + 分配器开销 + 余量 */
#define NX_APP_MEM_POOL_SIZE    (30 * 1024)     /* NetX 内存池(.NetXPoolSection @0x341F8180, 匹配 MPU region1) */

#endif