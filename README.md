# terrascope

A modular and efficient firmware for LoRa-enabled mesh sensor nodes running on Zephyr RTOS.

This project creates a scalable, low-power mesh of sensor nodes that collect environmental data and reliably forward it to gateway nodes, which in turn upload it to cloud services. Designed for robust rural, industrial, or remote environmental monitoring.

## Features

- 📡 **LoRa Communication** -- Bidirectional LoRa TX/RX with CBOR-encoded messages
- 🌐 **Mesh Networking** -- Multi-hop flooding with TTL, RSSI-based contention forwarding, duplicate suppression, and neighbor tracking
- 🌡 **Sensor Support** -- BME280 environmental sensor (temperature, humidity, pressure) on RAK4631; mock data on QEMU
- 🛠 **Modular Architecture** -- Zephyr Zbus message bus with clear separation of sensor, LoRa, routing, and message modules
- 🔒 **Network Security** -- AES-128-CMAC message authentication (PSA Crypto API), per-deployment network key, key rotation via `key_id`
- 🧪 **Testable** -- 63 unit tests across CBOR, routing, contention, neighbor table, auth, and config modules; mock LoRa driver with loopback for full pipeline testing in QEMU
- 🔄 **CI/CD** -- GitHub Actions matrix build for all targets plus unit tests via `west twister`

## Supported Hardware

| Board                 | Target                               | Notes                                                 |
| --------------------- | ------------------------------------ | ----------------------------------------------------- |
| RAK4631               | `rak4631`                            | nRF52840 + SX1262 LoRa. BME280 on I2C0 (address 0x76) |
| Heltec WiFi LoRa32 V2 | `heltec_wifi_lora32_v2/esp32/procpu` | ESP32 + SX1276. Future gateway role (WiFi-capable)    |
| QEMU RISC-V 64        | `qemu_riscv64`                       | Simulation with mock LoRa driver (loopback)           |

## Getting Started

### Prerequisites

Development runs inside a devcontainer based on `ghcr.io/zephyrproject-rtos/zephyr-build:main`. If using VS Code, open the repo and select **Reopen in Container** when prompted.

### First-Time Setup

After cloning or recreating the devcontainer:

```bash
west init -l .
west update --narrow --fetch-opt=--depth=1
```

### Build

```bash
# Build for QEMU (simulation with mock LoRa driver)
west build -b qemu_riscv64 -p

# Build for hardware targets
west build -b rak4631 -p
west build -b heltec_wifi_lora32_v2/esp32/procpu -p
```

### Run

```bash
# Run in QEMU
west build -t run
```

### Test

```bash
# Run unit tests in QEMU via twister
west twister -T tests -p qemu_riscv64 --no-shuffle

# Clean build
west build -t clean
```

## Architecture

### Message Bus

The firmware uses **Zephyr Zbus** as its central communication bus. All inter-module data flows through typed zbus channels:

- **`ts_lora_out_chan`** -- carries `ts_msg_lora_outgoing` (with route header) from producers and the flooding forwarder to the LoRa transmit task
- **`ts_lora_in_chan`** -- carries `ts_msg_lora_incoming` (decoded message + RSSI/SNR) from the LoRa receive task to local consumers

### Message Flow

```
+----------------+     +-------------+     +-----------+
| Sensor Manager |---->|             |     |           |
| (periodic)     |     | ts_lora_out |---->| LoRa TX   |~~~> radio
+----------------+     |   _chan     |     | (CBOR     |
                       |             |     |  encode)  |
+----------------+     |             |     |           |
| Main Loop      |---->|             |     +-----------+
| (heartbeats)   |     |             |
+----------------+     |             |
                       |             |
+--[contention]--+     |             |
| Delayed        |---->|             |
| Rebroadcast    |     +-------------+
+----------------+
       ^
       |  schedule (RSSI-based delay)
       |
radio ~~~> +-----------+     +-------------+
           | LoRa RX   |---->| ts_lora_in  |----> local delivery
           | (CBOR     |     |   _chan     |
           |  decode,  |     +-------------+
           |  routing, |
           |  flooding)|---->  neighbor table update
           +-----------+
```

### Message Types

Defined in `src/messages/messages.h` as a tagged union (`ts_msg_lora_outgoing`). Every message carries a route header (`src`, `dst`, `msg_id`, `ttl`, `key_id`) for mesh forwarding. An 8-byte AES-128-CMAC tag is appended after the CBOR payload on the wire.

| Type                 | Fields                                     | Units                      |
| -------------------- | ------------------------------------------ | -------------------------- |
| `TS_MSG_TELEMETRY`   | timestamp, temperature, humidity, pressure | s, centi-°C, centi-%RH, Pa |
| `TS_MSG_NODE_STATUS` | timestamp, uptime, status                  | s, s, enum                 |

### Modules

| Module           | Path                      | Role                                                                          |
| ---------------- | ------------------------- | ----------------------------------------------------------------------------- |
| LoRa             | `src/lora/`               | Device init, config, TX/RX threads, CBOR serialization, contention forwarding, message authentication |
| Routing          | `src/routing/`            | Node addressing, TTL, duplicate detection, neighbor table                     |
| Sensors          | `src/sensors/`            | Sensor backend abstraction; BME280 on RAK4631, mock on QEMU                   |
| Messages         | `src/messages/`           | Shared message type definitions (including route header)                      |
| Logging          | `src/logging/`            | Zbus publish error logging helper                                             |
| Config           | `src/config/`             | Runtime configuration schema, NVS persistence, defaults                       |
| Mock LoRa driver | `src/drivers/lora_mock.c` | Loopback simulation driver for QEMU (`CONFIG_LORA_MOCK=y`)                    |

### Board Configuration

Per-board Kconfig fragments and devicetree overlays live in `boards/`, using Zephyr's normalized board target naming (e.g., `rak4631_nrf52840.overlay`). Custom devicetree bindings are in `dts/bindings/`.

### LoRa Radio Configuration

All targets share: 865.1 MHz, SF10, BW 125 kHz, CR 4/5, TX power 4 dBm.

## Project Structure

```
terrascope/
├── boards/                     Board-specific Kconfig and DT overlays
├── drivers/lora/               Mock LoRa driver Kconfig
├── dts/bindings/               Custom devicetree bindings
├── src/
│   ├── drivers/lora_mock.c     Mock LoRa driver (loopback via k_msgq)
│   ├── lora/                   LoRa TX/RX tasks, CBOR, contention forwarding, auth
│   ├── routing/                Node addressing, duplicate detection, neighbor table
│   ├── messages/               Message type definitions (with route header)
│   ├── sensors/                Sensor backend abstraction (BME280 or mock)
│   ├── config/                 Runtime configuration schema and persistence
│   ├── logging/                Zbus error logging helper
│   └── main.c                  Entry point, zbus channels, routing init
├── tests/
│   ├── auth/                   Auth sign/verify tests (7 tests)
│   ├── cbor/                   CBOR serialization tests (9 tests)
│   ├── routing/                Routing logic tests (15 tests)
│   ├── contention/             Contention forwarding tests (11 tests)
│   ├── routing_table/          Neighbor table tests (13 tests)
│   └── config/                 Config module tests (8 tests)
├── prj.conf                    Common Kconfig
├── CMakeLists.txt              Build configuration
├── Kconfig                     Application Kconfig root
└── west.yml                    Zephyr manifest
```

## Code Style

- **Formatter**: clang-format (Google base, 4-space indent, 80-col limit) -- see `.clang-format`
- **Naming**: `ts_` prefix for all application types/channels, `snake_case` functions, `UPPER_SNAKE_CASE` macros
- **Memory**: Static allocation only, fixed-width types (`uint32_t`, `int16_t`, etc.)
- **Error handling**: Return 0 on success, negative errno on failure
- **Includes**: Zephyr/system headers first, project headers second, `<zephyr/logging/log.h>` last

See `CLAUDE.md` for full coding guidelines.

## Development Status

**Completed:**
- Phase 1 -- Housekeeping & Quality (version info, CI, timestamps, coding guidelines)
- Phase 2 -- Tests & LoRa Receive (CBOR tests, deserialization, RX task, mock loopback)
- Phase 3 -- Real Sensor Integration (BME280 on RAK4631, sensor backend abstraction)
- Phase 4 -- Mesh Networking (node addressing, flooding with TTL, RSSI contention forwarding, neighbor routing table)
- Phase 5 -- Network Security (AES-128-CMAC auth, network key provisioning, key rotation via key_id)

**Planned:**
- Phase 6 -- Runtime Configuration (NVS-backed settings, remote config via mesh)
- Phase 7 -- Gateway & Cloud (Heltec WiFi uplink, low-power optimization)

See `PLAN.md` for the full roadmap.

## License

GNU Affero General Public License v3 -- see `LICENSE`.
