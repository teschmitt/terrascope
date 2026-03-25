#include "logging.h"
#include <errno.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(terrascope);

void log_chan_pub_ret(int ret) {
    switch (ret) {
        case ENOMSG:
            LOG_ERR(
                "The message is invalid based on the validator function or "
                "some of the observers could not receive the "
                "notification.");
            break;
        case EBUSY:
            LOG_ERR("The channel is busy.");
            break;
        case EAGAIN:
            LOG_ERR("Waiting period timed out.");
            break;
        case EFAULT:
            LOG_ERR(
                "A parameter is incorrect, the notification could not be "
                "sent to one or more observer, or the function context is "
                "invalid (inside an ISR).");
            break;

        default:
            LOG_DBG("Message published");
    }
}