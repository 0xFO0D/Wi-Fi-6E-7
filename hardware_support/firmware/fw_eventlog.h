#ifndef _FW_EVENTLOG_H_
#define _FW_EVENTLOG_H_

#include <linux/types.h>

/* Maximum size of event data that can be exported */
#define MAX_EVENT_DATA_SIZE 1024

/* Event log statistics */
struct eventlog_stats {
    u32 event_count;
    u64 last_update;
};

/* Exported event entry */
struct event_entry_export {
    u32 pcr_index;
    u32 event_type;
    u8 digest[32];
    u64 timestamp;
    u32 data_len;
    u8 data[MAX_EVENT_DATA_SIZE];
};

/* Event export structure */
struct event_export {
    struct event_entry_export *events;
    u32 count;
};

/* Event log functions */
int fw_eventlog_init(void);
void fw_eventlog_exit(void);
int fw_eventlog_update(void);
int fw_eventlog_validate_pcr(u32 pcr_index, const u8 *pcr_value);
int fw_eventlog_get_stats(struct eventlog_stats *stats);
int fw_eventlog_export(struct event_export *export,
                      u32 start_idx, u32 count);

#endif /* _FW_EVENTLOG_H_ */ 