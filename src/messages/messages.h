#ifndef MESSAGES_H
#define MESSAGES_H

#include <stdint.h>

// Message types
typedef enum {
    TS_MSG_TELEMETRY = 0,
    TS_MSG_NODE_STATUS = 1,
} ts_msg_type_t;

// Status enum
typedef enum {
    OK = 0,
    ERROR = 1,
} ts_status_t;

// Telemetry message structure
struct ts_msg_telemetry {
    uint32_t timestamp;
    uint32_t temperature;
    uint32_t humidity;
    uint32_t pressure;
};

// Node status message structure
struct ts_msg_node_status {
    uint32_t timestamp;
    uint32_t uptime;
    ts_status_t status;
};

// Main outgoing message structure
struct ts_msg_lora_outgoing {
    ts_msg_type_t type;
    union {
        struct ts_msg_telemetry telemetry;
        struct ts_msg_node_status node_status;
    } data;
};

#endif  // MESSAGES_H