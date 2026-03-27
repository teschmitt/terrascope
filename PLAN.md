# Terrascope Development Plan

Created: 2026-03-25

## Known Issues

- **Sensor data is random mock** — sensor_manager.c uses sys_rand32_get(), no real driver integration


## Roadmap

### Phase 1 — Housekeeping & Quality (low risk, high value)
- [x] 1. Fix version.h — wire git commit hash into the build via CMake configure_file
- [x] 2. Resolve node_status.h duplication — removed orphaned header (messages.h is canonical)
- [x] 3. Implement timestamps — uses k_uptime_seconds() in main.c and sensor_manager.c
- [x] 4. Commit pending changes (header guard renames, logging extraction, version logging, coding guidelines)
- [x] 5. Add basic CI build check — GitHub Actions matrix build for all three boards

### Phase 2 — Tests & LoRa Receive
- [x] 6. Add ztest infrastructure — extracted cbor module, created tests/cbor/ targeting qemu_riscv64
- [x] 7. Unit test CBOR serialization — 4 tests: both message types, invalid type, buffer overflow
- [x] 8. Add `west twister` to CI — separate test job in GitHub Actions workflow
- [x] 9. Write tests for CBOR deserialization (TDD) — 4 tests: roundtrip telemetry/node_status, truncated buffer, empty buffer
- [x] 10. Implement `cbor_deserialize` in src/lora/cbor.c — all 8 tests passing
- [x] 11. Add incoming zbus channel (`ts_lora_in_chan`) — carries `ts_msg_lora_incoming` (decoded msg + RSSI/SNR)
- [x] 12. Add LoRa receive task — `lora_in_task` polls radio, deserializes CBOR, publishes to `ts_lora_in_chan`
- [x] 13. Extend mock driver — loopback via k_msgq: sent packets are queued and returned by recv

### Phase 3 — Real Sensor Integration
- [x] 14. RAK4631 sensor drivers — BME280 on I2C0, conditional compile via DT_HAS_COMPAT_STATUS_OKAY
- [x] 15. Sensor abstraction — make sensor_manager work with both real and mock backends

### Phase 4 — Mesh Networking
- [x] 16. Write tests for routing logic (TDD) — test node addressing, TTL decrement, duplicate detection before implementing
- [x] 17. Node addressing — assign unique IDs, add source/destination to messages
- [x] 18. Simple flooding protocol — rebroadcast received messages with TTL
- [x] 18a. RSSI-based contention forwarding — delay rebroadcast by signal strength, cancel on duplicate
- [x] 19. Routing table — track neighbors, implement basic multi-hop routing

### Phase 5 — Network Security
- [x] 20. Network key provisioning — `CONFIG_TS_NETWORK_KEY` Kconfig hex string, `src/lora/auth.h` module
- [x] 21. Write tests for auth module (TDD) — sign/verify roundtrip, tampered payload/tag, truncated tag, zero-length, NULL input
- [x] 22. Implement message authentication — AES-128-CMAC via PSA Crypto; 8-byte tag appended after CBOR; verify before deserialize on RX
- [x] 23. Key rotation groundwork — `key_id` field in route header and CBOR; `CONFIG_TS_KEY_ID`; reject on mismatch
- [ ] 24. Hardware-backed key storage — CryptoCell KDR on nRF52840, eFuse on ESP32; per-device keys via HKDF (see SECURITY.md)

### Phase 6 — Runtime Configuration

The goal is to make behavioral parameters adjustable on a running device without recompiling and reflashing. Values are stored in NVS flash and loaded at boot; compile-time `#define` values become defaults for first boot or factory reset.

Note: constants that size static arrays (`TS_ROUTING_SEEN_CACHE_SIZE`, `TS_CONTENTION_POOL_SIZE`, `TS_ROUTING_TABLE_SIZE`) cannot be runtime-configurable without dynamic allocation. They remain as Kconfig symbols (compile-time) and are excluded from the runtime config store.

- [ ] 25. Define configuration schema — create `src/config/config.h` with `struct ts_config` grouping all runtime-tunable parameters: routing TTL and RSSI bounds, contention delays, routing table stale timeout, node ID, LoRa radio parameters (frequency, SF, BW, CR), sensor poll interval, heartbeat interval; retain existing `#define TS_*` constants as `TS_*_DEFAULT` fallbacks for first boot
- [ ] 26. Write tests for config module (TDD) — defaults returned when store is empty; stored value survives re-init without erase; out-of-range value on `set` returns `-EINVAL`; unknown key on `set` returns `-ENOENT`; `reset` restores all defaults
- [ ] 27. Implement config persistence — `src/config/config.c` using Zephyr Settings subsystem (`CONFIG_SETTINGS=y`, NVS backend); `ts_config_init()` calls `settings_load()` at boot and fills any missing key with its default; `ts_config_set(key, value)` validates range, updates the live struct, and calls `settings_save_one()`; `ts_config_reset()` erases all keys and reloads defaults; `ts_config_get()` returns a `const struct ts_config *` for read-only access
- [ ] 28. Migrate modules to config lookups — replace direct use of `TS_ROUTING_DEFAULT_TTL`, `TS_CONTENTION_DELAY_MAX_MS`, `TS_CONTENTION_RSSI_WEAK` / `_STRONG`, `TS_ROUTING_TABLE_STALE_TIMEOUT_S`, etc. with `ts_config_get()->field` in routing.c, contention.c, routing_table.c, sensor_manager.c, and main.c; call `ts_config_init()` in `main()` before all other module inits
- [ ] 29. Remote config via LoRa mesh — add `TS_MSG_CONFIG_SET` and `TS_MSG_CONFIG_ACK` message types; a gateway or admin node can push a key-value pair to any node ID over the mesh; receiving node calls `ts_config_set()` and replies with an ack carrying the applied value; protected by the Phase 5 auth MAC so only authenticated nodes can issue config changes

### Phase 7 — Gateway & Cloud
- [ ] 30. Gateway role for Heltec (WiFi-capable) — aggregate mesh data, uplink to cloud
- [ ] 31. Low-power optimization — sleep scheduling, duty cycling for battery nodes

## Notes

The foundation (zbus, CBOR, mock LoRa, modular structure) is solid. This plan sequences work so each phase builds on the previous one, and early phases reduce technical debt before adding complexity.

TDD approach: write unit tests before implementation for pure logic (CBOR deserialization, routing algorithms). Hardware-dependent code (driver init, zbus wiring, sensor reads) is validated via QEMU integration builds rather than unit tests.

Coding guidelines were added to CLAUDE.md covering naming, module structure, error handling, types, and memory conventions.

Phase 1 complete. Phase 2 complete. Full TX→RX loopback verified in QEMU. Phase 5 (tasks 20–23) complete: AES-128-CMAC auth, key_id rotation, 55 tests passing.
