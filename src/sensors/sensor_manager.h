#if !defined(SENSOR_MANAGER_H)
#define SENSOR_MANAGER_H

#include <zephyr/random/random.h>
#include <zephyr/zbus/zbus.h>

extern const struct zbus_channel telemetry_chan;

struct ts_msg_telemetry {
    uint32_t timestamp;
    uint16_t temperature;
    uint16_t humidity;
    uint16_t pressure;
};

void sensor_take_reading_wrapper(struct k_work* work);
void periodic_work_handler(const struct zbus_channel* chan);

#endif  // SENSOR_MANAGER_H
