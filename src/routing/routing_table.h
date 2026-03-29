#ifndef TS_ROUTING_TABLE_H
#define TS_ROUTING_TABLE_H

/**
 * @defgroup routing_table Routing Table
 * @brief Neighbor tracking with RSSI, SNR, and aging.
 * @{
 */

#include <stdbool.h>
#include <stdint.h>

/** @brief Maximum number of neighbors tracked. */
#define TS_ROUTING_TABLE_SIZE 16

/** @brief Default staleness timeout in seconds (5 minutes). */
#define TS_ROUTING_TABLE_STALE_TIMEOUT_S 300

/** @brief A neighbor entry in the routing table. */
struct ts_neighbor {
    uint16_t node_id;
    int16_t rssi;
    int8_t snr;
    bool direct;
    uint32_t last_seen;
    bool occupied;
};

/**
 * @brief Clear the routing table.
 */
void ts_routing_table_init(void);

/**
 * @brief Insert or update a neighbor entry.
 *
 * If the node is already in the table, updates RSSI, SNR, and timestamp.
 * If the table is full, evicts the oldest entry.
 *
 * @param node_id  Neighbor's node ID
 * @param rssi     Received signal strength (dBm)
 * @param snr      Signal-to-noise ratio (dB)
 * @param ttl      Received packet's TTL (used to infer direct neighbor)
 * @return 0 on success
 */
int ts_routing_table_update(uint16_t node_id, int16_t rssi, int8_t snr,
                            uint8_t ttl);

/**
 * @brief Look up a neighbor by node ID.
 *
 * Copies the entry into the output parameter to avoid cross-thread aliasing.
 *
 * @param node_id     Node ID to look up
 * @param p_neighbor  Output neighbor struct
 * @return 0 if found, -ENOENT if not in table
 */
int ts_routing_table_lookup(uint16_t node_id, struct ts_neighbor* p_neighbor);

/**
 * @brief Remove entries older than a given threshold.
 *
 * @param max_age_s  Maximum age in seconds before eviction
 * @return Number of entries removed
 */
int ts_routing_table_age_seconds(uint32_t max_age_s);

/**
 * @brief Get the number of occupied entries.
 *
 * @return Current neighbor count
 */
uint32_t ts_routing_table_count(void);

/** @} */

#endif  // TS_ROUTING_TABLE_H
