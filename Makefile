obj-m += wifi67.o
obj-m += managh_wifi_usb.o
obj-m += managh_wifi_pci.o
obj-m += test_fw_common.o
obj-m += test_fw_secure.o
obj-m += test_fw_tpm.o
obj-m += test_fw_eventlog.o
obj-m += test_fw_attest.o

wifi67-objs := \
    src/core/main.o \
    src/core/bands.o \
    src/core/caps.o \
    src/core/ops.o \
    src/core/pci.o \
    src/core/mlo.o \
    src/core/emlsr.o \
    src/core/emlps.o \
    src/core/emlmr.o \
    src/core/emlrc.o \
    src/hal/hardware.o \
    src/mac/mac_core.o \
    src/mac/wifi7_mac.o \
    src/mac/wifi7_mlo.o \
    src/mac/wifi7_spatial.o \
    src/mac/wifi7_rate.o \
    src/mac/wifi7_qos.o \
    src/mac/wifi7_ba.o \
    src/mac/wifi7_aggregation.o \
    src/vendors/tplink/wifi7_tplink.o \
    src/phy/phy_core.o \
    src/dma/dma_core.o \
    src/dma/dma_monitor.o \
    src/regulatory/reg_core.o \
    src/crypto/crypto_core.o \
    src/firmware/fw_core.o \
    src/debug/debug.o \
    src/perf/perf_monitor.o \
    src/diag/hw_diag.o \
    src/power/wifi7_power.o \
    src/power/power_mgmt.o \
    src/power/power_stats.o \
    src/power/power_debug.o \
    src/power/power_thermal.o \
    src/power/power_profile.o \
    src/power/power_domain.o \
    src/power/power_dvfs.o \
    src/hal/wifi7_rf.o \
    automotive/v2x/wifi7_v2x.o \
    automotive/can/wifi7_can.o \
    automotive/signal/wifi7_auto_signal.o \
    hardware_support/firmware/fw_common.o \
    hardware_support/firmware/fw_secure.o \
    hardware_support/firmware/fw_keys.o \
    hardware_support/firmware/fw_tpm.o \
    hardware_support/firmware/fw_eventlog.o \
    hardware_support/firmware/fw_attest.o \
    hardware_support/firmware/fw_debugfs.o \
    hardware_support/firmware/fw_rollback.o \
    hardware_support/firmware/fw_encrypt.o \
    hardware_support/firmware/fw_policy_sim.o

managh_wifi_usb-objs := \
    hardware_support/usb/usb_driver.o \
    hardware_support/firmware/firmware_loader.o

managh_wifi_pci-objs := \
    hardware_support/pci/pci_driver.o \
    hardware_support/firmware/firmware_loader.o

test_fw_common-objs := hardware_support/firmware/test_fw_common.o
test_fw_secure-objs := hardware_support/firmware/test_fw_secure.o
test_fw_tpm-objs := hardware_support/firmware/test_fw_tpm.o
test_fw_eventlog-objs := hardware_support/firmware/test_fw_eventlog.o
test_fw_attest-objs := hardware_support/firmware/test_fw_attest.o

# Test modules
obj-m += test_framework.o
obj-m += dma_test.o
obj-m += mac_test.o
obj-m += phy_test.o
obj-m += firmware_test.o
obj-m += crypto_test.o
obj-m += power_test.o
obj-m += rate_test.o
obj-m += qos_test.o
obj-m += v2x_test.o
obj-m += can_test.o
obj-m += auto_signal_test.o
obj-m += auto_test.o

test_framework-objs := hardware_support/tests/test_framework.o
dma_test-objs := hardware_support/tests/dma_test.o
mac_test-objs := hardware_support/tests/mac_test.o
phy_test-objs := hardware_support/tests/phy_test.o
firmware_test-objs := hardware_support/tests/firmware_test.o
crypto_test-objs := hardware_support/tests/crypto_test.o
power_test-objs := hardware_support/tests/power_test.o
rate_test-objs := hardware_support/tests/rate_test.o
qos_test-objs := hardware_support/tests/qos_test.o
v2x_test-objs := automotive/tests/v2x_test.o
can_test-objs := automotive/tests/can_test.o
auto_signal_test-objs := automotive/tests/auto_signal_test.o
auto_test-objs := hardware_support/tests/auto_test.o

# Test targets
TEST_MODULES := test_framework.ko dma_test.ko mac_test.ko phy_test.ko \
                firmware_test.ko crypto_test.ko power_test.ko rate_test.ko \
                qos_test.ko v2x_test.ko can_test.ko auto_signal_test.ko auto_test.ko

# Kernel build directory
KDIR ?= /lib/modules/$(shell uname -r)/build

# Build flags
EXTRA_CFLAGS := -I$(PWD)/include -DDEBUG
EXTRA_CFLAGS += -I$(src)/include

obj-$(CONFIG_WIFI7) += \
    src/core/wifi7_core.o \
    src/mac/wifi7_mac.o \
    src/mac/wifi7_qos.o \
    src/mac/wifi7_ba.o \
    src/mac/wifi7_aggregation.o \
    src/mac/wifi7_rate.o \
    src/mac/wifi7_mlo.o \
    src/regulatory/wifi7_afc.o \
    src/hardware_support/usb/usb_driver.o \
    src/hardware_support/firmware/firmware_loader.o

ccflags-y += -I$(src)/include -DDEBUG

# Optional features
wifi7-$(CONFIG_WIFI7_MLO) += src/mac/wifi7_mac_mlo.o
wifi7-$(CONFIG_WIFI7_QOS) += src/mac/wifi7_mac_qos.o
wifi7-$(CONFIG_WIFI7_POWER) += src/mac/wifi7_mac_power.o
wifi7-$(CONFIG_WIFI7_DEBUG) += src/debug/wifi7_debug.o
wifi7-$(CONFIG_WIFI7_V2X) += automotive/v2x/wifi7_v2x.o
wifi7-$(CONFIG_WIFI7_CAN) += automotive/can/wifi7_can.o
wifi7-$(CONFIG_WIFI7_AUTO_SIGNAL) += automotive/signal/wifi7_auto_signal.o

# Hardware support
wifi7-$(CONFIG_WIFI7_PCI) += src/pci/wifi7_pci.o
wifi7-$(CONFIG_WIFI7_USB) += src/usb/wifi7_usb.o

# Build options
KBUILD_CFLAGS += -O2
KBUILD_CFLAGS += -fno-strict-aliasing
KBUILD_CFLAGS += -fno-common
KBUILD_CFLAGS += -pipe

all: modules

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f modules.order Module.symvers

test: modules
	@echo "Running test framework initialization..."
	sudo insmod test_framework.ko
	@sleep 1
	@echo "Running DMA tests..."
	sudo insmod dma_test.ko
	@sleep 1
	sudo rmmod dma_test
	@echo "Running MAC tests..."
	sudo insmod mac_test.ko
	@sleep 1
	sudo rmmod mac_test
	@echo "Running PHY tests..."
	sudo insmod phy_test.ko
	@sleep 1
	sudo rmmod phy_test
	@echo "Running firmware tests..."
	sudo insmod firmware_test.ko
	@sleep 1
	sudo rmmod firmware_test
	@echo "Running crypto tests..."
	sudo insmod crypto_test.ko
	@sleep 1
	sudo rmmod crypto_test
	@echo "Running power management tests..."
	sudo insmod power_test.ko
	@sleep 1
	sudo rmmod power_test
	@echo "Cleaning up test framework..."
	sudo rmmod test_framework
	@echo "Test results:"
	@dmesg | tail -n 200 | grep -E "test|Test|TEST"

# Install modules and firmware
install: modules
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	# Install firmware files
	install -d /lib/firmware/mediatek
	install -d /lib/firmware/realtek
	install -m 644 firmware/mediatek/* /lib/firmware/mediatek/
	install -m 644 firmware/realtek/* /lib/firmware/realtek/
	depmod -a

# Debug targets
debugfs:
	@echo "DMA Monitor debugfs interface:"
	@cat /sys/kernel/debug/wifi67_dma/monitor
	@echo "TPM Policy debugfs interface:"
	@cat /sys/kernel/debug/wifi67_tpm/policy
	@echo "Event Log debugfs interface:"
	@cat /sys/kernel/debug/wifi67_eventlog/log
	@echo "Attestation Status:"
	@cat /sys/kernel/debug/wifi67_attest/status

monitor: modules
	@echo "Starting monitoring..."
	@sudo insmod wifi67.ko
	@sleep 1
	@cat /sys/kernel/debug/wifi67_dma/monitor
	@cat /sys/kernel/debug/wifi67_tpm/policy
	@cat /sys/kernel/debug/wifi67_eventlog/log
	@cat /sys/kernel/debug/wifi67_attest/status
	@sudo rmmod wifi67

# Automotive components
obj-m += wifi7_v2x.o
obj-m += wifi7_can.o
obj-m += wifi7_auto_signal.o

wifi7_v2x-objs := automotive/v2x/wifi7_v2x.o
wifi7_can-objs := automotive/can/wifi7_can.o
wifi7_auto_signal-objs := automotive/signal/wifi7_auto_signal.o

.PHONY: all modules clean test install debugfs monitor