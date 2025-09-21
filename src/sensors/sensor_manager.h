#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
#include <zephyr/zbus/zbus.h>
#include "messages/messages.h"

void periodic_work_handler(const struct zbus_channel* chan);
void sensor_take_reading_wrapper(struct k_work* work);

#endif  // SENSOR_MANAGER_H
