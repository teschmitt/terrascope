#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/lora.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(lora_mock);

// Loopback: sent packets are queued here so lora_mock_recv can return them.
// This simulates receiving our own transmissions, which exercises the full
// TX→CBOR→radio→CBOR→RX pipeline during QEMU testing.
#define MOCK_LOOPBACK_BUF_SIZE 256
#define MOCK_LOOPBACK_QUEUE_DEPTH 4

struct lora_mock_packet {
    uint8_t data[MOCK_LOOPBACK_BUF_SIZE];
    uint8_t len;
};

struct lora_mock_data {
    struct lora_modem_config config;
    bool configured;
    struct k_msgq rx_msgq;
    char rx_msgq_buf[MOCK_LOOPBACK_QUEUE_DEPTH *
                     sizeof(struct lora_mock_packet)];
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
    struct lora_mock_data* drv_data = dev->data;

    LOG_INF("Mock LoRa send: %d bytes", data_len);

    if (data_len > MOCK_LOOPBACK_BUF_SIZE) {
        return -EMSGSIZE;
    }

    // Queue the packet for the receive side
    struct lora_mock_packet pkt = {0};
    memcpy(pkt.data, data, data_len);
    pkt.len = (uint8_t)data_len;

    int ret = k_msgq_put(&drv_data->rx_msgq, &pkt, K_NO_WAIT);
    if (ret != 0) {
        // Queue full — drop silently, like a real radio would
        LOG_WRN("Mock RX queue full, dropping loopback packet");
    }

    return 0;
}

static int lora_mock_recv(const struct device* dev, uint8_t* data, uint8_t size,
                          k_timeout_t timeout, int16_t* rssi, int8_t* snr) {
    struct lora_mock_data* drv_data = dev->data;
    struct lora_mock_packet pkt;

    int ret = k_msgq_get(&drv_data->rx_msgq, &pkt, timeout);
    if (ret == -EAGAIN || ret == -ENOMSG) {
        return -EAGAIN;
    }
    if (ret != 0) {
        return ret;
    }

    if (pkt.len > size) {
        return -ENOMEM;
    }

    memcpy(data, pkt.data, pkt.len);
    *rssi = -42;
    *snr = 10;

    LOG_INF("Mock LoRa recv: %d bytes (loopback)", pkt.len);
    return pkt.len;
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
    struct lora_mock_data* data = dev->data;

    k_msgq_init(&data->rx_msgq, data->rx_msgq_buf,
                sizeof(struct lora_mock_packet), MOCK_LOOPBACK_QUEUE_DEPTH);

    LOG_INF("Mock LoRa driver initialized (loopback enabled)");
    return 0;
}

static struct lora_mock_data lora_mock_data_0;

DEVICE_DT_DEFINE(DT_NODELABEL(lora_mock), lora_mock_init, NULL,
                 &lora_mock_data_0, NULL, POST_KERNEL,
                 CONFIG_LORA_MOCK_INIT_PRIORITY, &lora_mock_api);
