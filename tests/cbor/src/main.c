#include <string.h>
#include <zephyr/ztest.h>

#include "lora/cbor.h"
#include "messages/messages.h"
#include "routing/routing.h"

#define TEST_ROUTE                                                     \
    {                                                                  \
        .src = 0x0001, .dst = TS_ROUTING_BROADCAST_ADDR, .msg_id = 42, \
        .ttl = TS_ROUTING_DEFAULT_TTL, .key_id = 0                     \
    }

ZTEST(cbor, test_serialize_telemetry_returns_ok)
{
    struct ts_msg_lora_outgoing msg = {
        .route = TEST_ROUTE,
        .type = TS_MSG_TELEMETRY,
        .data.telemetry = {.timestamp = 100,
                           .temperature = 2500,
                           .humidity = 6000,
                           .pressure = 101325}};
    uint8_t buf[ZBOR_ENCODE_BUFFER_SIZE];
    size_t size = 0;

    int ret = cbor_serialize(&msg, buf, sizeof(buf), &size);

    zassert_ok(ret, "cbor_serialize should return 0 for telemetry");
    zassert_true(size > 0, "encoded size should be non-zero");
}

ZTEST(cbor, test_serialize_node_status_returns_ok)
{
    struct ts_msg_lora_outgoing msg = {
        .route = TEST_ROUTE,
        .type = TS_MSG_NODE_STATUS,
        .data.node_status = {.timestamp = 200, .uptime = 200, .status = OK}};
    uint8_t buf[ZBOR_ENCODE_BUFFER_SIZE];
    size_t size = 0;

    int ret = cbor_serialize(&msg, buf, sizeof(buf), &size);

    zassert_ok(ret, "cbor_serialize should return 0 for node_status");
    zassert_true(size > 0, "encoded size should be non-zero");
}

ZTEST(cbor, test_serialize_invalid_type_returns_einval)
{
    struct ts_msg_lora_outgoing msg = {.route = TEST_ROUTE, .type = 99};
    uint8_t buf[ZBOR_ENCODE_BUFFER_SIZE];
    size_t size = 0;

    int ret = cbor_serialize(&msg, buf, sizeof(buf), &size);

    zassert_equal(ret, -EINVAL, "invalid type should return -EINVAL");
}

ZTEST(cbor, test_serialize_buffer_too_small)
{
    struct ts_msg_lora_outgoing msg = {
        .route = TEST_ROUTE,
        .type = TS_MSG_TELEMETRY,
        .data.telemetry = {.timestamp = 100,
                           .temperature = 2500,
                           .humidity = 6000,
                           .pressure = 101325}};
    uint8_t buf[4];
    size_t size = 0;

    int ret = cbor_serialize(&msg, buf, sizeof(buf), &size);

    zassert_not_equal(ret, 0, "should fail with tiny buffer");
}

/* Deserialization tests */

ZTEST(cbor, test_roundtrip_telemetry)
{
    struct ts_msg_lora_outgoing original = {
        .route = {.src = 0x0001, .dst = 0x0002, .msg_id = 99, .ttl = 3,
                  .key_id = 1},
        .type = TS_MSG_TELEMETRY,
        .data.telemetry = {.timestamp = 1234,
                           .temperature = 2500,
                           .humidity = 6000,
                           .pressure = 101325}};
    uint8_t buf[ZBOR_ENCODE_BUFFER_SIZE];
    size_t size = 0;

    int ret = cbor_serialize(&original, buf, sizeof(buf), &size);
    zassert_ok(ret);

    struct ts_msg_lora_outgoing decoded = {0};
    ret = cbor_deserialize(buf, size, &decoded);

    zassert_ok(ret, "deserialize should succeed");
    zassert_equal(decoded.type, TS_MSG_TELEMETRY);
    zassert_equal(decoded.route.src, 0x0001);
    zassert_equal(decoded.route.dst, 0x0002);
    zassert_equal(decoded.route.msg_id, 99);
    zassert_equal(decoded.route.ttl, 3);
    zassert_equal(decoded.route.key_id, 1);
    zassert_equal(decoded.data.telemetry.timestamp, 1234);
    zassert_equal(decoded.data.telemetry.temperature, 2500);
    zassert_equal(decoded.data.telemetry.humidity, 6000);
    zassert_equal(decoded.data.telemetry.pressure, 101325);
}

ZTEST(cbor, test_roundtrip_telemetry_negative_temp)
{
    struct ts_msg_lora_outgoing original = {
        .route = TEST_ROUTE,
        .type = TS_MSG_TELEMETRY,
        .data.telemetry = {.timestamp = 100,
                           .temperature = -550,
                           .humidity = 6000,
                           .pressure = 101325}};
    uint8_t buf[ZBOR_ENCODE_BUFFER_SIZE];
    size_t size = 0;

    int ret = cbor_serialize(&original, buf, sizeof(buf), &size);
    zassert_ok(ret);

    struct ts_msg_lora_outgoing decoded = {0};
    ret = cbor_deserialize(buf, size, &decoded);

    zassert_ok(ret);
    zassert_equal(decoded.data.telemetry.temperature, -550,
                  "Negative temperature should survive roundtrip");
}

ZTEST(cbor, test_roundtrip_node_status)
{
    struct ts_msg_lora_outgoing original = {
        .route = {.src = 0x0003,
                  .dst = TS_ROUTING_BROADCAST_ADDR,
                  .msg_id = 7,
                  .ttl = TS_ROUTING_DEFAULT_TTL,
                  .key_id = 2},
        .type = TS_MSG_NODE_STATUS,
        .data.node_status = {
            .timestamp = 5678, .uptime = 5678, .status = ERROR}};
    uint8_t buf[ZBOR_ENCODE_BUFFER_SIZE];
    size_t size = 0;

    int ret = cbor_serialize(&original, buf, sizeof(buf), &size);
    zassert_ok(ret);

    struct ts_msg_lora_outgoing decoded = {0};
    ret = cbor_deserialize(buf, size, &decoded);

    zassert_ok(ret, "deserialize should succeed");
    zassert_equal(decoded.type, TS_MSG_NODE_STATUS);
    zassert_equal(decoded.route.src, 0x0003);
    zassert_equal(decoded.route.dst, TS_ROUTING_BROADCAST_ADDR);
    zassert_equal(decoded.route.msg_id, 7);
    zassert_equal(decoded.route.ttl, TS_ROUTING_DEFAULT_TTL);
    zassert_equal(decoded.route.key_id, 2);
    zassert_equal(decoded.data.node_status.timestamp, 5678);
    zassert_equal(decoded.data.node_status.uptime, 5678);
    zassert_equal(decoded.data.node_status.status, ERROR);
}

ZTEST(cbor, test_deserialize_truncated_buffer)
{
    struct ts_msg_lora_outgoing msg = {
        .route = TEST_ROUTE,
        .type = TS_MSG_TELEMETRY,
        .data.telemetry = {.timestamp = 100,
                           .temperature = 2500,
                           .humidity = 6000,
                           .pressure = 101325}};
    uint8_t buf[ZBOR_ENCODE_BUFFER_SIZE];
    size_t size = 0;

    cbor_serialize(&msg, buf, sizeof(buf), &size);

    struct ts_msg_lora_outgoing decoded = {0};
    int ret = cbor_deserialize(buf, size / 2, &decoded);

    zassert_not_equal(ret, 0, "truncated buffer should fail");
}

ZTEST(cbor, test_deserialize_empty_buffer)
{
    struct ts_msg_lora_outgoing decoded = {0};
    int ret = cbor_deserialize(NULL, 0, &decoded);

    zassert_not_equal(ret, 0, "empty buffer should fail");
}

/* Config message roundtrips */

ZTEST(cbor, test_roundtrip_config_set)
{
    struct ts_msg_lora_outgoing original = {
        .route = {.src = 0x0001, .dst = 0x0002, .msg_id = 10, .ttl = 5,
                  .key_id = 0},
        .type = TS_MSG_CONFIG_SET,
    };
    strncpy(original.data.config_set.key, "ts/routing_ttl",
            TS_MSG_CONFIG_KEY_MAX - 1);
    original.data.config_set.value = 3;

    uint8_t buf[ZBOR_ENCODE_BUFFER_SIZE];
    size_t size = 0;

    int ret = cbor_serialize(&original, buf, sizeof(buf), &size);
    zassert_ok(ret, "serialize config_set should succeed");

    struct ts_msg_lora_outgoing decoded = {0};
    ret = cbor_deserialize(buf, size, &decoded);

    zassert_ok(ret, "deserialize config_set should succeed");
    zassert_equal(decoded.type, TS_MSG_CONFIG_SET);
    zassert_equal(decoded.route.src, 0x0001);
    zassert_equal(decoded.route.dst, 0x0002);
    zassert_mem_equal(decoded.data.config_set.key, "ts/routing_ttl", 14);
    zassert_equal(decoded.data.config_set.value, 3);
}

ZTEST(cbor, test_roundtrip_config_set_negative_value)
{
    struct ts_msg_lora_outgoing original = {
        .route = TEST_ROUTE,
        .type = TS_MSG_CONFIG_SET,
    };
    strncpy(original.data.config_set.key, "ts/lora_tx_power",
            TS_MSG_CONFIG_KEY_MAX - 1);
    original.data.config_set.value = -2;

    uint8_t buf[ZBOR_ENCODE_BUFFER_SIZE];
    size_t size = 0;

    int ret = cbor_serialize(&original, buf, sizeof(buf), &size);
    zassert_ok(ret);

    struct ts_msg_lora_outgoing decoded = {0};
    ret = cbor_deserialize(buf, size, &decoded);

    zassert_ok(ret);
    zassert_equal(decoded.data.config_set.value, -2,
                  "Negative value should survive roundtrip");
}

ZTEST(cbor, test_roundtrip_config_ack)
{
    struct ts_msg_lora_outgoing original = {
        .route = {.src = 0x0002, .dst = 0x0001, .msg_id = 11, .ttl = 5,
                  .key_id = 0},
        .type = TS_MSG_CONFIG_ACK,
    };
    strncpy(original.data.config_ack.key, "ts/routing_ttl",
            TS_MSG_CONFIG_KEY_MAX - 1);
    original.data.config_ack.value = 3;
    original.data.config_ack.result = 0;

    uint8_t buf[ZBOR_ENCODE_BUFFER_SIZE];
    size_t size = 0;

    int ret = cbor_serialize(&original, buf, sizeof(buf), &size);
    zassert_ok(ret, "serialize config_ack should succeed");

    struct ts_msg_lora_outgoing decoded = {0};
    ret = cbor_deserialize(buf, size, &decoded);

    zassert_ok(ret, "deserialize config_ack should succeed");
    zassert_equal(decoded.type, TS_MSG_CONFIG_ACK);
    zassert_equal(decoded.route.src, 0x0002);
    zassert_equal(decoded.route.dst, 0x0001);
    zassert_mem_equal(decoded.data.config_ack.key, "ts/routing_ttl", 14);
    zassert_equal(decoded.data.config_ack.value, 3);
    zassert_equal(decoded.data.config_ack.result, 0);
}

ZTEST(cbor, test_roundtrip_config_ack_with_error)
{
    struct ts_msg_lora_outgoing original = {
        .route = TEST_ROUTE,
        .type = TS_MSG_CONFIG_ACK,
    };
    strncpy(original.data.config_ack.key, "ts/nonexistent",
            TS_MSG_CONFIG_KEY_MAX - 1);
    original.data.config_ack.value = 42;
    original.data.config_ack.result = -ENOENT;

    uint8_t buf[ZBOR_ENCODE_BUFFER_SIZE];
    size_t size = 0;

    int ret = cbor_serialize(&original, buf, sizeof(buf), &size);
    zassert_ok(ret);

    struct ts_msg_lora_outgoing decoded = {0};
    ret = cbor_deserialize(buf, size, &decoded);

    zassert_ok(ret);
    zassert_equal(decoded.data.config_ack.result, -ENOENT,
                  "Negative result should survive roundtrip");
}

ZTEST_SUITE(cbor, NULL, NULL, NULL, NULL, NULL);
