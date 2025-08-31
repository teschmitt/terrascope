#if !defined(NODE_STATUS_H)
#define NODE_STATUS_H

#include <stdint.h>

typedef enum ts_node_status { OK, ERROR } ts_node_status;

struct ts_msg_node_status {
    uint32_t timestamp;
    uint32_t uptime;
    ts_node_status status;
};

#endif  // NODE_STATUS_H
