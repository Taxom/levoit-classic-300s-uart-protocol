# Levoit Classic 300S UART protocol notes

Reverse-engineered notes for the `A5` UART protocol used between the main MCU and the WiFi/ESP module in the Levoit Classic 300S humidifier.

This repository is intended to help with replacement WiFi/ESP firmware, local-control experiments, and Home Assistant / MQTT integrations.

> Status: draft, based on UART sniffing. Confirmed commands are documented separately from unknown or candidate fields.

## What is documented

- UART framing: `9600 8N1`, frame header, packet type, payload length, checksum.
- Known frame types:
  - `A5-22`: WiFi module → MCU command
  - `A5-12`: MCU → WiFi reply / ACK
  - `A5-02`: MCU → WiFi status / event status
- Confirmed commands:
  - power on/off
  - display on/off
  - night light off/low/high
  - timer icon on/off
  - manual mist level
  - auto mode target humidity band
  - sleep 
  - stop-at-target / auto-off behavior
  - full status request
- 20-byte status payload map.
- Startup sequence and mode-switch preamble behavior.
- Minimal C/C++ frame-builder example.

## Files

- [`docs/A5_UART_protocol.md`](docs/A5_UART_protocol.md) — full protocol description.
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

## Important notes

This is not official Levoit or VeSync documentation. It is a working map recovered from captured UART logs.

Unknown fields are intentionally left as `UNKNOWN` or `candidate`. Do not treat those fields as stable until they are confirmed by additional tests.

Physical front-panel button changes may be generated internally by the MCU and reported as status packets. Replacement firmware must keep listening for MCU-generated `A5-02` status packets even when it did not send a command.


## License

This repository contains reverse-engineered protocol documentation and illustrative examples. It is not intended to be a software library.

The original contents of this repository are licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.

You may use, share, and adapt the material for non-commercial purposes only, with attribution, and adaptations must be shared under the same or a compatible license.

Commercial use or incorporation into proprietary/closed-source commercial products is not permitted without separate written permission.

See [LICENSE](LICENSE) for details.
