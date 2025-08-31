#include <zephyr/drivers/lora.h>
#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include "lora/lora.h"
#include "node_status/node_status.h"
#include "sensors/sensor_manager.h"

#define DEFAULT_RADIO_NODE DT_ALIAS(lora0)

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(terrascope);

#define PRODUCER_DELAY K_MSEC(500)
#define CONSUMER_START_DELAY_MS 3000

#define ZBUS_SEND_TIMEOUT K_MSEC(200)

ZBUS_CHAN_DEFINE(telemetry_chan, struct ts_msg_telemetry, NULL, NULL,
                 ZBUS_OBSERVERS(tm_telemetry_sub_01), ZBUS_MSG_INIT(0));

ZBUS_CHAN_DEFINE(node_status_chan, struct ts_msg_node_status, NULL, NULL,
                 ZBUS_OBSERVERS(tm_nodests_sub_01), ZBUS_MSG_INIT(0));

ZBUS_CHAN_DEFINE(lora_outgoing_chan, struct ts_msg_lora_outgoing, NULL, NULL,
                 ZBUS_OBSERVERS_EMPTY, ZBUS_MSG_INIT(0));

ZBUS_SUBSCRIBER_DEFINE(tm_nodests_sub_01, 2);

// Set up periodic sensor readings using a timer and submit the work to the
// system workqueue
K_WORK_DEFINE(sensor_take_reading, sensor_take_reading_wrapper);
void sensor_periodic_timer_handler(struct k_timer* dummy) {
    k_work_submit(&sensor_take_reading);
}
K_TIMER_DEFINE(sensor_periodic_timer, sensor_periodic_timer_handler, NULL);

int main() {
    LOG_INF("Zbus multi-threaded example started");

    k_timer_start(&sensor_periodic_timer, K_SECONDS(1), K_SECONDS(1));

    while (1) {
        k_sleep(K_SECONDS(5));
        struct ts_msg_node_status msg = {
            .timestamp = 0,
            .uptime = k_uptime_seconds(),
            .status = OK};  // TODO: implement timestamp
        LOG_DBG("Notifying mesh of node status: uptime=%d, status=%d",
                msg.uptime, msg.status);
        zbus_chan_pub(&node_status_chan, &msg, ZBUS_SEND_TIMEOUT);
    }

    return 0;
}
