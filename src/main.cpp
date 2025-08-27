#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/drivers/lora.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <zephyr/sys/mpsc_pbuf.h>
#include <zephyr/sys/util.h>
#include "lora/LoraDevice.h"

#define DEFAULT_RADIO_NODE DT_ALIAS(lora0)

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#define BUF_SIZE 30
#define PRODUCER_DELAY K_MSEC(500)
#define CONSUMER_START_DELAY_MS 3000

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

BUILD_ASSERT(DT_NODE_HAS_STATUS_OKAY(DEFAULT_RADIO_NODE),
             "No default LoRa radio specified in DT");

K_SEM_DEFINE(pkt_sem, 0, K_SEM_MAX_LIMIT);
K_SEM_DEFINE(init_sem, 0, 1);

typedef enum { TS_PACKET_CONTROL, TS_PACKET_TELEMETRY } ts_packet_type_t;
#define LEN_TYPE 3

typedef enum {
    TS_CONTROL_MSG_CLOCK,
    TS_CONTROL_MSG_SENSE,
    TS_CONTROL_MSG_RESTART
} ts_control_msg_type_t;

struct ts_packet_header {
    MPSC_PBUF_HDR;
    ts_packet_type_t packet_type : LEN_TYPE;
    uint32_t length : 32 - MPSC_PBUF_HDR_BITS - LEN_TYPE;
};

struct ts_packet_telemetry {
    struct ts_packet_header hdr;
    uint32_t timestamp;
    uint16_t temperature;
    uint16_t humidity;
};

struct ts_packet_control {
    ts_control_msg_type_t msg_type;
    uint8_t data[];
};

static uint32_t buffer[BUF_SIZE];
static struct mpsc_pbuf_buffer pbuf;

void log_telemetry(struct ts_packet_telemetry* pkt, const char* prefix) {
    LOG_INF(
        "%s Header: busy=%d, valid=%d, length=%d | Telemetry: ts=%u, "
        "temp=%d.%02d°C, humidity=%d.%02d%%",
        prefix, pkt->hdr.busy, pkt->hdr.valid, pkt->hdr.length, pkt->timestamp,
        pkt->temperature / 100, pkt->temperature % 100, pkt->humidity / 100,
        pkt->humidity % 100);
}

static uint32_t get_packet_wlen(const union mpsc_pbuf_generic* packet) {
    struct ts_packet_telemetry* pkt = (struct ts_packet_telemetry*)packet;
    return pkt->hdr.length;
}

void producer_thread(void* arg1, void* arg2, void* arg3) {
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    k_sem_take(&init_sem, K_FOREVER);
    LOG_INF("Producer thread started");

    size_t wlen =
        DIV_ROUND_UP(sizeof(struct ts_packet_telemetry), sizeof(uint32_t));

    while (1) {
        struct ts_packet_telemetry* buf_packet =
            (struct ts_packet_telemetry*)mpsc_pbuf_alloc(&pbuf, wlen,
                                                         K_NO_WAIT);

        if (!buf_packet) {
            LOG_WRN("Producer: Buffer allocation failed, retrying...");
            k_sleep(PRODUCER_DELAY);
            continue;
        }

        buf_packet->hdr.length = wlen;
        buf_packet->timestamp = sys_rand32_get();
        buf_packet->temperature = sys_rand32_get() % 5000;
        buf_packet->humidity = sys_rand32_get() % 10000;

        log_telemetry(buf_packet, "PRODUCED:");
        mpsc_pbuf_commit(&pbuf, (union mpsc_pbuf_generic*)buf_packet);

        k_sem_give(&pkt_sem);

        k_sleep(PRODUCER_DELAY);
    }
}

void consumer_thread(void* arg1, void* arg2, void* arg3) {
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    LOG_INF("Consumer thread started");

    while (1) {
        // Wait for packet availability signal
        k_sem_take(&pkt_sem, K_FOREVER);

        // Claim packet from buffer
        const union mpsc_pbuf_generic* item = mpsc_pbuf_claim(&pbuf);
        if (item) {
            struct ts_packet_telemetry* consumed =
                (struct ts_packet_telemetry*)item;

            // Log the consumed packet
            log_telemetry(consumed, "CONSUMED:");

            // Free the packet
            mpsc_pbuf_free(&pbuf, item);
        } else {
            LOG_WRN("Consumer: No packet available despite semaphore signal");
        }
    }
}

// Define threads with appropriate priorities and stack sizes
K_THREAD_DEFINE(producer_tid, 1024, producer_thread, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(consumer_tid, 1024, consumer_thread, NULL, NULL, NULL, 6, 0,
                CONSUMER_START_DELAY_MS);

int main(void) {
    LOG_INF("Starting TerraSCOPE...");

    const struct mpsc_pbuf_buffer_config config = {
        .buf = buffer,
        .size = ARRAY_SIZE(buffer),
        .notify_drop = NULL,
        .get_wlen = get_packet_wlen,
        .flags = MPSC_PBUF_MODE_OVERWRITE};

    // Initialize buffer
    memset(buffer, 0, sizeof(buffer));
    mpsc_pbuf_init(&pbuf, &config);

    LOG_INF("MPSC Packet Buffer multi-threaded example started");
    LOG_INF(
        "Producer will start immediately, Consumer will start after 3 seconds");

    k_sem_give(&init_sem);

    while (1) {
        k_sleep(K_SECONDS(5));
        LOG_INF("Main thread heartbeat");
    }

    return 0;

    return 0;
}