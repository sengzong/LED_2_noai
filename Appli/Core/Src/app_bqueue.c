/**
 ****************************************************************************************************
 * @file        app_bqueue.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       app_bqueue.c文件
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 * 
 * 实验平台:正点原子 N647开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 * 
 ****************************************************************************************************
 */

#include "app_bqueue.h"

void app_bqueue_init(app_bqueue_t *bqueue, uint8_t buffer_nb, uint8_t **buffers)
{
    uint8_t i;

    tx_semaphore_create(&bqueue->free, NULL, buffer_nb);
    tx_semaphore_create(&bqueue->ready, NULL, 0);

    bqueue->buffer_nb = buffer_nb;
    for (i = 0; i < buffer_nb; i++)
    {
        bqueue->buffers[i] = buffers[i];
    }
    bqueue->free_idx = 0;
    bqueue->ready_idx = 0;
}

uint8_t *app_bqueue_get_free(app_bqueue_t *bqueue, uint8_t is_blocking)
{
    uint8_t *free;

    if (tx_semaphore_get(&bqueue->free, is_blocking ? TX_WAIT_FOREVER : TX_NO_WAIT) == TX_NO_INSTANCE)
    {
        return NULL;
    }

    free = bqueue->buffers[bqueue->free_idx];
    bqueue->free_idx = (bqueue->free_idx + 1) % bqueue->buffer_nb;

    return free;
}

void app_bqueue_put_free(app_bqueue_t *bqueue)
{
    tx_semaphore_put(&bqueue->free);
}

uint8_t *app_bqueue_get_ready(app_bqueue_t *bqueue)
{
    uint8_t *ready;

    tx_semaphore_get(&bqueue->ready, TX_WAIT_FOREVER);

    ready = bqueue->buffers[bqueue->ready_idx];
    bqueue->ready_idx = (bqueue->ready_idx + 1) % bqueue->buffer_nb;

    return ready;
}

void app_bqueue_put_ready(app_bqueue_t *bqueue)
{
    tx_semaphore_put(&bqueue->ready);
}
