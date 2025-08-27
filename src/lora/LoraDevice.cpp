#include "LoraDevice.h"
#include <string.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(lora_device, LOG_LEVEL_INF);

// Callback function for async receive
static void lora_receive_cb_here(const struct device* dev, uint8_t* data,
                                 uint16_t size, int16_t rssi, int8_t snr,
                                 void* user_data) {
    static int cnt;

    ARG_UNUSED(dev);
    ARG_UNUSED(size);
    ARG_UNUSED(user_data);

    LOG_INF("LoRa RX RSSI: %d dBm, SNR: %d dB", rssi, snr);
    LOG_HEXDUMP_INF(data, size, "LoRa RX payload");

    LOG_INF("LoRa async receive: %d bytes, RSSI: %d, SNR: %d", size, rssi, snr);
    // Handle received data here or signal the main application
}

// Default LoRa configuration - optimized for reliability
const LoraDevice::Config LoraDevice::DEFAULT_CONFIG = {.frequency = 865100000,
                                                       .bandwidth = BW_125_KHZ,
                                                       .datarate = SF_10,
                                                       .preamble_len = 8,
                                                       .coding_rate = CR_4_5,
                                                       .iq_inverted = false,
                                                       .public_network = false,
                                                       .tx_power = 4};

// Get the LoRa device from devicetree
#ifndef DEFAULT_RADIO_NODE
#define DEFAULT_RADIO_NODE DT_ALIAS(lora0)
#endif

LoraDevice::LoraDevice()
    : m_loraDevice(nullptr)
    , m_config(DEFAULT_CONFIG)
    , m_lastError(ErrorCode::SUCCESS)
    , m_deviceReady(false)
    , m_initialized(false) {
    // Get device reference from devicetree
    m_loraDevice = DEVICE_DT_GET(DEFAULT_RADIO_NODE);
}

LoraDevice::ErrorCode LoraDevice::init() { return init(DEFAULT_CONFIG); }

LoraDevice::ErrorCode LoraDevice::init(const Config& config) {
    m_lastError = ErrorCode::SUCCESS;

    // Check if device is ready
    if (!device_is_ready(m_loraDevice)) {
        LOG_ERR("LoRa device %s not ready", m_loraDevice->name);
        m_lastError = ErrorCode::DEVICE_NOT_READY;
        m_deviceReady = false;
        return m_lastError;
    }

    m_deviceReady = true;
    m_config = config;

    // Apply configuration
    m_lastError = applyConfiguration();
    if (m_lastError != ErrorCode::SUCCESS) {
        LOG_ERR("LoRa configuration failed");
        m_initialized = false;
        return m_lastError;
    }

    m_initialized = true;
    logConfiguration();
    LOG_INF("LoRa device initialized successfully");

    return ErrorCode::SUCCESS;
}

LoraDevice::ErrorCode LoraDevice::send(uint8_t* data, size_t length,
                                       uint32_t timeout_ms) {
    if (!m_initialized || !m_deviceReady) {
        m_lastError = ErrorCode::DEVICE_NOT_READY;
        return m_lastError;
    }

    if (data == nullptr || length == 0) {
        m_lastError = ErrorCode::INVALID_PARAMS;
        return m_lastError;
    }

    // Configure for transmit mode
    struct lora_modem_config tx_config;
    memcpy(&tx_config, &m_config, sizeof(m_config));
    tx_config.tx = true;

    int ret = lora_config(m_loraDevice, &tx_config);
    if (ret < 0) {
        LOG_ERR("Failed to configure LoRa for TX mode: %d", ret);
        m_lastError = convertZephyrError(ret);
        return m_lastError;
    }

    // Send data
    ret = lora_send(m_loraDevice, data, length);
    if (ret < 0) {
        LOG_ERR("LoRa send failed: %d", ret);
        m_lastError = convertZephyrError(ret);
        return m_lastError;
    }

    LOG_DBG("Sent %d bytes via LoRa", length);
    m_lastError = ErrorCode::SUCCESS;
    return m_lastError;
}

LoraDevice::ErrorCode LoraDevice::recv(uint8_t* buffer, size_t buffer_size,
                                       size_t* received_length, int16_t* rssi,
                                       int8_t* snr, uint32_t timeout_ms) {
    if (!m_initialized || !m_deviceReady) {
        m_lastError = ErrorCode::DEVICE_NOT_READY;
        return m_lastError;
    }

    if (buffer == nullptr || buffer_size == 0 || received_length == nullptr) {
        m_lastError = ErrorCode::INVALID_PARAMS;
        return m_lastError;
    }

    // Configure for receive mode
    struct lora_modem_config rx_config = {
        .frequency = m_config.frequency,
        .bandwidth = m_config.bandwidth,
        .datarate = m_config.datarate,
        .coding_rate = m_config.coding_rate,
        .preamble_len = m_config.preamble_len,
        .tx_power = m_config.tx_power,
        .tx = false,
        .iq_inverted = m_config.iq_inverted,
        .public_network = m_config.public_network};

    int ret = lora_config(m_loraDevice, &rx_config);
    if (ret < 0) {
        LOG_ERR("Failed to configure LoRa for RX mode: %d", ret);
        m_lastError = convertZephyrError(ret);
        return m_lastError;
    }

    // REMOVE ALL THE MANUAL TIMEOUT CODE - lora_recv handles timeout internally
    ret = lora_recv(m_loraDevice, buffer, buffer_size, K_MSEC(timeout_ms), rssi,
                    snr);

    if (ret > 0) {
        *received_length = static_cast<uint16_t>(ret);
        LOG_DBG("Received %d bytes via LoRa", ret);
        m_lastError = ErrorCode::SUCCESS;
        return m_lastError;
    } else if (ret == -EAGAIN || ret == -ETIMEDOUT) {
        *received_length = 0;
        m_lastError = ErrorCode::TIMEOUT;
        return m_lastError;
    } else {
        LOG_ERR("LoRa receive failed: %d", ret);
        m_lastError = convertZephyrError(ret);
        return m_lastError;
    }
}

bool LoraDevice::isDataAvailable() const {
    if (!m_initialized || !m_deviceReady) { return false; }

    // This would need to be implemented based on your specific LoRa driver
    // and callback mechanism. For now, return false as placeholder.
    return false;
}

LoraDevice::ErrorCode LoraDevice::setFrequency(uint32_t frequency) {
    if (!m_initialized) {
        m_lastError = ErrorCode::DEVICE_NOT_READY;
        return m_lastError;
    }

    m_config.frequency = frequency;
    m_lastError = applyConfiguration();

    if (m_lastError == ErrorCode::SUCCESS) {
        LOG_INF("LoRa frequency updated to %d Hz", frequency);
    }

    return m_lastError;
}

LoraDevice::ErrorCode LoraDevice::setTxPower(int8_t power) {
    if (!m_initialized) {
        m_lastError = ErrorCode::DEVICE_NOT_READY;
        return m_lastError;
    }

    m_config.tx_power = power;
    m_lastError = applyConfiguration();

    if (m_lastError == ErrorCode::SUCCESS) {
        LOG_INF("LoRa TX power updated to %d dBm", power);
    }

    return m_lastError;
}

LoraDevice::ErrorCode LoraDevice::setBandwidth(
    enum lora_signal_bandwidth bandwidth) {
    if (!m_initialized) {
        m_lastError = ErrorCode::DEVICE_NOT_READY;
        return m_lastError;
    }

    m_config.bandwidth = bandwidth;
    m_lastError = applyConfiguration();

    if (m_lastError == ErrorCode::SUCCESS) {
        LOG_INF("LoRa bandwidth updated");
    }

    return m_lastError;
}

LoraDevice::ErrorCode LoraDevice::setDatarate(enum lora_datarate datarate) {
    if (!m_initialized) {
        m_lastError = ErrorCode::DEVICE_NOT_READY;
        return m_lastError;
    }

    m_config.datarate = datarate;
    m_lastError = applyConfiguration();

    if (m_lastError == ErrorCode::SUCCESS) { LOG_INF("LoRa datarate updated"); }

    return m_lastError;
}

// Private methods
LoraDevice::ErrorCode LoraDevice::applyConfiguration() {
    if (!m_deviceReady) { return ErrorCode::DEVICE_NOT_READY; }

    struct lora_modem_config zephyr_config = {
        .frequency = m_config.frequency,
        .bandwidth = m_config.bandwidth,
        .datarate = m_config.datarate,
        .coding_rate = m_config.coding_rate,
        .preamble_len = m_config.preamble_len,
        .tx_power = m_config.tx_power,
        .tx = true,  // Default to TX mode, will be changed per operation
        .iq_inverted = m_config.iq_inverted,
        .public_network = m_config.public_network,
    };

    int ret = lora_config(m_loraDevice, &zephyr_config);
    if (ret < 0) {
        LOG_ERR("LoRa configuration failed: %d", ret);
        return convertZephyrError(ret);
    }

    return ErrorCode::SUCCESS;
}

LoraDevice::ErrorCode LoraDevice::convertZephyrError(int zephyr_ret) {
    switch (zephyr_ret) {
        case 0:
            return ErrorCode::SUCCESS;
        case -EBUSY:
            return ErrorCode::BUSY;
        case -ETIMEDOUT:
            return ErrorCode::TIMEOUT;
        case -EINVAL:
            return ErrorCode::INVALID_PARAMS;
        default:
            return ErrorCode::CONFIG_FAILED;
    }
}

void LoraDevice::logConfiguration() const {
    LOG_INF("LoRa Configuration:");
    LOG_INF("  Frequency: %d Hz", m_config.frequency);
    LOG_INF("  Bandwidth: %d", m_config.bandwidth);
    LOG_INF("  Datarate (SF): %d", m_config.datarate);
    LOG_INF("  Preamble Length: %d", m_config.preamble_len);
    LOG_INF("  Coding Rate: %d", m_config.coding_rate);
    LOG_INF("  TX Power: %d dBm", m_config.tx_power);
    LOG_INF("  Public Network: %s", m_config.public_network ? "Yes" : "No");
    LOG_INF("  IQ Inverted: %s", m_config.iq_inverted ? "Yes" : "No");
}
