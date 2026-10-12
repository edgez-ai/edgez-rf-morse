/*
 * Copyright 2026 EdgeZeron AI AB
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "edgez_rf_api.h"
#include "edgez_umac.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mmpkt;

/** Normalize Morse/MM8108 RX header conventions into the EdgeZ RX ABI. */
bool edgez_umac_morse_normalize_rx(struct edgez_umac_rx_frame *frame);

/** Allocate the next native Morse beacon packet from the copied template. */
struct mmpkt *edgez_umac_morse_get_beacon(void);

/** Future host-callback bridge. Each packet function consumes the packet. */
int edgez_umac_morse_dispatch_rx(struct edgez_umac *umac,
                                 struct mmpkt *packet, uint16_t driver_channel);
int edgez_umac_morse_dispatch_tx_status(struct edgez_umac *umac,
                                        struct mmpkt *packet);
int edgez_umac_morse_dispatch_tx_pause(struct edgez_umac *umac,
                                       uint16_t source_mask, bool paused);
int edgez_umac_morse_dispatch_hardware_restart(struct edgez_umac *umac);
int edgez_umac_morse_dispatch_scan_complete(struct edgez_umac *umac,
                                            int driver_scan_status);

/**
 * Attach the private UMAC event observer to the live Morse callbacks.
 *
 * The bridge borrows packets and always leaves ownership with the reference
 * UMAC. Its bool host-hook entry points are intentionally consume-capable so a
 * later authoritative switch does not require another driver ABI change.
 */
#ifdef __cplusplus
}
#endif
