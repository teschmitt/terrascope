#include "lora/contention.h"

#include <errno.h>

#include <zephyr/zbus/zbus.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(contention);

extern struct zbus_channel ts_lora_out_chan;

static struct ts_contention_slot pool[TS_CONTENTION_POOL_SIZE];
static bool pool_initialized;

static void contention_work_handler(struct k_work *work);

static struct ts_contention_slot *find_free_slot(void) {
    for (int i = 0; i < TS_CONTENTION_POOL_SIZE; i++) {
        if (!pool[i].occupied) {
            return &pool[i];
        }
    }
    return NULL;
}

static struct ts_contention_slot *find_slot_by_msg(uint16_t src,
                                                    uint32_t msg_id) {
    for (int i = 0; i < TS_CONTENTION_POOL_SIZE; i++) {
        if (pool[i].occupied && pool[i].src == src &&
            pool[i].msg_id == msg_id) {
            return &pool[i];
        }
    }
    return NULL;
}

static void contention_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct ts_contention_slot *slot =
        CONTAINER_OF(dwork, struct ts_contention_slot, work);

    if (!slot->occupied) {
        return;
    }

    int ret = zbus_chan_pub(&ts_lora_out_chan, &slot->msg, K_MSEC(200));
    if (ret != 0) {
        LOG_ERR("Contention forward publish failed: %d", ret);
    } else {
        LOG_DBG("Forwarded msg_id=%u from 0x%04x, TTL=%u", slot->msg_id,
                slot->src, slot->msg.route.ttl);
    }

    slot->occupied = false;
}

void ts_contention_init(void) {
    for (int i = 0; i < TS_CONTENTION_POOL_SIZE; i++) {
        // Cancel any pending work before reinit (safe for test reuse)
        if (pool_initialized) {
            struct k_work_sync sync;
            k_work_cancel_delayable_sync(&pool[i].work, &sync);
        }
        k_work_init_delayable(&pool[i].work, contention_work_handler);
        pool[i].occupied = false;
    }
    pool_initialized = true;
}

uint32_t ts_contention_rssi_to_delay_ms(int16_t rssi) {
    if (rssi <= TS_CONTENTION_RSSI_WEAK) {
        return TS_CONTENTION_DELAY_MIN_MS;
    }
    if (rssi >= TS_CONTENTION_RSSI_STRONG) {
        return TS_CONTENTION_DELAY_MAX_MS;
    }

    int32_t offset = (int32_t)rssi - TS_CONTENTION_RSSI_WEAK;
    int32_t range = TS_CONTENTION_RSSI_STRONG - TS_CONTENTION_RSSI_WEAK;

    return (uint32_t)((offset * TS_CONTENTION_DELAY_MAX_MS) / range);
}

int ts_contention_schedule(const struct ts_msg_lora_outgoing *p_msg,
                           int16_t rssi) {
    struct ts_contention_slot *slot = find_free_slot();
    if (slot == NULL) {
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
    return 0;
}

int ts_contention_cancel(uint16_t src, uint32_t msg_id) {
    struct ts_contention_slot *slot = find_slot_by_msg(src, msg_id);
    if (slot == NULL) {
        return -ENOENT;
    }

    k_work_cancel_delayable(&slot->work);
    slot->occupied = false;

    LOG_DBG("Cancelled forward: msg_id=%u from 0x%04x", msg_id, src);
    return 0;
}
