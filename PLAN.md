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
- [ ] 6. Add ztest infrastructure — `tests/` directory with CMakeLists.txt, prj.conf, testcase.yaml targeting qemu_riscv64
- [ ] 7. Unit test CBOR serialization — test `cbor_serialize` for both message types, verify output against known-good CBOR
- [ ] 8. Unit test message construction — verify struct packing, timestamp population, enum values
- [ ] 9. Add `west twister` to CI — run ztest suite as part of the GitHub Actions workflow
- [ ] 10. Add a LoRa receive task — subscribe to incoming radio messages, deserialize CBOR
- [ ] 11. Extend mock driver — simulate incoming messages for QEMU testing
- [ ] 12. Add incoming zbus channel (ts_lora_in_chan) for received messages

### Phase 3 — Real Sensor Integration
- [ ] 13. RAK4631 sensor drivers — wire up actual I2C/SPI sensors (BME280 or similar)
- [ ] 14. Sensor abstraction — make sensor_manager work with both real and mock backends

### Phase 4 — Mesh Networking
- [ ] 15. Node addressing — assign unique IDs, add source/destination to messages
- [ ] 16. Simple flooding protocol — rebroadcast received messages with TTL
- [ ] 17. Routing table — track neighbors, implement basic multi-hop routing

### Phase 5 — Gateway & Cloud
- [ ] 18. Gateway role for Heltec (WiFi-capable) — aggregate mesh data, uplink to cloud
- [ ] 19. Low-power optimization — sleep scheduling, duty cycling for battery nodes

## Notes

The foundation (zbus, CBOR, mock LoRa, modular structure) is solid. This plan sequences work so each phase builds on the previous one, and early phases reduce technical debt before adding complexity.

Coding guidelines were added to CLAUDE.md covering naming, module structure, error handling, types, and memory conventions.

Phase 1 complete. Later phases should be planned in detail before implementation.
