#include "lora.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(lora);

ZBUS_SUBSCRIBER_DEFINE(tm_telemetry_sub_01, 2);
K_THREAD_DEFINE(lora_out_tid, LORA_OUT_THREAD_STACK_SIZE, lora_out_task, NULL,
                NULL, NULL, 3, 0, 0);

void lora_out_task() {
    const struct zbus_channel* chan;

    while (!zbus_sub_wait(&tm_telemetry_sub_01, &chan, K_FOREVER)) {
        struct ts_msg_telemetry msg = {0};

        if (&telemetry_chan == chan) {
            zbus_chan_read(&telemetry_chan, &msg, K_NO_WAIT);
            LOG_DBG(
                "Received sensor readings: ts=%d, pressure=%d, temp=%d, hum=%d",
                msg.timestamp, msg.pressure, msg.temperature, msg.humidity);
        }
    }
}
