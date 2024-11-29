#!/bin/bash

# Load module
echo "Loading wifi67 module..."
sudo insmod wifi67.ko

# Wait a bit
sleep 1

# Check if module loaded
if lsmod | grep -q "wifi67"; then
    echo "Module loaded successfully"
else
    echo "Failed to load module"
    exit 1
fi

# Check dmesg output
echo "Kernel messages:"
dmesg | tail -n 20 | grep "WIFI67"

# Optional: Try some iw commands if your driver is registered
echo "Network interfaces:"
iw dev

# Unload module
echo "Unloading module..."
sudo rmmod wifi67
