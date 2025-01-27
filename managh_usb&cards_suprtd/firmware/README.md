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
- Support for key management

### TPM Integration (`fw_tpm.c`)
- Hardware-backed key storage
- PCR measurement and validation
- Quote generation and verification
- Policy-based key access control
- Session management for TPM operations
- Cached policy evaluation
- Support for multiple PCR banks

### Test Modules
- `test_fw_common.c`: Tests for basic firmware functionality
- `test_fw_secure.c`: Tests for secure boot features
- `test_fw_tpm.c`: Tests for TPM integration (coming soon)

## TPM Attestation Flow

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│  Key Access  │     │  TPM Policy  │     │  PCR Quote   │
│   Request    │ ──> │  Evaluation  │ ──> │ Verification │
└──────────────┘     └──────────────┘     └──────────────┘
                           │                      │
                           v                      v
                    ┌──────────────┐     ┌──────────────┐
                    │   Policy     │     │    Quote     │
                    │    Cache     │     │    Cache     │
                    └──────────────┘     └──────────────┘
```

## Security Model

The TPM integration provides the following security guarantees:

1. Hardware-backed key protection
2. System state validation through PCR measurements
3. Remote attestation capabilities
4. Policy-based access control
5. Protection against rollback attacks

## Policy Configuration

Example policy configuration:
```c
struct tpm_policy_info policy = {
    .pcr_mask = (1 << TPM_PCR_FIRMWARE_CODE) |
                (1 << TPM_PCR_FIRMWARE_CONFIG),
    .flags = POLICY_FLAG_CACHE | POLICY_FLAG_STRICT,
    .cache_timeout = 300  /* 5 minutes */
};
```

## Troubleshooting

Common TPM-related issues:

1. PCR quote verification failures
   - Check PCR values using `tpm2_pcrread`
   - Verify system state hasn't changed
   - Check quote signature

2. Policy evaluation failures
   - Verify PCR selection mask
   - Check policy cache timeout
   - Validate TPM session

## TODO List
✓ Implement secure key storage
✓ Add key revocation support
✓ Add TPM integration
✓ Implement PCR quote verification
✓ Add policy digest calculation

Remaining TODOs:
- [ ] Add support for custom PCR measurements
- [ ] Implement TPM event log validation
- [ ] Add remote attestation service integration
- [ ] Enhance TPM failure recovery mechanisms
- [ ] Add support for TPM NV counter for rollback protection
- [ ] Implement TPM-backed firmware encryption
- [ ] Add support for TPM2 enhanced authorization
- [ ] Create TPM policy simulator for testing

## Usage Example

```c
/* Verify key with TPM quote */
struct tpm_quote_info quote;
int ret = fw_tpm_get_quote(key_id, &quote);
if (ret == 0) {
    ret = fw_tpm_verify_quote(&quote);
    if (ret == 0) {
        /* Quote verification successful */
    }
}
```

## Build Instructions

1. Build the driver and test modules:
   ```bash
   make
   ```

2. Run the TPM tests:
   ```bash
   sudo make test TPM=1
   ```

## Error Codes

- `KEY_ERR_NONE`: Success
- `KEY_ERR_QUOTE`: Quote verification failed
- `KEY_ERR_POLICY`: Policy evaluation failed
- `KEY_ERR_SESSION`: TPM session error

## Contributing

When adding new TPM features:
1. Add appropriate test cases
2. Update policy documentation
3. Consider caching implications
4. Handle TPM errors gracefully
5. Update security model documentation

## Security Considerations

1. Always verify PCR quotes before key operations
2. Use strict policy evaluation when required
3. Implement proper session management
4. Monitor for TPM errors
5. Handle cached results carefully

## License

This code is licensed under the GNU General Public License v2.
See the LICENSE file for details. 