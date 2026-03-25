# Terrascope Development Plan

Created: 2026-03-25

## Known Issues

- **Sensor data is random mock** — sensor_manager.c uses sys_rand32_get(), no real driver integration
- **No LoRa receive path** — mock driver returns -ENOTSUP for recv, no receive task exists
- **No tests** — no unit or integration test infrastructure

## Roadmap

### Phase 1 — Housekeeping & Quality (low risk, high value)
- [x] 1. Fix version.h — wire git commit hash into the build via CMake configure_file
- [x] 2. Resolve node_status.h duplication — removed orphaned header (messages.h is canonical)
- [x] 3. Implement timestamps — uses k_uptime_seconds() in main.c and sensor_manager.c
- [x] 4. Commit pending changes (header guard renames, logging extraction, version logging, coding guidelines)
- [ ] 5. Add basic CI build check (GitHub Actions with Zephyr Docker image)

### Phase 2 — LoRa Receive & Message Handling
- [ ] 6. Add a LoRa receive task — subscribe to incoming radio messages, deserialize CBOR
- [ ] 7. Extend mock driver — simulate incoming messages for QEMU testing
- [ ] 8. Add incoming zbus channel (ts_lora_in_chan) for received messages

### Phase 3 — Real Sensor Integration
- [ ] 9. RAK4631 sensor drivers — wire up actual I2C/SPI sensors (BME280 or similar)
- [ ] 10. Sensor abstraction — make sensor_manager work with both real and mock backends

### Phase 4 — Mesh Networking
- [ ] 11. Node addressing — assign unique IDs, add source/destination to messages
- [ ] 12. Simple flooding protocol — rebroadcast received messages with TTL
- [ ] 13. Routing table — track neighbors, implement basic multi-hop routing

### Phase 5 — Gateway & Cloud
- [ ] 14. Gateway role for Heltec (WiFi-capable) — aggregate mesh data, uplink to cloud
- [ ] 15. Low-power optimization — sleep scheduling, duty cycling for battery nodes

## Notes

The foundation (zbus, CBOR, mock LoRa, modular structure) is solid. This plan sequences work so each phase builds on the previous one, and early phases reduce technical debt before adding complexity.

Coding guidelines were added to CLAUDE.md covering naming, module structure, error handling, types, and memory conventions.

Phase 1 items 1-4 complete. Item 5 (CI) remains. Later phases should be planned in detail before implementation.
