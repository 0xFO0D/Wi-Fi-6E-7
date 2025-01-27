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
    src/hal/hardware.o \
    src/mac/mac_core.o \
    src/phy/phy_core.o \
    src/dma/dma_core.o \
    src/dma/dma_monitor.o \
    src/regulatory/reg_core.o \
    src/crypto/crypto_core.o \
    src/firmware/fw_core.o \
    src/debug/debug.o \
    src/perf/perf_monitor.o \
    src/diag/hw_diag.o \
    src/power/power_mgmt.o \
    managh_usb&cards_suprtd/firmware/fw_common.o \
    managh_usb&cards_suprtd/firmware/fw_secure.o \
    managh_usb&cards_suprtd/firmware/fw_keys.o \
    managh_usb&cards_suprtd/firmware/fw_tpm.o \
    managh_usb&cards_suprtd/firmware/fw_eventlog.o \
    managh_usb&cards_suprtd/firmware/fw_attest.o \
    managh_usb&cards_suprtd/firmware/fw_debugfs.o \
    managh_usb&cards_suprtd/firmware/fw_rollback.o \
    managh_usb&cards_suprtd/firmware/fw_encrypt.o \
    managh_usb&cards_suprtd/firmware/fw_policy_sim.o

managh_wifi_usb-objs := \
    managh_usb&cards_suprtd/usb/usb_driver.o \
    managh_usb&cards_suprtd/firmware/firmware_loader.o

managh_wifi_pci-objs := \
    managh_usb&cards_suprtd/pci/pci_driver.o \
    managh_usb&cards_suprtd/firmware/firmware_loader.o

test_fw_common-objs := managh_usb&cards_suprtd/firmware/test_fw_common.o
test_fw_secure-objs := managh_usb&cards_suprtd/firmware/test_fw_secure.o
test_fw_tpm-objs := managh_usb&cards_suprtd/firmware/test_fw_tpm.o
test_fw_eventlog-objs := managh_usb&cards_suprtd/firmware/test_fw_eventlog.o
test_fw_attest-objs := managh_usb&cards_suprtd/firmware/test_fw_attest.o

# Test modules
obj-m += test_framework.o
obj-m += dma_test.o
obj-m += mac_test.o
obj-m += phy_test.o
obj-m += firmware_test.o
obj-m += crypto_test.o
obj-m += power_test.o

test_framework-objs := managh_usb&cards_suprtd/tests/test_framework.o
dma_test-objs := managh_usb&cards_suprtd/tests/dma_test.o
mac_test-objs := managh_usb&cards_suprtd/tests/mac_test.o
phy_test-objs := managh_usb&cards_suprtd/tests/phy_test.o
firmware_test-objs := managh_usb&cards_suprtd/tests/firmware_test.o
crypto_test-objs := managh_usb&cards_suprtd/tests/crypto_test.o
power_test-objs := managh_usb&cards_suprtd/tests/power_test.o

# Test targets
TEST_MODULES := test_framework.ko dma_test.ko mac_test.ko phy_test.ko \
                firmware_test.ko crypto_test.ko power_test.ko

# Kernel build directory
KDIR ?= /lib/modules/$(shell uname -r)/build

# Build flags
EXTRA_CFLAGS := -I$(PWD)/include -DDEBUG

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

.PHONY: all modules clean test install debugfs monitor