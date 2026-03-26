#ifndef TS_LORA_H
#define TS_LORA_H

/**
 * @defgroup lora LoRa
 * @brief LoRa device initialization, configuration, and TX/RX threads.
 * @{
 */

#include <zephyr/drivers/lora.h>
#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include "lora/cbor.h"
#include "messages/messages.h"

#define LORA_OUT_THREAD_STACK_SIZE 1024
#define LORA_IN_THREAD_STACK_SIZE 1024

/**
 * @brief Populate a LoRa modem config with the project's radio parameters.
 *
 * @param config  Output modem config struct
 * @return true on success
 */
bool lora_config_ready_device(struct lora_modem_config* config);

/**
 * @brief LoRa transmit task entry point.
 *
 * Subscribes to ts_lora_out_chan, CBOR-encodes messages, and transmits.
 *
 * @return Does not return
 */
int lora_out_task(void);

/**
 * @brief LoRa receive task entry point.
 *
 * Polls the radio, CBOR-decodes received packets, applies flooding
 * logic (duplicate detection, contention forwarding), and delivers
 * locally addressed messages to ts_lora_in_chan.
 *
 * @return Does not return
 */
int lora_in_task(void);

/** @} */

#endif  // TS_LORA_H
