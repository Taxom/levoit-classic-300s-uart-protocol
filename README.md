# Levoit Classic 300S UART protocol notes

Reverse-engineered notes for the `A5` UART protocol used between the main MCU and the WiFi/ESP module in the Levoit Classic 300S humidifier.

This repository is intended to help with replacement WiFi/ESP firmware, local-control experiments, and Home Assistant / MQTT integrations.

> Status: working draft, based on UART sniffing and active replacement-controller tests. Most user-facing commands are confirmed. Unknown or not fully proven fields are marked as `UNKNOWN`, `candidate`, or `observed`.

> This repository documents the protocol and hardware observations. It does **not** currently include ready-to-flash ESPHome YAML files or firmware binaries.

## What is documented

- UART framing: `9600 8N1`, frame header, packet type, payload length, checksum.
- Known frame types:
  - `A5-22`: WiFi module -> MCU command
  - `A5-12`: MCU -> WiFi reply / ACK/status reply
  - `A5-52`: MCU -> WiFi NACK / rejected command
  - `A5-02`: MCU -> WiFi full status or short event
- Confirmed commands:
  - power on/off
  - display on/off
  - night light raw brightness `0..100`
  - timer icon on/off
  - manual mist level
  - auto mode target humidity band
  - sleep mode target humidity band
  - stop-at-target / auto-off behavior command
  - full status request
  - mode-change sync preamble
- 20-byte status payload map.
- Updated `p[10]` interpretation:
  - `p[10]` is **Target Stop Active / output inhibited by target condition**.
  - It is not the stored Stop At Target setting itself.
- Error/fault field candidate:
  - `p[19] = 0x00`: OK / no error
  - `p[19] = 0x01`: E1 observed; likely tank Hall sensor / tank-presence fault
  - `p[19] = 0x02`: E2 candidate / unconfirmed
- Replacement-controller behavior validated on:
  - original `ESP32-SOLO-1C` / `ESP32-S0WD` WiFi module
  - newer `ESP32-C3-SOLO-1` WiFi module variant
- Hardware component notes:
  - main MCU family candidate: SinOne `SC92F84A*`
  - display/key controller: `AiP650E`

## Files

- [`docs/A5_UART_protocol.md`](docs/A5_UART_protocol.md) — full protocol description.
- [`docs/ESPHome_replacement_notes.md`](docs/ESPHome_replacement_notes.md) — practical notes from ESPHome replacement-controller tests.
- [`docs/hardware_components.md`](docs/hardware_components.md) — known/candidate ICs and hardware notes.
- [`examples/a5_frame_builder.cpp`](examples/a5_frame_builder.cpp) — minimal checksum and command frame builder.

## Basic frame format

```text
offset  size  description
0x00    1     0xA5 magic/header
0x01    1     frame type
0x02    1     packet id
0x03    1     payload length low byte
0x04    1     payload length high byte
0x05    1     checksum
0x06    N     payload
```

Checksum rule:

```text
sum(all frame bytes including checksum) & 0xFF == 0xFF
```

When building a frame:

```text
checksum = 0xFF - (sum(all bytes except checksum) & 0xFF)
```

## Current replacement-module pin summary

Original ESP32-SOLO-1C / ESP32-S0WD module:

```text
A5 UART RX from MCU TX: GPIO16
A5 UART TX to MCU RX:  GPIO17
```

Newer ESP32-C3-SOLO-1 module variant:

```text
A5 UART RX from MCU TX: GPIO18
A5 UART TX to MCU RX:  GPIO19
```

External diagnostic controllers must use level shifting/protection when connected to the MCU-board header. See [`docs/ESPHome_replacement_notes.md`](docs/ESPHome_replacement_notes.md).

## Important notes

This is not official Levoit or VeSync documentation. It is a working map recovered from captured UART logs and replacement-module tests.

Physical front-panel button changes may be generated internally by the MCU and reported as `A5-02` packets. Replacement firmware must keep listening for MCU-generated packets even when it did not send a command.

The display controller appears to support hardware brightness levels, but the stock MCU UART protocol currently exposes only binary display ON/OFF. Intermediate display brightness values sent through the `A5` protocol were rejected in testing.

The original contents of this repository are licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License. See [`LICENSE`](LICENSE).
