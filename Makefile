obj-m += wifi67.o

wifi67-objs := \
    src/core/main.o \
    src/core/bands.o \
    src/core/caps.o \
    src/hal/hardware.o \
    src/mac/mac_core.o \
    src/phy/phy_core.o \
    src/dma/dma_core.o \
    src/regulatory/reg_core.o \
    src/crypto/crypto_core.o \
    src/firmware/fw_core.o \
    src/debug/debug.o \
    src/perf/perf_monitor.o \
    src/diag/hw_diag.o \
    src/power/power_mgmt.o

# Test module
obj-m += wifi67_test.o
wifi67_test-objs := tests/band_test.o

# Kernel build directory
KDIR ?= /lib/modules/$(shell uname -r)/build

# Build flags
EXTRA_CFLAGS := -I$(PWD)/include -DDEBUG

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean

test: all
	sudo ./tests/test_driver.sh

.PHONY: all clean test