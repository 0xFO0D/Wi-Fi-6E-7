#!/bin/bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}Starting WiFi 6E/7 Driver Tests${NC}"

# 1. Clean and build
echo -e "\n${YELLOW}Cleaning previous build...${NC}"
make clean
if [ $? -ne 0 ]; then
    echo -e "${RED}Clean failed${NC}"
    exit 1
fi

echo -e "\n${YELLOW}Building driver...${NC}"
make
if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed${NC}"
    exit 1
fi
echo -e "${GREEN}Build successful${NC}"

# 2. Remove existing module if loaded
if lsmod | grep -q "wifi67"; then
    echo -e "\n${YELLOW}Removing existing module...${NC}"
    sudo rmmod wifi67
fi

# 3. Load module
echo -e "\n${YELLOW}Loading module...${NC}"
sudo insmod wifi67.ko
if [ $? -ne 0 ]; then
    echo -e "${RED}Module loading failed${NC}"
    exit 1
fi
echo -e "${GREEN}Module loaded successfully${NC}"

# 4. Check kernel logs
echo -e "\n${YELLOW}Checking kernel logs...${NC}"
dmesg | tail -n 20 | grep "wifi67"

# 5. Check module info
echo -e "\n${YELLOW}Module information:${NC}"
modinfo wifi67.ko

# 6. Check device presence
echo -e "\n${YELLOW}Checking for device presence...${NC}"
ls -l /sys/class/net/ | grep wifi67

# 7. Test module parameters
echo -e "\n${YELLOW}Testing module parameters...${NC}"
cat /sys/module/wifi67/parameters/* 2>/dev/null

# 8. Check interrupt registration
echo -e "\n${YELLOW}Checking interrupts...${NC}"
cat /proc/interrupts | grep wifi67

# 9. Check driver debug info
echo -e "\n${YELLOW}Driver debug information:${NC}"
sudo cat /sys/kernel/debug/wifi67/* 2>/dev/null

# 10. Unload module
echo -e "\n${YELLOW}Unloading module...${NC}"
sudo rmmod wifi67
if [ $? -ne 0 ]; then
    echo -e "${RED}Module unload failed${NC}"
    exit 1
fi
echo -e "${GREEN}Module unloaded successfully${NC}"

# Final check of kernel logs
echo -e "\n${YELLOW}Final kernel log check:${NC}"
dmesg | tail -n 20 | grep "wifi67"

echo -e "\n${GREEN}Test complete${NC}" 