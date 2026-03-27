# Terrascope Security Architecture

This document describes the threat model, planned mitigations, and hardware-specific
key protection options for the Terrascope LoRa mesh network. It corresponds to
Phase 5 of [PLAN.md](PLAN.md).

---

## Threat Model

Terrascope nodes communicate over an unencrypted radio channel. Any device within
LoRa range can receive packets, and — without authentication — inject arbitrary
packets that appear to originate from legitimate nodes. The primary threats are:

| Threat | Description |
|--------|-------------|
| **Spoofing** | Rogue device injects packets claiming to be a legitimate node |
| **Replay** | Attacker re-transmits a previously captured legitimate packet |
| **Tampering** | Attacker modifies a relayed packet in transit |
| **Key extraction** | Attacker physically obtains a node and reads key material from flash |

The current implementation (Phase 4) does not address any of these. The following
sections describe how each will be mitigated in Phase 5.

---

## Message Authentication

Every packet will carry an 8-byte AES-128-CMAC tag computed over the authenticated
fields of the message:

```
tag = AES-128-CMAC(network_key, src || dst || msg_id || key_id || type || cbor_payload)
```

The tag is appended after the CBOR payload before transmission and verified before
deserialization on receive. Packets with a missing or invalid tag are silently
dropped.

AES-128-CMAC is chosen over HMAC-SHA256 because:
- The nRF52840 CryptoCell-310 and ESP32 HMAC peripheral accelerate it in hardware
- 128-bit keys fit neatly into hardware key stores on both platforms
- 8-byte tags provide 64 bits of MAC security — adequate for the low-rate, low-volume
  traffic of a sensor mesh

### Replay protection

The existing `(src, msg_id)` ring-buffer deduplication cache (see `routing.c`)
already rejects replayed packets within its window (`TS_ROUTING_SEEN_CACHE_SIZE`
entries). A successfully authenticated packet with a duplicate `(src, msg_id)` pair
is dropped before it reaches the application.

---

## Key Management

### Development builds

During development, the network key is supplied as a Kconfig hex string
(`CONFIG_TS_NETWORK_KEY`) and compiled directly into the firmware. This is
acceptable for QEMU simulation and bench testing but **must not be used in
deployed hardware** — the key is readable from any firmware dump.

### Production builds — shared network key

All nodes in a deployment share the same 128-bit network key. The key is
provisioned once per device at manufacturing time and stored in platform-specific
secure hardware (see [Platform-Specific Key Storage](#platform-specific-key-storage)
below). A `key_id` byte in the route header allows nodes to reject messages
encrypted under a different key version and enables future key rotation without
a hard protocol break.

### Production builds — per-device key model (recommended)

A shared network key means that physically compromising one node exposes the key
for the entire network. A stronger model uses a **unique key per device**:

```
per_device_key = HKDF(master_provisioning_secret, hw_device_id || "terrascope-v1")
```

The `master_provisioning_secret` is generated once and stored securely on the
provisioning machine — it is never flashed onto any node. The `hw_device_id` is
the hardware serial number or BLE MAC address burned into the chip at manufacture.

Each node's individual key is burned into its hardware key store at provisioning
time. Compromising one device reveals only that device's key; the rest of the
network is unaffected.

---

## Platform-Specific Key Storage

### nRF52840 (RAK4631)

**CryptoCell-310 KDR (Key Derivation Root)**

The nRF52840 contains an ARM CryptoCell-310 coprocessor with a 128-bit OTP
e-fuse register called the KDR. Once programmed, this value is **never readable
by software or via any debug interface** — it is consumed internally by the
CryptoCell for key derivation only.

The provisioning flow:
1. Burn the per-device key into the KDR e-fuse via a one-time programming step
2. At runtime, the firmware calls the CryptoCell KDF to derive the working key:
   `network_key = KDF(KDR, device_id || "terrascope-v1")`
3. The derived key is used in working memory only and is never stored in flash

Enable via `CONFIG_CRYPTOCELL_CC310_ENABLED=y` in Zephyr.

**APPROTECT — debug port lockdown**

Without locking the debug port, an attacker can attach a J-Link and read the
entire flash contents regardless of any software protection. Enable permanently
with:

```kconfig
CONFIG_NRF_APPROTECT_LOCK=y
```

This writes the UICR `APPROTECT` register, disabling the SWD interface
permanently. Do this only on production builds — it makes in-field debugging
impossible without an erase-all (which wipes the key).

### ESP32 (Heltec WiFi LoRa32 V2)

**eFuse key blocks**

The ESP32 has three 256-bit OTP eFuse blocks (BLOCK1–BLOCK3). A key burned into
one of these blocks can be flagged as read-protected — the CPU can never read it
back after the protection bit is set. The hardware HMAC peripheral and Digital
Signature peripheral consume the key internally without exposing it to application
code.

The provisioning flow:
1. Burn the per-device key into an eFuse block via `espefuse.py`
2. Set the read-protect bit — key is now opaque to all software
3. The HMAC peripheral uses the key to compute tags; key material never reaches
   the CPU

**Flash encryption and Secure Boot**

ESP32 supports hardware AES-XTS flash encryption: the entire firmware image is
encrypted at rest using a key stored in eFuses. Even if the flash chip is
desoldered and read directly, the contents are ciphertext. Secure Boot
additionally verifies a signature on the bootloader and application image,
preventing an attacker from flashing modified firmware.

Both are enabled via `espefuse.py` and the corresponding `sdkconfig` options.
Zephyr's ESP32 support currently relies on ESP-IDF for the low-level eFuse and
bootloader configuration.

---

## What This Does Not Protect Against

| Threat | Status |
|--------|--------|
| Traffic analysis (who talks to whom, how often) | Not addressed — LoRa is broadcast |
| Jamming / denial of service | Not addressed — inherent to radio |
| Compromise of the provisioning machine | Out of scope — operational security |
| Key compromise after deployment | Mitigated partially by `key_id` rotation (task 23) |

---

## Implementation Checklist (Phase 5)

See [PLAN.md](PLAN.md) for the full task breakdown. In summary:

- [ ] Task 20 — `src/lora/auth.h` module, `CONFIG_TS_NETWORK_KEY` Kconfig fallback
- [ ] Task 21 — Unit tests: roundtrip, tampered payload, tampered tag, truncated tag
- [ ] Task 22 — AES-128-CMAC in `cbor_serialize` / receive path
- [ ] Task 23 — `key_id` in route header, rejection on mismatch
- [ ] Task 24 — Platform key stores: CryptoCell KDR (nRF52840), eFuse (ESP32), APPROTECT
