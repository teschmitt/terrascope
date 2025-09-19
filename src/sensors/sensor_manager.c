#include "sensor_manager.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sensor);

void periodic_work_handler(const struct zbus_channel* chan) {
    // TODO: do all readings and return results
    LOG_DBG("Taking readings now");
    // struct ts_msg_telemetry msg = {.humidity = sys_rand32_get(),
    //                                .pressure = sys_rand32_get(),
    //                                .temperature = sys_rand32_get()};
    static uint32_t counter = 0;
    counter++;

    struct ts_msg_telemetry msg = {.timestamp = k_uptime_get_32(),
                                   .humidity = counter * 100,
                                   .pressure = counter * 200,
                                   .temperature = counter * 50};
    LOG_DBG("Sending sensor reading: ts=%d, pressure=%d, temp=%d, hum=%d",
            msg.timestamp, msg.pressure, msg.temperature, msg.humidity);
    zbus_chan_pub(chan, &msg, K_MSEC(200));
}

void sensor_take_reading_wrapper(struct k_work* work) {
    periodic_work_handler(&telemetry_chan);
}
