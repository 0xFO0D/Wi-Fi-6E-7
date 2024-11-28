# Advanced Wi-Fi 6E/7 Linux Driver Implementation

High-performance Linux kernel driver for next-generation Wi-Fi 6E/7 devices with advanced features including MLO (Multi-Link Operation), 320MHz channels, and 4K QAM support.

## Hardware Architecture 
```plantuml
@startuml
!theme plain
skinparam componentStyle rectangle
package "RF Frontend" {
component "RF Core" as RF
component "PLL" as PLL
component "ADC/DAC" as ADC
}
package "Baseband" {
component "PHY Layer" as PHY {
component "FFT/IFFT" as FFT
component "FEC" as FEC
component "Equalizer" as EQ
}
component "MAC Layer" as MAC {
component "TX Engine" as TX
component "RX Engine" as RX
component "Queue Manager" as QM
}
}
package "System Interface" {
component "PCIe Controller" as PCIE
component "DMA Engine" as DMA
component "Memory Controller" as MEM
}
RF <--> ADC
ADC <--> PHY
PHY <--> MAC
MAC <--> DMA
DMA <--> PCIE
PCIE <--> MEM
@enduml
```

## Signal Processing Pipeline
```plantuml
@startuml
!theme plain
skinparam sequenceMessageAlign center
participant "Host CPU" as HOST
participant "DMA Engine" as DMA
participant "MAC" as MAC
participant "PHY" as PHY
participant "RF" as RF
participant "Antenna" as ANT
== TX Path ==
HOST -> DMA: Write TX Descriptor
activate DMA
DMA -> MAC: Frame Data Transfer
activate MAC
MAC -> PHY: PHY Protocol Data
activate PHY
PHY -> RF: I/Q Samples
activate RF
RF -> ANT: RF Signal
deactivate RF
deactivate PHY
deactivate MAC
deactivate DMA
== RX Path ==
ANT -> RF: RF Signal
activate RF
RF -> PHY: I/Q Samples
activate PHY
PHY -> MAC: Decoded Symbols
activate MAC
MAC -> DMA: Frame Assembly
activate DMA
DMA -> HOST: RX Completion
deactivate DMA
deactivate MAC
deactivate PHY
deactivate RF
@enduml
```

## Memory Architecture
```plantuml
@startuml
!theme plain
skinparam componentStyle rectangle
package "PCIe Memory Space" {
component "BAR0: Control Registers\n0x0000_0000 - 0x0000_FFFF" as BAR0
component "BAR1: TX/RX Queues\n0x0001_0000 - 0x0001_FFFF" as BAR1
component "BAR2: DMA Descriptors\n0x0002_0000 - 0x0002_FFFF" as BAR2
}
package "Internal Memory" {
component "PHY Memory\n128KB" as PHYMEM
component "MAC Memory\n256KB" as MACMEM
component "Packet Buffer\n512KB" as PBUF
}
BAR0 --> PHYMEM
BAR1 --> MACMEM
BAR2 --> PBUF
@enduml
```

## Build Requirements

- Linux Kernel Headers (≥ 6.1)
- GCC (≥ 12.0)
- CMake (≥ 3.25)
- libnl3
- crypto++ development headers

## Installation
### Install dependencies on Arch Linux 
```bash 
sudo pacman -S base-devel linux-headers cmake libnl crypto++
```
## Build 
```bash 
make 
sudo make install 
```


## Testing

Hardware testing requires specialized RF testing equipment:
- Vector Signal Analyzer
- Spectrum Analyzer (>7GHz capability)
- RF Shield Box