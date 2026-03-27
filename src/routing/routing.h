#ifndef TS_ROUTING_H
#define TS_ROUTING_H

/**
 * @defgroup routing Routing
 * @brief Node addressing, TTL management, and duplicate detection.
 * @{
 */

#include <stdbool.h>
#include <stdint.h>

/** @brief Broadcast destination address. */
#define TS_ROUTING_BROADCAST_ADDR 0xFFFF

/** @brief Default time-to-live for new outgoing messages. */
#define TS_ROUTING_DEFAULT_TTL 5

/** @brief Number of (src, msg_id) entries in the duplicate detection cache. */
#define TS_ROUTING_SEEN_CACHE_SIZE 32

/** @brief Routing header prepended to every mesh message. */
struct ts_route_header {
    uint16_t src;
    uint16_t dst;
    uint32_t msg_id;
    uint8_t ttl;
    uint8_t key_id;
};

/**
 * @brief Initialize the routing subsystem.
 *
 * Sets this node's address and resets the duplicate detection cache.
 *
 * @param node_id  Unique 16-bit address for this node
 */
void ts_routing_init(uint16_t node_id);

/**
 * @brief Get this node's address.
 *
 * @return The node ID set by ts_routing_init()
 */
uint16_t ts_routing_get_node_id(void);

/**
 * @brief Prepare a routing header for a new outgoing message.
 *
 * Sets src to this node's ID, dst to the given destination,
 * assigns an auto-incrementing msg_id, and sets TTL to default.
 *
 * @param p_hdr  Output routing header to populate
 * @param dst    Destination node ID or TS_ROUTING_BROADCAST_ADDR
 */
void ts_routing_prepare_header(struct ts_route_header *p_hdr, uint16_t dst);

/**
 * @brief Decrement TTL on a routing header.
 *
 * @param p_hdr  Routing header to modify
 * @return 0 if still valid after decrement, -EHOSTUNREACH if already expired
 */
int ts_routing_decrement_ttl(struct ts_route_header *p_hdr);

/**
 * @brief Check if a message is addressed to this node.
 *
 * @param p_hdr  Routing header to check
 * @return true if dst matches this node's ID or is broadcast
 */
bool ts_routing_is_for_us(const struct ts_route_header *p_hdr);

/**
 * @brief Check if a message has been seen before.
 *
 * @param p_hdr  Routing header to check
 * @return true if the (src, msg_id) pair is in the seen cache
 */
bool ts_routing_is_duplicate(const struct ts_route_header *p_hdr);

/**
 * @brief Record a message in the duplicate detection cache.
 *
 * @param p_hdr  Routing header to record
 */
void ts_routing_mark_seen(const struct ts_route_header *p_hdr);

/** @} */

#endif  // TS_ROUTING_H
