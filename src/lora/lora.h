#ifndef LORA_H
#define LORA_H

#include <zcbor_encode.h>
#include <zephyr/drivers/lora.h>
#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include "messages/messages.h"

#define LORA_OUT_THREAD_STACK_SIZE 1024
#define ZBOR_ENCODE_BUFFER_SIZE 256

bool lora_config_ready_device(struct lora_modem_config* config);
int lora_out_task(void);
int cbor_serialize(struct ts_msg_lora_outgoing* msg, uint8_t* p_buf,
                   size_t buf_len, size_t* p_size);

#endif  // LORA_H
