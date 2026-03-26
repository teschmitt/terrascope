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
    zassert_equal(ts_contention_rssi_to_delay_ms(-120), 0,
                  "Weakest RSSI should give 0 ms delay");
}

ZTEST(contention, test_rssi_strongest_gives_max_delay)
{
    zassert_equal(ts_contention_rssi_to_delay_ms(-30), 5000,
                  "Strongest RSSI should give 5000 ms delay");
}

ZTEST(contention, test_rssi_below_range_clamped)
{
    zassert_equal(ts_contention_rssi_to_delay_ms(-130), 0,
                  "RSSI below range should clamp to 0 ms");
}

ZTEST(contention, test_rssi_above_range_clamped)
{
    zassert_equal(ts_contention_rssi_to_delay_ms(-10), 5000,
                  "RSSI above range should clamp to 5000 ms");
}

ZTEST(contention, test_rssi_midpoint)
{
    // (-75 - (-120)) * 5000 / (-30 - (-120)) = 45 * 5000 / 90 = 2500
    zassert_equal(ts_contention_rssi_to_delay_ms(-75), 2500,
                  "Midpoint RSSI should give 2500 ms delay");
}

ZTEST(contention, test_rssi_monotonically_increasing)
{
    uint32_t prev = 0;
    for (int16_t rssi = -120; rssi <= -30; rssi++) {
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
        .route = {.src = src, .msg_id = msg_id, .dst = 0xFFFF, .ttl = 3},
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
