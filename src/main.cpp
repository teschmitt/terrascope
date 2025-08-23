#include <zephyr/device.h>
#include <zephyr/drivers/lora.h>
#include <errno.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>
#include <string.h>
#include "lora/LoraDevice.h"

#define DEFAULT_RADIO_NODE DT_ALIAS(lora0)
#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL

#include <zephyr/logging/log.h>
#include <string>

BUILD_ASSERT(DT_NODE_HAS_STATUS_OKAY(DEFAULT_RADIO_NODE),
             "No default LoRa radio specified in DT");

LOG_MODULE_REGISTER(main);

int main(void)
{
    LOG_INF("Starting LoRa application...");

    // Create LoRa device instance
    LoraDevice lora;

    // Initialize with default configuration
    LoraDevice::ErrorCode result = lora.init();
    if (result != LoraDevice::ErrorCode::SUCCESS)
    {
        LOG_ERR("LoRa initialization failed: %d", static_cast<int>(result));
        return -1;
    }

    LOG_INF("LoRa device initialized successfully");

    // Example usage: Send a test message
    uint8_t test_message[12] = {'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '0'};

    result = lora.send(test_message, 12);

    if (result == LoraDevice::ErrorCode::SUCCESS)
    {
        LOG_INF("Test message sent successfully");
    }
    else
    {
        LOG_ERR("Failed to send test message: %d", static_cast<int>(result));
    }

    // Example usage: Wait for incoming messages
    uint8_t rx_buffer[256];
    size_t received_length;
    int16_t rssi;
    int8_t snr;

    LOG_INF("Waiting for incoming LoRa messages...");

    while (1)
    {
        result = lora.recv(rx_buffer, sizeof(rx_buffer), &received_length,
                           &rssi, &snr, 5000); // 5 second timeout

        if (result == LoraDevice::ErrorCode::SUCCESS)
        {
            LOG_INF("Received %d bytes: %.*s (RSSI: %d, SNR: %d)",
                    received_length, received_length, rx_buffer, rssi, snr);
        }
        else if (result == LoraDevice::ErrorCode::TIMEOUT)
        {
            LOG_DBG("Receive timeout - continuing to listen...");
        }
        else
        {
            LOG_ERR("Receive error: %d", static_cast<int>(result));
        }

        k_msleep(100); // Small delay between attempts
    }

    return 0;
}