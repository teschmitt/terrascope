#include "lora/cbor.h"

#include <errno.h>
#include <zcbor_encode.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(cbor);

int cbor_serialize(struct ts_msg_lora_outgoing* msg, uint8_t* p_buf,
                   size_t buf_len, size_t* p_size) {

    ZCBOR_STATE_E(enc_state, 0, p_buf, buf_len, 0);
    int ret;

    if (!zcbor_map_start_encode(enc_state, 2)) {
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

    if (!zcbor_map_end_encode(enc_state, 2)) {
        ret = zcbor_peek_error(enc_state);
        LOG_ERR("Failed to end CBOR map, error: %d", ret);
        return -ENOMEM;
    }

    *p_size = enc_state->payload - p_buf;
    LOG_INF("CBOR encoding successful, size: %zu", *p_size);
    return 0;
}
