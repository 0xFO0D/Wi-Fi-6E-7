# WiFi 6E/7 Test Suite

This test suite provides comprehensive testing capabilities for WiFi 6E/7 features, focusing on hardware verification, performance measurement, and stress testing.

## Features Tested

The test suite covers the following WiFi 6E/7 features:

1. **MAC Layer**
   - Basic MAC functionality
   - Frame handling
   - QoS mechanisms
   - Rate control

2. **PHY Layer**
   - Signal processing
   - Channel estimation
   - Beamforming
   - OFDMA operation

3. **Firmware**
   - Firmware loading
   - Version verification
   - Feature configuration
   - Error handling

4. **Cryptography**
   - AES-GCM encryption
   - AES-CCM operation
   - Key management
   - MAC verification

5. **Power Management**
   - Power state transitions
   - Sleep/wake mechanisms
   - Power saving features
   - Wake-on-WLAN

6. **Multi-Link Operation (MLO)**
   - Link setup and teardown
   - Channel switching
   - Load balancing
   - Simultaneous transmission/reception

7. **4K QAM**
   - Modulation accuracy
   - Error rate testing
   - SNR requirements
   - Throughput measurement

8. **Multi-RU (Resource Unit)**
   - RU allocation
   - User multiplexing
   - Bandwidth utilization
   - Performance optimization

9. **Coordinated Multi-AP (CMP)**
   - AP synchronization
   - Joint transmission
   - Joint reception
   - Resource coordination

10. **Enhanced Link Adaptation (ELA)**
    - Dynamic parameter adjustment
    - Channel condition monitoring
    - Rate selection
    - Performance optimization

11. **Preamble Puncturing**
    - Static puncturing patterns
    - Dynamic interference avoidance
    - Throughput impact
    - Error rate analysis

## Requirements

- Linux kernel 5.15 or later
- GCC 9.0 or later
- Make build system
- Root privileges for module loading/unloading

## Building the Test Suite

1. Set the kernel directory:
   ```bash
   export KERNEL_DIR=/lib/modules/$(uname -r)/build
   ```

2. Build all test modules:
   ```bash
   make
   ```

3. Install the modules (optional):
   ```bash
   sudo make install
   ```

## Running Tests

1. Run the complete test suite:
   ```bash
   sudo make test
   ```

2. Run individual test modules:
   ```bash
   # Load test framework
   sudo insmod test_framework.ko

   # Load specific test module
   sudo insmod <module_name>.ko

   # Check kernel log for results
   dmesg | tail -n 200 | grep "test"

   # Unload module
   sudo rmmod <module_name>
   sudo rmmod test_framework
   ```

## Test Module Details

### Test Framework (`test_framework.ko`)
- Provides common testing infrastructure
- Handles test registration and execution
- Manages test results and reporting
- Supports various test flags and categories

### MAC Test (`mac_test.ko`)
- Tests basic MAC layer functionality
- Verifies frame handling and QoS
- Measures throughput and latency
- Validates rate control algorithms

### PHY Test (`phy_test.ko`)
- Tests physical layer operations
- Verifies signal processing
- Validates channel estimation
- Tests beamforming capabilities

### Firmware Test (`firmware_test.ko`)
- Tests firmware loading and updates
- Verifies version compatibility
- Validates feature configuration
- Tests error handling mechanisms

### Crypto Test (`crypto_test.ko`)
- Tests encryption/decryption operations
- Validates authentication mechanisms
- Measures cryptographic performance
- Tests key management functions

### Power Test (`power_test.ko`)
- Tests power state transitions
- Validates power saving features
- Measures power consumption
- Tests wake-up mechanisms

### MLO Test (`mlo_test.ko`)
- Tests multi-link operations
- Validates link management
- Measures multi-link performance
- Tests load balancing algorithms

### QAM Test (`qam_test.ko`)
- Tests 4K QAM modulation
- Measures modulation accuracy
- Validates error rates
- Tests throughput capabilities

### Multi-RU Test (`multi_ru_test.ko`)
- Tests resource unit allocation
- Validates user multiplexing
- Measures bandwidth utilization
- Tests performance optimization

### CMP Test (`cmp_test.ko`)
- Tests coordinated multi-AP operations
- Validates AP synchronization
- Tests joint transmission/reception
- Measures coordination efficiency

### ELA Test (`ela_test.ko`)
- Tests link adaptation mechanisms
- Validates parameter adjustment
- Measures adaptation performance
- Tests optimization algorithms

### Preamble Puncture Test (`preamble_puncture_test.ko`)
- Tests puncturing patterns
- Validates interference avoidance
- Measures throughput impact
- Tests dynamic adaptation

## Test Results

Test results are reported in the kernel log and include:
- Pass/fail status for each test case
- Performance measurements
- Error counts and types
- Timing information

## Troubleshooting

1. Module loading fails:
   - Check kernel version compatibility
   - Verify module dependencies
   - Check for hardware presence
   - Review kernel log for errors

2. Tests fail:
   - Check hardware configuration
   - Verify driver installation
   - Review test prerequisites
   - Check system resources

3. Performance issues:
   - Monitor system load
   - Check for interference
   - Verify hardware capabilities
   - Review test parameters

## Contributing

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push to the branch
5. Create a pull request

## License

This test suite is licensed under the MIT License. See the LICENSE file for details.

## Authors

- Fayssal Chokri

## Version

1.0 - Initial release 