#ifndef TS_LORA_H
#define TS_LORA_H

#include <zephyr/drivers/lora.h>
#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include "lora/cbor.h"
#include "messages/messages.h"

#define LORA_OUT_THREAD_STACK_SIZE 1024
#define LORA_IN_THREAD_STACK_SIZE 1024

bool lora_config_ready_device(struct lora_modem_config* config);
int lora_out_task(void);
int lora_in_task(void);

#endif  // TS_LORA_H
