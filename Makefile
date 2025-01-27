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
    managh_usb&cards_suprtd/firmware/fw_encrypt.o

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
obj-m += wifi67_test.o
obj-m += dma_test.o

wifi67_test-objs := tests/band_test.o
dma_test-objs := tests/dma_test.o

# Kernel build directory
KDIR ?= /lib/modules/$(shell uname -r)/build

# Build flags
EXTRA_CFLAGS := -I$(PWD)/include -DDEBUG

# Test targets
TEST_MODULES := wifi67_test.ko dma_test.ko test_fw_common.ko test_fw_secure.ko \
                test_fw_tpm.ko test_fw_eventlog.ko test_fw_attest.ko

all: modules

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f modules.order Module.symvers

test: modules
	@echo "Running firmware tests..."
	sudo insmod test_fw_common.ko
	@sleep 1
	sudo rmmod test_fw_common
	@echo "Running secure boot tests..."
	sudo insmod test_fw_secure.ko
	@sleep 1
	sudo rmmod test_fw_secure
	@echo "Running TPM integration tests..."
	sudo insmod test_fw_tpm.ko
	@sleep 1
	sudo rmmod test_fw_tpm
	@echo "Running event log tests..."
	sudo insmod test_fw_eventlog.ko
	@sleep 1
	sudo rmmod test_fw_eventlog
	@echo "Running attestation tests..."
	sudo insmod test_fw_attest.ko
	@sleep 1
	sudo rmmod test_fw_attest
	@dmesg | tail -n 100 | grep -E "firmware|secure|tpm|eventlog|attest"

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