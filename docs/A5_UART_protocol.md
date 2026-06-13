# A5 UART protocol notes for Levoit Classic 300S MCU / WiFi-module replacement

Version: draft 2026-06-13  
Purpose: packet/status/control description recovered from UART sniffing and active replacement-controller tests.

## IMPORTANT

This is a working protocol map from captured logs. Most user-facing commands are confirmed. Unknown or not fully proven fields are explicitly marked as `UNKNOWN` / `candidate` / `very likely`.

## UART

```text
Baud rate: 9600
Format:    8N1, no flow control
Direction: WiFi -> MCU : control commands
Direction: MCU  -> WiFi: replies and status/events
```

Tested replacement-controller wiring:

```text
External ESP32-C3 RX GPIO20 <- humidifier MCU TX
External ESP32-C3 TX GPIO21 -> humidifier MCU RX
```

Original module observed in this unit:

```text
ESP32-SOLO-1C module
Likely UART pins: ESP32 RX=GPIO16, TX=GPIO17
```

## Frame format

All captured `A5` frames use this structure:

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

Total frame size:

```text
total_length = payload_length + 6
```

Checksum rule:

```text
sum(all frame bytes including checksum) & 0xFF == 0xFF
```

When building a frame:

```text
checksum = 0xFF - (sum(all bytes except checksum) & 0xFF)
```

Known frame types:

```text
A5-22 = WiFi -> MCU command
A5-12 = MCU -> WiFi reply / ACK
A5-02 = MCU -> WiFi status / event status
A5-52 = MCU -> WiFi NACK / rejected command or unsupported value
```

Packet id notes:

- `A5-22` command and `A5-12` reply usually share the same id.
- `A5-02 STATUS` id is not a reliable monotonic packet counter.
- `A5-02` id may jump during events, boot sync, or command-triggered status.
- Therefore an `A5-02` packet id gap does not necessarily mean UART packet loss.

## Confirmed WiFi -> MCU commands, `A5-22`

The following sections list payload only. Full command frame is:

```text
A5 22 <id> <len_lo> <len_hi> <checksum> <payload...>
```

### 1. Power / main switch

```text
01 00 A0 00 00 = power OFF
01 00 A0 00 01 = power ON
```

Confirmed by:

- app power ON/OFF;
- replacement-controller tests;
- timer ON/OFF action;
- `STATUS p[7]` changes.

### 2. Night light / night lamp brightness

The night light command accepts a real brightness level, not only the stock app presets.

```text
01 03 A0 00 01 XX = set night light brightness
```

Where:

```text
XX = 0x00..0x64 = 0..100 decimal percent
```

Observed/stock values:

```text
01 03 A0 00 01 00 = night light OFF / 0%
01 03 A0 00 01 32 = night light LOW preset / 50%
01 03 A0 00 01 64 = night light HIGH preset / 100%
```

Intermediate values tested and accepted:

```text
0x0F = 15%
0x14 = 20%
0x1E = 30%
0x46 = 70%
```

All tested valid values returned normal `A5-12` ACK and were reflected in `STATUS p[18]`.

Related STATUS field:

```text
p[18] = night light brightness level, 0x00..0x64 = 0..100%
```

### 3. Display / front-panel display state

The display appears to be binary only.

```text
01 05 A1 00 00 = display OFF
01 05 A1 00 64 = display ON
```

Related STATUS field:

```text
p[11] = display state
        0x00 = OFF
        0x64 = ON
```

Intermediate values were tested and rejected by the MCU:

```text
01 05 A1 00 19 = rejected
01 05 A1 00 2D = rejected
01 05 A1 00 32 = rejected
01 05 A1 00 37 = rejected
01 05 A1 00 41 = rejected
01 05 A1 00 4B = rejected
01 05 A1 00 5A = rejected
```

Observed rejected-value reply form:

```text
A5 52 <same_id> 04 00 <checksum> 01 05 A1 34
```

Working interpretation:

```text
A5-52 = NACK / command rejected
0x34   = observed error code for unsupported display value/range
```

### 4. Timer indicator / timer icon

```text
01 6A A2 00 00 = timer icon OFF
01 6A A2 00 01 = timer icon ON
```

Important: this does **not** set timer duration. Timer duration is not sent to the MCU. It is counted by the WiFi/Tuya module or replacement ESP.

Observed timer behavior:

```text
When timer starts:
  WiFi -> MCU: 01 6A A2 00 01     // show timer icon

When timer expires:
  WiFi -> MCU: 01 00 A0 00 01     // power ON, for ON timer
  OR
  WiFi -> MCU: 01 00 A0 00 00     // power OFF, for OFF timer

  WiFi -> MCU: 01 6A A2 00 00     // hide timer icon
```

For replacement ESP firmware: implement timer duration on the ESP side. Use MCU only for timer icon and final power command.

### 5. Manual mode / mist level

```text
01 60 A2 00 00 01 XX = manual mode, selected mist level XX
```

Known values:

```text
01 = level 1 / minimum
05 = level 5 / middle
09 = level 9 / maximum
```

Likely valid range:

```text
01..09
```

Stock WiFi usually sends this before mode changes:

```text
01 29 A1 00 01 00 00 00 00 00
```

Then sends the actual manual command:

```text
01 60 A2 00 00 01 XX
```

Related STATUS fields:

```text
p[16] = 0x01 means MANUAL
p[17] = selected manual level, e.g. 0x01 / 0x05 / 0x09
p[12] = actual mist output active, 0x00 / 0x01
```

### 6. Auto mode / target humidity band

```text
01 80 40 00 TT LL HH 09 05 01 = Auto mode, humidity target/band
```

Where:

```text
TT = target humidity
LL = lower threshold
HH = upper threshold
```

Observed pattern:

```text
LL = target - 5
HH = target + 5
```

Examples:

```text
1E 19 23 = target 30%, low 25%, high 35%
21 1C 26 = target 33%, low 28%, high 38%
33 2E 38 = target 51%, low 46%, high 56%
3E 39 43 = target 62%, low 57%, high 67%
3F 3A 44 = target 63%, low 58%, high 68%
47 42 4C = target 71%, low 66%, high 76%
```

Stock WiFi usually sends this before mode changes:

```text
01 29 A1 00 01 00 00 00 00 00
```

Then sends the actual auto command:

```text
01 80 40 00 TT LL HH 09 05 01
```

Related STATUS fields:

```text
p[16] = 0x00 means AUTO
p[13] = target humidity
p[14] = current humidity
p[17] = auto output level/state
p[12] = actual mist output active, 0x00 / 0x01
```

### 7. Sleep mode

Sleep mode uses the same target/band structure as Auto, but it should not be exposed as a separate "sleep target" setting unless a separate app-level setting is later confirmed.

```text
01 82 40 00 TT LL HH 09 05 01 = Sleep mode using target/band
```

Where:

```text
TT = current/commanded target humidity
LL = lower threshold, usually TT - 5
HH = upper threshold, usually TT + 5
```

Example:

```text
3E 39 43 = target 62%, low 57%, high 67%
```

Observed behavior after switching to Sleep:

```text
p[16] = 0x02       // SLEEP
p[17] may become 0x05
p[18] may become 0x00   // night light off
p[10] may become 0x00   // stopAtTarget/auto-off disabled
p[12] may become 0x01   // mist/output active
```

Related STATUS fields:

```text
p[16] = 0x02 means SLEEP
p[13] = target humidity
p[14] = current humidity
p[17] = sleep output level/state
p[12] = actual mist output active, 0x00 / 0x01
```

### 8. Stop at target / Auto-off behavior

App setting name: auto-off. Better protocol meaning: stopAtTarget.

```text
01 E5 A5 00 00 = stopAtTarget OFF
                 When target is reached/exceeded, do not fully stop;
                 keep minimum/maintain output behavior.

01 E5 A5 00 01 = stopAtTarget ON
                 When target is reached/exceeded, stop output.
```

This is not hysteresis. Hysteresis/band is set by:

```text
01 80 40 00 TT LL HH 09 05 01
```

Observed with target below current humidity:

```text
stopAtTarget OFF -> p[10]=0, p[12]=1, p[17]=1
stopAtTarget ON  -> p[10]=1, p[12]=0, p[17]=0
```

Important interpretation:

```text
p[10] = stopAtTarget / auto-off setting flag, very likely confirmed by behavior
p[12] = actual mist/output active
p[17] = selected level / automatic output level/state
```

### 9. Request full status

```text
01 84 40 00 = request full status
```

MCU replies with `A5-12 REPLY`, payload length 20, status-like payload:

```text
01 84 40 00 0D 00 01 ...
```

Use this at startup for synchronization.

### 10. Sync / mode-change preamble / internal state

Main observed sync/preamble command:

```text
01 29 A1 00 01 00 00 00 00 00
```

Observed:

- before Auto -> Manual;
- before Manual -> Auto;
- before Auto/Manual/Sleep mode changes together with `01 60 A2`, `01 80 40`, `01 82 40`;
- MCU replies with short ACK: `01 29 A1 00`.

Working interpretation:

```text
01 29 A1 = sync / mode-change preamble / internal state command
```

For MVP replacement firmware: repeat this before switching Auto/Manual/Sleep.

Additional cold boot variants observed:

```text
01 29 A1 00 00 4B 00 4B 00 00
01 29 A1 00 00 7D 00 7D 00 00
01 29 A1 00 01 7D 00 7D 00 00
```

Exact meaning: `UNKNOWN`. Likely startup/internal sync or state restore after mains power-up.

## Status packets MCU -> WiFi, `A5-02`

Format:

```text
A5 02 <id> 14 00 <checksum> <20-byte payload>
```

STATUS payload is 20 bytes:

```text
p00 p01 p02 p03 p04 p05 p06 p07 p08 p09 p10 p11 p12 p13 p14 p15 p16 p17 p18 p19
```

### Full STATUS payload map

```text
p[0]  = 0x01 UNKNOWN / constant
p[1]  = 0x85 UNKNOWN / status object id candidate
        Note: A5-12 full status reply to 01 84 40 uses 0x84 instead of 0x85.
p[2]  = 0x40 UNKNOWN / object id part candidate
p[3]  = 0x00 UNKNOWN / constant
p[4]  = 0x0D UNKNOWN / constant
p[5]  = 0x00 UNKNOWN / constant
p[6]  = 0x01 UNKNOWN / constant

p[7]  = Power / main power state
        0x00 = OFF
        0x01 = ON

p[8]  = Tank removed flag
        0x00 = tank installed
        0x01 = tank removed

p[9]  = Water empty / low-water alarm
        0x00 = water OK
        0x01 = water empty / low

p[10] = StopAtTarget / Auto-off setting flag, very likely confirmed by behavior
        0x00 = disabled / keep minimum
        0x01 = enabled / stop output at target

p[11] = Display state
        0x00 = display OFF
        0x64 = display ON
        Intermediate values were tested and rejected by MCU.

p[12] = Actual mist/output active
        0x00 = mist/output OFF or blocked
        0x01 = mist/output ON / active

p[13] = Target humidity, percent
        Hex byte equals decimal percent.

p[14] = Current humidity, percent
        Hex byte equals decimal percent.

p[15] = Temperature, °C
        Confirmed by cooling and warming/breath tests.
        Observed lower reporting limit appears to be around 10°C.

p[16] = Work mode
        0x00 = AUTO
        0x01 = MANUAL
        0x02 = SLEEP

p[17] = Level / output level / mode state
        In MANUAL: selected mist level, e.g. 0x01 / 0x05 / 0x09.
        In AUTO/SLEEP: automatic output level/state, e.g. 0x00 / 0x01 / 0x05 / 0x09.
        Physical mist/output is better represented by p[12].

p[18] = Night light brightness level
        0x00..0x64 = 0..100%
        Stock app presets: 0x00 OFF, 0x32 LOW, 0x64 HIGH.

p[19] = 0x00 UNKNOWN / reserved / usually constant
```

Confirmed by:

- power ON/OFF tests for `p[7]`;
- tank remove/install tests for `p[8]`;
- low-water tests for `p[9]`;
- stopAtTarget command behavior for `p[10]`;
- display ON/OFF tests for `p[11]`;
- visible mist / blocked mist / target behavior for `p[12]`;
- target changes for `p[13]`;
- humidity sensor changes for `p[14]`;
- cooling and breath warming tests for `p[15]`;
- mode changes for `p[16]`;
- manual level and auto/sleep output tests for `p[17]`;
- night light brightness tests for `p[18]`.

### STATUS examples

#### 1. Power ON, tank installed, water OK, display ON, auto mode, no mist, night light OFF

```text
01 85 40 00 0D 00 01 01 00 00 01 64 00 33 42 16 00 00 00 00
```

Decoded:

```text
power=ON
tank=installed
water=OK
stopAtTarget=ON
display=ON
mistOutput=OFF
target=51%
humidity=66%
temperature=22°C
mode=AUTO
level/state=0
nightLight=0%
```

#### 2. Night light custom brightness 70%

```text
Command payload: 01 03 A0 00 01 46
STATUS p[18]:    0x46 = 70%
```

#### 3. Display intermediate value rejected

```text
Command payload: 01 05 A1 00 4B
MCU reply:       A5-52 NACK, payload 01 05 A1 34
STATUS p[11]:    remains 0x64 if display was ON
```

#### 4. Manual mode, level 9 selected, output active

```text
01 85 40 00 0D 00 01 01 00 00 01 64 01 32 41 16 01 09 00 00
```

Decoded:

```text
power=ON
tank=installed
water=OK
stopAtTarget=ON
display=ON
mistOutput=ON
target=50%
humidity=65%
temperature=22°C
mode=MANUAL
level=9
nightLight=0%
```

## MCU -> WiFi replies

### `A5-12` short ACK

Most accepted commands return short ACK:

```text
A5 12 <same_id> 04 00 <checksum> 01 <reg_lo> <reg_hi> 00
```

Examples:

```text
Command payload: 01 00 A0 00 01       Reply payload: 01 00 A0 00
Command payload: 01 03 A0 00 01 64    Reply payload: 01 03 A0 00
Command payload: 01 60 A2 00 00 01 09 Reply payload: 01 60 A2 00
```

### `A5-12` full status reply

Request:

```text
WiFi -> MCU: 01 84 40 00
```

MCU reply:

```text
A5-12 with len=20: 01 84 40 00 0D 00 01 ...
```

Useful for startup sync.

### `A5-52` NACK / rejected command or value

Observed when sending unsupported display intermediate values:

```text
WiFi -> MCU: 01 05 A1 00 4B
MCU -> WiFi: A5 52 <same_id> 04 00 <checksum> 01 05 A1 34
```

Working interpretation:

```text
A5-52 = NACK / error / rejected command value
0x34   = observed error code for unsupported display level/range
```

Valid display values `0x00` and `0x64` return normal `A5-12` ACK.
Night light intermediate values `0x00..0x64` return normal `A5-12` ACK.

## Startup / cold boot behavior

Observed after mains power is applied:

1. WiFi module sends `01 6A A2 00 00` meaning timer icon OFF.
2. WiFi module sends `01 84 40 00` meaning request full status.
3. MCU replies with `A5-12` full status.
4. WiFi module sends several `01 29 A1` startup sync variants:

```text
01 29 A1 00 00 4B 00 4B 00 00
01 29 A1 00 00 7D 00 7D 00 00
01 29 A1 00 01 7D 00 7D 00 00
```

5. MCU emits `A5-02 STATUS`.

Physical button behavior: when the user presses the physical power button, there may be no WiFi->MCU command. MCU changes state internally and sends `A5-02 STATUS`. Replacement ESP must always listen for MCU-generated state changes.

## Recommended init for replacement ESP

Minimum startup sequence:

1. Init UART `9600 8N1`.
2. Start A5 parser.
3. `nextPacketId = 1` or any uint8 counter.
4. Send timer icon OFF: `01 6A A2 00 00`.
5. Send request full status: `01 84 40 00`.
6. Wait for `A5-12` status reply and/or `A5-02 STATUS`.
7. Publish state to HA/MQTT/web.
8. Keep listening permanently because physical panel buttons are handled by MCU.

Mode switch recommendation: to match stock WiFi behavior, send sync preamble before mode commands:

```text
01 29 A1 00 01 00 00 00 00 00
```

Then send one of:

```text
Manual: 01 60 A2 00 00 01 XX
Auto:   01 80 40 00 TT LL HH 09 05 01
Sleep:  01 82 40 00 TT LL HH 09 05 01
```

## Command frame build example

Pseudo C/C++:

```cpp
uint8_t nextPacketId = 1;

void sendA5Command(const uint8_t *payload, uint16_t len) {
  uint8_t frame[64];
  uint8_t id = nextPacketId++;

  frame[0] = 0xA5;
  frame[1] = 0x22;
  frame[2] = id;
  frame[3] = len & 0xFF;
  frame[4] = len >> 8;
  frame[5] = 0x00;  // checksum placeholder

  memcpy(&frame[6], payload, len);

  uint16_t total = len + 6;
  uint16_t sum = 0;

  for (int i = 0; i < total; i++) {
    if (i != 5) sum += frame[i];
  }

  frame[5] = 0xFF - (sum & 0xFF);

  uart_write_bytes(UART_NUM_1, (const char *) frame, total);
}
```

Example payloads:

```cpp
const uint8_t powerOn[]          = {0x01,0x00,0xA0,0x00,0x01};
const uint8_t powerOff[]         = {0x01,0x00,0xA0,0x00,0x00};
const uint8_t displayOn[]        = {0x01,0x05,0xA1,0x00,0x64};
const uint8_t displayOff[]       = {0x01,0x05,0xA1,0x00,0x00};
const uint8_t nightLight70[]     = {0x01,0x03,0xA0,0x00,0x01,0x46};
const uint8_t timerIconOff[]     = {0x01,0x6A,0xA2,0x00,0x00};
const uint8_t requestStatus[]    = {0x01,0x84,0x40,0x00};
const uint8_t stopAtTargetOn[]   = {0x01,0xE5,0xA5,0x00,0x01};
const uint8_t stopAtTargetOff[]  = {0x01,0xE5,0xA5,0x00,0x00};
```

## Remaining unknown / not fully closed

1. `STATUS p[0]..p[6]`: mostly constant/service prefix: `01 85 40 00 0D 00 01`. Useful to log, not necessary for user-level control.
2. `STATUS p[19]`: usually `0x00`, reserved/unused/unknown.
3. Exact meaning of all `01 29 A1` variants. Mode-change sync is understood practically. Startup variants are not fully understood.
4. Whether the `01 29 A1` preamble is strictly required. Stock module sends it before mode changes. MVP replacement firmware should copy stock behavior first; later test if it can be omitted.
5. `A5-52` / `0x34` exact error semantics. Observed as rejected display intermediate values; broader error-code map is not known.

## Short command summary

```text
Power:
  01 00 A0 00 00 = OFF
  01 00 A0 00 01 = ON

Display:
  01 05 A1 00 00 = OFF
  01 05 A1 00 64 = ON
  Intermediate values rejected with A5-52 / payload 01 05 A1 34

Night light brightness:
  01 03 A0 00 01 XX = brightness 0..100%, XX = 0x00..0x64
  Presets used by stock app:
    01 03 A0 00 01 00 = OFF / 0%
    01 03 A0 00 01 32 = LOW / 50%
    01 03 A0 00 01 64 = HIGH / 100%

Timer icon:
  01 6A A2 00 00 = OFF
  01 6A A2 00 01 = ON

Manual:
  01 29 A1 00 01 00 00 00 00 00 = mode sync/preamble
  01 60 A2 00 00 01 XX          = manual level XX, likely 0x01..0x09

Auto:
  01 29 A1 00 01 00 00 00 00 00 = mode sync/preamble
  01 80 40 00 TT LL HH 09 05 01 = auto target/band

Sleep:
  01 29 A1 00 01 00 00 00 00 00 = mode sync/preamble
  01 82 40 00 TT LL HH 09 05 01 = sleep mode using target/band

Stop at target / Auto-off:
  01 E5 A5 00 00 = OFF, keep minimum/maintain
  01 E5 A5 00 01 = ON, stop output at target

Request status:
  01 84 40 00 = request full status

Sync/internal:
  01 29 A1 ... = sync/mode preamble/internal state
```
