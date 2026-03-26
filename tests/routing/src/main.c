#include <zephyr/ztest.h>

#include "routing/routing.h"

#define TEST_NODE_ID 0x0001
#define OTHER_NODE_ID 0x0002

static void before_each(void *fixture)
{
    ARG_UNUSED(fixture);
    ts_routing_init(TEST_NODE_ID);
}

/* --- Node addressing --- */

ZTEST(routing, test_init_sets_node_id)
{
    zassert_equal(ts_routing_get_node_id(), TEST_NODE_ID,
                  "Node ID should match the value passed to init");
}

ZTEST(routing, test_prepare_header_sets_src)
{
    struct ts_route_header hdr;
    ts_routing_prepare_header(&hdr, TS_ROUTING_BROADCAST_ADDR);
    zassert_equal(hdr.src, TEST_NODE_ID,
                  "Source should be set to this node's ID");
}

ZTEST(routing, test_prepare_header_sets_dst)
{
    struct ts_route_header hdr;
    ts_routing_prepare_header(&hdr, OTHER_NODE_ID);
    zassert_equal(hdr.dst, OTHER_NODE_ID,
                  "Destination should match the requested target");
}

ZTEST(routing, test_prepare_header_sets_default_ttl)
{
    struct ts_route_header hdr;
    ts_routing_prepare_header(&hdr, TS_ROUTING_BROADCAST_ADDR);
    zassert_equal(hdr.ttl, TS_ROUTING_DEFAULT_TTL,
                  "TTL should be initialized to default value");
}

ZTEST(routing, test_prepare_header_unique_msg_ids)
{
    struct ts_route_header hdr1, hdr2;
    ts_routing_prepare_header(&hdr1, TS_ROUTING_BROADCAST_ADDR);
    ts_routing_prepare_header(&hdr2, TS_ROUTING_BROADCAST_ADDR);
    zassert_not_equal(hdr1.msg_id, hdr2.msg_id,
                      "Successive headers should have unique msg_ids");
}

ZTEST(routing, test_is_for_us_unicast_match)
{
    struct ts_route_header hdr = {.dst = TEST_NODE_ID};
    zassert_true(ts_routing_is_for_us(&hdr),
                 "Message addressed to our node should be for us");
}

ZTEST(routing, test_is_for_us_broadcast)
{
    struct ts_route_header hdr = {.dst = TS_ROUTING_BROADCAST_ADDR};
    zassert_true(ts_routing_is_for_us(&hdr),
                 "Broadcast message should be for us");
}

ZTEST(routing, test_is_for_us_other_node)
{
    struct ts_route_header hdr = {.dst = OTHER_NODE_ID};
    zassert_false(ts_routing_is_for_us(&hdr),
                  "Message for another node should not be for us");
}

/* --- TTL decrement --- */

ZTEST(routing, test_decrement_ttl_decreases_value)
{
    struct ts_route_header hdr = {.ttl = 3};
    ts_routing_decrement_ttl(&hdr);
    zassert_equal(hdr.ttl, 2, "TTL should decrease by 1");
}

ZTEST(routing, test_decrement_ttl_to_zero_returns_ok)
{
    struct ts_route_header hdr = {.ttl = 1};
    int ret = ts_routing_decrement_ttl(&hdr);
    zassert_ok(ret, "Decrementing TTL to zero should succeed");
    zassert_equal(hdr.ttl, 0, "TTL should be zero after decrement");
}

ZTEST(routing, test_decrement_ttl_at_zero_returns_error)
{
    struct ts_route_header hdr = {.ttl = 0};
    int ret = ts_routing_decrement_ttl(&hdr);
    zassert_not_equal(ret, 0, "Decrementing expired TTL should fail");
}

/* --- Duplicate detection --- */

ZTEST(routing, test_first_message_not_duplicate)
{
    struct ts_route_header hdr = {.src = OTHER_NODE_ID, .msg_id = 42};
    zassert_false(ts_routing_is_duplicate(&hdr),
                  "First occurrence of a message should not be duplicate");
}

ZTEST(routing, test_same_message_is_duplicate_after_mark)
{
    struct ts_route_header hdr = {.src = OTHER_NODE_ID, .msg_id = 42};
    ts_routing_mark_seen(&hdr);
    zassert_true(ts_routing_is_duplicate(&hdr),
                 "Message should be duplicate after being marked seen");
}

ZTEST(routing, test_different_msg_id_not_duplicate)
{
    struct ts_route_header hdr = {.src = OTHER_NODE_ID, .msg_id = 42};
    ts_routing_mark_seen(&hdr);
    hdr.msg_id = 43;
    zassert_false(ts_routing_is_duplicate(&hdr),
                  "Different msg_id should not be duplicate");
}

ZTEST(routing, test_different_src_not_duplicate)
{
    struct ts_route_header hdr = {.src = OTHER_NODE_ID, .msg_id = 42};
    ts_routing_mark_seen(&hdr);
    hdr.src = 0x0003;
    zassert_false(ts_routing_is_duplicate(&hdr),
                  "Same msg_id from different source should not be duplicate");
}

ZTEST(routing, test_cache_evicts_oldest_entries)
{
    struct ts_route_header hdr = {.src = OTHER_NODE_ID};

    // Fill the seen cache completely
    for (uint32_t i = 0; i < TS_ROUTING_SEEN_CACHE_SIZE; i++) {
        hdr.msg_id = i;
        ts_routing_mark_seen(&hdr);
    }

    // Add one more entry, which should evict the oldest (msg_id=0)
    hdr.msg_id = TS_ROUTING_SEEN_CACHE_SIZE;
    ts_routing_mark_seen(&hdr);

    // Oldest entry should have been evicted
    hdr.msg_id = 0;
    zassert_false(ts_routing_is_duplicate(&hdr),
                  "Oldest entry should be evicted from full cache");

    // Newest entry should still be tracked
    hdr.msg_id = TS_ROUTING_SEEN_CACHE_SIZE;
    zassert_true(ts_routing_is_duplicate(&hdr),
                 "Newest entry should still be in cache");
}

ZTEST_SUITE(routing, NULL, NULL, before_each, NULL, NULL);
