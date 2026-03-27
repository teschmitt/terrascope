# Building Terrascope: A LoRa Mesh Sensor Node Firmware

This tutorial walks you through how Terrascope was designed and built — not just
*what* the code does, but *why* each decision was made. It is aimed at
programmers who are comfortable with C and have written some application-layer
code, but who have not yet built embedded firmware for a real-time operating
system.

---

## Table of Contents

1. [What We Are Building](#1-what-we-are-building)
2. [Why Zephyr RTOS?](#2-why-zephyr-rtos)
3. [Development Environment](#3-development-environment)
4. [The Build System: west, CMake, and Kconfig](#4-the-build-system-west-cmake-and-kconfig)
5. [Hardware Abstraction: Devicetree Overlays](#5-hardware-abstraction-devicetree-overlays)
6. [Defining the Message Contract](#6-defining-the-message-contract)
7. [Inter-Module Communication with Zbus](#7-inter-module-communication-with-zbus)
8. [LoRa Radio: Initialization and Threading](#8-lora-radio-initialization-and-threading)
9. [CBOR Serialization: Encoding Messages for the Air](#9-cbor-serialization-encoding-messages-for-the-air)
10. [Sensor Integration: Compile-Time Backend Selection](#10-sensor-integration-compile-time-backend-selection)
11. [Mesh Networking: Flooding with TTL](#11-mesh-networking-flooding-with-ttl)
12. [Contention Forwarding: RSSI-Based Delay](#12-contention-forwarding-rssi-based-delay)
13. [Neighbor Tracking: The Routing Table](#13-neighbor-tracking-the-routing-table)
14. [Testing Without Hardware](#14-testing-without-hardware)
15. [The Main Loop: Tying It Together](#15-the-main-loop-tying-it-together)

---

## 1. What We Are Building

Terrascope is a firmware for sensor nodes in a LoRa mesh network. Each node:

- Reads environmental data (temperature, humidity, pressure) from a BME280
  sensor
- Transmits that data over LoRa radio using CBOR encoding
- Forwards messages from other nodes toward a gateway (mesh flooding)
- Tracks which neighbors it can hear and how well

The typical deployment is a field of battery-powered nodes spread across a
remote area. No Wi-Fi, no cellular. The only transport is LoRa — a
low-bandwidth, long-range radio protocol. This constraint shapes every design
decision.

The supported hardware targets are:

| Board | SoC | Notes |
|---|---|---|
| RAK4631 | nRF52840 + SX1262 | Primary sensor node |
| Heltec WiFi LoRa32 V2 | ESP32 + SX1276 | Future gateway role |
| QEMU RISC-V 64 | (simulated) | Development and testing |

---

## 2. Why Zephyr RTOS?

An RTOS (Real-Time Operating System) gives you threads, timers, queues, and
synchronization primitives while keeping deterministic timing. For a sensor node
you could write bare-metal code — but you would have to build all of those
primitives yourself, for every new chip you port to.

Zephyr was chosen for three reasons:

**Unified hardware abstraction.** The BME280 sensor, LoRa radio, and GPIO are
all accessed through the same Zephyr driver APIs regardless of whether you are
on nRF52840, ESP32, or the RISC-V QEMU target. Porting to a new board is
mostly a matter of writing a board configuration, not rewriting driver code.

**The west build system.** Zephyr uses `west` — a meta-tool that manages
multiple git repositories (Zephyr itself, its HALs, your application) and wraps
CMake. This means `west build -b rak4631 -p` does everything: fetches the right
HAL, generates code from devicetree, compiles, and links. One command per
target.

**A real message bus.** Zephyr ships with Zbus, a publish-subscribe message bus
that lets modules communicate without knowing about each other. This is the
architectural backbone of Terrascope and is explored in detail in
[Section 7](#7-inter-module-communication-with-zbus).

---

## 3. Development Environment

All development happens inside a devcontainer based on
`ghcr.io/zephyrproject-rtos/zephyr-build:main`. This container has the Zephyr
toolchains, west, CMake, and QEMU pre-installed.

**Why a devcontainer?** Zephyr's toolchains and SDK versions are very
specific — wrong versions produce broken binaries with no obvious error. A
container pins all of this so every developer (and CI) works from the same
environment. It eliminates the "works on my machine" problem for an entire class
of build failures.

After cloning and opening the container, run first-time setup once:

```bash
west init -l .
west update --narrow --fetch-opt=--depth=1
```

`west init -l .` reads `west.yml` in the repository and sets up the workspace
(tells west where the Zephyr RTOS tree lives). The `--narrow` and
`--fetch-opt=--depth=1` flags limit what gets fetched — Zephyr's full git
history is enormous and you almost never need it.

---

## 4. The Build System: west, CMake, and Kconfig

Zephyr uses three layered systems for configuration and compilation:

```
west        — invokes CMake with the right board/target arguments
  └── CMake — locates sources, handles conditional compilation
        └── Kconfig — software feature flags (enabled/disabled subsystems)
```

### CMakeLists.txt: Conditional Sources

The top-level [CMakeLists.txt](CMakeLists.txt) does something important — it
selects which source files to compile based on what hardware the devicetree
describes:

```cmake
# Sensor backend: real BME280 when available in devicetree, mock otherwise
dt_comp_path(bme280_path COMPATIBLE "bosch,bme280")
if(DEFINED bme280_path)
    dt_node_has_status(bme280_okay PATH ${bme280_path} STATUS "okay")
else()
    set(bme280_okay FALSE)
endif()
if(bme280_okay)
    target_sources(app PRIVATE src/sensors/sensor_bme280.c)
else()
    target_sources(app PRIVATE src/sensors/sensor_mock.c)
endif()
```

Why do this at build time instead of at runtime? Because on a microcontroller,
dead code costs flash memory. The BME280 driver and the mock driver both
implement the same `ts_sensor_backend_read()` function — exactly one of them is
compiled in. No `#ifdef` scattered through the logic, no runtime branch.

The CMakeLists.txt also extracts the current git commit hash and bakes it into
a generated `version.h`:

```cmake
execute_process(
    COMMAND git rev-parse --short HEAD
    ...
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    ...
)
configure_file(
    ${CMAKE_SOURCE_DIR}/src/version.h.in
    ${CMAKE_BINARY_DIR}/generated/version.h
    @ONLY
)
```

This means every firmware image carries its own git hash. When a deployed node
is logging something unexpected, you can match the log output to an exact commit.
On an embedded device you cannot run `git log`.

### prj.conf: Kconfig Feature Flags

[prj.conf](prj.conf) is the main Kconfig configuration applied to all boards:

```
CONFIG_LORA=y
CONFIG_LOG=y
CONFIG_LOG_PRINTK=y
CONFIG_LOG_DEFAULT_LEVEL=4
CONFIG_MULTITHREADING=y
CONFIG_ZBUS=y
CONFIG_ZBUS_CHANNEL_NAME=y
CONFIG_ZBUS_OBSERVER_NAME=y
CONFIG_ZBUS_RUNTIME_OBSERVERS=y
CONFIG_ENTROPY_GENERATOR=y
CONFIG_ZCBOR=y
```

Each line enables a Zephyr subsystem. `CONFIG_LORA=y` pulls in the LoRa
driver framework. `CONFIG_ZBUS=y` enables the message bus. `CONFIG_ZCBOR=y`
enables the CBOR codec. None of this code appears in your binary unless you opt
in — Zephyr's Kconfig system is the gating mechanism.

`CONFIG_ZBUS_CHANNEL_NAME=y` and `CONFIG_ZBUS_OBSERVER_NAME=y` add string names
to channels and observers. This is only useful for debugging — the names appear
in log output. It is a zero-cost in production if you set log level to 0, but
invaluable during development.

---

## 5. Hardware Abstraction: Devicetree Overlays

Zephyr uses the **devicetree** (borrowed from Linux) to describe hardware
topology: which sensors are on which bus, at which address, with which GPIO
pins. The application code never hardcodes `I2C bus 0, address 0x76` — it asks
the devicetree.

Board-specific overlays live in [boards/](boards/). Compare the two:

**QEMU** — [boards/qemu_riscv64.overlay](boards/qemu_riscv64.overlay):

```dts
/ {
    aliases {
        lora0 = &lora_mock;
    };

    lora_mock: lora-mock {
        compatible = "zephyr,lora-mock";
        status = "okay";
    };
};
```

**RAK4631** — [boards/rak4631_nrf52840.overlay](boards/rak4631_nrf52840.overlay):

```dts
&i2c0 {
    bme280: bme280@76 {
        compatible = "bosch,bme280";
        reg = <0x76>;
    };
};
```

The QEMU overlay creates a fake `lora0` device (implemented in software). The
RAK4631 overlay adds the BME280 to the I2C bus. In both cases the application
code uses the same alias (`lora0`) and the same compatible string
(`"bosch,bme280"`). The devicetree is the glue.

**Why `compatible` strings?** They are the contract between driver and hardware.
The string `"bosch,bme280"` means: any node with this compatible string uses
the Bosch BME280 driver. CMake's `dt_comp_path` query (shown above) uses this
exact string to decide which sensor backend to compile.

Custom devicetree bindings for the mock LoRa device live in
[dts/bindings/lora/zephyr,lora-mock.yaml](dts/bindings/lora/zephyr,lora-mock.yaml).
The YAML file declares what the mock device node is allowed to contain, so the
devicetree compiler can validate it.

---

## 6. Defining the Message Contract

Before writing any networking or sensor code, Terrascope defines what messages
look like. Everything lives in
[src/messages/messages.h](src/messages/messages.h).

### The Route Header

Every message that travels over the mesh carries a routing header:

```c
struct ts_route_header {
    uint16_t src;
    uint16_t dst;
    uint32_t msg_id;
    uint8_t ttl;
};
```

- `src` and `dst` are 16-bit node addresses. 16 bits supports 65534 unique
  nodes — more than enough for any conceivable deployment.
- `msg_id` is an auto-incrementing counter per source node. Receivers use
  `(src, msg_id)` pairs to detect duplicates without needing a global counter.
- `ttl` (time-to-live) limits how many hops a message can take. It is
  decremented at each relay. When it hits zero, the message is dropped.

### The Outgoing Message: A Tagged Union

```c
struct ts_msg_lora_outgoing {
    struct ts_route_header route;
    ts_msg_type_t type;
    union {
        struct ts_msg_telemetry telemetry;
        struct ts_msg_node_status node_status;
    } data;
};
```

This is a **tagged union** — a common C pattern for polymorphism without
dynamic allocation. The `type` field tells you which arm of the `union` is
valid. The entire struct is a fixed size known at compile time.

Why a union and not separate message types? On an embedded system, you pay for
every byte. The sensor and the main loop both publish to the same Zbus channel.
If they published different struct types, the channel could not have a single
fixed-size message slot. With a tagged union, one channel handles all outgoing
message variants.

### The Incoming Message Wrapper

```c
struct ts_msg_lora_incoming {
    struct ts_msg_lora_outgoing msg;
    int16_t rssi;
    int8_t snr;
};
```

Incoming messages carry the decoded payload plus radio metadata — the received
signal strength (RSSI) and signal-to-noise ratio (SNR). These are used by the
contention forwarding logic (Section 12) and the neighbor table (Section 13).
The upper layers (if you wanted to display "link quality") also use them.

### Fixed-Width Types

Every field uses `uint32_t`, `int16_t`, `uint8_t`, etc. — never bare `int`.
This matters because `int` is platform-dependent in size (it can be 16, 32, or
64 bits). When data is serialized and sent over the radio to a node that might
run a different architecture, you need to know *exactly* how many bytes each
field occupies.

---

## 7. Inter-Module Communication with Zbus

Zbus is Zephyr's publish-subscribe message bus. Instead of modules calling each
other's functions directly (tight coupling), they publish typed messages to
named channels. Subscribers read from those channels independently.

The two main channels are declared in [src/main.c](src/main.c):

```c
ZBUS_CHAN_DEFINE(ts_lora_out_chan, struct ts_msg_lora_outgoing, NULL, NULL,
                 ZBUS_OBSERVERS(ts_lora_out_sub), ZBUS_MSG_INIT(0));

ZBUS_CHAN_DEFINE(ts_lora_in_chan, struct ts_msg_lora_incoming, NULL, NULL,
                 ZBUS_OBSERVERS_EMPTY, ZBUS_MSG_INIT(0));
```

`ts_lora_out_chan` carries outgoing messages. Its single observer is
`ts_lora_out_sub`, the LoRa TX task's subscriber handle. When any producer
publishes, the subscriber wakes up.

`ts_lora_in_chan` carries decoded incoming messages. It currently has no
observers (`ZBUS_OBSERVERS_EMPTY`) — this is where a future gateway module
would subscribe.

### Why Zbus Instead of Function Calls?

Consider the sensor manager publishing a reading:

```c
// sensor_manager.c
int ret = zbus_chan_pub(&ts_lora_out_chan, &out_msg, K_MSEC(200));
```

The sensor manager does not know about the LoRa module. It does not call
`lora_send()`. It publishes to a channel. If the LoRa module is replaced, or a
logging module is added as a second subscriber, the sensor code does not change.

This decoupling also means modules can be tested in isolation. The CBOR tests
in [tests/cbor/](tests/cbor/) call `cbor_serialize()` and `cbor_deserialize()`
without any LoRa hardware, Zbus, or threads.

### Publishing with a Timeout

```c
int ret = zbus_chan_pub(&ts_lora_out_chan, &out_msg, ZBUS_SEND_TIMEOUT);
log_chan_pub_ret(ret);
```

The timeout (`K_MSEC(200)`) prevents the publisher from blocking forever if the
channel is busy. On an embedded system, "block forever" means the watchdog
eventually resets the device. The `log_chan_pub_ret()` helper in
[src/logging/logging.c](src/logging/logging.c) translates the error code into a
meaningful log message.

### Declaring a Channel in Multiple Files

`ts_lora_out_chan` is defined once in `main.c` and referenced in other modules
with `extern`:

```c
// sensor_manager.c, contention.c, lora.c
extern struct zbus_channel ts_lora_out_chan;
```

This is standard C linkage. The channel is a global — one definition, multiple
declarations. You need the `extern` in each file that uses it but does not own
it.

---

## 8. LoRa Radio: Initialization and Threading

The LoRa module ([src/lora/lora.c](src/lora/lora.c)) has three distinct parts:
initialization, a TX thread, and an RX thread.

### Initialization via SYS_INIT

```c
static int lora_init(void) {
    lora_dev = DEVICE_DT_GET(DT_ALIAS(lora0));
    if (!device_is_ready(lora_dev)) {
        LOG_ERR("LoRa device not ready");
        return -ENODEV;
    }

    struct lora_modem_config config;
    lora_config_ready_device(&config);

    if (lora_config(lora_dev, &config) < 0) {
        LOG_ERR("LoRa config failed");
        return -EIO;
    }
    return 0;
}

SYS_INIT(lora_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
```

`SYS_INIT` registers a function to run automatically during Zephyr's boot
sequence, before `main()`. The `APPLICATION` phase runs after all drivers are
ready. This means by the time `main()` executes, the LoRa device is already
configured.

`DEVICE_DT_GET(DT_ALIAS(lora0))` obtains the device handle via the devicetree
alias `lora0`. Whether that resolves to the SX1262 on RAK4631 or the mock
driver on QEMU is determined by the overlay at build time, not runtime.

The radio is configured at 865.1 MHz, spreading factor 10, bandwidth 125 kHz,
coding rate 4/5. These settings determine the range/throughput tradeoff — SF10
at 125 kHz gives roughly 980 bps but excellent range and interference
resistance. LoRa parameters cannot be changed at runtime without disrupting
communication with other nodes.

### Threads for TX and RX

The TX and RX tasks each run in their own thread, created statically:

```c
K_THREAD_DEFINE(lora_out_tid, LORA_OUT_THREAD_STACK_SIZE, lora_out_task, NULL,
                NULL, NULL, 3, 0, 0);
K_THREAD_DEFINE(lora_in_tid, LORA_IN_THREAD_STACK_SIZE, lora_in_task, NULL,
                NULL, NULL, 3, 0, 0);
```

Why two threads? Because `lora_recv()` is a blocking call — it waits up to one
second for a packet to arrive. If TX and RX ran in the same thread, the node
could not transmit while it was listening, or listen while it was transmitting.
Separate threads let Zephyr's scheduler interleave them.

Thread priority 3 is higher than `main()` (priority 5 in `prj.conf`). This
ensures the LoRa threads are responsive even if `main()` is busy.

### The TX Loop

```c
int lora_out_task() {
    const struct zbus_channel* chan;

    while (!device_is_ready(lora_dev) || !lora_config_done) {
        LOG_WRN("LoRa device not ready, retrying in 5s");
        k_sleep(K_SECONDS(5));
    }

    while (true) {
        int ret = zbus_sub_wait(&ts_lora_out_sub, &chan, K_FOREVER);
        // ... read message, CBOR-encode, transmit
    }
    return 0;  // unreachable!
}
```

`zbus_sub_wait()` with `K_FOREVER` puts the thread to sleep until a message
arrives on the channel. This is efficient: the thread uses no CPU while waiting.
The `return 0` at the end is marked `unreachable!` — embedded RTOS threads
typically loop forever and never return.

The readiness check before the main loop is a defensive guard. `SYS_INIT` runs
before threads start, but `lora_config_done` is a belt-and-suspenders check to
ensure the radio is fully configured before the first transmit attempt.

---

## 9. CBOR Serialization: Encoding Messages for the Air

LoRa sends raw bytes. Terrascope needs to turn `struct ts_msg_lora_outgoing`
into bytes for transmission and back again on receipt. For this it uses
**CBOR** (Concise Binary Object Representation), via Zephyr's zcbor library.

### Why CBOR and Not JSON or Protobuf?

- **JSON** is human-readable but verbose. A telemetry reading as JSON might
  occupy 80–120 bytes. At 980 bps over LoRa, that costs over 1 second of
  airtime. CBOR encodes the same data in roughly 40–60 bytes.
- **Protobuf** is efficient but requires a separate code-generation step and a
  schema file. zcbor works directly in C without a build-time code generator.
- **Raw struct bytes** (`memcpy` to the wire) would be the most compact option,
  but it creates a fragile binary format: add a field, change endianness, or
  compile on a different platform and the format breaks. CBOR is
  self-describing, so a receiver can validate the structure.

### The Wire Format

Each message is a CBOR map with three top-level keys: `type`, `route`, and
`data`. Here is the serialization of the route header:

```c
static int serialize_route(zcbor_state_t *state,
                           const struct ts_route_header *p_route) {
    if (!zcbor_tstr_put_lit(state, "route") ||
        !zcbor_map_start_encode(state, 5) ||
        !zcbor_tstr_put_lit(state, "src") ||
        !zcbor_uint32_put(state, (uint32_t)p_route->src) ||
        !zcbor_tstr_put_lit(state, "dst") ||
        !zcbor_uint32_put(state, (uint32_t)p_route->dst) ||
        !zcbor_tstr_put_lit(state, "msg_id") ||
        !zcbor_uint32_put(state, p_route->msg_id) ||
        !zcbor_tstr_put_lit(state, "ttl") ||
        !zcbor_uint32_put(state, (uint32_t)p_route->ttl) ||
        !zcbor_tstr_put_lit(state, "key_id") ||
        !zcbor_uint32_put(state, (uint32_t)p_route->key_id) ||
        !zcbor_map_end_encode(state, 5)) {
        return -ENOMEM;
    }
    return 0;
}
```

zcbor uses a "continue encoding or fail" pattern: each call returns `true` on
success. The `||` chain short-circuits on the first failure. If encoding fails
(usually because the buffer is too small), the whole function returns `-ENOMEM`.

The `p_` prefix on the pointer parameter (`p_route`) follows the project naming
convention for output or input pointer parameters. It makes it immediately clear
at call sites that you are passing an address.

### Decoding: The Symmetric Side

Deserialization mirrors serialization exactly — same field order, same keys,
same nesting:

```c
static int deserialize_route(zcbor_state_t *state,
                             struct ts_route_header *p_route) {
    uint32_t src, dst, msg_id, ttl, key_id;

    if (!zcbor_tstr_expect_lit(state, "route") ||
        !zcbor_map_start_decode(state) ||
        !zcbor_tstr_expect_lit(state, "src") ||
        !zcbor_uint32_decode(state, &src) ||
        // ... dst, msg_id, ttl, key_id ...
        !zcbor_map_end_decode(state)) {
        return -EBADMSG;
    }

    p_route->src = (uint16_t)src;
    // ...
    return 0;
}
```

`zcbor_tstr_expect_lit()` checks that the next token is the exact string
literal — it acts as both a read and a validation step. If the wire format
doesn't match expectations the function returns `-EBADMSG`.

The temporary `uint32_t` variables for `src` and `dst` are there because zcbor
decodes integers into `uint32_t`, but the struct uses `uint16_t`. The explicit
cast happens at assignment, making the narrowing visible.

### Buffer Management

```c
static uint8_t cbor_buffer[ZBOR_ENCODE_BUFFER_SIZE];
```

This is a statically allocated buffer — no `malloc()`. On a microcontroller,
dynamic allocation creates fragmentation over time and can fail unpredictably.
Static allocation means the memory is reserved at link time; if the program
fits in flash, you know it will always have that buffer available.

The RX task uses its own separate buffer:

```c
static uint8_t rx_buffer[LORA_RX_BUFFER_SIZE];
```

The `static` keyword here keeps the buffer alive across calls (it lives in BSS,
not on the stack) and prevents the TX and RX threads from sharing memory without
a mutex.

---

## 10. Sensor Integration: Compile-Time Backend Selection

The sensor layer uses a **backend abstraction pattern**. Both sensor
implementations provide the same function signature, declared in
[src/sensors/sensor_backend.h](src/sensors/sensor_backend.h):

```c
int ts_sensor_backend_read(struct ts_msg_telemetry *p_tel);
```

The real BME280 implementation in
[src/sensors/sensor_bme280.c](src/sensors/sensor_bme280.c):

```c
int ts_sensor_backend_read(struct ts_msg_telemetry *p_tel) {
    struct sensor_value val;

    if (!device_is_ready(bme280)) {
        LOG_ERR("BME280 device not ready");
        return -ENODEV;
    }

    int ret = sensor_sample_fetch(bme280);
    if (ret != 0) {
        LOG_ERR("BME280 sample fetch failed: %d", ret);
        return ret;
    }

    // Temperature in centi-degrees C (e.g. 2512 = 25.12 °C)
    sensor_channel_get(bme280, SENSOR_CHAN_AMBIENT_TEMP, &val);
    p_tel->temperature = (uint32_t)(val.val1 * 100 + val.val2 / 10000);

    // Humidity in centi-percent RH (e.g. 6543 = 65.43 %RH)
    sensor_channel_get(bme280, SENSOR_CHAN_HUMIDITY, &val);
    p_tel->humidity = (uint32_t)(val.val1 * 100 + val.val2 / 10000);

    // Pressure in Pa (e.g. 101325 = 1013.25 hPa)
    sensor_channel_get(bme280, SENSOR_CHAN_PRESS, &val);
    p_tel->pressure = (uint32_t)(val.val1 * 1000 + val.val2 / 1000);

    return 0;
}
```

Zephyr's sensor API returns values as `struct sensor_value` — a pair of integer
and micro-unit fractional parts. Temperature 25.12°C comes back as
`val1 = 25`, `val2 = 120000`. The conversion formula `val1 * 100 + val2 / 10000`
produces `2512` — centi-degrees. Integer arithmetic, no floating point.

**Why avoid floating point?** Many microcontrollers (including the nRF52840)
have a hardware floating point unit, so it is not strictly necessary to avoid
it. But integer arithmetic is always faster, never subject to rounding
surprises, and easier to reason about when debugging. Centi-units are a
well-established convention in embedded sensing.

The mock backend in [src/sensors/sensor_mock.c](src/sensors/sensor_mock.c) is
trivial by comparison:

```c
int ts_sensor_backend_read(struct ts_msg_telemetry *p_tel) {
    p_tel->temperature = sys_rand32_get() / 1000000;
    p_tel->humidity = sys_rand32_get() / 1000000;
    p_tel->pressure = sys_rand32_get() / 10000;
    return 0;
}
```

This is not meant to produce realistic values — it is meant to produce *data*,
so the full pipeline (sensor → Zbus → CBOR → LoRa → CBOR → RX) can be
exercised in QEMU without any physical hardware.

The sensor manager in [src/sensors/sensor_manager.c](src/sensors/sensor_manager.c)
calls the backend, packages the reading into an outgoing message, and publishes
to the channel:

```c
void periodic_work_handler(const struct zbus_channel *chan) {
    struct ts_msg_lora_outgoing out_msg = {
        .type = TS_MSG_TELEMETRY,
        .data.telemetry.timestamp = (uint32_t)k_uptime_seconds(),
    };
    ts_routing_prepare_header(&out_msg.route, TS_ROUTING_BROADCAST_ADDR);

    if (ts_sensor_backend_read(&out_msg.data.telemetry) != 0) {
        return;
    }

    int ret = zbus_chan_pub(&ts_lora_out_chan, &out_msg, K_MSEC(200));
    log_chan_pub_ret(ret);
}
```

Telemetry is always sent to the **broadcast address** (`0xFFFF`) — every node
in the mesh can receive it, and any node with a gateway role can forward it to
the cloud. There is no concept of a single sink address.

---

## 11. Mesh Networking: Flooding with TTL

Terrascope uses **flooding** as its routing strategy: when a message is
received, it is rebroadcast so that nodes further away can hear it. Flooding is
simple to implement correctly and resilient to topology changes — nodes come and
go and the network adapts automatically.

The risk with flooding is infinite loops: node A receives from B, forwards to C,
which forwards back to A, which forwards again forever. TTL and duplicate
detection prevent this.

### The Routing Module

[src/routing/routing.c](src/routing/routing.c) owns node identity and duplicate
detection. The state is module-private:

```c
static uint16_t self_node_id;
static uint32_t next_msg_id;

static struct {
    uint16_t src;
    uint32_t msg_id;
} seen_cache[TS_ROUTING_SEEN_CACHE_SIZE];
static uint32_t seen_write_idx;
static uint32_t seen_count;
```

The `seen_cache` is a **ring buffer** of `(src, msg_id)` pairs. When a message
arrives, the RX task checks `ts_routing_is_duplicate()` before processing:

```c
bool ts_routing_is_duplicate(const struct ts_route_header *p_hdr) {
    uint32_t entries = (seen_count < TS_ROUTING_SEEN_CACHE_SIZE)
                           ? seen_count
                           : TS_ROUTING_SEEN_CACHE_SIZE;
    for (uint32_t i = 0; i < entries; i++) {
        if (seen_cache[i].src == p_hdr->src &&
            seen_cache[i].msg_id == p_hdr->msg_id) {
            return true;
        }
    }
    return false;
}
```

If the same `(src, msg_id)` pair arrives again (via a different relay path), it
is discarded. The cache holds 32 entries and evicts the oldest on overflow. In a
sparse network with slow message rates this is sufficient; in a dense network
with high rates you would increase `TS_ROUTING_SEEN_CACHE_SIZE`.

### Preparing a Header for a New Message

```c
void ts_routing_prepare_header(struct ts_route_header *p_hdr, uint16_t dst) {
    p_hdr->src = self_node_id;
    p_hdr->dst = dst;
    p_hdr->msg_id = next_msg_id++;
    p_hdr->ttl = TS_ROUTING_DEFAULT_TTL;
}
```

`msg_id` is a simple counter, auto-incremented per message originating from
this node. Since duplicates are keyed on `(src, msg_id)`, each node only needs
its counter to be unique among its own messages — not globally unique.

`TS_ROUTING_DEFAULT_TTL` is 5. A TTL of 5 means a message can traverse at most
5 hops. In a rural sensor deployment, 5 hops at 10–15 km per hop gives a
potential range of 50–75 km, which is far more than most deployments need.

### TTL Decrement in the RX Path

When a relay node decides to forward a message, it decrements the TTL first:

```c
// from lora_in_task() in lora.c
struct ts_msg_lora_outgoing fwd = in_msg.msg;
if (ts_routing_decrement_ttl(&fwd.route) == 0 && fwd.route.ttl > 0) {
    ret = ts_contention_schedule(&fwd, rssi);
}
```

`ts_routing_decrement_ttl()` returns `-EHOSTUNREACH` if the TTL was already 0
(message has expired). The check `fwd.route.ttl > 0` after decrement ensures
the message still has remaining hops. A message that arrives with TTL=1 after
decrement has TTL=0 and is not forwarded.

### The Broadcast Address

```c
#define TS_ROUTING_BROADCAST_ADDR 0xFFFF
```

```c
bool ts_routing_is_for_us(const struct ts_route_header *p_hdr) {
    return p_hdr->dst == self_node_id ||
           p_hdr->dst == TS_ROUTING_BROADCAST_ADDR;
}
```

All telemetry and heartbeat messages use the broadcast address. A message is
"for us" if it is addressed to our specific node ID or to `0xFFFF`. This means
every node receives every broadcast — and then, if the TTL allows, relays it.

---

## 12. Contention Forwarding: RSSI-Based Delay

Naive flooding causes a **broadcast storm**: if all relay nodes forward
immediately upon receipt, they all transmit at the same time, causing
collisions. Contention forwarding solves this by introducing a delay — and
making the delay depend on how strongly the message was received.

The key insight: a node that heard the message weakly (low RSSI, meaning it is
far from the source) should forward it *sooner*, because it is likely further
from the source and better positioned to extend range. A node that heard it
strongly (high RSSI, close to the source) should wait longer — another node
will probably forward first.

### The RSSI-to-Delay Mapping

```c
uint32_t ts_contention_rssi_to_delay_ms(int16_t rssi) {
    if (rssi <= TS_CONTENTION_RSSI_WEAK) {   // -120 dBm
        return TS_CONTENTION_DELAY_MIN_MS;   // 0 ms
    }
    if (rssi >= TS_CONTENTION_RSSI_STRONG) { // -30 dBm
        return TS_CONTENTION_DELAY_MAX_MS;   // 5000 ms
    }

    int32_t offset = (int32_t)rssi - TS_CONTENTION_RSSI_WEAK;
    int32_t range = TS_CONTENTION_RSSI_STRONG - TS_CONTENTION_RSSI_WEAK;

    return (uint32_t)((offset * TS_CONTENTION_DELAY_MAX_MS) / range);
}
```

RSSI of -120 dBm → 0 ms delay (forward immediately). RSSI of -30 dBm → 5000
ms delay (wait 5 seconds). The linear interpolation uses integer arithmetic,
with the multiply-before-divide ordering to avoid losing precision from integer
truncation.

### The Contention Pool: Pending Forwards

Each pending forward occupies a slot in a small pool:

```c
struct ts_contention_slot {
    struct k_work_delayable work;
    struct ts_msg_lora_outgoing msg;
    uint16_t src;
    uint32_t msg_id;
    bool occupied;
};

static struct ts_contention_slot pool[TS_CONTENTION_POOL_SIZE]; // 4 slots
```

`k_work_delayable` is Zephyr's mechanism for scheduling work to run after a
delay. When the delay expires, `contention_work_handler()` publishes the message
back to `ts_lora_out_chan`, which causes the TX task to transmit it:

```c
static void contention_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct ts_contention_slot *slot =
        CONTAINER_OF(dwork, struct ts_contention_slot, work);

    if (!slot->occupied) {
        return;
    }

    int ret = zbus_chan_pub(&ts_lora_out_chan, &slot->msg, K_MSEC(200));
    // ...
    slot->occupied = false;
}
```

`CONTAINER_OF` is a Zephyr macro that recovers a pointer to the containing
struct from a pointer to a member. Here it converts `struct k_work *` back to
`struct ts_contention_slot *`. This is a common C pattern for embedding
intrusive data structures.

### Cancellation When Another Node Forwards First

If a pending forward is scheduled and then the same message arrives from
another relay (meaning someone already forwarded it), the pending forward is
cancelled:

```c
// from lora_in_task() in lora.c
if (ts_routing_is_duplicate(&in_msg.msg.route)) {
    ts_contention_cancel(in_msg.msg.route.src,
                         in_msg.msg.route.msg_id);
    continue;
}
```

```c
int ts_contention_cancel(uint16_t src, uint32_t msg_id) {
    struct ts_contention_slot *slot = find_slot_by_msg(src, msg_id);
    if (slot == NULL) {
        return -ENOENT;
    }
    k_work_cancel_delayable(&slot->work);
    slot->occupied = false;
    return 0;
}
```

The combination of RSSI-based delay and duplicate cancellation means: the node
furthest from the original sender forwards first; nearby nodes that were about
to forward cancel their pending transmissions when they hear the successful
forward. The result is one retransmission per hop, not N simultaneous ones.

---

## 13. Neighbor Tracking: The Routing Table

As messages pass through the mesh, each relay node learns who its neighbors are
and what the link quality looks like. The routing table in
[src/routing/routing_table.c](src/routing/routing_table.c) stores this.

```c
struct ts_neighbor {
    uint16_t node_id;
    int16_t rssi;
    int8_t snr;
    bool direct;
    uint32_t last_seen;
    bool occupied;
};

static struct ts_neighbor table[TS_ROUTING_TABLE_SIZE]; // 16 entries
```

Every entry has a `direct` flag — set to `true` if the message arrived with
TTL equal to `TS_ROUTING_DEFAULT_TTL`. A message that starts with TTL=5 and
arrives with TTL=5 was received directly from the source, with no relays in
between. If TTL was decremented by relays, the source is a multi-hop neighbor.

```c
bool is_direct = (ttl == TS_ROUTING_DEFAULT_TTL);
```

This inference is clever but not always correct: a direct neighbor who is
briefly unreachable might appear to become indirect if another path exists.
The table uses a conservative rule: once `direct=true`, it is never downgraded
back to `false` by a subsequent indirect receipt.

```c
// Never downgrade direct flag
if (is_direct) {
    entry->direct = true;
}
```

### LRU Eviction

The table holds 16 entries. When it is full and a new neighbor is seen:

```c
entry = find_free_slot();
if (entry == NULL) {
    entry = find_oldest();
    LOG_DBG("Evicting neighbor 0x%04x for 0x%04x", entry->node_id, node_id);
}
```

`find_oldest()` picks the entry with the smallest `last_seen` timestamp. This
is a simple LRU (Least Recently Used) eviction: the neighbor we have not heard
from in the longest time is the one we drop. It is O(n) but with n=16 that is
trivial.

### Aging: Removing Stale Entries

The routing table is periodically pruned in [src/main.c](src/main.c):

```c
k_timer_start(&routing_table_age_timer, K_SECONDS(60), K_SECONDS(60));
```

Every 60 seconds a work item runs:

```c
static void routing_table_age_handler(struct k_work *work) {
    ts_routing_table_age_seconds(TS_ROUTING_TABLE_STALE_TIMEOUT_S);
}
```

`TS_ROUTING_TABLE_STALE_TIMEOUT_S` is 300 seconds (5 minutes). Any neighbor
not heard from in 5 minutes is removed. This prevents the table from filling
with ghost entries when nodes leave the network.

Why a separate timer for aging instead of doing it on every insert? Because
aging with a wall-clock timeout needs to run even when no messages are arriving.
A k_timer fires on a schedule regardless of message traffic.

---

## 14. Testing Without Hardware

Embedded firmware is notoriously difficult to test. You cannot run your code on
a developer laptop and you cannot easily attach a debugger in CI. Terrascope
handles this with two mechanisms: QEMU simulation and Zephyr's Ztest framework.

### The Mock LoRa Driver

[src/drivers/lora_mock.c](src/drivers/lora_mock.c) is a software-only LoRa
driver that implements the full Zephyr LoRa driver API:

```c
static const struct lora_driver_api lora_mock_api = {
    .config = lora_mock_config,
    .send   = lora_mock_send,
    .recv   = lora_mock_recv,
    .test_cw = lora_mock_test_cw,
};
```

The send and receive sides are connected via a `k_msgq` (message queue):

```c
static int lora_mock_send(const struct device* dev, uint8_t* data,
                          uint32_t data_len) {
    struct lora_mock_packet pkt = {0};
    memcpy(pkt.data, data, data_len);
    pkt.len = (uint8_t)data_len;

    int ret = k_msgq_put(&drv_data->rx_msgq, &pkt, K_NO_WAIT);
    // ...
    return 0;
}
```

When `lora_send()` is called, the packet is queued. When `lora_recv()` is
called shortly after, it dequeues the same packet and returns it — as if the
node had received its own transmission. This loopback exercises the full
`TX → CBOR encode → radio → CBOR decode → RX` pipeline in QEMU without any
radio hardware.

The mock returns fixed RSSI `-42` and SNR `10`. These values feed the
contention and routing table logic, ensuring those code paths are exercised too.

### Unit Tests with Ztest

Each testable module has a corresponding test suite in `tests/`. Tests are
built as separate firmware images, each running only the module under test.

A CBOR roundtrip test from [tests/cbor/src/main.c](tests/cbor/src/main.c):

```c
ZTEST(cbor, test_roundtrip_telemetry)
{
    struct ts_msg_lora_outgoing original = {
        .route = {.src = 0x0001, .dst = 0x0002, .msg_id = 99, .ttl = 3},
        .type = TS_MSG_TELEMETRY,
        .data.telemetry = {.timestamp = 1234,
                           .temperature = 2500,
                           .humidity = 6000,
                           .pressure = 101325}};
    uint8_t buf[ZBOR_ENCODE_BUFFER_SIZE];
    size_t size = 0;

    int ret = cbor_serialize(&original, buf, sizeof(buf), &size);
    zassert_ok(ret);

    struct ts_msg_lora_outgoing decoded = {0};
    ret = cbor_deserialize(buf, size, &decoded);

    zassert_ok(ret, "deserialize should succeed");
    zassert_equal(decoded.type, TS_MSG_TELEMETRY);
    zassert_equal(decoded.data.telemetry.temperature, 2500);
    // ...
}
```

`ZTEST(suite_name, test_name)` is the Ztest test macro. `zassert_ok()` fails
the test if the value is not 0. `zassert_equal()` compares two values.

Tests are run via `west twister`:

```bash
west twister -T tests -p qemu_riscv64 --no-shuffle
```

`west twister` discovers test suites automatically, builds them for the
specified board (QEMU), runs them, and reports results. All 55 tests across the
auth, CBOR, routing, contention, and routing table suites run in QEMU in CI.

**Why test CBOR?** Because the serialization format is the protocol between
nodes — a bug in the encoder or decoder silently corrupts data in flight with
no obvious error at the radio layer. Tests like `test_serialize_buffer_too_small`
and `test_deserialize_truncated_buffer` verify that the code handles adversarial
inputs gracefully rather than crashing.

---

## 15. The Main Loop: Tying It Together

[src/main.c](src/main.c) is the entry point. It is deliberately thin — it
initializes subsystems and then runs the heartbeat loop. All the real work
happens in threads and timers.

```c
int main() {
    LOG_INF("Terrascope v%s (%s %s) started", FIRMWARE_VERSION_STRING,
            BUILD_TIMESTAMP, GIT_COMMIT_HASH);

    int auth_ret = ts_auth_init();
    if (auth_ret != 0) {
        LOG_ERR("Failed to initialize auth module: %d", auth_ret);
        return auth_ret;
    }

    ts_routing_init(TS_NODE_ID);
    ts_routing_table_init();
    LOG_INF("Node ID: 0x%04x", ts_routing_get_node_id());

    k_timer_start(&sensor_periodic_timer, K_SECONDS(1), K_SECONDS(10));
    k_timer_start(&routing_table_age_timer, K_SECONDS(60), K_SECONDS(60));

    while (true) {
        k_sleep(K_SECONDS(7));
        // ... publish TS_MSG_NODE_STATUS heartbeat
    }
    return 0;
}
```

The sensor timer fires after 1 second and then every 10 seconds. The 1-second
initial delay lets the system settle before the first reading. The routing table
aging timer fires every 60 seconds.

The heartbeat is published every 7 seconds. Heartbeats let other nodes know
this node is alive and update their neighbor tables. The 7-second interval is
offset from the 10-second sensor interval to spread traffic — if both fired at
the same time you would get a burst of LoRa activity.

### Timer → Work → System Workqueue Pattern

```c
K_WORK_DEFINE(sensor_take_reading, sensor_take_reading_wrapper);

void sensor_periodic_timer_handler(struct k_timer* dummy) {
    k_work_submit(&sensor_take_reading);
}

K_TIMER_DEFINE(sensor_periodic_timer, sensor_periodic_timer_handler, NULL);
```

Timer handlers run in interrupt context — you cannot do I2C reads, floating
point, or long operations there. The pattern is: the timer handler submits a
work item to the **system workqueue** (a dedicated thread). The work item runs
in thread context, where blocking and long operations are safe.

`K_WORK_DEFINE` and `K_TIMER_DEFINE` are static initializers — they allocate
and initialize the objects at compile time, not runtime. No `malloc()`, no
constructor calls at boot.

### The Module Hierarchy

Looking at the include structure of `main.c` tells you the dependency graph:

```c
#include "logging/logging.h"
#include "lora/auth.h"
#include "lora/lora.h"
#include "messages/messages.h"
#include "routing/routing.h"
#include "routing/routing_table.h"
#include "sensors/sensor_manager.h"
```

`main.c` knows about all modules. But the modules themselves have narrow
dependencies:

- `sensor_manager.c` knows about `messages/` and `routing/`
- `lora/lora.c` knows about `lora/auth.h`, `lora/contention.h`, and `routing/`
- `lora/cbor.c` knows only about `messages/`
- `lora/auth.c` knows only about its own header and PSA Crypto
- `routing/routing.c` knows only about its own header

This layered structure means you can test `cbor.c` without linking in the
routing or sensor modules. It also means you can replace `sensor_bme280.c` with
`sensor_mock.c` without touching anything else. Good module design keeps
coupling minimal.

---

## Summary: Key Patterns to Carry Forward

| Pattern | Where Used | Why |
|---|---|---|
| Tagged union | `messages.h` | Single channel type for all message variants |
| Zbus publish-subscribe | All modules | Decoupled communication, testability |
| `SYS_INIT` | `lora.c` | Initialize hardware before `main()` |
| Static allocation | Everywhere | No heap fragmentation |
| Compile-time backend selection | `CMakeLists.txt` | Zero dead code in flash |
| Devicetree overlays | `boards/` | One codebase, multiple hardware targets |
| Timer → Work queue | `main.c` | Defer interrupt-context work to thread context |
| Ring buffer duplicate cache | `routing.c` | Loop prevention without global state |
| RSSI-based contention | `contention.c` | Flood control without coordination |
| Separate TX/RX threads | `lora.c` | Simultaneous listen and transmit |
| Fixed-width types | All structs | Cross-platform wire format correctness |
| Mock driver loopback | `lora_mock.c` | Full pipeline testing without hardware |
| PSA Crypto key store | `auth.c` | Hardware-portable key management |
| Truncated CMAC tag | `lora.c` TX/RX | Auth with minimal LoRa airtime overhead |

These are not Terrascope-specific ideas — they are embedded systems fundamentals.
The same patterns appear in production IoT firmware across industries.
