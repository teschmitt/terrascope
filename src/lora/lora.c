#include "lora.h"
#include <zephyr/logging/log.h>

#define LORA_CHAN_OUT_READ_TIMEOUT K_MSEC(1)

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

    // Wait for LoRa device to be ready, retrying every 5 seconds
    while (!device_is_ready(lora_dev) || !lora_config_done) {
        LOG_WRN("LoRa device not ready, retrying in 5s");
        k_sleep(K_SECONDS(5));
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
