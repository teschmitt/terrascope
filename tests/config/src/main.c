#include <zephyr/ztest.h>

#include "config/config.h"

static void before_each(void *fixture) {
    ARG_UNUSED(fixture);
    ts_config_reset();
}

/* --- Defaults --- */

ZTEST(config, test_defaults_when_empty) {
    int ret = ts_config_init();
    zassert_ok(ret, "ts_config_init should succeed");

    const struct ts_config *cfg = ts_config_get();
    zassert_not_null(cfg, "ts_config_get should never return NULL");

    zassert_equal(cfg->routing_ttl, TS_CONFIG_ROUTING_TTL_DEFAULT,
                  "routing_ttl should be default");
    zassert_equal(cfg->contention_delay_min_ms,
                  TS_CONFIG_CONTENTION_DELAY_MIN_MS_DEFAULT,
                  "contention_delay_min_ms should be default");
    zassert_equal(cfg->contention_delay_max_ms,
                  TS_CONFIG_CONTENTION_DELAY_MAX_MS_DEFAULT,
                  "contention_delay_max_ms should be default");
    zassert_equal(cfg->contention_rssi_weak,
                  TS_CONFIG_CONTENTION_RSSI_WEAK_DEFAULT,
                  "contention_rssi_weak should be default");
    zassert_equal(cfg->contention_rssi_strong,
                  TS_CONFIG_CONTENTION_RSSI_STRONG_DEFAULT,
                  "contention_rssi_strong should be default");
    zassert_equal(cfg->routing_table_stale_timeout_s,
                  TS_CONFIG_ROUTING_TABLE_STALE_TIMEOUT_S_DEFAULT,
                  "routing_table_stale_timeout_s should be default");
    zassert_equal(cfg->node_id, TS_CONFIG_NODE_ID_DEFAULT,
                  "node_id should be default");
    zassert_equal(cfg->lora_frequency, TS_CONFIG_LORA_FREQUENCY_DEFAULT,
                  "lora_frequency should be default");
    zassert_equal(cfg->lora_sf, TS_CONFIG_LORA_SF_DEFAULT,
                  "lora_sf should be default");
    zassert_equal(cfg->lora_bw, TS_CONFIG_LORA_BW_DEFAULT,
                  "lora_bw should be default");
    zassert_equal(cfg->lora_cr, TS_CONFIG_LORA_CR_DEFAULT,
                  "lora_cr should be default");
    zassert_equal(cfg->lora_tx_power, TS_CONFIG_LORA_TX_POWER_DEFAULT,
                  "lora_tx_power should be default");
    zassert_equal(cfg->sensor_interval_s,
                  TS_CONFIG_SENSOR_INTERVAL_S_DEFAULT,
                  "sensor_interval_s should be default");
    zassert_equal(cfg->heartbeat_interval_s,
                  TS_CONFIG_HEARTBEAT_INTERVAL_S_DEFAULT,
                  "heartbeat_interval_s should be default");
    zassert_equal(cfg->routing_table_age_interval_s,
                  TS_CONFIG_ROUTING_TABLE_AGE_INTERVAL_S_DEFAULT,
                  "routing_table_age_interval_s should be default");
}

/* --- Persistence across re-init --- */

ZTEST(config, test_set_value_survives_reinit) {
    int ret = ts_config_set("ts/routing_ttl", 3);
    zassert_ok(ret, "Setting routing_ttl to 3 should succeed");

    const struct ts_config *cfg = ts_config_get();
    zassert_equal(cfg->routing_ttl, 3,
                  "routing_ttl should be 3 after set");

    // Re-initialize without erasing — value must survive
    ret = ts_config_init();
    zassert_ok(ret, "ts_config_init should succeed on re-init");

    cfg = ts_config_get();
    zassert_equal(cfg->routing_ttl, 3,
                  "routing_ttl should survive re-init");
}

/* --- Validation --- */

ZTEST(config, test_set_out_of_range_returns_einval) {
    // TTL 0 is invalid (messages would be dead on arrival)
    int ret = ts_config_set("ts/routing_ttl", 0);
    zassert_equal(ret, -EINVAL,
                  "TTL of 0 should be rejected as out of range");

    // Spreading factor must be 5–12
    ret = ts_config_set("ts/lora_sf", 20);
    zassert_equal(ret, -EINVAL,
                  "SF of 20 should be rejected as out of range");

    // Coding rate must be 1–4
    ret = ts_config_set("ts/lora_cr", 0);
    zassert_equal(ret, -EINVAL,
                  "CR of 0 should be rejected as out of range");

    // Verify the struct was not modified by invalid sets
    const struct ts_config *cfg = ts_config_get();
    zassert_equal(cfg->routing_ttl, TS_CONFIG_ROUTING_TTL_DEFAULT,
                  "routing_ttl should be unchanged after invalid set");
    zassert_equal(cfg->lora_sf, TS_CONFIG_LORA_SF_DEFAULT,
                  "lora_sf should be unchanged after invalid set");
    zassert_equal(cfg->lora_cr, TS_CONFIG_LORA_CR_DEFAULT,
                  "lora_cr should be unchanged after invalid set");
}

/* --- Boundary values --- */

ZTEST(config, test_set_boundary_values_accepted) {
    // SF lower bound (5) should be accepted
    int ret = ts_config_set("ts/lora_sf", 5);
    zassert_ok(ret, "SF=5 (lower bound) should be accepted");
    zassert_equal(ts_config_get()->lora_sf, 5);

    // SF upper bound (12) should be accepted
    ret = ts_config_set("ts/lora_sf", 12);
    zassert_ok(ret, "SF=12 (upper bound) should be accepted");
    zassert_equal(ts_config_get()->lora_sf, 12);

    // SF just below lower bound should be rejected
    ret = ts_config_set("ts/lora_sf", 4);
    zassert_equal(ret, -EINVAL, "SF=4 should be rejected");
    zassert_equal(ts_config_get()->lora_sf, 12,
                  "Value should remain 12 after rejected set");

    // SF just above upper bound should be rejected
    ret = ts_config_set("ts/lora_sf", 13);
    zassert_equal(ret, -EINVAL, "SF=13 should be rejected");

    // CR bounds: 1–4
    ret = ts_config_set("ts/lora_cr", 1);
    zassert_ok(ret, "CR=1 (lower bound) should be accepted");
    ret = ts_config_set("ts/lora_cr", 4);
    zassert_ok(ret, "CR=4 (upper bound) should be accepted");
    ret = ts_config_set("ts/lora_cr", 5);
    zassert_equal(ret, -EINVAL, "CR=5 should be rejected");

    // TTL bounds: 1–255
    ret = ts_config_set("ts/routing_ttl", 1);
    zassert_ok(ret, "TTL=1 (lower bound) should be accepted");
    ret = ts_config_set("ts/routing_ttl", 255);
    zassert_ok(ret, "TTL=255 (upper bound) should be accepted");
}

/* --- Signed fields --- */

ZTEST(config, test_set_signed_field) {
    // TX power can be negative (e.g., -2 dBm for low power)
    int ret = ts_config_set("ts/lora_tx_power", -2);
    zassert_ok(ret, "Negative TX power should be accepted");
    zassert_equal(ts_config_get()->lora_tx_power, -2,
                  "TX power should be -2 after set");

    // RSSI thresholds are negative
    ret = ts_config_set("ts/contention_rssi_weak", -100);
    zassert_ok(ret, "RSSI weak threshold should accept -100");
    zassert_equal(ts_config_get()->contention_rssi_weak, -100);
}

/* --- Multiple keys persist --- */

ZTEST(config, test_multiple_keys_survive_reinit) {
    int ret = ts_config_set("ts/routing_ttl", 8);
    zassert_ok(ret);
    ret = ts_config_set("ts/lora_sf", 7);
    zassert_ok(ret);
    ret = ts_config_set("ts/heartbeat_interval_s", 15);
    zassert_ok(ret);

    // Re-init and verify all three survived
    ret = ts_config_init();
    zassert_ok(ret);

    const struct ts_config *cfg = ts_config_get();
    zassert_equal(cfg->routing_ttl, 8,
                  "routing_ttl should survive re-init");
    zassert_equal(cfg->lora_sf, 7,
                  "lora_sf should survive re-init");
    zassert_equal(cfg->heartbeat_interval_s, 15,
                  "heartbeat_interval_s should survive re-init");
}

/* --- Unknown key --- */

ZTEST(config, test_set_unknown_key_returns_enoent) {
    int ret = ts_config_set("ts/nonexistent", 42);
    zassert_equal(ret, -ENOENT,
                  "Unknown key should return -ENOENT");
}

/* --- Reset --- */

ZTEST(config, test_reset_restores_defaults) {
    // Change several values
    int ret = ts_config_set("ts/routing_ttl", 3);
    zassert_ok(ret, "Setting routing_ttl should succeed");
    ret = ts_config_set("ts/lora_sf", 12);
    zassert_ok(ret, "Setting lora_sf should succeed");
    ret = ts_config_set("ts/sensor_interval_s", 30);
    zassert_ok(ret, "Setting sensor_interval_s should succeed");

    // Verify they changed
    const struct ts_config *cfg = ts_config_get();
    zassert_equal(cfg->routing_ttl, 3);
    zassert_equal(cfg->lora_sf, 12);
    zassert_equal(cfg->sensor_interval_s, 30);

    // Reset
    ret = ts_config_reset();
    zassert_ok(ret, "ts_config_reset should succeed");

    // All values should be back to defaults
    cfg = ts_config_get();
    zassert_equal(cfg->routing_ttl, TS_CONFIG_ROUTING_TTL_DEFAULT,
                  "routing_ttl should be default after reset");
    zassert_equal(cfg->lora_sf, TS_CONFIG_LORA_SF_DEFAULT,
                  "lora_sf should be default after reset");
    zassert_equal(cfg->sensor_interval_s,
                  TS_CONFIG_SENSOR_INTERVAL_S_DEFAULT,
                  "sensor_interval_s should be default after reset");
}

ZTEST_SUITE(config, NULL, NULL, before_each, NULL, NULL);
