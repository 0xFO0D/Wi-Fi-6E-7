#ifndef _WIFI67_OPS_H_
#define _WIFI67_OPS_H_

#include <linux/ieee80211.h>

const struct ieee80211_ops wifi67_ops = {
    .tx = wifi67_tx,
    .start = wifi67_start,
    .stop = wifi67_stop,
    .config = wifi67_config,
    .add_interface = wifi67_add_interface,
    .remove_interface = wifi67_remove_interface,
    .configure_filter = wifi67_configure_filter,
    .bss_info_changed = wifi67_bss_info_changed,
    .conf_tx = wifi67_conf_tx,
    .sta_add = wifi67_sta_add,
    .sta_remove = wifi67_sta_remove,
    .set_key = wifi67_set_key,
    .set_rts_threshold = wifi67_set_rts_threshold,
    .wake_tx_queue = wifi67_wake_tx_queue,
    .sw_scan_start = wifi67_sw_scan_start,
    .sw_scan_complete = wifi67_sw_scan_complete,
};

#endif /* _WIFI67_OPS_H_ */ 