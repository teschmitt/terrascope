#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/lora.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(lora_mock);

struct lora_mock_data {
    struct lora_modem_config config;
    bool configured;
};

static int lora_mock_config(const struct device* dev,
                            struct lora_modem_config* config) {
    struct lora_mock_data* data = dev->data;

    LOG_INF("Mock LoRa config: freq=%u, bw=%d, sf=%d, cr=%d", config->frequency,
            config->bandwidth, config->datarate, config->coding_rate);

    memcpy(&data->config, config, sizeof(struct lora_modem_config));
    data->configured = true;

    return 0;
}

static int lora_mock_send(const struct device* dev, uint8_t* data,
                          uint32_t data_len) {
    LOG_INF("Mock LoRa send: %d bytes", data_len);
    return 0;
}

static int lora_mock_recv(const struct device* dev, uint8_t* data, uint8_t size,
                          k_timeout_t timeout, int16_t* rssi, int8_t* snr) {
    return -ENOTSUP;  // Mock doesn't receive
}
static int lora_mock_test_cw(const struct device* dev, uint32_t frequency,
                             int8_t tx_power, uint16_t duration) {
    return 0;
}

static const struct lora_driver_api lora_mock_api = {
    .config = lora_mock_config,
    .send = lora_mock_send,
    .recv = lora_mock_recv,
    .test_cw = lora_mock_test_cw,
};

static int lora_mock_init(const struct device* dev) {
    LOG_INF("Mock LoRa driver initialized");
    return 0;
}

static struct lora_mock_data lora_mock_data_0;

DEVICE_DT_DEFINE(DT_NODELABEL(lora_mock), lora_mock_init, NULL,
                 &lora_mock_data_0, NULL, POST_KERNEL,
                 CONFIG_LORA_MOCK_INIT_PRIORITY, &lora_mock_api);
