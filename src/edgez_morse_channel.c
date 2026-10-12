/*
 * Copyright 2026 EdgeZeron AI AB
 * SPDX-License-Identifier: Apache-2.0
 */

#include "edgez_rf_api.h"

#include "mmregdb.h"
#include "mmwlan.h"

bool edgez_rf_resolve_s1g_channel(const char country_code[3], uint8_t channel,
                                  uint32_t *frequency_khz,
                                  uint8_t *bandwidth_mhz)
{
    const struct mmwlan_s1g_channel_list *domain;

    if (country_code == NULL || country_code[0] == '\0' ||
        country_code[1] == '\0' || country_code[2] != '\0' || channel == 0U ||
        frequency_khz == NULL || bandwidth_mhz == NULL) {
        return false;
    }

    domain = mmwlan_lookup_regulatory_domain(get_regulatory_db(), country_code);
    if (domain == NULL) {
        return false;
    }
    for (uint32_t index = 0; index < domain->num_channels; ++index) {
        const struct mmwlan_s1g_channel *candidate = &domain->channels[index];

        if (candidate->s1g_chan_num == channel) {
            *frequency_khz = candidate->centre_freq_hz / 1000U;
            *bandwidth_mhz = candidate->bw_mhz;
            return true;
        }
    }
    return false;
}
