/*
 * Copyright 2026 EdgeZeron AI AB
 * SPDX-License-Identifier: Apache-2.0
 */

#include "edgez_mmdrv_host.h"
#include "edgez_umac_morse_adapter.h"
#include "mmhal_os.h"
#include "mmosal.h"
#include "mmpkt.h"

#include <stdatomic.h>

#define EDGEZ_MORSE_RX_QUEUE_DEPTH 32U
#define EDGEZ_MORSE_RX_WORKER_STACK_WORDS 4096U

struct edgez_morse_rx_work
{
    struct mmpkt *packet;
    uint16_t channel;
};

static struct mmosal_queue *rx_queue;
static struct mmosal_task *rx_worker_task;
static atomic_bool rx_worker_stopped = ATOMIC_VAR_INIT(true);

int edgez_rf_platform_init(void)
{
    mmhal_init();
    return 0;
}

static void edgez_rf_rx_worker(void *ctx)
{
    struct edgez_morse_rx_work work;

    while (mmosal_queue_pop(rx_queue, &work, UINT32_MAX))
    {
        if (work.packet == NULL)
        {
            break;
        }
        (void)edgez_umac_morse_dispatch_rx(ctx, work.packet, work.channel);
    }

    /* A stop marker can overtake no entries in this FIFO. Still drain any
     * entries left by a failed/forced shutdown so native packets never leak. */
    while (mmosal_queue_pop(rx_queue, &work, 0U))
    {
        if (work.packet != NULL)
        {
            mmpkt_release(work.packet);
        }
    }
    atomic_store_explicit(&rx_worker_stopped, true, memory_order_release);
    mmosal_task_delete(NULL);
}

static bool edgez_rf_bound_host_rx(struct mmpkt *packet, uint16_t channel,
                                   void *ctx)
{
    (void)ctx;
    const struct edgez_morse_rx_work work = {
        .packet = packet,
        .channel = channel,
    };

    /* Morse invokes this hook from its driver task. Dispatching the private
     * UMAC inline can issue synchronous firmware commands (for example
     * SET_STA_STATE), which deadlock because this same driver task must handle
     * the command response. Transfer packet ownership to a worker instead. */
    return rx_queue != NULL && mmosal_queue_push(rx_queue, &work, 0U);
}

static bool edgez_rf_bound_host_tx_status(struct mmpkt *packet, void *ctx)
{
    (void)edgez_umac_morse_dispatch_tx_status(ctx, packet);
    return true; /* dispatch_tx_status always consumes the native packet */
}

static void edgez_rf_bound_host_tx_pause(uint16_t sources_mask, bool paused,
                                         void *ctx)
{
    (void)edgez_umac_morse_dispatch_tx_pause(ctx, sources_mask, paused);
}

static void edgez_rf_bound_host_hardware_restart(void *ctx)
{
    (void)edgez_umac_morse_dispatch_hardware_restart(ctx);
}

static void edgez_rf_bound_host_scan_complete(int state, void *ctx)
{
    (void)edgez_umac_morse_dispatch_scan_complete(ctx, (int)state);
}

static struct mmpkt *edgez_rf_host_get_beacon(void *ctx)
{
    (void)ctx;
    return edgez_umac_morse_get_beacon();
}

int edgez_rf_bind_umac(struct edgez_umac *umac)
{
    if (umac == NULL || rx_queue != NULL || rx_worker_task != NULL)
    {
        return EDGEZ_UMAC_INVALID_ARGUMENT;
    }
    rx_queue = mmosal_queue_create(EDGEZ_MORSE_RX_QUEUE_DEPTH,
                                   sizeof(struct edgez_morse_rx_work),
                                   "edgez_rf_rx");
    if (rx_queue == NULL)
    {
        return EDGEZ_UMAC_NO_CAPACITY;
    }
    atomic_store_explicit(&rx_worker_stopped, false, memory_order_release);
    rx_worker_task = mmosal_task_create(
        edgez_rf_rx_worker, umac, MMOSAL_TASK_PRI_NORM,
        EDGEZ_MORSE_RX_WORKER_STACK_WORDS, "edgez_umac_rx");
    if (rx_worker_task == NULL)
    {
        atomic_store_explicit(&rx_worker_stopped, true, memory_order_release);
        mmosal_queue_delete(rx_queue);
        rx_queue = NULL;
        return EDGEZ_UMAC_NO_CAPACITY;
    }
    const struct edgez_mmdrv_host_ops ops = {
        .version = EDGEZ_MMDRV_HOST_OPS_VERSION,
        .size = sizeof(struct edgez_mmdrv_host_ops),
        .rx = edgez_rf_bound_host_rx,
        .tx_status = edgez_rf_bound_host_tx_status,
        .tx_pause = edgez_rf_bound_host_tx_pause,
        .hardware_restart = edgez_rf_bound_host_hardware_restart,
        .scan_complete = edgez_rf_bound_host_scan_complete,
        .get_beacon = edgez_rf_host_get_beacon,
    };
    if (edgez_mmdrv_host_set_ops(&ops, umac) != EDGEZ_MORSE_SUCCESS)
    {
        edgez_rf_unbind_umac();
        return EDGEZ_UMAC_INVALID_STATE;
    }
    return EDGEZ_UMAC_OK;
}

void edgez_rf_unbind_umac(void)
{
    edgez_mmdrv_host_clear_ops();
    if (rx_queue == NULL)
    {
        return;
    }

    const struct edgez_morse_rx_work stop = { 0 };
    (void)mmosal_queue_push(rx_queue, &stop, 1000U);
    for (unsigned elapsed_ms = 0U;
         elapsed_ms < 1500U &&
         !atomic_load_explicit(&rx_worker_stopped, memory_order_acquire);
         elapsed_ms += 10U)
    {
        mmosal_task_sleep(10U);
    }
    if (!atomic_load_explicit(&rx_worker_stopped, memory_order_acquire) &&
        rx_worker_task != NULL)
    {
        mmosal_task_delete(rx_worker_task);
    }
    rx_worker_task = NULL;

    struct edgez_morse_rx_work work;
    while (mmosal_queue_pop(rx_queue, &work, 0U))
    {
        if (work.packet != NULL)
        {
            mmpkt_release(work.packet);
        }
    }
    mmosal_queue_delete(rx_queue);
    rx_queue = NULL;
    atomic_store_explicit(&rx_worker_stopped, true, memory_order_release);
}

/* ABI bridge for edgez-halow-mesh archives released before edgez-rf-api was
 * split out. New source uses only the generic names above; remove these three
 * aliases after all supported prebuilt archives have moved to that ABI. */
int edgez_umac_morse_bind_rf_host(struct edgez_umac *umac)
{
    return edgez_rf_bind_umac(umac);
}

void edgez_umac_morse_unbind_rf_host(void)
{
    edgez_rf_unbind_umac();
}
