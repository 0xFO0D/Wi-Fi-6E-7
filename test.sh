#!/bin/bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Test configuration
TEST_ITERATIONS=3
DMA_TEST_CHANNELS="0 1 2 3"

echo -e "${YELLOW}Starting WiFi 6E/7 Driver Tests${NC}"

# Build the driver and test modules
echo "Building driver and test modules..."
make clean
make || {
    echo -e "${RED}Build failed${NC}"
    exit 1
}

# Function to run a test module
run_test() {
    local module=$1
    local desc=$2
    echo -e "\n${YELLOW}Running $desc${NC}"
    
    # Remove module if already loaded
    if lsmod | grep -q "^$module"; then
        sudo rmmod $module
    fi
    
    # Insert module
    sudo insmod $module.ko || {
        echo -e "${RED}Failed to load $module${NC}"
        return 1
    }
    
    # Check dmesg for errors
    if dmesg | tail -n 50 | grep -i "error\|fail\|warn"; then
        echo -e "${RED}Errors detected in dmesg for $module${NC}"
        sudo rmmod $module
        return 1
    fi
    
    # Remove module
    sudo rmmod $module || {
        echo -e "${RED}Failed to unload $module${NC}"
        return 1
    }
    
    echo -e "${GREEN}$desc completed successfully${NC}"
    return 0
}

# Run band tests
for i in $(seq 1 $TEST_ITERATIONS); do
    echo -e "\n${YELLOW}Test Iteration $i/$TEST_ITERATIONS${NC}"
    
    # Run band test
    run_test "wifi67_test" "Band Test" || exit 1
    
    # Run DMA tests for each channel
    for channel in $DMA_TEST_CHANNELS; do
        echo -e "\n${YELLOW}Testing DMA channel $channel${NC}"
        sudo insmod dma_test.ko test_channel=$channel || {
            echo -e "${RED}Failed to load DMA test module for channel $channel${NC}"
            exit 1
        }
        
        # Check dmesg for test results
        if dmesg | tail -n 50 | grep -i "error\|fail"; then
            echo -e "${RED}DMA test failed for channel $channel${NC}"
            sudo rmmod dma_test
            exit 1
        fi
        
        # Print test results
        dmesg | tail -n 10 | grep "DMA Test Results"
        
        sudo rmmod dma_test || {
            echo -e "${RED}Failed to unload DMA test module${NC}"
            exit 1
        }
        
        echo -e "${GREEN}DMA test completed successfully for channel $channel${NC}"
    done
done

echo -e "\n${GREEN}All tests completed successfully${NC}"

# Clean up
make clean

exit 0
