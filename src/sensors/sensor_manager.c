#include "sensor_manager.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sensor);

extern struct zbus_channel ts_lora_out_chan;

void periodic_work_handler(const struct zbus_channel* chan) {
    // TODO: do all readings and return results
    LOG_DBG("Taking readings now");

    struct ts_msg_lora_outgoing out_msg = {
        .type = TS_MSG_TELEMETRY,
        .data.telemetry = {.humidity = sys_rand32_get() / 1000,
                           .pressure = sys_rand32_get() / 1000,
                           .temperature = sys_rand32_get() / 1000}};

    LOG_DBG("Sending sensor reading: ts=%d, pressure=%d, temp=%d, hum=%d",
            out_msg.data.telemetry.timestamp, out_msg.data.telemetry.pressure,
            out_msg.data.telemetry.temperature,
            out_msg.data.telemetry.humidity);

    zbus_chan_pub(&ts_lora_out_chan, &out_msg, K_MSEC(200));
}

void sensor_take_reading_wrapper(struct k_work* work) {
    periodic_work_handler(&ts_lora_out_chan);
}
