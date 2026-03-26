#ifndef TS_SENSOR_MANAGER_H
#define TS_SENSOR_MANAGER_H

/**
 * @defgroup sensors Sensors
 * @brief Sensor manager and backend abstraction.
 * @{
 */

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>

#include "messages/messages.h"

/**
 * @brief Read sensors and publish telemetry to the LoRa outgoing channel.
 *
 * @param chan  Zbus channel to publish to
 */
void periodic_work_handler(const struct zbus_channel* chan);

/**
 * @brief Work queue wrapper for periodic_work_handler.
 *
 * @param work  Work item (unused)
 */
void sensor_take_reading_wrapper(struct k_work* work);

/** @} */

#endif  // TS_SENSOR_MANAGER_H
