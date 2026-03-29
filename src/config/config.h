#ifndef TS_CONFIG_H
#define TS_CONFIG_H

/**
 * @defgroup config Runtime Configuration
 * @brief Runtime-tunable parameters with NVS-backed persistence.
 *
 * Compile-time defaults (TS_*_DEFAULT) are used on first boot or after
 * factory reset.  At runtime, values are read from the live config struct
 * via ts_config_get().
 *
 * Constants that size static arrays (TS_ROUTING_SEEN_CACHE_SIZE,
 * TS_CONTENTION_POOL_SIZE, TS_ROUTING_TABLE_SIZE) remain compile-time
 * only and are not part of this schema.
 * @{
 */

#include <stdint.h>

/* ── Routing defaults ──────────────────────────────────────────────── */

/** @brief Default time-to-live for new outgoing messages. */
#define TS_CONFIG_ROUTING_TTL_DEFAULT 5

/* ── Contention forwarding defaults ────────────────────────────────── */

/** @brief Default minimum forwarding delay in ms (weakest signal). */
#define TS_CONFIG_CONTENTION_DELAY_MIN_MS_DEFAULT 0

/** @brief Default maximum forwarding delay in ms (strongest signal). */
#define TS_CONFIG_CONTENTION_DELAY_MAX_MS_DEFAULT 5000

/** @brief Default RSSI threshold for weakest signal (dBm). */
#define TS_CONFIG_CONTENTION_RSSI_WEAK_DEFAULT (-120)

/** @brief Default RSSI threshold for strongest signal (dBm). */
#define TS_CONFIG_CONTENTION_RSSI_STRONG_DEFAULT (-30)

/* ── Routing table defaults ────────────────────────────────────────── */

/** @brief Default staleness timeout in seconds (5 minutes). */
#define TS_CONFIG_ROUTING_TABLE_STALE_TIMEOUT_S_DEFAULT 300

/* ── Node identity defaults ────────────────────────────────────────── */

/** @brief Default node ID (should be unique per device). */
#define TS_CONFIG_NODE_ID_DEFAULT 0x0001

/* ── LoRa radio defaults ──────────────────────────────────────────── */

/** @brief Default LoRa frequency in Hz. */
#define TS_CONFIG_LORA_FREQUENCY_DEFAULT 865100000

/** @brief Default LoRa spreading factor (SF5–SF12). */
#define TS_CONFIG_LORA_SF_DEFAULT 10

/** @brief Default LoRa bandwidth in kHz (enum value). */
#define TS_CONFIG_LORA_BW_DEFAULT 125

/** @brief Default LoRa coding rate (1 = 4/5, 2 = 4/6, 3 = 4/7, 4 = 4/8). */
#define TS_CONFIG_LORA_CR_DEFAULT 1

/** @brief Default LoRa TX power in dBm. */
#define TS_CONFIG_LORA_TX_POWER_DEFAULT 4

/* ── Timing defaults ───────────────────────────────────────────────── */

/** @brief Default sensor poll interval in seconds. */
#define TS_CONFIG_SENSOR_INTERVAL_S_DEFAULT 10

/** @brief Default heartbeat (node status) interval in seconds. */
#define TS_CONFIG_HEARTBEAT_INTERVAL_S_DEFAULT 7

/** @brief Default routing table aging interval in seconds. */
#define TS_CONFIG_ROUTING_TABLE_AGE_INTERVAL_S_DEFAULT 60

/**
 * @brief Runtime configuration for all tunable parameters.
 *
 * Populated with TS_*_DEFAULT values at first boot or factory reset.
 * Persisted to NVS via the Settings subsystem (see config.c).
 */
struct ts_config {
    /* Routing */
    uint8_t routing_ttl;

    /* Contention forwarding */
    uint32_t contention_delay_min_ms;
    uint32_t contention_delay_max_ms;
    int16_t contention_rssi_weak;
    int16_t contention_rssi_strong;

    /* Routing table */
    uint32_t routing_table_stale_timeout_s;

    /* Node identity */
    uint16_t node_id;

    /* LoRa radio */
    uint32_t lora_frequency;
    uint8_t lora_sf;
    uint16_t lora_bw;
    uint8_t lora_cr;
    int8_t lora_tx_power;

    /* Timing intervals */
    uint32_t sensor_interval_s;
    uint32_t heartbeat_interval_s;
    uint32_t routing_table_age_interval_s;
};

/** @brief Static initializer that fills every field with its default. */
#define TS_CONFIG_DEFAULTS                                                   \
    {                                                                        \
        .routing_ttl = TS_CONFIG_ROUTING_TTL_DEFAULT,                        \
        .contention_delay_min_ms =                                           \
            TS_CONFIG_CONTENTION_DELAY_MIN_MS_DEFAULT,                       \
        .contention_delay_max_ms =                                           \
            TS_CONFIG_CONTENTION_DELAY_MAX_MS_DEFAULT,                       \
        .contention_rssi_weak = TS_CONFIG_CONTENTION_RSSI_WEAK_DEFAULT,      \
        .contention_rssi_strong = TS_CONFIG_CONTENTION_RSSI_STRONG_DEFAULT,  \
        .routing_table_stale_timeout_s =                                     \
            TS_CONFIG_ROUTING_TABLE_STALE_TIMEOUT_S_DEFAULT,                 \
        .node_id = TS_CONFIG_NODE_ID_DEFAULT,                                \
        .lora_frequency = TS_CONFIG_LORA_FREQUENCY_DEFAULT,                  \
        .lora_sf = TS_CONFIG_LORA_SF_DEFAULT,                                \
        .lora_bw = TS_CONFIG_LORA_BW_DEFAULT,                                \
        .lora_cr = TS_CONFIG_LORA_CR_DEFAULT,                                \
        .lora_tx_power = TS_CONFIG_LORA_TX_POWER_DEFAULT,                    \
        .sensor_interval_s = TS_CONFIG_SENSOR_INTERVAL_S_DEFAULT,            \
        .heartbeat_interval_s = TS_CONFIG_HEARTBEAT_INTERVAL_S_DEFAULT,      \
        .routing_table_age_interval_s =                                      \
            TS_CONFIG_ROUTING_TABLE_AGE_INTERVAL_S_DEFAULT,                  \
    }

/**
 * @brief Initialize the config subsystem.
 *
 * Loads saved values from NVS.  Any key not yet persisted is filled
 * with its compile-time default from TS_CONFIG_DEFAULTS.
 *
 * @return 0 on success, negative errno on failure
 */
int ts_config_init(void);

/**
 * @brief Get read-only access to the live configuration.
 *
 * The returned pointer is valid for the lifetime of the application.
 *
 * @return Pointer to the current configuration (never NULL)
 */
const struct ts_config* ts_config_get(void);

/**
 * @brief Set a configuration value by key name.
 *
 * Validates the value against the allowed range for the key, updates the
 * live struct, and persists to NVS.
 *
 * @param p_key    NUL-terminated settings key (e.g. "ts/routing_ttl")
 * @param value    New value (interpreted per key)
 * @return 0 on success, -EINVAL if out of range, -ENOENT if unknown key
 */
int ts_config_set(const char* p_key, int32_t value);

/**
 * @brief Reset all configuration to compile-time defaults.
 *
 * Erases persisted values and reloads TS_CONFIG_DEFAULTS.
 *
 * @return 0 on success, negative errno on failure
 */
int ts_config_reset(void);

/** @} */

#endif  // TS_CONFIG_H
