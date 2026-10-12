/*
 * Copyright 2026 EdgeZeron AI AB
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Morse Micro's prebuilt code calls the mmhal_mesh_ccm_* ABI. Keep that
 * vendor compatibility surface in this adapter and forward it to the
 * vendor-neutral implementation supplied by the EdgeZ platform archive.
 */

#include <stddef.h>
#include <stdint.h>

int edgez_mesh_ccm_encrypt(const uint8_t *key, size_t key_len,
                           const uint8_t *nonce, size_t mic_len,
                           const uint8_t *plain, size_t len,
                           const uint8_t *aad, size_t aad_len,
                           uint8_t *crypt, uint8_t *tag);
int edgez_mesh_ccm_decrypt(const uint8_t *key, size_t key_len,
                           const uint8_t *nonce, size_t mic_len,
                           const uint8_t *crypt, size_t len,
                           const uint8_t *aad, size_t aad_len,
                           const uint8_t *tag, uint8_t *plain);

int mmhal_mesh_ccm_encrypt(const uint8_t *key, size_t key_len,
                           const uint8_t *nonce, size_t mic_len,
                           const uint8_t *plain, size_t len,
                           const uint8_t *aad, size_t aad_len,
                           uint8_t *crypt, uint8_t *tag)
{
    return edgez_mesh_ccm_encrypt(key, key_len, nonce, mic_len, plain, len,
                                  aad, aad_len, crypt, tag);
}

int mmhal_mesh_ccm_decrypt(const uint8_t *key, size_t key_len,
                           const uint8_t *nonce, size_t mic_len,
                           const uint8_t *crypt, size_t len,
                           const uint8_t *aad, size_t aad_len,
                           const uint8_t *tag, uint8_t *plain)
{
    return edgez_mesh_ccm_decrypt(key, key_len, nonce, mic_len, crypt, len,
                                  aad, aad_len, tag, plain);
}
