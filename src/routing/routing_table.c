#include "routing/routing_table.h"

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>

#include "routing/routing.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(routing_table);

static struct ts_neighbor table[TS_ROUTING_TABLE_SIZE];

static struct ts_neighbor *find_by_node_id(uint16_t node_id) {
    for (int i = 0; i < TS_ROUTING_TABLE_SIZE; i++) {
        if (table[i].occupied && table[i].node_id == node_id) {
            return &table[i];
        }
    }
    return NULL;
}

static struct ts_neighbor *find_free_slot(void) {
    for (int i = 0; i < TS_ROUTING_TABLE_SIZE; i++) {
        if (!table[i].occupied) {
            return &table[i];
        }
    }
    return NULL;
}

static struct ts_neighbor *find_oldest(void) {
    struct ts_neighbor *oldest = NULL;
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
    memset(table, 0, sizeof(table));
}

int ts_routing_table_update(uint16_t node_id, int16_t rssi, int8_t snr,
                            uint8_t ttl) {
    uint32_t now = (uint32_t)k_uptime_seconds();
    bool is_direct = (ttl == TS_ROUTING_DEFAULT_TTL);

    struct ts_neighbor *entry = find_by_node_id(node_id);
    if (entry != NULL) {
        entry->rssi = rssi;
        entry->snr = snr;
        entry->last_seen = now;
        // Never downgrade direct flag
        if (is_direct) {
            entry->direct = true;
        }
        return 0;
    }

    entry = find_free_slot();
    if (entry == NULL) {
        entry = find_oldest();
        LOG_DBG("Evicting neighbor 0x%04x for 0x%04x", entry->node_id,
                node_id);
    }

    entry->node_id = node_id;
    entry->rssi = rssi;
    entry->snr = snr;
    entry->direct = is_direct;
    entry->last_seen = now;
    entry->occupied = true;

    return 0;
}

int ts_routing_table_lookup(uint16_t node_id,
                            struct ts_neighbor *p_neighbor) {
    struct ts_neighbor *entry = find_by_node_id(node_id);
    if (entry == NULL) {
        return -ENOENT;
    }
    *p_neighbor = *entry;
    return 0;
}

int ts_routing_table_age_seconds(uint32_t max_age_s) {
    uint32_t now = (uint32_t)k_uptime_seconds();
    int removed = 0;

    for (int i = 0; i < TS_ROUTING_TABLE_SIZE; i++) {
        if (table[i].occupied &&
            (now - table[i].last_seen) >= max_age_s) {
            LOG_DBG("Aging out neighbor 0x%04x", table[i].node_id);
            table[i].occupied = false;
            removed++;
        }
    }
    return removed;
}

uint32_t ts_routing_table_count(void) {
    uint32_t count = 0;
    for (int i = 0; i < TS_ROUTING_TABLE_SIZE; i++) {
        if (table[i].occupied) {
            count++;
        }
    }
    return count;
}
