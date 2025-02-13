#ifndef _FW_ROLLBACK_H_
#define _FW_ROLLBACK_H_

/* Rollback protection functions */
int fw_rollback_init(void);
int fw_rollback_get_version(u64 *version);
int fw_rollback_increment(void);
int fw_rollback_verify(u64 fw_version);

#endif /* _FW_ROLLBACK_H_ */ 