obj-m += wifi67.o

wifi67-objs := \
    src/core/main.o \
    src/hal/hardware.o \
    src/mac/mac_core.o \
    src/phy/phy_core.o

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean 