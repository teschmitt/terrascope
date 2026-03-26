#include <zephyr/ztest.h>

#include "routing/routing.h"
#include "routing/routing_table.h"

static void before_each(void *fixture)
{
    ARG_UNUSED(fixture);
    ts_routing_table_init();
}

/* --- Empty table --- */

ZTEST(routing_table, test_empty_table_count_is_zero)
{
    zassert_equal(ts_routing_table_count(), 0,
                  "Empty table should have count 0");
}

ZTEST(routing_table, test_lookup_empty_returns_enoent)
{
    struct ts_neighbor nb;
    int ret = ts_routing_table_lookup(0x0002, &nb);
    zassert_equal(ret, -ENOENT, "Lookup on empty table should return -ENOENT");
}

/* --- Update and lookup --- */

ZTEST(routing_table, test_update_and_lookup)
{
    ts_routing_table_update(0x0002, -75, 8, TS_ROUTING_DEFAULT_TTL);

    struct ts_neighbor nb;
    int ret = ts_routing_table_lookup(0x0002, &nb);
    zassert_ok(ret, "Lookup should succeed after update");
    zassert_equal(nb.node_id, 0x0002);
    zassert_equal(nb.rssi, -75);
    zassert_equal(nb.snr, 8);
}

ZTEST(routing_table, test_update_same_node_refreshes_rssi)
{
    ts_routing_table_update(0x0002, -75, 8, TS_ROUTING_DEFAULT_TTL);
    ts_routing_table_update(0x0002, -90, 3, TS_ROUTING_DEFAULT_TTL);

    struct ts_neighbor nb;
    ts_routing_table_lookup(0x0002, &nb);
    zassert_equal(nb.rssi, -90, "RSSI should be updated to latest value");
    zassert_equal(nb.snr, 3, "SNR should be updated to latest value");
}

/* --- Direct flag --- */

ZTEST(routing_table, test_direct_flag_full_ttl)
{
    ts_routing_table_update(0x0002, -75, 8, TS_ROUTING_DEFAULT_TTL);

    struct ts_neighbor nb;
    ts_routing_table_lookup(0x0002, &nb);
    zassert_true(nb.direct, "Full TTL should set direct=true");
}

ZTEST(routing_table, test_direct_flag_decremented_ttl)
{
    ts_routing_table_update(0x0002, -75, 8, TS_ROUTING_DEFAULT_TTL - 1);

    struct ts_neighbor nb;
    ts_routing_table_lookup(0x0002, &nb);
    zassert_false(nb.direct, "Decremented TTL should set direct=false");
}

ZTEST(routing_table, test_direct_flag_not_downgraded)
{
    ts_routing_table_update(0x0002, -75, 8, TS_ROUTING_DEFAULT_TTL);
    ts_routing_table_update(0x0002, -90, 3, TS_ROUTING_DEFAULT_TTL - 1);

    struct ts_neighbor nb;
    ts_routing_table_lookup(0x0002, &nb);
    zassert_true(nb.direct,
                 "Direct flag should not be downgraded by forwarded packet");
}

/* --- Count --- */

ZTEST(routing_table, test_count_after_inserts)
{
    ts_routing_table_update(0x0002, -75, 8, TS_ROUTING_DEFAULT_TTL);
    ts_routing_table_update(0x0003, -80, 6, TS_ROUTING_DEFAULT_TTL);
    ts_routing_table_update(0x0004, -90, 3, TS_ROUTING_DEFAULT_TTL);

    zassert_equal(ts_routing_table_count(), 3);
}

/* --- Eviction --- */

ZTEST(routing_table, test_table_full_evicts_oldest)
{
    // Fill table — first entry (node 0x0100) will be the oldest
    for (uint16_t i = 0; i < TS_ROUTING_TABLE_SIZE; i++) {
        ts_routing_table_update(0x0100 + i, -75, 8, TS_ROUTING_DEFAULT_TTL);
        k_sleep(K_MSEC(10));
    }

    // Add one more — should evict 0x0100 (oldest)
    ts_routing_table_update(0x0200, -60, 10, TS_ROUTING_DEFAULT_TTL);

    struct ts_neighbor nb;
    int ret = ts_routing_table_lookup(0x0100, &nb);
    zassert_equal(ret, -ENOENT, "Oldest entry should have been evicted");
}

ZTEST(routing_table, test_table_full_new_entry_present)
{
    for (uint16_t i = 0; i < TS_ROUTING_TABLE_SIZE; i++) {
        ts_routing_table_update(0x0100 + i, -75, 8, TS_ROUTING_DEFAULT_TTL);
        k_sleep(K_MSEC(10));
    }

    ts_routing_table_update(0x0200, -60, 10, TS_ROUTING_DEFAULT_TTL);

    struct ts_neighbor nb;
    int ret = ts_routing_table_lookup(0x0200, &nb);
    zassert_ok(ret, "New entry should be present after eviction");
    zassert_equal(nb.rssi, -60);
}

/* --- Aging --- */

ZTEST(routing_table, test_age_removes_stale)
{
    ts_routing_table_update(0x0002, -75, 8, TS_ROUTING_DEFAULT_TTL);
    k_sleep(K_SECONDS(2));

    ts_routing_table_age_seconds(1);

    struct ts_neighbor nb;
    int ret = ts_routing_table_lookup(0x0002, &nb);
    zassert_equal(ret, -ENOENT, "Stale entry should be removed");
}

ZTEST(routing_table, test_age_keeps_fresh)
{
    ts_routing_table_update(0x0002, -75, 8, TS_ROUTING_DEFAULT_TTL);

    ts_routing_table_age_seconds(300);

    struct ts_neighbor nb;
    int ret = ts_routing_table_lookup(0x0002, &nb);
    zassert_ok(ret, "Fresh entry should survive aging");
}

ZTEST(routing_table, test_age_returns_removal_count)
{
    ts_routing_table_update(0x0002, -75, 8, TS_ROUTING_DEFAULT_TTL);
    ts_routing_table_update(0x0003, -80, 6, TS_ROUTING_DEFAULT_TTL);
    ts_routing_table_update(0x0004, -90, 3, TS_ROUTING_DEFAULT_TTL);
    k_sleep(K_SECONDS(2));

    int removed = ts_routing_table_age_seconds(1);
    zassert_equal(removed, 3, "Should report 3 entries removed");
}

/* --- Clear --- */

ZTEST(routing_table, test_clear_resets_table)
{
    ts_routing_table_update(0x0002, -75, 8, TS_ROUTING_DEFAULT_TTL);
    ts_routing_table_update(0x0003, -80, 6, TS_ROUTING_DEFAULT_TTL);

    ts_routing_table_init();

    zassert_equal(ts_routing_table_count(), 0,
                  "Table should be empty after clear");
}

ZTEST_SUITE(routing_table, NULL, NULL, before_each, NULL, NULL);
