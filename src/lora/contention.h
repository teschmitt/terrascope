#ifndef TS_CONTENTION_H
#define TS_CONTENTION_H

/**
 * @defgroup contention Contention Forwarding
 * @brief RSSI-based delayed forwarding with duplicate cancellation.
 *
 * Nodes that receive a message with weaker RSSI (farther from sender)
 * forward sooner. If a duplicate arrives while a forward is pending,
 * the forward is cancelled (another node already forwarded).
 * @{
 */

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/kernel.h>

#include "messages/messages.h"

/** @brief Number of concurrent pending forwards. */
#define TS_CONTENTION_POOL_SIZE 4

/** @brief Minimum forwarding delay in milliseconds (weakest signal). */
#define TS_CONTENTION_DELAY_MIN_MS 0

/** @brief Maximum forwarding delay in milliseconds (strongest signal). */
#define TS_CONTENTION_DELAY_MAX_MS 5000

/** @brief RSSI threshold for weakest signal (dBm). */
#define TS_CONTENTION_RSSI_WEAK (-120)

/** @brief RSSI threshold for strongest signal (dBm). */
#define TS_CONTENTION_RSSI_STRONG (-30)

/** @brief A slot in the contention forwarding pool. */
struct ts_contention_slot {
    struct k_work_delayable work;
    struct ts_msg_lora_outgoing msg;
    uint16_t src;
    uint32_t msg_id;
    bool occupied;
};

/**
 * @brief Initialize the contention forwarding pool.
 */
void ts_contention_init(void);

/**
 * @brief Schedule a message for delayed forwarding based on RSSI.
 *
 * Weaker signal results in shorter delay (forward sooner).
 *
 * @param p_msg  Message to forward (copied into pool slot)
 * @param rssi   Received signal strength (dBm)
 * @return 0 on success, -ENOMEM if no free slot
 */
int ts_contention_schedule(const struct ts_msg_lora_outgoing *p_msg,
                           int16_t rssi);

/**
 * @brief Cancel a pending forward matching (src, msg_id).
 *
 * Called when a duplicate is received, indicating another node forwarded.
 *
 * @param src     Original source node ID
 * @param msg_id  Message identifier
 * @return 0 if found and cancelled, -ENOENT if not found
 */
int ts_contention_cancel(uint16_t src, uint32_t msg_id);

/**
 * @brief Convert RSSI to forwarding delay in milliseconds.
 *
 * Linear mapping: -120 dBm → 0 ms, -30 dBm → 5000 ms.
 *
 * @param rssi  Signal strength (dBm)
 * @return Delay in milliseconds
 */
uint32_t ts_contention_rssi_to_delay_ms(int16_t rssi);

/** @} */

#endif  // TS_CONTENTION_H
