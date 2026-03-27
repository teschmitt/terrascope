#ifndef TS_AUTH_H
#define TS_AUTH_H

/**
 * @defgroup auth Authentication
 * @brief Network key provisioning and message authentication.
 *
 * All nodes in a deployment must share the same 128-bit network key.
 * During development the key is supplied via CONFIG_TS_NETWORK_KEY
 * (Kconfig hex string). For production builds, use platform-specific
 * secure key storage (see SECURITY.md).
 * @{
 */

#include <stddef.h>
#include <stdint.h>

/** @brief Network key size in bytes (128-bit AES key). */
#define TS_AUTH_KEY_SIZE 16

/**
 * @brief Truncated CMAC tag size appended to each packet.
 *
 * 8 bytes (64 bits) balances authentication strength against LoRa
 * airtime — the full 16-byte CMAC is unnecessary for a low-rate
 * sensor mesh where brute-force forgery is impractical.
 */
#define TS_AUTH_TAG_SIZE 8

/**
 * @brief Initialize the auth module.
 *
 * Parses the CONFIG_TS_NETWORK_KEY hex string into a binary key.
 * Must be called once at startup before any auth operations.
 *
 * @return 0 on success, -EINVAL if the key string is malformed
 */
int ts_auth_init(void);

/**
 * @brief Get a pointer to the parsed network key.
 *
 * @return Pointer to TS_AUTH_KEY_SIZE bytes of key material
 */
const uint8_t *ts_auth_get_key(void);

/**
 * @brief Get the active key identifier.
 *
 * @return CONFIG_TS_KEY_ID value (0-255)
 */
uint8_t ts_auth_get_key_id(void);

/**
 * @brief Compute AES-128-CMAC tag over a buffer.
 *
 * Produces a TS_AUTH_TAG_SIZE-byte truncated CMAC tag using the
 * network key loaded by ts_auth_init().
 *
 * @param p_data   Input data to authenticate
 * @param data_len Length of input data
 * @param p_tag    Output buffer (must be at least TS_AUTH_TAG_SIZE bytes)
 * @return 0 on success, -EINVAL if p_data is NULL and data_len > 0,
 *         -EIO on crypto failure
 */
int ts_auth_sign(const uint8_t *p_data, size_t data_len, uint8_t *p_tag);

/**
 * @brief Verify AES-128-CMAC tag over a buffer.
 *
 * Recomputes the tag and compares it against the provided tag
 * using constant-time comparison.
 *
 * @param p_data   Input data that was authenticated
 * @param data_len Length of input data
 * @param p_tag    Tag to verify (TS_AUTH_TAG_SIZE bytes)
 * @return 0 if tag is valid, -EINVAL if p_data is NULL and data_len > 0,
 *         -EACCES if tag does not match, -EIO on crypto failure
 */
int ts_auth_verify(const uint8_t *p_data, size_t data_len,
                   const uint8_t *p_tag);

/** @} */

#endif  // TS_AUTH_H
