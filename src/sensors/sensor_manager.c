#include "sensor_manager.h"

#include <zephyr/logging/log.h>

#include "logging/logging.h"
#include "routing/routing.h"
#include "sensors/sensor_backend.h"

LOG_MODULE_REGISTER(sensor);

extern struct zbus_channel ts_lora_out_chan;

void periodic_work_handler(const struct zbus_channel* chan) {
    struct ts_msg_lora_outgoing out_msg = {
        .type = TS_MSG_TELEMETRY,
        .data.telemetry.timestamp = (uint32_t)k_uptime_seconds(),
    };
    ts_routing_prepare_header(&out_msg.route, TS_ROUTING_BROADCAST_ADDR);

    if (ts_sensor_backend_read(&out_msg.data.telemetry) != 0) { return; }

    LOG_DBG("Sending sensor reading: ts=%d, pressure=%d, temp=%d, hum=%d",
            out_msg.data.telemetry.timestamp, out_msg.data.telemetry.pressure,
            out_msg.data.telemetry.temperature,
            out_msg.data.telemetry.humidity);

    int ret = zbus_chan_pub(&ts_lora_out_chan, &out_msg, K_MSEC(200));
    log_chan_pub_ret(ret);
}

void sensor_take_reading_wrapper(struct k_work* work) {
    periodic_work_handler(&ts_lora_out_chan);
}
