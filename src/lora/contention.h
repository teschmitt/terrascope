#ifndef TS_CONTENTION_H
#define TS_CONTENTION_H

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/kernel.h>

#include "messages/messages.h"

#define TS_CONTENTION_POOL_SIZE 4
#define TS_CONTENTION_DELAY_MIN_MS 0
#define TS_CONTENTION_DELAY_MAX_MS 5000
#define TS_CONTENTION_RSSI_WEAK (-120)
#define TS_CONTENTION_RSSI_STRONG (-30)

struct ts_contention_slot {
    struct k_work_delayable work;
    struct ts_msg_lora_outgoing msg;
    uint16_t src;
    uint32_t msg_id;
    bool occupied;
};

// Initialize the contention forwarding pool
void ts_contention_init(void);

// Schedule a message for delayed forwarding based on RSSI.
// Weaker signal → shorter delay (forward sooner).
// Returns 0 on success, -ENOMEM if no free slot.
int ts_contention_schedule(const struct ts_msg_lora_outgoing *p_msg,
                           int16_t rssi);

// Cancel a pending forward matching (src, msg_id).
// Returns 0 if found and cancelled, -ENOENT if not found.
int ts_contention_cancel(uint16_t src, uint32_t msg_id);

// Convert RSSI (dBm) to forwarding delay (ms). Exposed for testing.
uint32_t ts_contention_rssi_to_delay_ms(int16_t rssi);

#endif  // TS_CONTENTION_H
