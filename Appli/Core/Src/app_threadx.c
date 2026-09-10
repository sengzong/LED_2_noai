/**
 * @brief  ThreadX 应用线程：LCD/触摸 UI 线程（OTA 页）
 */
#include "app_threadx.h"
#include "model_select.h"
#include "stdio.h"

#define UI_THREAD_STACK_SIZE   8192
#define UI_THREAD_PRIORITY     22   /* 低优先级 */

static TX_THREAD ui_thread;

static VOID ui_thread_entry(ULONG id)
{
    (void)id;

    printf("ThreadX: UI thread started\r\n");
    for (;;)
    {
        model_select_run();       /* OTA 页(APP 选择); Yes 确认后返回, 稍停重新进入 */
        tx_thread_sleep(100);
    }
}

UINT app_threadx_init(VOID *memory_ptr)
{
    TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL *)memory_ptr;
    UCHAR *stack;

    if (tx_byte_allocate(byte_pool, (VOID **)&stack, UI_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
    {
        return TX_POOL_ERROR;
    }

    if (tx_thread_create(&ui_thread, "UI Thread", ui_thread_entry, 0, stack, UI_THREAD_STACK_SIZE,
                         UI_THREAD_PRIORITY, UI_THREAD_PRIORITY, 4, TX_AUTO_START) != TX_SUCCESS)
    {
        return TX_THREAD_ERROR;
    }

    return TX_SUCCESS;
}