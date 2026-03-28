#include "lora.h"

#include "lora/auth.h"
#include "lora/contention.h"
#include "routing/routing.h"
#include "routing/routing_table.h"

#include <zephyr/logging/log.h>

#define LORA_CHAN_OUT_READ_TIMEOUT K_MSEC(1)
#define LORA_RECV_TIMEOUT K_MSEC(1000)
#define LORA_CHAN_IN_PUB_TIMEOUT K_MSEC(200)
// lora_recv size parameter is uint8_t, so RX buffer is capped at UINT8_MAX
#define LORA_RX_BUFFER_SIZE UINT8_MAX
BUILD_ASSERT(LORA_RX_BUFFER_SIZE <= UINT8_MAX,
             "LORA_RX_BUFFER_SIZE exceeds lora_recv uint8_t size parameter");
BUILD_ASSERT(ZBOR_ENCODE_BUFFER_SIZE > TS_AUTH_TAG_SIZE,
             "CBOR buffer must be larger than auth tag to hold any payload");

LOG_MODULE_REGISTER(lora);

ZBUS_SUBSCRIBER_DEFINE(ts_lora_out_sub, 2);
extern struct zbus_channel ts_lora_out_chan;
extern struct zbus_channel ts_lora_in_chan;

K_THREAD_DEFINE(lora_out_tid, LORA_OUT_THREAD_STACK_SIZE, lora_out_task, NULL,
                NULL, NULL, 3, 0, 0);
K_THREAD_DEFINE(lora_in_tid, LORA_IN_THREAD_STACK_SIZE, lora_in_task, NULL,
                NULL, NULL, 3, 0, 0);

static const struct device* lora_dev;
// Semaphore: replaces the plain bool lora_config_done flag.  SYS_INIT
// gives the semaphore after configuring the radio; both TX and RX
// threads take-then-regive it to wait without polling and with a
// proper memory barrier so all preceding config writes are visible.
static K_SEM_DEFINE(lora_ready_sem, 0, 1);
static uint8_t cbor_buffer[ZBOR_ENCODE_BUFFER_SIZE];

// Initialize the LoRa device reference
static int lora_init(void) {
    lora_dev = DEVICE_DT_GET(DT_ALIAS(lora0));
    if (!device_is_ready(lora_dev)) {
        LOG_ERR("LoRa device not ready");
        return -ENODEV;
    }

    // Configure the device
    struct lora_modem_config config;
    lora_config_ready_device(&config);

    if (lora_config(lora_dev, &config) < 0) {
        LOG_ERR("LoRa config failed");
        return -EIO;
    }

    return 0;
}

SYS_INIT(lora_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

bool lora_config_ready_device(struct lora_modem_config* config) {
    config->frequency = 865100000;
    config->bandwidth = BW_125_KHZ;
    config->datarate = SF_10;
    config->coding_rate = CR_4_5;
    config->iq_inverted = false;
    config->public_network = false;
    config->tx_power = 4;
    config->tx = true;
    k_sem_give(&lora_ready_sem);
    return true;
}

int lora_out_task() {
    const struct zbus_channel* chan;

    // Block until SYS_INIT has configured the radio and signalled
    // readiness.  The semaphore acts as a memory barrier so all
    // config writes made before k_sem_give are visible here.
    k_sem_take(&lora_ready_sem, K_FOREVER);
    // Re-give so the other thread (RX) can also proceed.
    k_sem_give(&lora_ready_sem);

    LOG_INF("LoRa output task started");

    while (true) {
        int ret = zbus_sub_wait(&ts_lora_out_sub, &chan, K_FOREVER);
        if (ret != 0) {
            LOG_ERR("zbus_sub_wait failed: %d", ret);
            continue;
        }

        if (chan == &ts_lora_out_chan) {
            struct ts_msg_lora_outgoing msg = {0};

            ret = zbus_chan_read(&ts_lora_out_chan, &msg,
                                 LORA_CHAN_OUT_READ_TIMEOUT);
            if (ret != 0) {
                LOG_ERR("Failed to read from channel: %d", ret);
                continue;
            }

            LOG_DBG("Processing message type: %d", msg.type);
            // Stamp key version here (not at publish site) so producers
            // don't need to know about the auth module.
            msg.route.key_id = ts_auth_get_key_id();

            // Reserve tail room for the auth tag that will be appended
            // after the CBOR payload in the same contiguous buffer.
            size_t cbor_size = 0;
            ret = cbor_serialize(&msg, cbor_buffer,
                                sizeof(cbor_buffer) - TS_AUTH_TAG_SIZE,
                                &cbor_size);
            if (ret != 0) {
                LOG_ERR("CBOR serialization failed: %d", ret);
                continue;
            }

            ret = ts_auth_sign(cbor_buffer, cbor_size,
                               cbor_buffer + cbor_size);
            if (ret != 0) {
                LOG_ERR("Auth sign failed: %d", ret);
                continue;
            }

            size_t total_size = cbor_size + TS_AUTH_TAG_SIZE;
            LOG_HEXDUMP_DBG(cbor_buffer, total_size, "TX payload: ");

            ret = lora_send(lora_dev, cbor_buffer, (uint32_t)total_size);
            if (ret < 0) {
                LOG_ERR("LoRa send failed: %d", ret);
                continue;
            }

            LOG_DBG("Message sent successfully");
        } else {
            LOG_WRN("Received message on unexpected channel");
        }
    }
    return 0;  // unreachable!
}

int lora_in_task() {
    // Separate buffer from the output task to avoid contention between
    // the TX and RX threads without needing a mutex
    static uint8_t rx_buffer[LORA_RX_BUFFER_SIZE];

    // Block until SYS_INIT has configured the radio.
    k_sem_take(&lora_ready_sem, K_FOREVER);
    k_sem_give(&lora_ready_sem);

    ts_contention_init();
    LOG_INF("LoRa receive task started");

    while (true) {
        int16_t rssi;
        int8_t snr;

        // lora_recv returns positive byte count on success, negative on error
        int len = lora_recv(lora_dev, rx_buffer, (uint8_t)sizeof(rx_buffer),
                            LORA_RECV_TIMEOUT, &rssi, &snr);
        if (len < 0) {
            if (len == -EAGAIN) {
                // Timeout with no data is normal operation
                continue;
            }
            LOG_ERR("LoRa recv failed: %d", len);
            continue;
        }

        LOG_INF("LoRa RX: %d bytes, RSSI=%d, SNR=%d", len, rssi, snr);
        LOG_HEXDUMP_DBG(rx_buffer, len, "LoRa RX raw: ");

        // Verify auth before CBOR decode so unauthenticated packets
        // never reach the parser — limits attack surface to the tag
        // check alone.  Wire format: [CBOR payload | 8-byte CMAC tag].
        if (len <= TS_AUTH_TAG_SIZE) {
            LOG_WRN("Packet too short for auth tag (%d bytes)", len);
            continue;
        }

        size_t cbor_len = (size_t)len - TS_AUTH_TAG_SIZE;
        int ret = ts_auth_verify(rx_buffer, cbor_len,
                                 rx_buffer + cbor_len);
        if (ret != 0) {
            LOG_WRN("Auth verification failed, dropping packet");
            continue;
        }

        struct ts_msg_lora_incoming in_msg = {0};
        in_msg.rssi = rssi;
        in_msg.snr = snr;

        ret = cbor_deserialize(rx_buffer, cbor_len, &in_msg.msg);
        if (ret != 0) {
            LOG_ERR("CBOR deserialization failed: %d", ret);
            continue;
        }

        // key_id is checked after deserialize because it lives inside
        // the CBOR-encoded route header.  This is safe: the CMAC already
        // proved the packet is authentic, so a mismatched key_id just
        // means the sender is on a different key rotation epoch.
        if (in_msg.msg.route.key_id != ts_auth_get_key_id()) {
            LOG_WRN("Key ID mismatch: got %u, expected %u",
                    in_msg.msg.route.key_id, ts_auth_get_key_id());
            continue;
        }

        // Flooding: drop own messages that returned via other nodes
        if (in_msg.msg.route.src == ts_routing_get_node_id()) {
            continue;
        }

        // Flooding: drop duplicates and cancel any pending contention forward
        if (ts_routing_is_duplicate(&in_msg.msg.route)) {
            LOG_DBG("Dropping duplicate msg_id=%u from 0x%04x",
                    in_msg.msg.route.msg_id, in_msg.msg.route.src);
            ts_contention_cancel(in_msg.msg.route.src,
                                 in_msg.msg.route.msg_id);
            continue;
        }
        ts_routing_mark_seen(&in_msg.msg.route);
        ts_routing_table_update(in_msg.msg.route.src, rssi, snr,
                                in_msg.msg.route.ttl);

        // Deliver locally if addressed to this node or broadcast
        if (ts_routing_is_for_us(&in_msg.msg.route)) {
            ret = zbus_chan_pub(&ts_lora_in_chan, &in_msg,
                               LORA_CHAN_IN_PUB_TIMEOUT);
            if (ret != 0) {
                LOG_ERR("Failed to publish incoming message: %d", ret);
            }
        }

        // Contention-based rebroadcast: delay based on RSSI
        struct ts_msg_lora_outgoing fwd = in_msg.msg;
        if (ts_routing_decrement_ttl(&fwd.route) == 0 && fwd.route.ttl > 0) {
            ret = ts_contention_schedule(&fwd, rssi);
            if (ret != 0) {
                LOG_ERR("Failed to schedule contention forward: %d", ret);
            }
        }
    }
    return 0;  // unreachable!
}
