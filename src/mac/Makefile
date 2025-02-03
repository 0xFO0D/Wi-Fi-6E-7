# WiFi 7 MAC Layer Makefile

ccflags-y := -I$(src)/../../include

obj-$(CONFIG_WIFI7_MAC) += wifi7_mac.o
wifi7_mac-objs := wifi7_mac_core.o wifi7_mac_debugfs.o

# Optional features
obj-$(CONFIG_WIFI7_MAC_MLO) += wifi7_mac_mlo.o
obj-$(CONFIG_WIFI7_MAC_QOS) += wifi7_mac_qos.o
obj-$(CONFIG_WIFI7_MAC_POWER) += wifi7_mac_power.o

# Debug options
ccflags-$(CONFIG_WIFI7_MAC_DEBUG) += -DDEBUG 