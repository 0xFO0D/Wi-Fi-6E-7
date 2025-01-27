#ifndef _FW_ATTEST_H_
#define _FW_ATTEST_H_

#include <linux/types.h>

/* Maximum size of attestation data */
#define MAX_ATTEST_DATA_SIZE 4096

/* Attestation challenge structure */
struct attest_challenge {
    u8 nonce[32];
    u64 timestamp;
};

/* Attestation response structure */
struct attest_response {
    u8 nonce[32];
    u64 timestamp;
    u8 pcr_values[96];  /* 3 PCRs, 32 bytes each */
    void *data;
    size_t data_len;
    u8 tag[16];
};

/* Attestation export structure */
struct attest_export {
    void *data;
    size_t data_len;
    u8 tag[16];
};

/* Attestation functions */
int fw_attest_init(void);
void fw_attest_exit(void);
int fw_attest_challenge(const u8 *session_id,
                       struct attest_challenge *challenge);
int fw_attest_verify(const u8 *session_id,
                    const struct attest_response *response);
int fw_attest_export(const u8 *session_id,
                    struct attest_export *export);

#endif /* _FW_ATTEST_H_ */ 