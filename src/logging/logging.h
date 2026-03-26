#if !defined(TS_LOGGING_H)
#define TS_LOGGING_H

/**
 * @defgroup logging Logging
 * @brief Zbus publish error logging helper.
 * @{
 */

/**
 * @brief Log the result of a zbus channel publish.
 *
 * Logs specific error messages for common failure codes.
 *
 * @param ret  Return value from zbus_chan_pub()
 */
void log_chan_pub_ret(int ret);

/** @} */

#endif  // TS_LOGGING_H
