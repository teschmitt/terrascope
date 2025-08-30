#include <zephyr/drivers/lora.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <zephyr/zbus/zbus.h>
#include "lora/LoraDevice.h"

#define DEFAULT_RADIO_NODE DT_ALIAS(lora0)

LOG_MODULE_DECLARE(zbus, CONFIG_ZBUS_LOG_LEVEL);

#define PRODUCER_DELAY K_MSEC(500)
#define CONSUMER_START_DELAY_MS 3000

struct ts_msg_telemetry {
    uint32_t timestamp;
    uint16_t temperature;
    uint16_t humidity;
    uint16_t pressure;
};

ZBUS_CHAN_DEFINE(telemetry_chan, struct ts_msg_telemetry, NULL, NULL,
                 ZBUS_OBSERVERS(tm_msg_sub_01),
                 ZBUS_MSG_INIT(.timestamp = 0, .temperature = 0, .humidity = 0,
                               .pressure = 0));

void log_telemetry(struct ts_msg_telemetry* pkt, const char* prefix) {
    LOG_INF("%s Telemetry: ts=%u, temp=%d.%02d°C, humidity=%d.%02d%%", prefix,
            pkt->timestamp, pkt->temperature / 100, pkt->temperature % 100,
            pkt->humidity / 100, pkt->humidity % 100);
}

void producer_thread(void) {
    LOG_INF("Producer thread started");
    struct ts_msg_telemetry msg;

    while (1) {
        msg.timestamp = sys_rand32_get();
        msg.temperature = sys_rand32_get() % 5000;
        msg.humidity = sys_rand32_get() % 10000;
        zbus_chan_pub(&telemetry_chan, &msg, K_MSEC(200));
        log_telemetry(&msg, "[PUBLISH] ");
        k_sleep(PRODUCER_DELAY);
    };
}

ZBUS_SUBSCRIBER_DEFINE(tm_msg_sub_01, 10);
void consumer_thread(void) {
    LOG_INF("Consumer thread started");
    const struct zbus_channel* chan;
    struct ts_msg_telemetry msg;

    while (!zbus_sub_wait(&tm_msg_sub_01, &chan, K_FOREVER)) {
        if (&telemetry_chan == chan) {
            zbus_chan_read(&telemetry_chan, &msg, K_MSEC(500));
            log_telemetry(&msg, "[CONSUMER] ");
        }
    }
}

// Define threads with appropriate priorities and stack sizes
K_THREAD_DEFINE(producer_tid, 1024, producer_thread, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(consumer_tid, 1024, consumer_thread, NULL, NULL, NULL, 6, 0,
                CONSUMER_START_DELAY_MS);

int main() {
    LOG_INF("Zbus multi-threaded example started");
    LOG_INF(
        "Producer will start immediately, Consumer will start after 3 seconds");

    while (1) {
        k_sleep(K_SECONDS(5));
        LOG_INF("Main thread heartbeat");
    }

    return 0;
}
