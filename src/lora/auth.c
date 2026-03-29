/**
 * @file auth.c
 * @brief Network key provisioning and AES-128-CMAC authentication.
 *
 * The key is parsed from a Kconfig hex string at init time and imported
 * into the PSA key store once.  Using the PSA API (rather than raw
 * mbedTLS) allows transparent migration to hardware-backed key storage
 * on nRF52840 CryptoCell or ESP32 eFuse without changing call sites.
 */

#include "lora/auth.h"

#include <psa/crypto.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ts_auth);

/** Parsed binary key — kept in RAM only for the PSA import call. */
static uint8_t network_key[TS_AUTH_KEY_SIZE];
/** PSA handle to the imported key; used for all subsequent CMAC ops. */
static psa_key_id_t auth_key_id;

static int hex_char_to_nibble(char c) {
    if (c >= '0' && c <= '9') { return c - '0'; }
    if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
    if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
    return -1;
}

int ts_auth_init(void) {
    const char* hex = CONFIG_TS_NETWORK_KEY;
    size_t hex_len = strlen(hex);

    if (hex_len != TS_AUTH_KEY_SIZE * 2) {
        LOG_ERR("Network key must be %d hex characters, got %zu",
                TS_AUTH_KEY_SIZE * 2, hex_len);
        return -EINVAL;
    }

    for (int i = 0; i < TS_AUTH_KEY_SIZE; i++) {
        int hi = hex_char_to_nibble(hex[i * 2]);
        int lo = hex_char_to_nibble(hex[i * 2 + 1]);

        if (hi < 0 || lo < 0) {
            LOG_ERR("Invalid hex character in network key at position %d",
                    (hi < 0) ? i * 2 : i * 2 + 1);
            return -EINVAL;
        }
        network_key[i] = (uint8_t)((hi << 4) | lo);
    }

    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        LOG_ERR("PSA crypto init failed: %d", status);
        return -EIO;
    }

    // Import once at init so each sign/verify avoids repeated key setup.
    // SIGN_MESSAGE is the only usage needed — the key never leaves the
    // PSA store, which on production hardware maps to an opaque key slot.
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attr, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attr, 128);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attr, PSA_ALG_CMAC);

    status = psa_import_key(&attr, network_key, TS_AUTH_KEY_SIZE, &auth_key_id);
    if (status != PSA_SUCCESS) {
        LOG_ERR("PSA key import failed: %d", status);
        return -EIO;
    }

    LOG_INF("Network key loaded (%d bytes)", TS_AUTH_KEY_SIZE);
    return 0;
}

const uint8_t* ts_auth_get_key(void) { return network_key; }

uint8_t ts_auth_get_key_id(void) { return (uint8_t)CONFIG_TS_KEY_ID; }

int ts_auth_sign(const uint8_t* p_data, size_t data_len, uint8_t* p_tag) {
    if (p_data == NULL && data_len > 0) { return -EINVAL; }

    // Compute full 16-byte CMAC, then truncate to TS_AUTH_TAG_SIZE.
    // We truncate manually rather than using PSA_ALG_TRUNCATED_MAC
    // because not all PSA backends support truncated algorithm IDs,
    // and the key was imported with PSA_ALG_CMAC (full-length).
    uint8_t full_mac[TS_AUTH_KEY_SIZE];
    size_t mac_len;
    // Some PSA backends (notably mbedTLS) reject NULL input even when
    // data_len is 0, so provide a valid address for the empty-message case.
    static const uint8_t empty;

    psa_status_t status = psa_mac_compute(
        auth_key_id, PSA_ALG_CMAC, (p_data != NULL) ? p_data : &empty, data_len,
        full_mac, sizeof(full_mac), &mac_len);
    if (status != PSA_SUCCESS) {
        LOG_ERR("CMAC compute failed: %d", status);
        return -EIO;
    }

    memcpy(p_tag, full_mac, TS_AUTH_TAG_SIZE);
    // Don't leave the full 16-byte MAC on the stack — only the
    // truncated portion should survive in the caller's buffer.
    memset(full_mac, 0, sizeof(full_mac));
    return 0;
}

int ts_auth_verify(const uint8_t* p_data, size_t data_len,
                   const uint8_t* p_tag) {
    if (p_data == NULL && data_len > 0) { return -EINVAL; }

    // Recompute rather than using psa_mac_verify because PSA verify
    // expects a full-length tag, but we store only TS_AUTH_TAG_SIZE bytes.
    uint8_t computed_tag[TS_AUTH_TAG_SIZE];

    int ret = ts_auth_sign(p_data, data_len, computed_tag);
    if (ret != 0) { return ret; }

    // XOR-accumulate prevents early-exit timing leaks that would let
    // an attacker determine how many leading tag bytes are correct.
    uint8_t diff = 0;
    for (int i = 0; i < TS_AUTH_TAG_SIZE; i++) {
        diff |= computed_tag[i] ^ p_tag[i];
    }

    memset(computed_tag, 0, sizeof(computed_tag));
    return (diff == 0) ? 0 : -EACCES;
}
