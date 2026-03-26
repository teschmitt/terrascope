#include "sensors/sensor_backend.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sensor_bme280);

static const struct device *const bme280 =
    DEVICE_DT_GET(DT_COMPAT_GET_ANY_STATUS_OKAY(bosch_bme280));

int ts_sensor_backend_read(struct ts_msg_telemetry *p_tel) {
    struct sensor_value val;

    if (!device_is_ready(bme280)) {
        LOG_ERR("BME280 device not ready");
        return -ENODEV;
    }

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
