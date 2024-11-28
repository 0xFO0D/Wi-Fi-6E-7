obj-m += wifi67.o

wifi67-objs := \
    src/core/main.o \
    src/hal/hardware.o \
    src/mac/mac_core.o \
    src/phy/phy_core.o \
    src/dma/dma_core.o

# Add CFLAGS for debugging and optimization
ccflags-y := -Wall -Wextra -g -O2
# Include path for our header files
ccflags-y += -I$(src)/include

# Path to kernel build directory
KDIR ?= /lib/modules/$(shell uname -r)/build

# Targets
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

# Install target
install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	depmod -a

# Debug target with additional debug flags
debug: ccflags-y += -DDEBUG -DVERBOSE_DEBUG
debug: all

# Check coding style
checkstyle:
	$(KDIR)/scripts/checkpatch.pl -f --no-tree src/*/*.c include/*/*.h

.PHONY: all clean install debug checkstyle