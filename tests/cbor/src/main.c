#include <zephyr/ztest.h>
#include "lora/cbor.h"
#include "messages/messages.h"

ZTEST(cbor, test_serialize_telemetry_returns_ok)
{
    struct ts_msg_lora_outgoing msg = {
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
    struct ts_msg_lora_outgoing msg = {.type = 99};
    uint8_t buf[ZBOR_ENCODE_BUFFER_SIZE];
    size_t size = 0;

    int ret = cbor_serialize(&msg, buf, sizeof(buf), &size);

    zassert_equal(ret, -EINVAL, "invalid type should return -EINVAL");
}

ZTEST(cbor, test_serialize_buffer_too_small)
{
    struct ts_msg_lora_outgoing msg = {
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

/* Deserialization tests (TDD — written before cbor_deserialize implementation) */

ZTEST(cbor, test_roundtrip_telemetry)
{
    struct ts_msg_lora_outgoing original = {
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
    zassert_equal(decoded.data.telemetry.timestamp, 1234);
    zassert_equal(decoded.data.telemetry.temperature, 2500);
    zassert_equal(decoded.data.telemetry.humidity, 6000);
    zassert_equal(decoded.data.telemetry.pressure, 101325);
}

ZTEST(cbor, test_roundtrip_node_status)
{
    struct ts_msg_lora_outgoing original = {
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
    zassert_equal(decoded.data.node_status.timestamp, 5678);
    zassert_equal(decoded.data.node_status.uptime, 5678);
    zassert_equal(decoded.data.node_status.status, ERROR);
}

ZTEST(cbor, test_deserialize_truncated_buffer)
{
    struct ts_msg_lora_outgoing msg = {
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

ZTEST_SUITE(cbor, NULL, NULL, NULL, NULL, NULL);
