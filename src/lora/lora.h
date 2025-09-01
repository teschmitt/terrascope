#if !defined(LORA_DEVICE_H)
#define LORA_DEVICE_H
#include <zcbor_encode.h>
#include <zephyr/zbus/zbus.h>
#include "node_status/node_status.h"
#include "sensors/sensor_manager.h"

#define LORA_OUT_THREAD_STACK_SIZE 1024
#define ZBOR_ENCODE_BUFFER_SIZE 256

enum ts_msg_lora_type { TS_MSG_TELEMETRY, TS_MSG_NODE_STATUS };

struct ts_msg_lora_outgoing {
    enum ts_msg_lora_type type;
    union {
        struct ts_msg_telemetry* telemetry;
        struct ts_msg_node_status* node_status;
    } data;
};

void lora_out_task();

int serialize(struct ts_msg_lora_outgoing* msg, uint8_t* p_buf, size_t buf_len,
              size_t* p_size);

#endif  // LORA_DEVICE_H