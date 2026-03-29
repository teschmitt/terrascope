#include "lora/cbor.h"

#include <errno.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(cbor);

static int serialize_route(zcbor_state_t* state,
                           const struct ts_route_header* p_route) {
    if (!zcbor_tstr_put_lit(state, "route") ||
        !zcbor_map_start_encode(state, 5) ||
        !zcbor_tstr_put_lit(state, "src") ||
        !zcbor_uint32_put(state, (uint32_t)p_route->src) ||
        !zcbor_tstr_put_lit(state, "dst") ||
        !zcbor_uint32_put(state, (uint32_t)p_route->dst) ||
        !zcbor_tstr_put_lit(state, "msg_id") ||
        !zcbor_uint32_put(state, p_route->msg_id) ||
        !zcbor_tstr_put_lit(state, "ttl") ||
        !zcbor_uint32_put(state, (uint32_t)p_route->ttl) ||
        !zcbor_tstr_put_lit(state, "key_id") ||
        !zcbor_uint32_put(state, (uint32_t)p_route->key_id) ||
        !zcbor_map_end_encode(state, 5)) {
        return -ENOMEM;
    }
    return 0;
}

int cbor_serialize(struct ts_msg_lora_outgoing* msg, uint8_t* p_buf,
                   size_t buf_len, size_t* p_size) {
    ZCBOR_STATE_E(enc_state, 0, p_buf, buf_len, 0);
    int ret;

    if (!zcbor_map_start_encode(enc_state, 3)) {
        ret = zcbor_peek_error(enc_state);
        LOG_ERR("Failed to start CBOR map, error: %d", ret);
        return -ENOMEM;
    }

    if (!zcbor_tstr_put_lit(enc_state, "type") ||
        !zcbor_uint32_put(enc_state, msg->type)) {
        ret = zcbor_peek_error(enc_state);
        LOG_ERR("Failed to encode type, error: %d", ret);
        return -ENOMEM;
    }

    ret = serialize_route(enc_state, &msg->route);
    if (ret != 0) {
        LOG_ERR("Failed to encode route header");
        return ret;
    }

    if (!zcbor_tstr_put_lit(enc_state, "data")) {
        ret = zcbor_peek_error(enc_state);
        LOG_ERR("Failed to encode data key, error: %d", ret);
        return -ENOMEM;
    }

    switch (msg->type) {
        case TS_MSG_TELEMETRY:
            if (!zcbor_map_start_encode(enc_state, 4) ||
                !zcbor_tstr_put_lit(enc_state, "timestamp") ||
                !zcbor_uint32_put(enc_state, msg->data.telemetry.timestamp) ||
                !zcbor_tstr_put_lit(enc_state, "temperature") ||
                !zcbor_uint32_put(enc_state, msg->data.telemetry.temperature) ||
                !zcbor_tstr_put_lit(enc_state, "humidity") ||
                !zcbor_uint32_put(enc_state, msg->data.telemetry.humidity) ||
                !zcbor_tstr_put_lit(enc_state, "pressure") ||
                !zcbor_uint32_put(enc_state, msg->data.telemetry.pressure) ||
                !zcbor_map_end_encode(enc_state, 4)) {
                ret = zcbor_peek_error(enc_state);
                LOG_ERR("Failed to encode telemetry data, error: %d", ret);
                return -ENOMEM;
            }
            break;

        case TS_MSG_NODE_STATUS:
            if (!zcbor_map_start_encode(enc_state, 3) ||
                !zcbor_tstr_put_lit(enc_state, "timestamp") ||
                !zcbor_uint32_put(enc_state, msg->data.node_status.timestamp) ||
                !zcbor_tstr_put_lit(enc_state, "uptime") ||
                !zcbor_uint32_put(enc_state, msg->data.node_status.uptime) ||
                !zcbor_tstr_put_lit(enc_state, "status") ||
                !zcbor_uint32_put(enc_state,
                                  (uint32_t)msg->data.node_status.status) ||
                !zcbor_map_end_encode(enc_state, 3)) {
                ret = zcbor_peek_error(enc_state);
                LOG_ERR("Failed to encode node_status data, error: %d", ret);
                return -ENOMEM;
            }
            break;

        default:
            LOG_ERR("Unknown message type: %d", msg->type);
            return -EINVAL;
    }

    if (!zcbor_map_end_encode(enc_state, 3)) {
        ret = zcbor_peek_error(enc_state);
        LOG_ERR("Failed to end CBOR map, error: %d", ret);
        return -ENOMEM;
    }

    *p_size = enc_state->payload - p_buf;
    LOG_INF("CBOR encoding successful, size: %zu", *p_size);
    return 0;
}

static int deserialize_route(zcbor_state_t* state,
                             struct ts_route_header* p_route) {
    uint32_t src, dst, msg_id, ttl, key_id;

    if (!zcbor_tstr_expect_lit(state, "route") ||
        !zcbor_map_start_decode(state) ||
        !zcbor_tstr_expect_lit(state, "src") ||
        !zcbor_uint32_decode(state, &src) ||
        !zcbor_tstr_expect_lit(state, "dst") ||
        !zcbor_uint32_decode(state, &dst) ||
        !zcbor_tstr_expect_lit(state, "msg_id") ||
        !zcbor_uint32_decode(state, &msg_id) ||
        !zcbor_tstr_expect_lit(state, "ttl") ||
        !zcbor_uint32_decode(state, &ttl) ||
        !zcbor_tstr_expect_lit(state, "key_id") ||
        !zcbor_uint32_decode(state, &key_id) || !zcbor_map_end_decode(state)) {
        return -EBADMSG;
    }

    p_route->src = (uint16_t)src;
    p_route->dst = (uint16_t)dst;
    p_route->msg_id = msg_id;
    p_route->ttl = (uint8_t)ttl;
    p_route->key_id = (uint8_t)key_id;
    return 0;
}

static int deserialize_telemetry(zcbor_state_t* state,
                                 struct ts_msg_telemetry* p_tel) {
    if (!zcbor_map_start_decode(state) ||
        !zcbor_tstr_expect_lit(state, "timestamp") ||
        !zcbor_uint32_decode(state, &p_tel->timestamp) ||
        !zcbor_tstr_expect_lit(state, "temperature") ||
        !zcbor_uint32_decode(state, &p_tel->temperature) ||
        !zcbor_tstr_expect_lit(state, "humidity") ||
        !zcbor_uint32_decode(state, &p_tel->humidity) ||
        !zcbor_tstr_expect_lit(state, "pressure") ||
        !zcbor_uint32_decode(state, &p_tel->pressure) ||
        !zcbor_map_end_decode(state)) {
        return -EBADMSG;
    }
    return 0;
}

static int deserialize_node_status(zcbor_state_t* state,
                                   struct ts_msg_node_status* p_ns) {
    uint32_t status_val;

    if (!zcbor_map_start_decode(state) ||
        !zcbor_tstr_expect_lit(state, "timestamp") ||
        !zcbor_uint32_decode(state, &p_ns->timestamp) ||
        !zcbor_tstr_expect_lit(state, "uptime") ||
        !zcbor_uint32_decode(state, &p_ns->uptime) ||
        !zcbor_tstr_expect_lit(state, "status") ||
        !zcbor_uint32_decode(state, &status_val) ||
        !zcbor_map_end_decode(state)) {
        return -EBADMSG;
    }
    p_ns->status = (ts_status_t)status_val;
    return 0;
}

int cbor_deserialize(const uint8_t* p_buf, size_t buf_len,
                     struct ts_msg_lora_outgoing* p_msg) {
    if (p_buf == NULL || buf_len == 0) { return -EINVAL; }

    // 2 backups for nested containers (outer map + route/data map)
    ZCBOR_STATE_D(dec_state, 2, p_buf, buf_len, 1, 0);

    uint32_t type_val;

    if (!zcbor_map_start_decode(dec_state) ||
        !zcbor_tstr_expect_lit(dec_state, "type") ||
        !zcbor_uint32_decode(dec_state, &type_val)) {
        LOG_ERR("Failed to decode CBOR envelope");
        return -EBADMSG;
    }

    p_msg->type = (ts_msg_type_t)type_val;

    int ret = deserialize_route(dec_state, &p_msg->route);
    if (ret != 0) {
        LOG_ERR("Failed to decode route header");
        return ret;
    }

    if (!zcbor_tstr_expect_lit(dec_state, "data")) {
        LOG_ERR("Failed to decode data key");
        return -EBADMSG;
    }

    switch (p_msg->type) {
        case TS_MSG_TELEMETRY:
            ret = deserialize_telemetry(dec_state, &p_msg->data.telemetry);
            if (ret != 0) {
                LOG_ERR("Failed to decode telemetry data");
                return ret;
            }
            break;

        case TS_MSG_NODE_STATUS:
            ret = deserialize_node_status(dec_state, &p_msg->data.node_status);
            if (ret != 0) {
                LOG_ERR("Failed to decode node_status data");
                return ret;
            }
            break;

        default:
            LOG_ERR("Unknown message type: %d", p_msg->type);
            return -EINVAL;
    }

    if (!zcbor_map_end_decode(dec_state)) {
        LOG_ERR("Failed to close CBOR envelope");
        return -EBADMSG;
    }

    LOG_INF("CBOR decoding successful, type: %d", p_msg->type);
    return 0;
}
