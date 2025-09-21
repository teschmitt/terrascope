#include <zephyr/drivers/lora.h>
#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include "lora/lora.h"
#include "messages/messages.h"
#include "sensors/sensor_manager.h"

#define DEFAULT_RADIO_NODE DT_ALIAS(lora0)

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(terrascope);

#define PRODUCER_DELAY K_MSEC(500)
#define CONSUMER_START_DELAY_MS 3000

#define ZBUS_SEND_TIMEOUT K_MSEC(200)

ZBUS_CHAN_DEFINE(ts_lora_out_chan, struct ts_msg_lora_outgoing, NULL, NULL,
                 ZBUS_OBSERVERS(ts_lora_out_sub), ZBUS_MSG_INIT(0));

// Set up periodic sensor readings using a timer and submit the work to the
// system workqueue
K_WORK_DEFINE(sensor_take_reading, sensor_take_reading_wrapper);
void sensor_periodic_timer_handler(struct k_timer* dummy) {
    k_work_submit(&sensor_take_reading);
}
K_TIMER_DEFINE(sensor_periodic_timer, sensor_periodic_timer_handler, NULL);

int main() {
    LOG_INF("Zbus multi-threaded example started");

    k_timer_start(&sensor_periodic_timer, K_SECONDS(1), K_SECONDS(10));

    while (1) {
        k_sleep(K_SECONDS(7));
        struct ts_msg_lora_outgoing out_msg = {
            .type = TS_MSG_NODE_STATUS,
            .data.node_status = {.timestamp = 0,
                                 .uptime = k_uptime_seconds(),
                                 .status = OK}  // TODO: implement timestamp
        };
        LOG_DBG("Notifying mesh of node status: uptime=%d, status=%d",
                out_msg.data.node_status.uptime,
                out_msg.data.node_status.status);

        int ret = zbus_chan_pub(&ts_lora_out_chan, &out_msg, ZBUS_SEND_TIMEOUT);
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

    return 0;
}
