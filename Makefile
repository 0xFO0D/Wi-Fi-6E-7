obj-m += wifi67.o
obj-m += managh_wifi_usb.o
obj-m += managh_wifi_pci.o

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
    src/power/power_mgmt.o

managh_wifi_usb-objs := \
    managh_usb&cards_suprtd/usb/usb_driver.o

managh_wifi_pci-objs := \
    managh_usb&cards_suprtd/pci/pci_driver.o

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
TEST_MODULES := wifi67_test.ko dma_test.ko

all: modules

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

test: modules
	sudo ./test.sh

# Install modules
install: modules
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	depmod -a

# Debug targets
debugfs:
	@echo "DMA Monitor debugfs interface:"
	@cat /sys/kernel/debug/wifi67_dma/monitor

monitor: modules
	@echo "Starting DMA monitoring..."
	@sudo insmod wifi67.ko
	@sleep 1
	@cat /sys/kernel/debug/wifi67_dma/monitor
	@sudo rmmod wifi67

.PHONY: all modules clean test install debugfs monitor