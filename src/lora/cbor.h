#ifndef TS_CBOR_H
#define TS_CBOR_H

#include <stddef.h>
#include <stdint.h>
#include "messages/messages.h"

#define ZBOR_ENCODE_BUFFER_SIZE 256

int cbor_serialize(struct ts_msg_lora_outgoing* msg, uint8_t* p_buf,
                   size_t buf_len, size_t* p_size);

#endif  // TS_CBOR_H
