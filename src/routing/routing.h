#ifndef TS_ROUTING_H
#define TS_ROUTING_H

#include <stdbool.h>
#include <stdint.h>

#define TS_ROUTING_BROADCAST_ADDR 0xFFFF
#define TS_ROUTING_DEFAULT_TTL 5
#define TS_ROUTING_SEEN_CACHE_SIZE 32

struct ts_route_header {
    uint16_t src;
    uint16_t dst;
    uint32_t msg_id;
    uint8_t ttl;
};

// Initialize routing subsystem with this node's address and reset state
void ts_routing_init(uint16_t node_id);

// Get this node's address
uint16_t ts_routing_get_node_id(void);

// Prepare a routing header for a new outgoing message
void ts_routing_prepare_header(struct ts_route_header *p_hdr, uint16_t dst);

// Decrement TTL. Returns 0 if still valid, -EHOSTUNREACH if already expired
int ts_routing_decrement_ttl(struct ts_route_header *p_hdr);

// Check if message is addressed to this node (unicast match or broadcast)
bool ts_routing_is_for_us(const struct ts_route_header *p_hdr);

// Check if this message has been seen before
bool ts_routing_is_duplicate(const struct ts_route_header *p_hdr);

// Record a message in the seen cache
void ts_routing_mark_seen(const struct ts_route_header *p_hdr);

#endif  // TS_ROUTING_H
