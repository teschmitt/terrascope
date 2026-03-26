#include "sensor_manager.h"

#include <zephyr/device.h>

#include "logging/logging.h"

#if DT_HAS_COMPAT_STATUS_OKAY(bosch_bme280)
#include <zephyr/drivers/sensor.h>
#endif

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sensor);

extern struct zbus_channel ts_lora_out_chan;

#if DT_HAS_COMPAT_STATUS_OKAY(bosch_bme280)

static const struct device *const bme280 =
    DEVICE_DT_GET(DT_COMPAT_GET_ANY_STATUS_OKAY(bosch_bme280));

static int sensor_read_bme280(struct ts_msg_telemetry *p_tel) {
    struct sensor_value val;

    int ret = sensor_sample_fetch(bme280);
    if (ret != 0) {
        LOG_ERR("BME280 sample fetch failed: %d", ret);
        return ret;
    }

    // Temperature in centi-degrees C (e.g. 2512 = 25.12 °C)
    sensor_channel_get(bme280, SENSOR_CHAN_AMBIENT_TEMP, &val);
    p_tel->temperature = (uint32_t)(val.val1 * 100 + val.val2 / 10000);

    // Humidity in centi-percent RH (e.g. 6543 = 65.43 %RH)
    sensor_channel_get(bme280, SENSOR_CHAN_HUMIDITY, &val);
    p_tel->humidity = (uint32_t)(val.val1 * 100 + val.val2 / 10000);

    // Pressure in Pa (e.g. 101325 = 1013.25 hPa)
    sensor_channel_get(bme280, SENSOR_CHAN_PRESS, &val);
    p_tel->pressure = (uint32_t)(val.val1 * 1000 + val.val2 / 1000);

    return 0;
}

#endif  // DT_HAS_COMPAT_STATUS_OKAY(bosch_bme280)

void periodic_work_handler(const struct zbus_channel *chan) {
    struct ts_msg_lora_outgoing out_msg = {
        .type = TS_MSG_TELEMETRY,
        .data.telemetry.timestamp = (uint32_t)k_uptime_seconds(),
    };

#if DT_HAS_COMPAT_STATUS_OKAY(bosch_bme280)
    if (!device_is_ready(bme280)) {
        LOG_ERR("BME280 device not ready");
        return;
    }

    if (sensor_read_bme280(&out_msg.data.telemetry) != 0) {
        return;
    }
#else
    out_msg.data.telemetry.humidity = sys_rand32_get() / 1000000;
    out_msg.data.telemetry.pressure = sys_rand32_get() / 10000;
    out_msg.data.telemetry.temperature = sys_rand32_get() / 1000000;
#endif

    LOG_DBG("Sending sensor reading: ts=%d, pressure=%d, temp=%d, hum=%d",
            out_msg.data.telemetry.timestamp, out_msg.data.telemetry.pressure,
            out_msg.data.telemetry.temperature,
            out_msg.data.telemetry.humidity);

    int ret = zbus_chan_pub(&ts_lora_out_chan, &out_msg, K_MSEC(200));
    log_chan_pub_ret(ret);
}

void sensor_take_reading_wrapper(struct k_work *work) {
    periodic_work_handler(&ts_lora_out_chan);
}
