#include <zephyr/ztest.h>
#include <string.h>
#include "lora/auth.h"

#define TEST_PAYLOAD_SIZE 32

static uint8_t test_payload[TEST_PAYLOAD_SIZE];

static void *auth_suite_setup(void) {
    // Fill test payload with known pattern
    for (int i = 0; i < TEST_PAYLOAD_SIZE; i++) {
        test_payload[i] = (uint8_t)i;
    }
    int ret = ts_auth_init();
    zassert_ok(ret, "ts_auth_init should succeed");
    return NULL;
}

ZTEST(auth, test_sign_verify_roundtrip)
{
    uint8_t tag[TS_AUTH_TAG_SIZE];

    int ret = ts_auth_sign(test_payload, TEST_PAYLOAD_SIZE, tag);
    zassert_ok(ret, "sign should succeed");

    ret = ts_auth_verify(test_payload, TEST_PAYLOAD_SIZE, tag);
    zassert_ok(ret, "verify should accept valid tag");
}

ZTEST(auth, test_tampered_payload_rejected)
{
    uint8_t tag[TS_AUTH_TAG_SIZE];

    int ret = ts_auth_sign(test_payload, TEST_PAYLOAD_SIZE, tag);
    zassert_ok(ret, "sign should succeed");

    // Tamper with payload
    uint8_t tampered[TEST_PAYLOAD_SIZE];
    memcpy(tampered, test_payload, TEST_PAYLOAD_SIZE);
    tampered[0] ^= 0xFF;

    ret = ts_auth_verify(tampered, TEST_PAYLOAD_SIZE, tag);
    zassert_equal(ret, -EACCES, "tampered payload should be rejected");
}

ZTEST(auth, test_tampered_tag_rejected)
{
    uint8_t tag[TS_AUTH_TAG_SIZE];

    int ret = ts_auth_sign(test_payload, TEST_PAYLOAD_SIZE, tag);
    zassert_ok(ret, "sign should succeed");

    // Tamper with tag
    tag[0] ^= 0xFF;

    ret = ts_auth_verify(test_payload, TEST_PAYLOAD_SIZE, tag);
    zassert_equal(ret, -EACCES, "tampered tag should be rejected");
}

ZTEST(auth, test_truncated_tag_rejected)
{
    uint8_t tag[TS_AUTH_TAG_SIZE];

    int ret = ts_auth_sign(test_payload, TEST_PAYLOAD_SIZE, tag);
    zassert_ok(ret, "sign should succeed");

    // Verify always reads TS_AUTH_TAG_SIZE bytes, so simulate a
    // truncated tag by zeroing the last byte of a valid tag.
    uint8_t corrupted_tag[TS_AUTH_TAG_SIZE];
    memcpy(corrupted_tag, tag, TS_AUTH_TAG_SIZE);
    corrupted_tag[TS_AUTH_TAG_SIZE - 1] = 0x00;

    ret = ts_auth_verify(test_payload, TEST_PAYLOAD_SIZE, corrupted_tag);
    zassert_equal(ret, -EACCES,
                  "tag with wrong trailing byte should be rejected");
}

ZTEST(auth, test_zero_length_input)
{
    uint8_t tag[TS_AUTH_TAG_SIZE];

    // Sign with zero-length input should succeed (valid edge case for CMAC)
    int ret = ts_auth_sign(NULL, 0, tag);
    zassert_ok(ret, "sign with zero-length input should succeed");

    // Verify should accept the tag for zero-length input
    ret = ts_auth_verify(NULL, 0, tag);
    zassert_ok(ret, "verify with zero-length input should accept valid tag");
}

ZTEST(auth, test_sign_null_data_nonzero_len_rejected)
{
    uint8_t tag[TS_AUTH_TAG_SIZE];

    int ret = ts_auth_sign(NULL, 10, tag);
    zassert_equal(ret, -EINVAL,
                  "NULL data with nonzero length should return -EINVAL");
}

ZTEST(auth, test_verify_null_data_nonzero_len_rejected)
{
    uint8_t tag[TS_AUTH_TAG_SIZE] = {0};

    int ret = ts_auth_verify(NULL, 10, tag);
    zassert_equal(ret, -EINVAL,
                  "NULL data with nonzero length should return -EINVAL");
}

ZTEST_SUITE(auth, NULL, auth_suite_setup, NULL, NULL, NULL);
