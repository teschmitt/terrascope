#ifndef TS_SENSOR_BACKEND_H
#define TS_SENSOR_BACKEND_H

/**
 * @addtogroup sensors
 * @{
 */

#include "messages/messages.h"

/**
 * @brief Read sensor data into a telemetry message.
 *
 * Each backend (BME280, mock) provides its own implementation.
 * The active backend is selected at build time via CMake/devicetree.
 *
 * @param p_tel  Output telemetry struct to populate
 * @return 0 on success, negative errno on failure
 */
int ts_sensor_backend_read(struct ts_msg_telemetry *p_tel);

/** @} */

#endif  // TS_SENSOR_BACKEND_H
