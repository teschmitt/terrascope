#include <zephyr/zbus/zbus.h>
#include <zephyr/ztest.h>

#include "lora/contention.h"
#include "messages/messages.h"

// Test-local zbus channel required by contention work handler
ZBUS_CHAN_DEFINE(ts_lora_out_chan, struct ts_msg_lora_outgoing, NULL, NULL,
                 ZBUS_OBSERVERS_EMPTY, ZBUS_MSG_INIT(0));

static void before_each(void *fixture)
{
    ARG_UNUSED(fixture);
    ts_contention_init();
}

/* --- RSSI-to-delay mapping --- */

ZTEST(contention, test_rssi_weakest_gives_zero_delay)
{
    zassert_equal(ts_contention_rssi_to_delay_ms(TS_CONTENTION_RSSI_WEAK),
                  TS_CONTENTION_DELAY_MIN_MS,
                  "Weakest RSSI should give min delay");
}

ZTEST(contention, test_rssi_strongest_gives_max_delay)
{
    zassert_equal(ts_contention_rssi_to_delay_ms(TS_CONTENTION_RSSI_STRONG),
                  TS_CONTENTION_DELAY_MAX_MS,
                  "Strongest RSSI should give max delay");
}

ZTEST(contention, test_rssi_below_range_clamped)
{
    zassert_equal(
        ts_contention_rssi_to_delay_ms(TS_CONTENTION_RSSI_WEAK - 10),
        TS_CONTENTION_DELAY_MIN_MS,
        "RSSI below range should clamp to min delay");
}

ZTEST(contention, test_rssi_above_range_clamped)
{
    zassert_equal(
        ts_contention_rssi_to_delay_ms(TS_CONTENTION_RSSI_STRONG + 20),
        TS_CONTENTION_DELAY_MAX_MS,
        "RSSI above range should clamp to max delay");
}

ZTEST(contention, test_rssi_midpoint)
{
    int16_t mid = (TS_CONTENTION_RSSI_WEAK + TS_CONTENTION_RSSI_STRONG) / 2;
    uint32_t expected =
        (uint32_t)((mid - TS_CONTENTION_RSSI_WEAK) *
                    TS_CONTENTION_DELAY_MAX_MS) /
        (TS_CONTENTION_RSSI_STRONG - TS_CONTENTION_RSSI_WEAK);
    zassert_equal(ts_contention_rssi_to_delay_ms(mid), expected,
                  "Midpoint RSSI should give midpoint delay");
}

ZTEST(contention, test_rssi_monotonically_increasing)
{
    uint32_t prev = 0;
    for (int16_t rssi = TS_CONTENTION_RSSI_WEAK;
         rssi <= TS_CONTENTION_RSSI_STRONG; rssi++) {
        uint32_t delay = ts_contention_rssi_to_delay_ms(rssi);
        zassert_true(delay >= prev,
                     "Delay must be monotonically increasing with RSSI");
        prev = delay;
    }
}

/* --- Pool management --- */

static struct ts_msg_lora_outgoing make_msg(uint16_t src, uint32_t msg_id)
{
    struct ts_msg_lora_outgoing msg = {
        .route = {.src = src,
                  .msg_id = msg_id,
                  .dst = TS_ROUTING_BROADCAST_ADDR,
                  .ttl = 3},
        .type = TS_MSG_TELEMETRY,
    };
    return msg;
}

ZTEST(contention, test_schedule_returns_success)
{
    struct ts_msg_lora_outgoing msg = make_msg(0x0002, 1);
    int ret = ts_contention_schedule(&msg, -75);
    zassert_ok(ret, "Schedule into empty pool should succeed");
}

ZTEST(contention, test_schedule_pool_exhaustion)
{
    for (uint32_t i = 0; i < TS_CONTENTION_POOL_SIZE; i++) {
        struct ts_msg_lora_outgoing msg = make_msg(0x0002, i);
        int ret = ts_contention_schedule(&msg, -75);
        zassert_ok(ret, "Schedule should succeed for slot %u", i);
    }

    struct ts_msg_lora_outgoing msg = make_msg(0x0002, 99);
    int ret = ts_contention_schedule(&msg, -75);
    zassert_equal(ret, -ENOMEM, "Schedule into full pool should fail");
}

ZTEST(contention, test_cancel_pending_forward)
{
    struct ts_msg_lora_outgoing msg = make_msg(0x0002, 42);
    ts_contention_schedule(&msg, -75);

    int ret = ts_contention_cancel(0x0002, 42);
    zassert_ok(ret, "Cancel should find and remove the pending forward");

    // Slot should now be free — scheduling should succeed for all slots
    for (uint32_t i = 0; i < TS_CONTENTION_POOL_SIZE; i++) {
        struct ts_msg_lora_outgoing m = make_msg(0x0003, i);
        ret = ts_contention_schedule(&m, -75);
        zassert_ok(ret, "Slot should be available after cancel");
    }
}

ZTEST(contention, test_cancel_nonexistent_returns_enoent)
{
    int ret = ts_contention_cancel(0x0002, 99);
    zassert_equal(ret, -ENOENT,
                  "Cancel of nonexistent forward should return -ENOENT");
}

ZTEST_SUITE(contention, NULL, NULL, before_each, NULL, NULL);
