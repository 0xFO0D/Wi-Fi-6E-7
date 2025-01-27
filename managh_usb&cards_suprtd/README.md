# Managh WiFi 6E/7 Supported Devices

This directory contains device-specific implementations and firmware support for various WiFi 6E and WiFi 7 devices.

## Supported Devices

### MediaTek
- MT7921 (PCI) - WiFi 6E
  - 2x2 MIMO
  - 160MHz bandwidth
  - Features: MU-MIMO, OFDMA
- MT7922 (PCI) - WiFi 6E
  - 2x2 MIMO
  - 160MHz bandwidth
  - Features: MU-MIMO, OFDMA
- MT7925 (USB) - WiFi 6E
  - 2x2 MIMO
  - 160MHz bandwidth
  - Features: MU-MIMO, OFDMA

### Realtek
- RTL8852BE (PCI) - WiFi 6E
  - 2x2 MIMO
  - 160MHz bandwidth
  - Features: MU-MIMO, OFDMA
- RTL8852AE (PCI) - WiFi 6E
  - 2x2 MIMO
  - 160MHz bandwidth
  - Features: MU-MIMO, OFDMA
- RTL8852BU (USB) - WiFi 6E
  - 2x2 MIMO
  - 160MHz bandwidth
  - Features: MU-MIMO, OFDMA

### Intel
- AX211 (PCI) - WiFi 6E
  - 2x2 MIMO
  - 160MHz bandwidth
  - Features: MU-MIMO, OFDMA
- AX411 (PCI) - WiFi 6E
  - 2x2 MIMO
  - 160MHz bandwidth
  - Features: MU-MIMO, OFDMA
- BE200 (PCI) - WiFi 7
  - 4x4 MIMO
  - 320MHz bandwidth
  - Features: MU-MIMO, OFDMA, MLO

### Qualcomm
- QCA6490 (PCI) - WiFi 6E
  - 2x2 MIMO
  - 160MHz bandwidth
  - Features: MU-MIMO, OFDMA
- QCA6750 (PCI) - WiFi 6E
  - 2x2 MIMO
  - 160MHz bandwidth
  - Features: MU-MIMO, OFDMA
- QCN9074 (PCI) - WiFi 7
  - 4x4 MIMO
  - 320MHz bandwidth
  - Features: MU-MIMO, OFDMA, MLO

### Broadcom
- BCM4389 (PCI) - WiFi 6E
  - 2x2 MIMO
  - 160MHz bandwidth
  - Features: MU-MIMO, OFDMA
- BCM4398 (PCI) - WiFi 7
  - 4x4 MIMO
  - 320MHz bandwidth
  - Features: MU-MIMO, OFDMA, MLO

## Directory Structure
```
managh_usb&cards_suprtd/
├── firmware/
│   ├── mt7921_fw.h
│   ├── rtl8852_fw.h
│   └── firmware_loader.c
├── pci/
│   └── pci_driver.c
├── usb/
│   └── usb_driver.c
├── tests/
│   └── firmware_test.c
└── supported_devices.h
```

## Firmware Support
Each supported device requires specific firmware files that must be placed in the appropriate directory:

### MediaTek
- `/lib/firmware/mediatek/mt7921_rom_patch.bin`
- `/lib/firmware/mediatek/mt7921_firmware.bin`

### Realtek
- `/lib/firmware/realtek/rtl8852_fw.bin`
- `/lib/firmware/realtek/rtl8852_rom.bin`
- `/lib/firmware/realtek/rtl8852_ram_code.bin`

## TODO List
- [ ] Add Intel firmware support
- [ ] Add Qualcomm firmware support
- [ ] Add Broadcom firmware support
- [ ] Implement firmware version query
- [ ] Implement firmware features query
- [ ] Add secure boot support
- [ ] Add firmware compression support
- [ ] Add firmware update mechanism
- [ ] Add firmware rollback support
- [ ] Add power calibration data support
- [ ] Add firmware debugging capabilities

## Testing
To run the firmware tests:
```bash
make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
sudo insmod managh_firmware_test.ko
sudo rmmod managh_firmware_test
dmesg | tail
```

## Contributing
1. Fork the repository
2. Create your feature branch
3. Commit your changes
4. Push to the branch
5. Create a Pull Request

## License
This module is licensed under MIT. 