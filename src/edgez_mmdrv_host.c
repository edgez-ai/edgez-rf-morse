/*
 * Copyright 2026 EdgeZeron AI AB
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdatomic.h>

#include "edgez_mmdrv_host.h"
#include "edgez_morse_tx_policy.h"
#include "mmdrv.h"
#include "mmpkt.h"

/*
 * Host side of the Morse RF-only boundary. This file is deliberately built
 * into the experimental RF archive, not the production reference-UMAC image.
 * Packet ownership terminates here when the private UMAC has not consumed an
 * event, preventing a future RF-only bring-up from leaking native packets.
 */

static atomic_uint_least16_t tx_pause_sources;
static struct edgez_mmdrv_host_ops host_ops = EDGEZ_MMDRV_HOST_OPS_INIT;
static void *host_ctx;

int edgez_mmdrv_host_set_ops(
    const struct edgez_mmdrv_host_ops *ops, void *ctx)
{
    if (ops == NULL || ops->version != EDGEZ_MMDRV_HOST_OPS_VERSION ||
        ops->size != sizeof(*ops))
    {
        return MMWLAN_INVALID_ARGUMENT;
    }
    host_ops = *ops;
    host_ctx = ctx;
    atomic_store(&tx_pause_sources, 0U);
    return MMWLAN_SUCCESS;
}

void edgez_mmdrv_host_clear_ops(void)
{
    const struct edgez_mmdrv_host_ops empty = EDGEZ_MMDRV_HOST_OPS_INIT;
    host_ops = empty;
    host_ctx = NULL;
    atomic_store(&tx_pause_sources, 0U);
}

uint16_t edgez_mmdrv_host_tx_pause_sources(void)
{
    return (uint16_t)atomic_load(&tx_pause_sources);
}

static bool edgez_mmdrv_mac_is_zero(const uint8_t mac[EDGEZ_MORSE_MAC_LEN])
{
    uint8_t combined = 0U;
    for (size_t i = 0; i < EDGEZ_MORSE_MAC_LEN; ++i)
    {
        combined |= mac[i];
    }
    return combined == 0U;
}

int edgez_mmdrv_raw_radio_start(
    const struct edgez_mmdrv_raw_radio_config *config,
    struct edgez_mmdrv_raw_radio_info *info)
{
    struct mmdrv_chip_info chip_info = { 0 };
    uint16_t vif_id = MMDRV_VIF_ID_INVALID;
    int status;

    if (config == NULL || info == NULL || config->country_code[0] == '\0' ||
        config->country_code[1] == '\0' ||
        config->country_code[2] != '\0' ||
        config->operating_frequency_hz == 0U ||
        config->operating_bw_mhz == 0U || config->primary_bw_mhz == 0U)
    {
        return MMWLAN_INVALID_ARGUMENT;
    }
    mmdrv_pre_init();
    status = mmdrv_init(&chip_info, config->country_code);
    if (status != MMWLAN_SUCCESS)
    {
        mmdrv_post_deinit();
        return status;
    }

    const uint8_t *mac = edgez_mmdrv_mac_is_zero(config->mac_addr) ?
        chip_info.mac_addr : config->mac_addr;
    status = mmdrv_add_if(&vif_id, mac, MMDRV_INTERFACE_TYPE_MESH);
    if (status == MMWLAN_SUCCESS)
    {
        status = mmdrv_set_channel(config->operating_frequency_hz,
                                   config->primary_1mhz_channel_index,
                                   config->operating_bw_mhz,
                                   config->primary_bw_mhz, false);
    }
    if (status != MMWLAN_SUCCESS)
    {
        if (vif_id != MMDRV_VIF_ID_INVALID)
        {
            (void)mmdrv_rm_if(vif_id);
        }
        mmdrv_deinit();
        mmdrv_post_deinit();
        return status;
    }

    info->vif_id = vif_id;
    for (size_t i = 0; i < EDGEZ_MORSE_MAC_LEN; ++i)
    {
        info->mac_addr[i] = mac[i];
    }
    info->chip_id = chip_info.morse_chip_id;
    return MMWLAN_SUCCESS;
}

void edgez_mmdrv_raw_radio_stop(uint16_t vif_id)
{
    if (vif_id != MMDRV_VIF_ID_INVALID)
    {
        (void)mmdrv_rm_if(vif_id);
    }
    mmdrv_deinit();
    mmdrv_post_deinit();
    edgez_mmdrv_host_clear_ops();
}

int edgez_mmdrv_raw_tx(
    const uint8_t *frame, size_t frame_length,
    const struct edgez_mmdrv_raw_tx_params *params)
{
    uint8_t packet_class;

    if (frame == NULL || frame_length == 0U || frame_length > UINT32_MAX ||
        params == NULL || params->attempts == 0U)
    {
        return MMWLAN_INVALID_ARGUMENT;
    }
    if (params->frame_type == EDGEZ_MMDRV_RAW_MANAGEMENT)
    {
        packet_class = MMDRV_PKT_CLASS_MGMT;
    }
    else if (params->frame_type == EDGEZ_MMDRV_RAW_DATA && params->tid <= 7U)
    {
        packet_class = (uint8_t)(MMDRV_PKT_CLASS_DATA_TID0 + params->tid);
    }
    else
    {
        return MMWLAN_INVALID_ARGUMENT;
    }

    struct mmpkt *packet = mmdrv_alloc_mmpkt_for_tx(
        packet_class, 0U, (uint32_t)frame_length);
    if (packet == NULL)
    {
        return MMWLAN_NO_MEM;
    }
    struct mmpktview *view = mmpkt_open(packet);
    mmpkt_append_data(view, frame, (uint32_t)frame_length);
    mmpkt_close(&view);

    struct mmdrv_tx_metadata *metadata = mmdrv_get_tx_metadata(packet);
    metadata->vif_id = params->vif_id;
    metadata->aid = params->aid;
    metadata->tid = params->tid;
    metadata->key_idx = edgez_morse_tx_descriptor_key_index(
        params->hardware_encrypt, params->key_index);
    metadata->flags = 0U;
    if (params->hardware_encrypt)
    {
        metadata->flags |= MMDRV_TX_FLAG_HW_ENC;
    }
    if (params->no_ack)
    {
        metadata->flags |= MMDRV_TX_FLAG_NO_ACK;
    }
    if (params->immediate_status)
    {
        metadata->flags |= MMDRV_TX_FLAG_IMMEDIATE_REPORT;
    }
    const bool management =
        params->frame_type == EDGEZ_MMDRV_RAW_MANAGEMENT;
    metadata->is_mgmt = management;
    metadata->rc_data.rates[0].attempts = edgez_morse_tx_attempts(
        management, params->attempts);
    metadata->rc_data.rates[0].rate = params->mcs;
    switch (params->bandwidth_mhz)
    {
        case 1: metadata->rc_data.rates[0].bw = MMRC_BW_1MHZ; break;
        case 2: metadata->rc_data.rates[0].bw = MMRC_BW_2MHZ; break;
        case 4: metadata->rc_data.rates[0].bw = MMRC_BW_4MHZ; break;
        case 8: metadata->rc_data.rates[0].bw = MMRC_BW_8MHZ; break;
        case 16: metadata->rc_data.rates[0].bw = MMRC_BW_16MHZ; break;
        default:
            mmpkt_release(packet);
            return MMWLAN_INVALID_ARGUMENT;
    }
    metadata->rc_data.rates[0].guard = edgez_morse_tx_short_guard(
        management, params->short_guard) ?
        MMRC_GUARD_SHORT : MMRC_GUARD_LONG;
    metadata->rc_data.rates[0].ss = MMRC_SPATIAL_STREAM_1;
    metadata->rc_data.rates[0].flags = params->rts_cts ?
        MMRC_MASK(MMRC_FLAGS_CTS_RTS) : 0U;
    for (size_t i = 1; i < MMRC_MAX_CHAIN_LENGTH; ++i)
    {
        metadata->rc_data.rates[i].rate = MMRC_MCS_UNUSED;
    }
    return mmdrv_tx_frame(packet,
                          params->frame_type == EDGEZ_MMDRV_RAW_MANAGEMENT);
}

void mmdrv_host_process_rx_frame(struct mmpkt *packet, uint16_t channel)
{
    if (host_ops.rx == NULL || !host_ops.rx(packet, channel, host_ctx))
    {
        mmpkt_release(packet);
    }
}

void mmdrv_host_process_tx_status(struct mmpkt *packet)
{
    if (host_ops.tx_status == NULL ||
        !host_ops.tx_status(packet, host_ctx))
    {
        mmpkt_release(packet);
    }
}

void mmdrv_host_set_tx_paused(uint16_t sources_mask, bool paused)
{
    if (paused)
    {
        atomic_fetch_or(&tx_pause_sources, sources_mask);
    }
    else
    {
        atomic_fetch_and(&tx_pause_sources, (uint16_t)~sources_mask);
    }
    if (host_ops.tx_pause != NULL)
    {
        host_ops.tx_pause(sources_mask, paused, host_ctx);
    }
}

void mmdrv_host_update_tx_paused(uint16_t sources_mask,
                                 mmdrv_host_update_tx_paused_cb_t callback)
{
    mmdrv_host_set_tx_paused(sources_mask,
                             callback != NULL && callback());
}

void mmdrv_host_hw_restart_required(void)
{
    if (host_ops.hardware_restart != NULL)
    {
        host_ops.hardware_restart(host_ctx);
    }
}

void mmdrv_host_health_check_required(void)
{
    if (host_ops.health_check != NULL)
    {
        host_ops.health_check(host_ctx);
    }
}

void mmdrv_host_beacon_loss(uint32_t num_beacons)
{
    if (host_ops.beacon_loss != NULL)
    {
        host_ops.beacon_loss(num_beacons, host_ctx);
    }
}

void mmdrv_host_connection_loss(uint32_t reason)
{
    if (host_ops.connection_loss != NULL)
    {
        host_ops.connection_loss(reason, host_ctx);
    }
}

void mmdrv_host_cqm_event(enum mmdrv_cqm_event event, int16_t rssi)
{
    if (host_ops.cqm != NULL)
    {
        host_ops.cqm(event, rssi, host_ctx);
    }
}

void mmdrv_host_hw_scan_complete(enum mmwlan_scan_state state)
{
    if (host_ops.scan_complete != NULL)
    {
        host_ops.scan_complete(state, host_ctx);
    }
}

void mmdrv_host_stats_increment_datapath_driver_rx_alloc_failures(void)
{
}

void mmdrv_host_stats_increment_datapath_driver_rx_read_failures(void)
{
}

void mmdrv_host_stats_increment_datapath_driver_tx_skbq_timeout(void)
{
}

void mmdrv_host_stats_increment_datapath_driver_tx_pending_status_timeout(void)
{
}

struct mmpkt *mmdrv_host_get_beacon(void)
{
    return host_ops.get_beacon != NULL ? host_ops.get_beacon(host_ctx) : NULL;
}
