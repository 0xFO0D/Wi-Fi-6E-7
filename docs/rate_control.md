# WiFi67 Rate Control System

## Overview
The WiFi67 rate control system implements an adaptive rate selection algorithm 
with support for legacy rates, HT, VHT, HE, and EHT rates. It includes three
different rate adaptation algorithms:

- Minstrel: Throughput-based rate selection
- PID: Feedback control based adaptation
- Adaptive: Simple threshold-based adaptation

## Key Features
- Multi-algorithm support
- Per-station rate tracking
- MU-MIMO awareness
- Historical statistics
- Debugfs interface
- A-MPDU support

## Rate Selection Process
1. Normal operation uses current rate based on success ratio
2. Periodic probing of higher rates when conditions are good
3. Fallback mechanism for handling failures
4. Support for different guard intervals and bandwidth

## Configuration
Key parameters can be adjusted through debugfs:
- Update interval
- Probe interval
- Success thresholds
- Retry limits
- Algorithm selection

## Usage
The rate control system is automatically initialized with the driver
and requires no manual configuration. Fine-tuning can be done through
the debugfs interface at `/sys/kernel/debug/wifi67/rate_control/`. 

## System Architecture 
![dg1](https://www.plantuml.com/plantuml/png/PLB1RjGm4BtxAuQzxAc2Sa8SK5Sj4fMw26tsm6qrTIQ9XMEdCnDAXVXtR6zelEAIFByylUTvanjHP9r7RzxeG2F1vD458pzSc91n11l7AGOAskYB9s2oYg7tPBPmHvqBj2h4l4BOx7Ut7zwzlxw61tmXtW0AZ6XhnG6Lu3O6vUZJcwc8sCBl_rGD-PJhOZXBBe8QHNMYpWfy3hqBPnj9x8Lir-Aac3tW8VRE5XLcqhIe-8X2IzWH7pr9-Qu5cZ-rqHVazXcPgi66vxKD5vIuGqjYNY5iZjyXttDUR7-Tf9ngISa-qDDCeaN5YJWnFQsYDJptrhb-IJ_PSZBRJ0UoMkyajGvNLvzMkuMeRi65g8nO2h6CNxgiYL9RJMnXrxOGeYQSdy2k7nHYjpIyytrafyCe7ytU1L4cRxxTt_rRlodRSJ_9K3ZxNDeiwzoJihF9evcj9H78dPdZm77kXsbMSsYQ-xVDryDgSvDG_hl_0W00)

## Rate Adaptation State Machine
![dg2](https://www.plantuml.com/plantuml/png/LL11QiCm4Bph5TjS2eLye8UGGEcnb5nBCOeqZaLboQnMAVrzPTaYySNQdPcTNSynYMR9erTFCc04JLwxe9xf3RqBe48BageFxJuYZzhlI2SAjMp49yUB2zVAJAL68yumyGR-gmolJIFVjVSNRNugsQ_DVRUm3ic9Yo6fhvTlskyFRUtoLAIkerTAsHYarF5iVI0P66c8tgf5S1yksWfrwyI3e1r8JT9hvrL3jnuDxSIDY5aURsh_KcIcN0-JnTqMjyn8gl5RnoJwuj5DO1TjYU5aRI7d-ESliRiEGh5V0pw9O9o4XWA3YRL0kNxUPvVCw1gB3S9YilMufts6pGPrd7u710Nco8ai14RmO70iLHxzFm00)


## Signal Processing Flow
![dg3](https://www.plantuml.com/plantuml/png/FOsnJWCn38RtF8KfSr-00HKnTO3XCd8CxfrLJfSuaUqCyFJ4YgwZ_-_JvuiQYhMSmfFzK2Qe2JSEkczSKJ333PVzBgNn-bvI4J11rew8BQYX4Poc6AypNxasEm4gYEfsiFWvpVn1g4rwdOUWB0_UbeLKPvxwEU1O3i7tcICbMcmhF1xqBVWNE7yztrAZx_EhH-458kQQAFZHcT3QqP7i-8qn_VvrFXPtxaB4RddLbHMhXGlnsdBw1m00)