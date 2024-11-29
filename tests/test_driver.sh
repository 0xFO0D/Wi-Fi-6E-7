#!/bin/bash

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo "Starting WiFi 6E/7 Driver Tests"
echo "------------------------------"

# Clean previous build
echo -e "${GREEN}Cleaning previous build...${NC}"
make clean

# Build driver
echo -e "${GREEN}Building driver...${NC}"
make

# Check if build was successful
if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed${NC}"
    exit 1
fi

# Remove existing modules if loaded
echo -e "${GREEN}Removing existing modules...${NC}"
sudo rmmod wifi67_test 2>/dev/null
sudo rmmod wifi67 2>/dev/null

# Load main driver
echo -e "${GREEN}Loading main driver...${NC}"
sudo insmod wifi67.ko
if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to load main driver${NC}"
    exit 1
fi

# Load test module
echo -e "${GREEN}Loading test module...${NC}"
sudo insmod wifi67_test.ko
if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to load test module${NC}"
    sudo rmmod wifi67
    exit 1
fi

# Check dmesg for test results
echo -e "${GREEN}Test Results:${NC}"
dmesg | grep -A 20 "Starting WiFi 6E/7 driver tests"

# Cleanup
echo -e "${GREEN}Cleaning up...${NC}"
sudo rmmod wifi67_test
sudo rmmod wifi67

# Check for any errors in dmesg
if dmesg | grep -i "wifi67.*error" > /dev/null; then
    echo -e "${RED}Tests failed - check dmesg for details${NC}"
    exit 1
else
    echo -e "${GREEN}All tests passed successfully${NC}"
fi 