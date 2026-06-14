# ESPHome replacement-controller notes

These notes summarize the replacement-controller tests performed for the Levoit Classic 300S UART protocol project.

This file is not required to understand the protocol, but it records practical implementation details that were useful for replacing the original WiFi/ESP module.

## Tested hardware

### External test controller

```text
Board: ESP32-C3 Super Mini
UART to humidifier MCU:
  ESP32-C3 RX GPIO20 <- humidifier MCU TX
  ESP32-C3 TX GPIO21 -> humidifier MCU RX
UART: 9600 8N1
```

Electrical warning for external controllers:

```text
The MCU-board header is 5 V.
The UART levels observed at the MCU-board header are 5 V logic.
The original module board contains the level conversion/protection.
External 3.3 V controllers must use level shifting or other input protection.
```

During diagnostics, the external ESP32-C3 was connected through a transistor level shifter. Directly connecting the humidifier MCU TX line from the MCU-board header to a 3.3 V ESP RX pin is not recommended.

### Original module replacement

The original WiFi module in this tested unit uses an ESP32 single-core module.

```text
Chip detected by esptool: ESP32-S0WD revision v1.0
Original module class:    ESP32-SOLO-1C / single-core ESP32
Flash size:               4 MB
MAC:                       device-specific, omitted
```

Important: the original ESP32-SOLO module board is not just an ESP32 module. In the tested unit it also provides the interface electronics between the 5 V MCU-board header and the 3.3 V ESP32-SOLO GPIOs. If replacing this board with a different ESP board, replicate the required level shifting/protection.

Working UART pins on the original ESP32-SOLO module:

```text
ESP32 GPIO16 = RX from humidifier MCU TX
ESP32 GPIO17 = TX to humidifier MCU RX
UART: 9600 8N1
```

Flashing/backup UART:

```text
ESP32 GPIO3  = U0RXD <- USB-UART TX
ESP32 GPIO1  = U0TXD -> USB-UART RX
GPIO0        = BOOT strap low for UART bootloader mode
EN/RST       = reset
GND          = common ground
3.3V only
```

## Original firmware backup

Before replacing the original firmware, make a private full-flash backup of the original ESP32-SOLO module. Do **not** publish this backup: the flash image may contain device-specific data such as WiFi credentials, tokens, calibration/NVS data, MAC-related records, and pairing/provisioning state.

For a 4 MB flash chip, read the full image twice and compare the files:

```powershell
python -m esptool --chip esp32 --port COMx --baud 460800 --before no-reset --after no-reset read-flash 0x000000 0x400000 backup_1.bin
python -m esptool --chip esp32 --port COMx --baud 460800 --before no-reset --after no-reset read-flash 0x000000 0x400000 backup_2.bin
cmd /c fc /b backup_1.bin backup_2.bin
Get-FileHash .\backup_1.bin -Algorithm SHA256
```

A correct full backup for a 4 MB module should be 4,194,304 bytes. The SHA256 value is intentionally not documented here because it is device-specific and not useful to other users.

Note: repeated backups may differ if the stock firmware boots between reads and updates the NVS/WiFi area. A stable backup can be produced by keeping the chip in bootloader mode and using `--before no-reset --after no-reset` for repeated reads.

## ESPHome build notes for ESP32-SOLO

The ESP32-SOLO is single-core. The tested ESPHome build used ESP-IDF and unicore configuration.

Example ESPHome `esp32:` block:

```yaml
esp32:
  board: esp32dev
  variant: esp32
  flash_size: 4MB
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_FREERTOS_UNICORE: y
```

UART:

```yaml
uart:
  id: a5_uart_bus
  rx_pin: GPIO16
  tx_pin: GPIO17
  baud_rate: 9600
```

Tested firmware size was approximately half of the default 4 MB OTA app slot.

## Recovery behavior implemented in the tested ESPHome replacement

The MCU reports physical power-button long-press events as short `A5-02 len=5` packets.

Implemented/tested replacement behavior:

```text
01 02 D1 00 02 = 5-second hold threshold
                 arm pending reboot, but do not reboot yet

01 02 D1 00 03 = release after 5-second hold
                 reboot ESP

01 03 D1 00 04 = 15-second hold threshold
                 cancel pending reboot and start recovery AP
```

Tested recovery AP:

```text
SSID: Levoit-Classic300S-Recovery
IP:   192.168.4.1
```

Confirmed OTA paths on the original ESP32-SOLO module:

```text
Normal WiFi OTA:
  esphome upload levoit_classic300s_solo.yaml --device <normal_ip>

Recovery AP OTA:
  15s hold -> connect to Levoit-Classic300S-Recovery
  esphome upload levoit_classic300s_solo.yaml --device 192.168.4.1
```

## Replacement-firmware behavior recommendations

### Always listen to MCU-generated state changes

The physical front-panel button is handled by the MCU. The MCU may change state and send `A5-02` STATUS without any WiFi/ESP command.

Replacement firmware should treat `A5-02` STATUS as authoritative.

### Startup sequence

Recommended minimal sequence:

```text
1. Init UART 9600 8N1.
2. Start A5 parser.
3. Send timer icon OFF: 01 6A A2 00 00
4. Send request full status: 01 84 40 00
5. Decode A5-12 status reply and/or A5-02 full STATUS.
6. Publish state to HA/MQTT/web.
```

### Mode switching

To match stock behavior:

```text
1. Send mode-sync preamble:
   01 29 A1 00 01 00 00 00 00 00

2. Then send one of:
   Manual: 01 60 A2 00 00 01 xx
   Auto:   01 80 40 00 TT LL HH 09 05 01
   Sleep:  01 82 40 00 TT LL HH 09 05 01
```

If the device is power OFF and the user requests a mode command, the tested replacement firmware first sends power ON, then sends sync + mode command.

### Water empty

When water-empty alarm is active, mist-related commands may be rejected with `A5-52` code `0x14`.

The tested replacement firmware skips mist-related mode/level commands while `water_empty` is active.

### Tank removed

Tank removed is an output interlock, not a command interlock.

The MCU accepts mode/manual-level commands while the tank is removed, but keeps actual mist output OFF. When the tank is installed again, output may resume using the stored mode/level.

### Manual level cache

In MANUAL mode, `p[17]` is selected manual level.  
In AUTO/SLEEP, `p[17]` is an output state/level, not the saved manual setting.

Replacement firmware should store/update manual level only when `p[16] == 0x01` (MANUAL).
