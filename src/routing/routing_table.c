#include "routing/routing_table.h"

#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "routing/routing.h"

LOG_MODULE_REGISTER(routing_table);

// Mutex: the table is accessed from both the lora_in_task thread
// (ts_routing_table_update) and the system work queue
// (ts_routing_table_age_seconds via the aging timer).  k_mutex gives
// priority inheritance so the aging handler doesn't block RX
// indefinitely.
static K_MUTEX_DEFINE(table_mutex);
static struct ts_neighbor table[TS_ROUTING_TABLE_SIZE];

static struct ts_neighbor* find_by_node_id(uint16_t node_id) {
    for (int i = 0; i < TS_ROUTING_TABLE_SIZE; i++) {
        if (table[i].occupied && table[i].node_id == node_id) {
            return &table[i];
        }
    }
    return NULL;
}

static struct ts_neighbor* find_free_slot(void) {
    for (int i = 0; i < TS_ROUTING_TABLE_SIZE; i++) {
        if (!table[i].occupied) { return &table[i]; }
    }
    return NULL;
}

static struct ts_neighbor* find_oldest(void) {
    struct ts_neighbor* oldest = NULL;
    for (int i = 0; i < TS_ROUTING_TABLE_SIZE; i++) {
        if (table[i].occupied) {
            if (oldest == NULL || table[i].last_seen < oldest->last_seen) {
                oldest = &table[i];
            }
        }
    }
    return oldest;
}

void ts_routing_table_init(void) {
    k_mutex_lock(&table_mutex, K_FOREVER);
    memset(table, 0, sizeof(table));
    k_mutex_unlock(&table_mutex);
}

int ts_routing_table_update(uint16_t node_id, int16_t rssi, int8_t snr,
                            uint8_t ttl) {
    uint32_t now = (uint32_t)k_uptime_seconds();
    bool is_direct = (ttl == TS_ROUTING_DEFAULT_TTL);

    k_mutex_lock(&table_mutex, K_FOREVER);

    struct ts_neighbor* entry = find_by_node_id(node_id);
    if (entry != NULL) {
        entry->rssi = rssi;
        entry->snr = snr;
        entry->last_seen = now;
        // Never downgrade direct flag
        if (is_direct) { entry->direct = true; }
        k_mutex_unlock(&table_mutex);
        return 0;
    }

    entry = find_free_slot();
    if (entry == NULL) {
        entry = find_oldest();
        LOG_DBG("Evicting neighbor 0x%04x for 0x%04x", entry->node_id, node_id);
    }

    entry->node_id = node_id;
    entry->rssi = rssi;
    entry->snr = snr;
    entry->direct = is_direct;
    entry->last_seen = now;
    entry->occupied = true;

    k_mutex_unlock(&table_mutex);
    return 0;
}

int ts_routing_table_lookup(uint16_t node_id, struct ts_neighbor* p_neighbor) {
    k_mutex_lock(&table_mutex, K_FOREVER);
    struct ts_neighbor* entry = find_by_node_id(node_id);
    if (entry == NULL) {
        k_mutex_unlock(&table_mutex);
        return -ENOENT;
    }
    *p_neighbor = *entry;
    k_mutex_unlock(&table_mutex);
    return 0;
}

int ts_routing_table_age_seconds(uint32_t max_age_s) {
    uint32_t now = (uint32_t)k_uptime_seconds();
    int removed = 0;

    k_mutex_lock(&table_mutex, K_FOREVER);
    for (int i = 0; i < TS_ROUTING_TABLE_SIZE; i++) {
        if (table[i].occupied && (now - table[i].last_seen) >= max_age_s) {
            LOG_DBG("Aging out neighbor 0x%04x", table[i].node_id);
            table[i].occupied = false;
            removed++;
        }
    }
    k_mutex_unlock(&table_mutex);
    return removed;
}

uint32_t ts_routing_table_count(void) {
    uint32_t count = 0;
    k_mutex_lock(&table_mutex, K_FOREVER);
    for (int i = 0; i < TS_ROUTING_TABLE_SIZE; i++) {
        if (table[i].occupied) { count++; }
    }
    k_mutex_unlock(&table_mutex);
    return count;
}
