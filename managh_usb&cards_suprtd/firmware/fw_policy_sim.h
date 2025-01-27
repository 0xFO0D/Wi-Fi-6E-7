#ifndef _FW_POLICY_SIM_H_
#define _FW_POLICY_SIM_H_

#include "fw_keys.h"

/* Policy simulator functions */
int fw_policy_sim_init(void);
int fw_policy_sim_set_pcr(u32 pcr_index, const u8 *value);
int fw_policy_sim_get_pcr(u32 pcr_index, u8 *value);
int fw_policy_sim_extend_pcr(u32 pcr_index, const u8 *data, size_t len);
int fw_policy_sim_evaluate(const struct tpm_policy_info *policy,
                          u8 *policy_digest);
void fw_policy_sim_reset(void);

#endif /* _FW_POLICY_SIM_H_ */ 