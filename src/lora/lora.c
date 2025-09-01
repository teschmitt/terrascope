#include "lora.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(lora);

ZBUS_SUBSCRIBER_DEFINE(tm_telemetry_sub_01, 2);
K_THREAD_DEFINE(lora_out_tid, LORA_OUT_THREAD_STACK_SIZE, lora_out_task, NULL,
                NULL, NULL, 3, 0, 0);

#define DEFAULT_RADIO_NODE DT_ALIAS(lora0)
BUILD_ASSERT(DT_NODE_HAS_STATUS_OKAY(DEFAULT_RADIO_NODE),
             "No default LoRa radio specified in DT");
const struct device* const lora_dev = DEVICE_DT_GET(DEFAULT_RADIO_NODE);
struct lora_modem_config config;
bool lora_config_done = false;

bool lora_config_ready_device(struct lora_modem_config* config) {
    config->frequency = 865100000;
    config->bandwidth = BW_125_KHZ;
    config->datarate = SF_10;
    config->coding_rate = CR_4_5;
    config->iq_inverted = false;
    config->public_network = false;
    config->tx_power = 4;
    config->tx = true;
    lora_config_done = true;
    return true;
}

int lora_out_task() {
    const struct zbus_channel* chan;

    if (!device_is_ready(lora_dev)) {
        LOG_ERR("%s Device not ready", lora_dev->name);
        return -1;
    }

    if (!lora_config_done) {
        LOG_ERR(
            "%s Device has not been properly configured. Call lora_config()",
            lora_dev->name);
        return -1;
    }

    while (!zbus_sub_wait(&tm_telemetry_sub_01, &chan, K_FOREVER)) {
        struct ts_msg_telemetry msg = {0};

        if (&telemetry_chan == chan) {
            zbus_chan_read(&telemetry_chan, &msg, K_NO_WAIT);
            LOG_DBG(
                "Received sensor readings: ts=%d, pressure=%d, temp=%d, hum=%d",
                msg.timestamp, msg.pressure, msg.temperature, msg.humidity);

            struct ts_msg_lora_outgoing out = {.type = TS_MSG_TELEMETRY,
                                               .data.telemetry = &msg};
            uint8_t buf[ZBOR_ENCODE_BUFFER_SIZE];
            size_t size = 0;
            cbor_serialize(&out, buf, sizeof(buf), &size);
            LOG_HEXDUMP_DBG(buf, size, "CBOR encoded: ");

            if (lora_send(lora_dev, buf, (uint32_t)size) < 0) {
                LOG_ERR("LoRa send failed");
                return -1;
            }
        }
    }
    return 0;  // unreachable!
}

int cbor_serialize(struct ts_msg_lora_outgoing* msg, uint8_t* p_buf,
                   size_t buf_len, size_t* p_size) {

    ZCBOR_STATE_E(enc_state, 0, p_buf, buf_len, 0);

    if (!zcbor_map_start_encode(enc_state, 2)) {
        LOG_ERR("Did not start CBOR map correctly. Err: %i",
                zcbor_peek_error(enc_state));
        return -ENOMEM;
    }

    if (!zcbor_tstr_put_lit(enc_state, "type")) return -ENOMEM;
    if (!zcbor_uint32_put(enc_state, msg->type)) return -ENOMEM;

    if (!zcbor_tstr_put_lit(enc_state, "data")) return -ENOMEM;
    switch (msg->type) {
        case TS_MSG_TELEMETRY:
            if (!zcbor_map_start_encode(enc_state, 4)) return -ENOMEM;
            if (!zcbor_tstr_put_lit(enc_state, "timestamp")) return -ENOMEM;
            if (!zcbor_uint32_put(enc_state, msg->data.telemetry->timestamp))
                return -ENOMEM;
            if (!zcbor_tstr_put_lit(enc_state, "temperature")) return -ENOMEM;
            if (!zcbor_uint32_put(enc_state,
                                  (uint32_t)msg->data.telemetry->temperature))
                return -ENOMEM;
            if (!zcbor_tstr_put_lit(enc_state, "humidity")) return -ENOMEM;
            if (!zcbor_uint32_put(enc_state,
                                  (uint32_t)msg->data.telemetry->humidity))
                return -ENOMEM;
            if (!zcbor_tstr_put_lit(enc_state, "pressure")) return -ENOMEM;
            if (!zcbor_uint32_put(enc_state,
                                  (uint32_t)msg->data.telemetry->pressure))
                return -ENOMEM;
            if (!zcbor_map_end_encode(enc_state, 4)) return -ENOMEM;
            break;
        case TS_MSG_NODE_STATUS:
            if (!zcbor_map_start_encode(enc_state, 3)) return -ENOMEM;
            if (!zcbor_tstr_put_lit(enc_state, "timestamp")) return -ENOMEM;
            if (!zcbor_uint32_put(enc_state, msg->data.node_status->timestamp))
                return -ENOMEM;
            if (!zcbor_tstr_put_lit(enc_state, "uptime")) return -ENOMEM;
            if (!zcbor_uint32_put(enc_state, msg->data.node_status->uptime))
                return -ENOMEM;
            if (!zcbor_tstr_put_lit(enc_state, "status")) return -ENOMEM;
            if (!zcbor_uint32_put(enc_state, msg->data.node_status->status))
                return -ENOMEM;
            if (!zcbor_map_end_encode(enc_state, 3)) return -ENOMEM;
            break;
        default:
            return -ENOMEM;
    }

    if (!zcbor_map_end_encode(enc_state, 2)) {
        LOG_ERR("Did not encode CBOR map correctly. Err: %i",
                zcbor_peek_error(enc_state));
        return -ENOMEM;
    }

    *p_size = enc_state->payload - p_buf;
    LOG_INF("Size: %i", *p_size);
    return 0;
}
