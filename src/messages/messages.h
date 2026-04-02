#ifndef TS_MESSAGES_H
#define TS_MESSAGES_H

/**
 * @defgroup messages Messages
 * @brief Message type definitions and wire format structures.
 * @{
 */

#include <stdint.h>

#include "routing/routing.h"

/** @brief Maximum key length for config set/ack messages. */
#define TS_MSG_CONFIG_KEY_MAX 32

/** @brief Message type discriminator. */
typedef enum {
    TS_MSG_TELEMETRY = 0,
    TS_MSG_NODE_STATUS = 1,
    TS_MSG_CONFIG_SET = 2,
    TS_MSG_CONFIG_ACK = 3,
} ts_msg_type_t;

/** @brief Node status codes. */
typedef enum {
    OK = 0,
    ERROR = 1,
} ts_status_t;

/** @brief Telemetry payload (temperature, humidity, pressure). */
struct ts_msg_telemetry {
    uint32_t timestamp;
    uint32_t temperature;
    uint32_t humidity;
    uint32_t pressure;
};

/** @brief Node status payload (uptime and health). */
struct ts_msg_node_status {
    uint32_t timestamp;
    uint32_t uptime;
    ts_status_t status;
};

/** @brief Config set request (key-value pair to apply). */
struct ts_msg_config_set {
    char key[TS_MSG_CONFIG_KEY_MAX];
    int32_t value;
};

/** @brief Config ack response (result of applying a config set). */
struct ts_msg_config_ack {
    char key[TS_MSG_CONFIG_KEY_MAX];
    int32_t value;
    int32_t result;
};

/** @brief Outgoing message with route header and typed payload. */
struct ts_msg_lora_outgoing {
    struct ts_route_header route;
    ts_msg_type_t type;
    union {
        struct ts_msg_telemetry telemetry;
        struct ts_msg_node_status node_status;
        struct ts_msg_config_set config_set;
        struct ts_msg_config_ack config_ack;
    } data;
};

/** @brief Incoming message wrapper with PHY-layer radio metadata. */
struct ts_msg_lora_incoming {
    struct ts_msg_lora_outgoing msg;
    int16_t rssi;
    int8_t snr;
};

/** @} */

#endif  // TS_MESSAGES_H
