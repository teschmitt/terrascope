#include "lora/contention.h"

#include <errno.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

LOG_MODULE_REGISTER(contention);

extern struct zbus_channel ts_lora_out_chan;

// Mutex: pool slots are accessed from the lora_in_task thread
// (schedule/cancel) and from the system work queue
// (contention_work_handler).  The handler uses a copy-then-release
// pattern so the lock is only held for the memcpy + flag flip, never
// across the blocking zbus publish.
static K_MUTEX_DEFINE(pool_mutex);
static struct ts_contention_slot pool[TS_CONTENTION_POOL_SIZE];
static bool pool_initialized;

static struct ts_contention_slot* find_free_slot(void) {
    for (int i = 0; i < TS_CONTENTION_POOL_SIZE; i++) {
        if (!pool[i].occupied) { return &pool[i]; }
    }
    return NULL;
}

static struct ts_contention_slot* find_slot_by_msg(uint16_t src,
                                                   uint32_t msg_id) {
    for (int i = 0; i < TS_CONTENTION_POOL_SIZE; i++) {
        if (pool[i].occupied && pool[i].src == src &&
            pool[i].msg_id == msg_id) {
            return &pool[i];
        }
    }
    return NULL;
}

void ts_contention_work_handler(struct k_work* work) {
    struct k_work_delayable* dwork = k_work_delayable_from_work(work);
    struct ts_contention_slot* slot =
        CONTAINER_OF(dwork, struct ts_contention_slot, work);

    // Copy-then-release: take the message out of the slot under the
    // lock and immediately free it.  The zbus publish that follows can
    // block for up to 200 ms, and holding the mutex across that would
    // stall schedule/cancel calls on the RX thread.
    struct ts_msg_lora_outgoing msg_copy;
    uint32_t msg_id;
    uint16_t src;

    k_mutex_lock(&pool_mutex, K_FOREVER);
    if (!slot->occupied) {
        k_mutex_unlock(&pool_mutex);
        return;
    }
    msg_copy = slot->msg;
    msg_id = slot->msg_id;
    src = slot->src;
    slot->occupied = false;
    k_mutex_unlock(&pool_mutex);

    int ret = zbus_chan_pub(&ts_lora_out_chan, &msg_copy, K_MSEC(200));
    if (ret != 0) {
        LOG_ERR("Contention forward publish failed: %d", ret);
    } else {
        LOG_DBG("Forwarded msg_id=%u from 0x%04x, TTL=%u", msg_id, src,
                msg_copy.route.ttl);
    }
}

void ts_contention_init(void) {
    k_mutex_lock(&pool_mutex, K_FOREVER);
    for (int i = 0; i < TS_CONTENTION_POOL_SIZE; i++) {
        // Cancel any pending work before reinit (safe for test reuse)
        if (pool_initialized) {
            struct k_work_sync sync;
            k_work_cancel_delayable_sync(&pool[i].work, &sync);
        }
        k_work_init_delayable(&pool[i].work, ts_contention_work_handler);
        pool[i].occupied = false;
    }
    pool_initialized = true;
    k_mutex_unlock(&pool_mutex);
}

uint32_t ts_contention_rssi_to_delay_ms(int16_t rssi) {
    if (rssi <= TS_CONTENTION_RSSI_WEAK) {
        return TS_CONTENTION_DELAY_MIN_MS;
    } else if (rssi >= TS_CONTENTION_RSSI_STRONG) {
        return TS_CONTENTION_DELAY_MAX_MS;
    }

    int32_t offset = (int32_t)rssi - TS_CONTENTION_RSSI_WEAK;
    int32_t range = TS_CONTENTION_RSSI_STRONG - TS_CONTENTION_RSSI_WEAK;

    return (uint32_t)((offset * TS_CONTENTION_DELAY_MAX_MS) / range);
}

int ts_contention_schedule(const struct ts_msg_lora_outgoing* p_msg,
                           int16_t rssi) {
    k_mutex_lock(&pool_mutex, K_FOREVER);
    struct ts_contention_slot* slot = find_free_slot();
    if (slot == NULL) {
        k_mutex_unlock(&pool_mutex);
        LOG_WRN("Contention pool full, dropping forward for msg_id=%u",
                p_msg->route.msg_id);
        return -ENOMEM;
    }

    slot->msg = *p_msg;
    slot->src = p_msg->route.src;
    slot->msg_id = p_msg->route.msg_id;
    slot->occupied = true;

    uint32_t delay_ms = ts_contention_rssi_to_delay_ms(rssi);
    LOG_DBG("Scheduling forward: msg_id=%u from 0x%04x, delay=%u ms",
            slot->msg_id, slot->src, delay_ms);

    k_work_schedule(&slot->work, K_MSEC(delay_ms));
    k_mutex_unlock(&pool_mutex);
    return 0;
}

int ts_contention_cancel(uint16_t src, uint32_t msg_id) {
    k_mutex_lock(&pool_mutex, K_FOREVER);
    struct ts_contention_slot* slot = find_slot_by_msg(src, msg_id);
    if (slot == NULL) {
        k_mutex_unlock(&pool_mutex);
        return -ENOENT;
    }

    // No need for k_work_cancel_delayable_sync() here: if the handler
    // has already started executing, it will have claimed the msg and
    // cleared occupied under the lock before we acquired it, so
    // find_slot_by_msg would have returned NULL above.  If the work is
    // still pending (not yet started), plain cancel is sufficient.
    k_work_cancel_delayable(&slot->work);
    slot->occupied = false;
    k_mutex_unlock(&pool_mutex);

    LOG_DBG("Cancelled forward: msg_id=%u from 0x%04x", msg_id, src);
    return 0;
}
