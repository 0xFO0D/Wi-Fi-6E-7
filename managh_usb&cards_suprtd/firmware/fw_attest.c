#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <crypto/hash.h>
#include <crypto/aead.h>
#include "fw_keys.h"
#include "fw_eventlog.h"
#include "fw_attest.h"

/* Attestation session structure */
struct attest_session {
    u8 session_id[16];
    u8 nonce[32];
    u8 key[32];
    u8 iv[16];
    struct crypto_aead *tfm;
    u64 timestamp;
    bool active;
};

#define MAX_SESSIONS 16
static struct attest_session sessions[MAX_SESSIONS];
static DEFINE_SPINLOCK(session_lock);

/* Initialize attestation service */
int fw_attest_init(void)
{
    memset(sessions, 0, sizeof(sessions));
    return 0;
}

/* Clean up attestation resources */
void fw_attest_exit(void)
{
    int i;
    
    for (i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].tfm)
            crypto_free_aead(sessions[i].tfm);
    }
}

/* Find or create session */
static struct attest_session *get_session(const u8 *session_id)
{
    int i;
    unsigned long flags;
    struct attest_session *session = NULL;

    spin_lock_irqsave(&session_lock, flags);

    /* Look for existing session */
    for (i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].active &&
            memcmp(sessions[i].session_id, session_id, 16) == 0) {
            session = &sessions[i];
            goto out;
        }
    }

    /* Find free slot */
    for (i = 0; i < MAX_SESSIONS; i++) {
        if (!sessions[i].active) {
            session = &sessions[i];
            memcpy(session->session_id, session_id, 16);
            session->active = true;
            goto out;
        }
    }

out:
    spin_unlock_irqrestore(&session_lock, flags);
    return session;
}

/* Generate new attestation challenge */
int fw_attest_challenge(const u8 *session_id,
                       struct attest_challenge *challenge)
{
    struct attest_session *session;
    struct crypto_aead *tfm;
    int ret;

    if (!session_id || !challenge)
        return -EINVAL;

    session = get_session(session_id);
    if (!session)
        return -ENOMEM;

    /* Generate random nonce */
    get_random_bytes(session->nonce, sizeof(session->nonce));
    memcpy(challenge->nonce, session->nonce, sizeof(challenge->nonce));

    /* Generate session key and IV */
    get_random_bytes(session->key, sizeof(session->key));
    get_random_bytes(session->iv, sizeof(session->iv));

    /* Initialize AEAD cipher */
    tfm = crypto_alloc_aead("gcm(aes)", 0, 0);
    if (IS_ERR(tfm))
        return PTR_ERR(tfm);

    ret = crypto_aead_setkey(tfm, session->key, sizeof(session->key));
    if (ret < 0) {
        crypto_free_aead(tfm);
        return ret;
    }

    ret = crypto_aead_setauthsize(tfm, 16);
    if (ret < 0) {
        crypto_free_aead(tfm);
        return ret;
    }

    if (session->tfm)
        crypto_free_aead(session->tfm);
    session->tfm = tfm;

    session->timestamp = ktime_get_real_seconds();
    challenge->timestamp = session->timestamp;

    return 0;
}

/* Verify attestation response */
int fw_attest_verify(const u8 *session_id,
                    const struct attest_response *response)
{
    struct attest_session *session;
    struct crypto_aead *tfm;
    struct scatterlist sg[3];
    struct aead_request *req;
    u8 *plaintext = NULL;
    int ret;

    if (!session_id || !response)
        return -EINVAL;

    session = get_session(session_id);
    if (!session || !session->active)
        return -EINVAL;

    /* Verify nonce */
    if (memcmp(response->nonce, session->nonce, sizeof(session->nonce)))
        return -EINVAL;

    /* Verify timestamp */
    if (response->timestamp != session->timestamp)
        return -EINVAL;

    /* Allocate buffer for decrypted data */
    plaintext = kmalloc(response->data_len, GFP_KERNEL);
    if (!plaintext)
        return -ENOMEM;

    tfm = session->tfm;
    req = aead_request_alloc(tfm, GFP_KERNEL);
    if (!req) {
        ret = -ENOMEM;
        goto out;
    }

    sg_init_table(sg, 3);
    sg_set_buf(&sg[0], session->iv, sizeof(session->iv));
    sg_set_buf(&sg[1], response->data, response->data_len);
    sg_set_buf(&sg[2], response->tag, 16);

    aead_request_set_crypt(req, sg, sg, response->data_len, session->iv);
    aead_request_set_ad(req, sizeof(session->iv));

    ret = crypto_aead_decrypt(req);
    aead_request_free(req);

    if (ret < 0)
        goto out;

    /* Verify PCR values */
    ret = fw_eventlog_validate_pcr(TPM_PCR_FIRMWARE_CODE,
                                 response->pcr_values);
    if (ret < 0)
        goto out;

    ret = fw_eventlog_validate_pcr(TPM_PCR_FIRMWARE_CONFIG,
                                 response->pcr_values + 32);
    if (ret < 0)
        goto out;

    ret = fw_eventlog_validate_pcr(TPM_PCR_KEYS,
                                 response->pcr_values + 64);

out:
    kfree(plaintext);
    return ret;
}

/* Export attestation data */
int fw_attest_export(const u8 *session_id,
                    struct attest_export *export)
{
    struct attest_session *session;
    struct crypto_aead *tfm;
    struct scatterlist sg[3];
    struct aead_request *req;
    int ret;

    if (!session_id || !export || !export->data || !export->data_len)
        return -EINVAL;

    session = get_session(session_id);
    if (!session || !session->active)
        return -EINVAL;

    tfm = session->tfm;
    req = aead_request_alloc(tfm, GFP_KERNEL);
    if (!req)
        return -ENOMEM;

    sg_init_table(sg, 3);
    sg_set_buf(&sg[0], session->iv, sizeof(session->iv));
    sg_set_buf(&sg[1], export->data, export->data_len);
    sg_set_buf(&sg[2], export->tag, 16);

    aead_request_set_crypt(req, sg, sg, export->data_len, session->iv);
    aead_request_set_ad(req, sizeof(session->iv));

    ret = crypto_aead_encrypt(req);
    aead_request_free(req);

    return ret;
}

EXPORT_SYMBOL_GPL(fw_attest_init);
EXPORT_SYMBOL_GPL(fw_attest_exit);
EXPORT_SYMBOL_GPL(fw_attest_challenge);
EXPORT_SYMBOL_GPL(fw_attest_verify);
EXPORT_SYMBOL_GPL(fw_attest_export); 