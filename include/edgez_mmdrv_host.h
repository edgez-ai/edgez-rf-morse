/*
 * Copyright 2026 EdgeZeron AI AB
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EDGEZ_MMDRV_HOST_OPS_VERSION 1U
#define EDGEZ_MORSE_MAC_LEN 6U

/* Native packets stay opaque at the public adapter boundary. The consumer
 * binding owns the concrete Morse packet definition and lifetime rules. */
struct mmpkt;

enum edgez_morse_status
{
    EDGEZ_MORSE_SUCCESS = 0,
    EDGEZ_MORSE_ERROR = 1,
    EDGEZ_MORSE_INVALID_ARGUMENT = 2,
    EDGEZ_MORSE_UNAVAILABLE = 3,
    EDGEZ_MORSE_NO_MEMORY = 5,
};

/**
 * Consumer callbacks for the reusable Morse RF boundary.
 *
 * RX and TX-status callbacks return true only when they consume ownership of
 * the native packet. The boundary releases unconsumed packets. Register this
 * table before calling mmdrv_init().
 */
struct edgez_mmdrv_host_ops
{
    uint16_t version;
    uint16_t size;
    bool (*rx)(struct mmpkt *packet, uint16_t channel, void *ctx);
    bool (*tx_status)(struct mmpkt *packet, void *ctx);
    void (*tx_pause)(uint16_t sources_mask, bool paused, void *ctx);
    void (*hardware_restart)(void *ctx);
    void (*health_check)(void *ctx);
    void (*beacon_loss)(uint32_t num_beacons, void *ctx);
    void (*connection_loss)(uint32_t reason, void *ctx);
    void (*cqm)(int event, int16_t rssi, void *ctx);
    void (*scan_complete)(int state, void *ctx);
    struct mmpkt *(*get_beacon)(void *ctx);
};

#define EDGEZ_MMDRV_HOST_OPS_INIT                         \
    {                                                     \
        .version = EDGEZ_MMDRV_HOST_OPS_VERSION,          \
        .size = sizeof(struct edgez_mmdrv_host_ops),      \
    }

int edgez_mmdrv_host_set_ops(
    const struct edgez_mmdrv_host_ops *ops, void *ctx);
void edgez_mmdrv_host_clear_ops(void);
uint16_t edgez_mmdrv_host_tx_pause_sources(void);

enum edgez_mmdrv_raw_frame_type
{
    EDGEZ_MMDRV_RAW_DATA = 0,
    EDGEZ_MMDRV_RAW_MANAGEMENT,
};

struct edgez_mmdrv_raw_tx_params
{
    enum edgez_mmdrv_raw_frame_type frame_type;
    uint16_t aid;
    uint8_t vif_id;
    uint8_t tid;
    uint8_t key_index;
    uint8_t mcs;
    uint8_t attempts;
    uint8_t bandwidth_mhz;
    bool hardware_encrypt;
    bool no_ack;
    bool immediate_status;
    bool short_guard;
    bool rts_cts;
};

/** Allocate a native packet, copy a complete raw 802.11 frame, and queue it. */
int edgez_mmdrv_raw_tx(
    const uint8_t *frame, size_t frame_length,
    const struct edgez_mmdrv_raw_tx_params *params);

struct edgez_mmdrv_raw_radio_config
{
    char country_code[3];
    uint8_t mac_addr[EDGEZ_MORSE_MAC_LEN];
    uint32_t operating_frequency_hz;
    uint8_t operating_bw_mhz;
    uint8_t primary_bw_mhz;
    uint8_t primary_1mhz_channel_index;
};

struct edgez_mmdrv_raw_radio_info
{
    uint16_t vif_id;
    uint8_t mac_addr[EDGEZ_MORSE_MAC_LEN];
    uint32_t chip_id;
};

/** Boot MM8108 and create one fixed-channel mesh interface for raw testing. */
int edgez_mmdrv_raw_radio_start(
    const struct edgez_mmdrv_raw_radio_config *config,
    struct edgez_mmdrv_raw_radio_info *info);
void edgez_mmdrv_raw_radio_stop(uint16_t vif_id);

#ifdef __cplusplus
}
#endif
