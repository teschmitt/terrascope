#include "lora.h"
#include <zephyr/logging/log.h>

#define LORA_CHAN_OUT_READ_TIMEOUT K_MSEC(2)

LOG_MODULE_REGISTER(lora);

ZBUS_SUBSCRIBER_DEFINE(ts_lora_out_sub, 2);
extern struct zbus_channel ts_lora_out_chan;

K_THREAD_DEFINE(lora_out_tid, LORA_OUT_THREAD_STACK_SIZE, lora_out_task, NULL,
                NULL, NULL, 3, 0, 0);

static const struct device* lora_dev;
static bool lora_config_done = false;
static uint8_t cbor_buffer[ZBOR_ENCODE_BUFFER_SIZE];

// Initialize the LoRa device reference
static int lora_init(void) {
    lora_dev = DEVICE_DT_GET(DT_ALIAS(lora0));
    if (!device_is_ready(lora_dev)) {
        LOG_ERR("LoRa device not ready");
        return -ENODEV;
    }

    // Configure the device
    struct lora_modem_config config;
    lora_config_ready_device(&config);

    if (lora_config(lora_dev, &config) < 0) {
        LOG_ERR("LoRa config failed");
        return -EIO;
    }

    return 0;
}

SYS_INIT(lora_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

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

    LOG_INF("LoRa output task started");

    while (true) {
        int ret = zbus_sub_wait(&ts_lora_out_sub, &chan, K_FOREVER);
        if (ret != 0) {
            LOG_ERR("zbus_sub_wait failed: %d", ret);
            continue;
        }

        if (chan == &ts_lora_out_chan) {
            struct ts_msg_lora_outgoing msg = {0};

            ret = zbus_chan_read(&ts_lora_out_chan, &msg,
                                 LORA_CHAN_OUT_READ_TIMEOUT);
            if (ret != 0) {
                LOG_ERR("Failed to read from channel: %d", ret);
                continue;
            }

            LOG_DBG("Processing message type: %d", msg.type);

            size_t size = 0;
            ret = cbor_serialize(&msg, cbor_buffer, sizeof(cbor_buffer), &size);
            if (ret != 0) {
                LOG_ERR("CBOR serialization failed: %d", ret);
                continue;
            }

            LOG_HEXDUMP_DBG(cbor_buffer, size, "CBOR encoded: ");

            ret = lora_send(lora_dev, cbor_buffer, (uint32_t)size);
            if (ret < 0) {
                LOG_ERR("LoRa send failed: %d", ret);
                continue;
            }

            LOG_DBG("Message sent successfully");
        } else {
            LOG_WRN("Received message on unexpected channel");
        }
    }
    return 0;  // unreachable!
}

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
    LOG_INF("CBOR encoding successful, size: %ld", *p_size);
    return 0;
}
