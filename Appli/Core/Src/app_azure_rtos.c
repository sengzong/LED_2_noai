/**
 * @brief  Azure RTOS(ThreadX) 应用入口：tx_application_define + 静态内存池
 */
#include "app_azure_rtos.h"
#include "app_threadx.h"
#include "app_netxduo.h"
#include "main.h"

#if (USE_STATIC_ALLOCATION == 1)
__ALIGN_BEGIN static UCHAR tx_byte_pool_buffer[TX_APP_MEM_POOL_SIZE] __ALIGN_END;
static TX_BYTE_POOL tx_app_byte_pool;
__ALIGN_BEGIN static UCHAR nx_byte_pool_buffer[NX_APP_MEM_POOL_SIZE] __attribute__((section(".NetXPoolSection"))) __ALIGN_END;
static TX_BYTE_POOL nx_app_byte_pool;
#endif

void tx_application_define(void *first_unused_memory)
{
#if (USE_STATIC_ALLOCATION == 1)
    UINT status = TX_SUCCESS;

    if (tx_byte_pool_create(&tx_app_byte_pool, "ThreadX App Memory Pool", tx_byte_pool_buffer, TX_APP_MEM_POOL_SIZE) != TX_SUCCESS)
    {
        Error_Handler();
    }

    status = app_threadx_init((VOID *)&tx_app_byte_pool);
    if (status != TX_SUCCESS)
    {
        Error_Handler();
    }

    if (tx_byte_pool_create(&nx_app_byte_pool, "NetXDuo App Memory Pool", nx_byte_pool_buffer, NX_APP_MEM_POOL_SIZE) != TX_SUCCESS)
    {
        Error_Handler();
    }

    status = app_netxduo_init((VOID *)&nx_app_byte_pool);
    if (status != TX_SUCCESS)
    {
        Error_Handler();
    }
#else
    TX_PARAMETER_NOT_USED(first_unused_memory);
#endif
}