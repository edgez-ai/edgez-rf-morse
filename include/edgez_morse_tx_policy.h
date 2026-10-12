/*
 * Copyright 2026 EdgeZeron AI AB
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* MM8108 firmware's mesh compatibility layout cannot form the standard
 * four-address CCMP AAD.  EdgeZ mesh keys must therefore stay in the private
 * UMAC for both receive and transmit processing. */
static inline bool edgez_morse_mesh_ccmp_software_only(void)
{
    return true;
}

/*
 * The Morse descriptor key index is meaningful only when firmware performs
 * encryption.  Private UMAC management frames are already protected in
 * software; leaking an installed pairwise-key slot into their descriptor can
 * make firmware treat those raw frames as hardware-encrypted traffic.
 */
static inline uint8_t edgez_morse_tx_descriptor_key_index(
    bool hardware_encrypt, uint8_t requested_key_index)
{
    return hardware_encrypt ? requested_key_index : 0U;
}

/* Morse's management path uses a conservative bootstrap rate policy. Keep
 * this vendor behavior at the RF boundary instead of leaking it into the
 * vendor-neutral UMAC request. */
static inline uint8_t edgez_morse_tx_attempts(
    bool management, uint8_t requested_attempts)
{
    return management ? 5U : requested_attempts;
}

static inline bool edgez_morse_tx_short_guard(
    bool management, bool requested_short_guard)
{
    return management ? false : requested_short_guard;
}
