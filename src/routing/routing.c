#include "routing/routing.h"

#include <errno.h>
#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(routing);

static uint16_t self_node_id;
// Atomic: incremented from both the main thread (heartbeat) and the
// system work queue (sensor timer), so a plain uint32_t would race.
static atomic_t next_msg_id;

// Ring buffer for duplicate detection
static struct {
    uint16_t src;
    uint32_t msg_id;
} seen_cache[TS_ROUTING_SEEN_CACHE_SIZE];
static uint32_t seen_write_idx;
static uint32_t seen_count;

void ts_routing_init(uint16_t node_id) {
    self_node_id = node_id;
    atomic_set(&next_msg_id, 0);
    seen_write_idx = 0;
    seen_count = 0;
    memset(seen_cache, 0, sizeof(seen_cache));
}

uint16_t ts_routing_get_node_id(void) { return self_node_id; }

void ts_routing_prepare_header(struct ts_route_header* p_hdr, uint16_t dst) {
    p_hdr->src = self_node_id;
    p_hdr->dst = dst;
    p_hdr->msg_id = (uint32_t)atomic_inc(&next_msg_id);
    p_hdr->ttl = TS_ROUTING_DEFAULT_TTL;
}

int ts_routing_decrement_ttl(struct ts_route_header* p_hdr) {
    if (p_hdr->ttl == 0) { return -EHOSTUNREACH; }
    p_hdr->ttl--;
    return 0;
}

bool ts_routing_is_for_us(const struct ts_route_header* p_hdr) {
    return p_hdr->dst == self_node_id ||
           p_hdr->dst == TS_ROUTING_BROADCAST_ADDR;
}

bool ts_routing_is_duplicate(const struct ts_route_header* p_hdr) {
    uint32_t entries = (seen_count < TS_ROUTING_SEEN_CACHE_SIZE)
                           ? seen_count
                           : TS_ROUTING_SEEN_CACHE_SIZE;
    for (uint32_t i = 0; i < entries; i++) {
        if (seen_cache[i].src == p_hdr->src &&
            seen_cache[i].msg_id == p_hdr->msg_id) {
            return true;
        }
    }
    return false;
}

void ts_routing_mark_seen(const struct ts_route_header* p_hdr) {
    seen_cache[seen_write_idx].src = p_hdr->src;
    seen_cache[seen_write_idx].msg_id = p_hdr->msg_id;
    seen_write_idx = (seen_write_idx + 1) % TS_ROUTING_SEEN_CACHE_SIZE;
    seen_count++;
}
