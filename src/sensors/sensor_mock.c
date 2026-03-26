#include "sensors/sensor_backend.h"

#include <zephyr/random/random.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sensor_mock);

int ts_sensor_backend_read(struct ts_msg_telemetry *p_tel) {
    p_tel->temperature = sys_rand32_get() / 1000000;
    p_tel->humidity = sys_rand32_get() / 1000000;
    p_tel->pressure = sys_rand32_get() / 10000;

    return 0;
}
