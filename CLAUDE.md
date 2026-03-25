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
west build -b heltec_wifi_lora32_v2/esp32/procpu -p

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

## Coding Guidelines

### Naming

- All application types, structs, enums, and zbus channels use `ts_` prefix
- Enum typedefs use `_t` suffix (e.g., `ts_msg_type_t`); structs do not (e.g., `struct ts_msg_telemetry`)
- Functions and variables: `snake_case`
- Macros and constants: `UPPER_SNAKE_CASE`
- Output pointer parameters: `p_` prefix (e.g., `uint8_t* p_buf`, `size_t* p_size`)

### Header Guards

- Format: `#ifndef TS_<MODULE>_H` / `#define TS_<MODULE>_H` / `#endif  // TS_<MODULE>_H`

### Includes

- Order: Zephyr/system headers (`<zephyr/...>`) first, then project headers (`"module/header.h"`)
- `<zephyr/logging/log.h>` placed last in includes, immediately before `LOG_MODULE_REGISTER()`

### Module Structure

- One directory per module with paired `module.h` + `module.c`
- Each `.c` file registers its own `LOG_MODULE_REGISTER(name)`
- Public API in header, private functions `static` in source
- Cross-module channel access via `extern struct zbus_channel`

### Error Handling

- Functions return `int`: 0 = success, negative errno on failure (`-ENODEV`, `-EIO`, `-EINVAL`, `-ENOMEM`)
- Log errors with `LOG_ERR()` at the point of failure with relevant context
- In event loops: log error and `continue` on recoverable failures

### Types

- Use explicit fixed-width types (`uint32_t`, `int16_t`, etc.) — never bare `int` for data fields
- Use `const` for pointer-to-device and read-only data
- Use Zephyr's `BUILD_ASSERT` for compile-time validation of devicetree nodes

### Memory

- Static allocation only — no `malloc`/dynamic allocation
- Stack-allocated buffers for known sizes; pass buffer length explicitly

### Comments

- Explain "why", not "what" — code should be self-documenting
- Use `//` for inline comments; keep them minimal
- Mark incomplete work with `// TODO:` at the point of issue
