#include "config/config.h"

#include <string.h>
#include <zephyr/settings/settings.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(config);

// Live configuration — starts with compile-time defaults
static struct ts_config live_config = TS_CONFIG_DEFAULTS;

// Descriptor for each config key: name, offset, size, and valid range.
// All range checks use int32_t so signed and unsigned fields share
// the same validation path.
struct ts_config_key {
    const char *name;
    uint16_t offset;
    uint8_t size;
    int32_t min;
    int32_t max;
};

#define CONFIG_KEY(field, lo, hi)                    \
    {                                                \
        .name = #field,                              \
        .offset = offsetof(struct ts_config, field), \
        .size = sizeof(((struct ts_config *)0)->field), \
        .min = (lo),                                 \
        .max = (hi),                                 \
    }

static const struct ts_config_key config_keys[] = {
    CONFIG_KEY(routing_ttl, 1, 255),
    CONFIG_KEY(contention_delay_min_ms, 0, 60000),
    CONFIG_KEY(contention_delay_max_ms, 0, 60000),
    CONFIG_KEY(contention_rssi_weak, -150, 0),
    CONFIG_KEY(contention_rssi_strong, -150, 0),
    CONFIG_KEY(routing_table_stale_timeout_s, 1, 86400),
    CONFIG_KEY(node_id, 0, 0xFFFF),
    CONFIG_KEY(lora_frequency, 150000000, 960000000),
    CONFIG_KEY(lora_sf, 5, 12),
    CONFIG_KEY(lora_bw, 7, 1600),
    CONFIG_KEY(lora_cr, 1, 4),
    CONFIG_KEY(lora_tx_power, -9, 22),
    CONFIG_KEY(sensor_interval_s, 1, 86400),
    CONFIG_KEY(heartbeat_interval_s, 1, 86400),
    CONFIG_KEY(routing_table_age_interval_s, 1, 86400),
};

#define CONFIG_KEY_COUNT ARRAY_SIZE(config_keys)

static const struct ts_config_key *find_key(const char *name) {
    for (size_t i = 0; i < CONFIG_KEY_COUNT; i++) {
        if (strcmp(config_keys[i].name, name) == 0) {
            return &config_keys[i];
        }
    }
    return NULL;
}

// Write an int32_t value into the field at its native size
static void write_field(const struct ts_config_key *p_key, int32_t value) {
    uint8_t *base = (uint8_t *)&live_config;
    switch (p_key->size) {
        case 1:
            if (p_key->min < 0) {
                *(int8_t *)(base + p_key->offset) = (int8_t)value;
            } else {
                *(uint8_t *)(base + p_key->offset) = (uint8_t)value;
            }
            break;
        case 2:
            if (p_key->min < 0) {
                *(int16_t *)(base + p_key->offset) = (int16_t)value;
            } else {
                *(uint16_t *)(base + p_key->offset) = (uint16_t)value;
            }
            break;
        case 4:
            *(uint32_t *)(base + p_key->offset) = (uint32_t)value;
            break;
    }
}

// Settings h_set callback — called by settings_load() for each persisted key
static int config_settings_set(const char *key, size_t len,
                               settings_read_cb read_cb, void *cb_arg) {
    const struct ts_config_key *entry = find_key(key);
    if (entry == NULL) {
        LOG_WRN("Unknown settings key: %s", key);
        return -ENOENT;
    }

    if (len != entry->size) {
        LOG_ERR("Size mismatch for %s: expected %u, got %zu", key,
                entry->size, len);
        return -EINVAL;
    }

    uint8_t *base = (uint8_t *)&live_config;
    ssize_t rc = read_cb(cb_arg, base + entry->offset, entry->size);
    if (rc < 0) {
        LOG_ERR("Failed to read setting %s: %zd", key, rc);
        return (int)rc;
    }

    return 0;
}

// Settings h_export callback — called by settings_save() to persist all keys
static int config_settings_export(
    int (*export_func)(const char *name, const void *val, size_t val_len)) {
    const uint8_t *base = (const uint8_t *)&live_config;

    for (size_t i = 0; i < CONFIG_KEY_COUNT; i++) {
        char full_name[32];
        snprintf(full_name, sizeof(full_name), "ts/%s",
                 config_keys[i].name);
        int ret =
            export_func(full_name, base + config_keys[i].offset,
                        config_keys[i].size);
        if (ret != 0) {
            return ret;
        }
    }
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(ts_config, "ts", NULL, config_settings_set,
                               NULL, config_settings_export);

int ts_config_init(void) {
    // Start with defaults; settings_load will overwrite any persisted values
    live_config = (struct ts_config)TS_CONFIG_DEFAULTS;

    int ret = settings_subsys_init();
    if (ret != 0) {
        LOG_ERR("settings_subsys_init failed: %d", ret);
        return ret;
    }

    ret = settings_load();
    if (ret != 0) {
        LOG_ERR("settings_load failed: %d", ret);
        return ret;
    }

    LOG_INF("Config loaded (node_id=0x%04x, ttl=%u, sf=%u)",
            live_config.node_id, live_config.routing_ttl,
            live_config.lora_sf);
    return 0;
}

const struct ts_config *ts_config_get(void) { return &live_config; }

int ts_config_set(const char *p_key, int32_t value) {
    // Strip "ts/" prefix to find the key entry
    const char *field = p_key;
    if (strncmp(field, "ts/", 3) == 0) {
        field = field + 3;
    }

    const struct ts_config_key *entry = find_key(field);
    if (entry == NULL) {
        return -ENOENT;
    }

    if (value < entry->min || value > entry->max) {
        return -EINVAL;
    }

    write_field(entry, value);

    // Persist to NVS using the full "ts/field" key
    char full_name[32];
    snprintf(full_name, sizeof(full_name), "ts/%s", entry->name);
    const uint8_t *base = (const uint8_t *)&live_config;
    int ret =
        settings_save_one(full_name, base + entry->offset, entry->size);
    if (ret != 0) {
        LOG_ERR("settings_save_one(%s) failed: %d", full_name, ret);
        return ret;
    }

    return 0;
}

int ts_config_reset(void) {
    // Delete all persisted keys
    for (size_t i = 0; i < CONFIG_KEY_COUNT; i++) {
        char full_name[32];
        snprintf(full_name, sizeof(full_name), "ts/%s",
                 config_keys[i].name);
        settings_delete(full_name);
    }

    // Restore live struct to compile-time defaults
    live_config = (struct ts_config)TS_CONFIG_DEFAULTS;
    return 0;
}
