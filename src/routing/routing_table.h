#ifndef TS_ROUTING_TABLE_H
#define TS_ROUTING_TABLE_H

#include <stdbool.h>
#include <stdint.h>

#define TS_ROUTING_TABLE_SIZE 16
#define TS_ROUTING_TABLE_STALE_TIMEOUT_S 300

struct ts_neighbor {
    uint16_t node_id;
    int16_t rssi;
    int8_t snr;
    bool direct;
    uint32_t last_seen;
    bool occupied;
};

// Clear the routing table
void ts_routing_table_init(void);

// Insert or update a neighbor. Evicts oldest if table is full.
int ts_routing_table_update(uint16_t node_id, int16_t rssi, int8_t snr,
                            uint8_t ttl);

// Copy neighbor entry to *p_neighbor. Returns 0 or -ENOENT.
int ts_routing_table_lookup(uint16_t node_id,
                            struct ts_neighbor *p_neighbor);

// Remove entries older than max_age_s seconds. Returns count removed.
int ts_routing_table_age_seconds(uint32_t max_age_s);

// Number of occupied entries
uint32_t ts_routing_table_count(void);

#endif  // TS_ROUTING_TABLE_H
