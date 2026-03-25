# Terrascope Development Plan

Created: 2026-03-25

## Known Issues

- **Sensor data is random mock** — sensor_manager.c uses sys_rand32_get(), no real driver integration
- **No LoRa receive path** — mock driver returns -ENOTSUP for recv, no receive task exists


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
- [ ] 8. Add `west twister` to CI — run ztest suite as part of the GitHub Actions workflow
- [ ] 9. Write tests for CBOR deserialization (TDD) — test `cbor_deserialize` before implementing it
- [ ] 10. Implement `cbor_deserialize` in src/lora/cbor.c — decode incoming CBOR back to `ts_msg_lora_outgoing`
- [ ] 11. Add incoming zbus channel (`ts_lora_in_chan`) for received messages
- [ ] 12. Add a LoRa receive task — subscribe to incoming radio, deserialize CBOR, publish to `ts_lora_in_chan`
- [ ] 13. Extend mock driver — simulate incoming messages for QEMU testing

### Phase 3 — Real Sensor Integration
- [ ] 14. RAK4631 sensor drivers — wire up actual I2C/SPI sensors (BME280 or similar)
- [ ] 15. Sensor abstraction — make sensor_manager work with both real and mock backends

### Phase 4 — Mesh Networking
- [ ] 16. Write tests for routing logic (TDD) — test node addressing, TTL decrement, duplicate detection before implementing
- [ ] 17. Node addressing — assign unique IDs, add source/destination to messages
- [ ] 18. Simple flooding protocol — rebroadcast received messages with TTL
- [ ] 19. Routing table — track neighbors, implement basic multi-hop routing

### Phase 5 — Gateway & Cloud
- [ ] 20. Gateway role for Heltec (WiFi-capable) — aggregate mesh data, uplink to cloud
- [ ] 21. Low-power optimization — sleep scheduling, duty cycling for battery nodes

## Notes

The foundation (zbus, CBOR, mock LoRa, modular structure) is solid. This plan sequences work so each phase builds on the previous one, and early phases reduce technical debt before adding complexity.

TDD approach: write unit tests before implementation for pure logic (CBOR deserialization, routing algorithms). Hardware-dependent code (driver init, zbus wiring, sensor reads) is validated via QEMU integration builds rather than unit tests.

Coding guidelines were added to CLAUDE.md covering naming, module structure, error handling, types, and memory conventions.

Phase 1 complete. Phase 2 items 6–7 complete. Later phases should be planned in detail before implementation.
