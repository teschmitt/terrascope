#ifndef TS_CBOR_H
#define TS_CBOR_H

/**
 * @defgroup cbor CBOR
 * @brief CBOR serialization and deserialization for mesh messages.
 * @{
 */

#include <stddef.h>
#include <stdint.h>
#include "messages/messages.h"

/** @brief Maximum buffer size for CBOR encoding. */
#define ZBOR_ENCODE_BUFFER_SIZE 256

/**
 * @brief Serialize a message to CBOR binary format.
 *
 * Encodes the route header, message type, and payload data.
 *
 * @param msg      Message to serialize
 * @param p_buf    Output buffer
 * @param buf_len  Size of the output buffer
 * @param p_size   Output: number of bytes written
 * @return 0 on success, -ENOMEM if buffer too small, -EINVAL if unknown type
 */
int cbor_serialize(struct ts_msg_lora_outgoing* msg, uint8_t* p_buf,
                   size_t buf_len, size_t* p_size);

/**
 * @brief Deserialize CBOR binary data into a message.
 *
 * Decodes the route header, message type, and payload data.
 *
 * @param p_buf    Input CBOR buffer
 * @param buf_len  Length of the input buffer
 * @param p_msg    Output message struct
 * @return 0 on success, -EINVAL if null/empty, -EBADMSG if malformed
 */
int cbor_deserialize(const uint8_t* p_buf, size_t buf_len,
                     struct ts_msg_lora_outgoing* p_msg);

/** @} */

#endif  // TS_CBOR_H
