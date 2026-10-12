/*
 * Copyright 2026 EdgeZeron AI AB
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>

#include "edgez_morse_tx_policy.h"

int main(void)
{
    assert(edgez_morse_mesh_ccmp_software_only());

    assert(edgez_morse_tx_descriptor_key_index(false, 0U) == 0U);
    assert(edgez_morse_tx_descriptor_key_index(false, 3U) == 0U);
    assert(edgez_morse_tx_descriptor_key_index(true, 0U) == 0U);
    assert(edgez_morse_tx_descriptor_key_index(true, 3U) == 3U);

    assert(edgez_morse_tx_attempts(true, 2U) == 5U);
    assert(!edgez_morse_tx_short_guard(true, true));
    assert(edgez_morse_tx_attempts(false, 2U) == 2U);
    assert(edgez_morse_tx_short_guard(false, true));
    return 0;
}
