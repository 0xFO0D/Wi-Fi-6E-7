# WiFi 6E/7 Driver Firmware Support

This directory contains the firmware support components for the WiFi 6E/7 driver, including firmware loading, verification, and secure boot functionality.

## Components

### Common Firmware Support (`fw_common.h`, `fw_common.c`)
- Basic firmware operations (loading, decompression)
- Version management
- Error handling
- Statistics tracking
- Helper functions for checksums and version comparison

### Secure Boot Support (`fw_secure.c`)
- Firmware image verification using SHA-256
- RSA signature verification
- Secure header parsing and validation
- Support for key management (TODO)

### Test Modules
- `test_fw_common.c`: Tests for basic firmware functionality
- `test_fw_secure.c`: Tests for secure boot features

## Firmware Format

### Header Structure
```c
struct secure_header {
    u32 magic;      /* "WiFi" magic number */
    u32 version;    /* Header format version */
    u32 img_size;   /* Size of firmware image */
    u32 sig_size;   /* Size of signature */
    u8 hash[32];    /* SHA-256 hash of image */
    u8 signature[]; /* RSA signature */
};
```

### Supported Features
- Firmware compression (zlib, xz)
- Version comparison and rollback prevention
- Secure boot with RSA-2048 signatures
- Runtime integrity checking

## TODO List
1. Implement secure key storage mechanism
2. Add support for key revocation
3. Implement firmware update recovery
4. Add support for multiple signature algorithms
5. Enhance compression options
6. Add firmware caching support
7. Implement firmware telemetry

## Usage Example

```c
/* Load and verify firmware */
const struct firmware *fw;
int ret = request_firmware(&fw, "wifi67_fw.bin", device);
if (ret == 0) {
    ret = fw_secure_verify(fw->data, fw->size, public_key, key_size);
    if (ret == FW_ERR_NONE) {
        /* Firmware is valid, proceed with loading */
    }
    release_firmware(fw);
}
```

## Build Instructions

1. Build the driver and test modules:
   ```bash
   make
   ```

2. Run the tests:
   ```bash
   make test
   ```

3. Install the modules:
   ```bash
   sudo make install
   ```

## Error Codes

- `FW_ERR_NONE`: Success
- `FW_ERR_REQUEST`: Firmware request failed
- `FW_ERR_VERIFY`: Verification failed
- `FW_ERR_LOAD`: Loading failed
- `FW_ERR_VERSION`: Version check failed
- `FW_ERR_COMPRESS`: Decompression failed
- `FW_ERR_SECURE`: Secure boot check failed
- `FW_ERR_ROLLBACK`: Version rollback detected

## Contributing

When adding new features or fixing bugs:
1. Add appropriate test cases
2. Update documentation
3. Follow the Linux kernel coding style
4. Ensure backward compatibility
5. Add proper error handling

## Security Considerations

1. Always verify firmware signatures before loading
2. Use secure key storage
3. Implement rollback protection
4. Monitor for tampering attempts
5. Log security-related events

## License

This code is licensed under the GNU General Public License v2.
See the LICENSE file for details. 