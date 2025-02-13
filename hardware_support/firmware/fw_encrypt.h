#ifndef _FW_ENCRYPT_H_
#define _FW_ENCRYPT_H_

/* Firmware encryption functions */
int fw_encrypt_init(void);
int fw_encrypt_data(const void *data, size_t data_len,
                   void *out, size_t *out_len,
                   u8 *tag, size_t tag_len);
int fw_decrypt_data(const void *data, size_t data_len,
                   void *out, size_t *out_len,
                   const u8 *tag, size_t tag_len);

#endif /* _FW_ENCRYPT_H_ */ 