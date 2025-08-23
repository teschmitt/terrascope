#ifndef LORA_DEVICE_H
#define LORA_DEVICE_H

#include <zephyr/device.h>
#include <zephyr/drivers/lora.h>
#include <zephyr/logging/log.h>
#include <stdint.h>
#include <stdbool.h>

// Forward declarations
struct device;
struct lora_modem_config;

static void lora_receive_cb(const struct device *dev, uint8_t *data, uint16_t size,
                            int16_t rssi, int8_t snr, void *user_data);

class LoraDevice
{
public:
    // LoRa configuration structure for easy parameter changes
    struct Config
    {
        uint32_t frequency;
        enum lora_signal_bandwidth bandwidth;
        enum lora_datarate datarate;
        uint8_t preamble_len;
        enum lora_coding_rate coding_rate;
        bool iq_inverted;
        bool public_network;
        int8_t tx_power;
    };

    // Error codes
    enum class ErrorCode : uint8_t
    {
        SUCCESS = 0,
        DEVICE_NOT_READY,
        CONFIG_FAILED,
        SEND_FAILED,
        RECV_FAILED,
        INVALID_PARAMS,
        TIMEOUT,
        BUSY
    };

    // Constructor
    LoraDevice();

    // Destructor
    ~LoraDevice() = default;

    // Initialize LoRa device with default configuration
    ErrorCode init();

    // Initialize LoRa device with custom configuration
    ErrorCode init(const Config &config);

    // Send data
    ErrorCode send(uint8_t *data, size_t length, uint32_t timeout_ms = 5000);

    // Receive data (blocking with timeout)
    ErrorCode recv(uint8_t *buffer, size_t buffer_size, size_t *received_length,
                   int16_t *rssi = nullptr, int8_t *snr = nullptr, uint32_t timeout_ms = 10000);

    // Non-blocking receive check
    bool isDataAvailable() const;

    // Get current configuration
    const Config &getConfig() const { return m_config; }

    // Update specific configuration parameters
    ErrorCode setFrequency(uint32_t frequency);
    ErrorCode setTxPower(int8_t power);
    ErrorCode setBandwidth(enum lora_signal_bandwidth bandwidth);
    ErrorCode setDatarate(enum lora_datarate datarate);

    // Get last error code
    ErrorCode getLastError() const { return m_lastError; }

    // Check if device is ready
    bool isReady() const { return m_deviceReady; }

private:
    // Private member variables
    const struct device *m_loraDevice;
    Config m_config;
    ErrorCode m_lastError;
    bool m_deviceReady;
    bool m_initialized;

    // Default configuration values
    static const Config DEFAULT_CONFIG;

    // Private methods
    ErrorCode applyConfiguration();
    ErrorCode convertZephyrError(int zephyr_ret);
    void logConfiguration() const;
};

#endif // LORA_DEVICE_H