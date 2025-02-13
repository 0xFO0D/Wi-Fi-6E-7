# WiFi 6E/7 Test Modules Makefile

# Compiler and flags
ccflags-y := -I$(src)/../include -DDEBUG

# Test modules
obj-m += test_framework.o
obj-m += mac_test.o
obj-m += phy_test.o
obj-m += firmware_test.o
obj-m += crypto_test.o
obj-m += power_test.o
obj-m += mlo_test.o
obj-m += qam_test.o
obj-m += multi_ru_test.o
obj-m += cmp_test.o
obj-m += ela_test.o
obj-m += preamble_puncture_test.o

# Module paths
TEST_MODULES := test_framework.ko \
               mac_test.ko \
               phy_test.ko \
               firmware_test.ko \
               crypto_test.ko \
               power_test.ko \
               mlo_test.ko \
               qam_test.ko \
               multi_ru_test.ko \
               cmp_test.ko \
               ela_test.ko \
               preamble_puncture_test.ko

# Default target
all: modules

# Build modules
modules:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

# Clean build artifacts
clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

# Install modules
install: modules
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules_install

# Test target
test: modules
	@echo "Running WiFi 6E/7 test suite..."
	@# Load test framework first
	sudo insmod test_framework.ko
	@# Load and test MAC layer
	sudo insmod mac_test.ko
	@sleep 2
	sudo rmmod mac_test
	@# Load and test PHY layer
	sudo insmod phy_test.ko
	@sleep 2
	sudo rmmod phy_test
	@# Load and test firmware
	sudo insmod firmware_test.ko
	@sleep 2
	sudo rmmod firmware_test
	@# Load and test crypto
	sudo insmod crypto_test.ko
	@sleep 2
	sudo rmmod crypto_test
	@# Load and test power management
	sudo insmod power_test.ko
	@sleep 2
	sudo rmmod power_test
	@# Load and test MLO
	sudo insmod mlo_test.ko
	@sleep 2
	sudo rmmod mlo_test
	@# Load and test 4K QAM
	sudo insmod qam_test.ko
	@sleep 2
	sudo rmmod qam_test
	@# Load and test Multi-RU
	sudo insmod multi_ru_test.ko
	@sleep 2
	sudo rmmod multi_ru_test
	@# Load and test CMP
	sudo insmod cmp_test.ko
	@sleep 2
	sudo rmmod cmp_test
	@# Load and test ELA
	sudo insmod ela_test.ko
	@sleep 2
	sudo rmmod ela_test
	@# Load and test Preamble Puncturing
	sudo insmod preamble_puncture_test.ko
	@sleep 2
	sudo rmmod preamble_puncture_test
	@# Unload test framework
	sudo rmmod test_framework
	@# Display test results
	@dmesg | tail -n 200 | grep "test"

.PHONY: all modules clean install test 