# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Terrascope is a Zephyr RTOS firmware for LoRa-enabled mesh sensor nodes. It targets embedded hardware (RAK4631, Heltec WiFi LoRa32 V2) and can be simulated with QEMU RISC-V.

## Build Commands

The project uses Zephyr's `west` build system inside a devcontainer (`ghcr.io/zephyrproject-rtos/zephyr-build:main`).

```bash
# Build for QEMU (simulation with mock LoRa driver)
west build -b qemu_riscv64 -p

# Build for hardware targets
west build -b rak4631 -p
west build -b heltec_wifi_lora32_v2/procpu -p

# Run in QEMU
west build -t run

# Clean build
west build -t clean
```

## Architecture

### Communication Bus (Zbus)

The firmware uses **Zephyr Zbus** as its central message bus. All inter-module communication flows through typed zbus channels:

- `ts_lora_out_chan` (defined in `main.c`) — carries `struct ts_msg_lora_outgoing` messages from producers to the LoRa transmit task
- Modules publish to the channel; the LoRa output subscriber (`ts_lora_out_sub`) receives and transmits

### Message Flow

1. **Sensor manager** (`src/sensors/`) reads sensors on a periodic timer (K_TIMER → K_WORK → system workqueue), publishes `TS_MSG_TELEMETRY` to the outgoing channel
2. **Main loop** periodically publishes `TS_MSG_NODE_STATUS` heartbeats
3. **LoRa output task** (`src/lora/`) runs in its own thread, subscribes to the channel, CBOR-encodes messages with zcbor, and transmits via the LoRa radio

### Message Types

Defined in `src/messages/messages.h` — a tagged union (`ts_msg_lora_outgoing`) with:
- `TS_MSG_TELEMETRY` — temperature, humidity, pressure
- `TS_MSG_NODE_STATUS` — uptime, status

### Key Modules

| Module | Path | Role |
|--------|------|------|
| LoRa | `src/lora/` | Device init (SYS_INIT), config, outgoing thread, CBOR serialization |
| Sensors | `src/sensors/` | Periodic sensor readings via work queue |
| Messages | `src/messages/` | Shared message type definitions |
| Logging | `src/logging/` | Zbus publish error logging helper |
| Mock LoRa driver | `src/drivers/lora_mock.c` + `drivers/lora/` | Simulation driver, enabled via `CONFIG_LORA_MOCK=y` |

### Board Configuration

Per-board configs live in `boards/`. The QEMU target enables the mock LoRa driver and wires it via a devicetree overlay (`boards/qemu_riscv64.overlay`). Custom DT bindings are in `dts/bindings/`.

## Code Style

- C code formatted with clang-format (Google base style, 4-space indent, 80-col limit) — see `.clang-format`
- All types/structs use `ts_` prefix
- Zephyr logging: each module registers its own `LOG_MODULE_REGISTER(name)`
- LoRa device configured at 865.1 MHz, SF10, BW 125 kHz, CR 4/5
