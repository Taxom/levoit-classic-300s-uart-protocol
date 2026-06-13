<!--
Reverse-engineered protocol notes for the Levoit Classic 300S humidifier UART link.
This file is based on captured UART traffic between the appliance MCU and the original WiFi module.
-->

A5 UART protocol notes for humidifier MCU / WiFi-module replacement
Version: draft 2026-06-12
Purpose: packet/status/control description recovered from UART sniffing.

# IMPORTANT
This is a working protocol map from captured logs. Most user-facing commands are confirmed.
Unknown or not fully proven fields are explicitly marked as UNKNOWN / candidate.

### UART
Baud rate: 9600
Format: 8N1, no flow control
Logical directions:
  WiFi -> MCU : control commands
  MCU  -> WiFi: replies and status/events

### FRAME FORMAT
All captured A5 frames use this structure:

  offset  size  description
  0x00    1     0xA5, magic/header
  0x01    1     frame type
  0x02    1     packet id
  0x03    1     payload length low byte
  0x04    1     payload length high byte
  0x05    1     checksum
  0x06    N     payload

Total frame size:

  total_length = payload_length + 6

Checksum rule:

  sum(all frame bytes including checksum) & 0xFF == 0xFF

When building a frame:

  checksum = 0xFF - (sum(all bytes except checksum) & 0xFF)

Known frame types:

  A5-22 = WiFi -> MCU command
  A5-12 = MCU -> WiFi reply / ACK
  A5-02 = MCU -> WiFi status / event status

Packet id notes:
  - A5-22 command and A5-12 reply usually share the same id.
  - A5-02 STATUS id is not a reliable monotonic packet counter.
  - A5-02 id may jump during events, boot sync, or command-triggered status.
  - Therefore A5-02 packet id gap does not necessarily mean UART packet loss.


## CONFIRMED WiFi -> MCU COMMANDS, A5-22

The following sections list payload only. Full command frame is:

  A5 22 <id> <len_lo> <len_hi> <checksum> <payload...>


### 1. Power / main switch

  01 00 A0 00 00 = power OFF
  01 00 A0 00 01 = power ON

Confirmed by:
  - manual power ON/OFF from app;
  - timer ON/OFF action;
  - STATUS p07 changes.


### 2. Night light / night lamp

  01 03 A0 00 01 00 = night light OFF
  01 03 A0 00 01 32 = night light LOW
  01 03 A0 00 01 64 = night light HIGH

Related STATUS field:

  p18 = night light level
    00 = OFF
    32 = LOW
    64 = HIGH

Note:
  Display OFF may also turn night light off, but Display ON does not restore the previous night light level.
  A replacement ESP should store and restore previous night light state itself if desired.


### 3. Display / front-panel display brightness

  01 05 A1 00 00 = display OFF / brightness 0
  01 05 A1 00 64 = display ON / brightness 100%

Related STATUS field:

  p11 = display brightness
    00 = OFF
    64 = ON


### 4. Timer indicator / timer icon

  01 6A A2 00 00 = timer icon OFF
  01 6A A2 00 01 = timer icon ON

Important:
  This does NOT set timer duration.
  Timer duration is not sent to the MCU. It is counted by the WiFi/Tuya module.

Observed timer behavior:
  When timer starts:
    WiFi -> MCU: 01 6A A2 00 01     // show timer icon

  When timer expires:
    WiFi -> MCU: 01 00 A0 00 01     // power ON, for ON timer
    OR
    WiFi -> MCU: 01 00 A0 00 00     // power OFF, for OFF timer

    WiFi -> MCU: 01 6A A2 00 00     // hide timer icon

For replacement ESP:
  Implement timer duration on ESP side.
  Use MCU only for timer icon and final power command.


### 5. Manual mode / mist level

  01 60 A2 00 00 01 xx = Manual mode, selected mist level xx

Known xx values:
  01 = level 1 / minimum
  05 = level 5 / middle
  09 = level 9 / maximum

Likely valid range:
  01..09

Stock WiFi usually sends this before mode changes:

  01 29 A1 00 01 00 00 00 00 00

Then sends the actual manual command:

  01 60 A2 00 00 01 xx

Related STATUS fields:
  p16 = 01 means MANUAL
  p17 = selected manual level, e.g. 01 / 05 / 09
  p12 = actual mist output active, 00 / 01


### 6. Auto mode / target humidity band

  01 80 40 00 TT LL HH 09 05 01 = Auto mode, humidity target/band

Where:
  TT = target humidity
  LL = lower threshold
  HH = upper threshold

Observed pattern:
  LL = target - 5
  HH = target + 5

Examples:
  1E 19 23 = target 30%, low 25%, high 35%
  21 1C 26 = target 33%, low 28%, high 38%
  33 2E 38 = target 51%, low 46%, high 56%
  3F 3A 44 = target 63%, low 58%, high 68%
  47 42 4C = target 71%, low 66%, high 76%

Stock WiFi usually sends this before mode changes:

  01 29 A1 00 01 00 00 00 00 00

Then sends the actual auto command:

  01 80 40 00 TT LL HH 09 05 01

Related STATUS fields:
  p16 = 00 means AUTO
  p13 = target humidity
  p14 = current humidity
  p17 = auto output state:
    00 = stopped / target reached
    01 = active / minimum / maintain state
  p12 = actual mist output active, 00 / 01


### 7. Sleep mode / sleep humidity band

  01 82 40 00 TT LL HH 09 05 01 = Sleep mode, humidity target/band

Where:
  TT = target humidity
  LL = lower threshold
  HH = upper threshold

Example:
  33 2E 38 = target 51%, low 46%, high 56%

Related STATUS fields:
  p16 = 02 means SLEEP
  p13 = target humidity
  p14 = current humidity
  p17 = sleep/auto output state:
    00 = stopped
    01 = active / minimum / maintain state


### 8. Stop at target / Auto-off behavior

App setting name: auto-off.
Better protocol meaning: stopAtTarget.

  01 E5 A5 00 00 = stopAtTarget OFF
                   When target is reached/exceeded, do not fully stop;
                   keep minimum/maintain output behavior.

  01 E5 A5 00 01 = stopAtTarget ON
                   When target is reached/exceeded, stop output.

This is NOT hysteresis. Hysteresis/band is set by:

  01 80 40 00 TT LL HH 09 05 01

Observed with target=30 and current=66:
  stopAtTarget OFF -> STATUS p17=01, p12=01
  stopAtTarget ON  -> STATUS p17=00, p12=00


### 9. Request full status

  01 84 40 00 = request full status

MCU replies with A5-12 REPLY, payload length 20, status-like payload:

  01 84 40 00 0D 00 01 ... <status fields...>

Use this at startup for synchronization.


### 10. Sync / mode-change preamble / internal state

Main observed sync/preamble command:

  01 29 A1 00 01 00 00 00 00 00

Observed:
  - before Auto -> Manual;
  - before Manual -> Auto;
  - before mode changes together with 01 60 A2 / 01 80 40 / 01 82 40;
  - MCU replies with short ACK:
      01 29 A1 00

Working interpretation:
  01 29 A1 = sync / mode-change preamble / internal state command

For MVP replacement firmware:
  repeat this before switching Auto/Manual/Sleep.

Additional cold boot variants observed:

  01 29 A1 00 00 4B 00 4B 00 00
  01 29 A1 00 00 7D 00 7D 00 00
  01 29 A1 00 01 7D 00 7D 00 00

Exact meaning: UNKNOWN.
Likely startup/internal sync or state restore after mains power-up.


## STATUS PACKETS MCU -> WiFi, A5-02

Format:

  A5 02 <id> 14 00 <checksum> <20-byte payload>

STATUS payload is 20 bytes:

  p00 p01 p02 p03 p04 p05 p06 p07 p08 p09 p10 p11 p12 p13 p14 p15 p16 p17 p18 p19


### Full STATUS payload map

p00 = 0x01
  UNKNOWN / constant

p01 = 0x85
  UNKNOWN / status object id candidate
  Note: A5-12 full status reply to 01 84 40 uses 0x84 instead of 0x85.

p02 = 0x40
  UNKNOWN / object id part candidate

p03 = 0x00
  UNKNOWN / constant

p04 = 0x0D
  UNKNOWN / constant

p05 = 0x00
  UNKNOWN / constant

p06 = 0x01
  UNKNOWN / constant

p07 = power / main power state
  00 = OFF
  01 = ON

Confirmed by:
  - app power ON/OFF;
  - physical button;
  - water auto-shutdown.

p08 = tank flag
  00 = tank installed
  01 = tank removed

Confirmed by tank remove/install tests.

p09 = noWater / low-water alarm
  00 = water OK
  01 = low water / empty / water alarm

Confirmed independently from tank flag.

p10 = UNKNOWN / ready-ish / internal interlock candidate
  Not power.
  Not tank.
  Not noWater.
  Not mist output.

Observed examples:
  water low before shutdown:       p10=01
  emergency shutdown by water:     p10=00
  water restored while power OFF:  p10=01

Recommendation:
  publish/log as p10_raw for now.

p11 = display brightness
  00 = display OFF
  64 = display ON / 100%

Confirmed by display off/on.

p12 = actual mist/output active
  00 = mist/output OFF or blocked
  01 = mist/output ON / active

Confirmed by:
  - manual max + water OK + mist visible -> p12=01
  - manual max + no water / tank removed / no mist -> p12=00
  - auto target below current / no mist -> p12=00
  - stopAtTarget OFF / minimum maintain -> p12=01

p13 = target humidity
  Value is percent. Hex byte equals decimal percent.

Examples:
  0x1E = 30%
  0x32 = 50%
  0x33 = 51%
  0x3F = 63%
  0x47 = 71%

p14 = current humidity
  Value is percent. Hex byte equals decimal percent.

Examples:
  0x3F = 63%
  0x40 = 64%
  0x41 = 65%
  0x42 = 66%

p15 = 0x16
  UNKNOWN / constant / separator-like field

p16 = work mode
  00 = AUTO
  01 = MANUAL
  02 = SLEEP

Confirmed by Auto/Manual/Sleep switching.

p17 = selected level / mode output state
  In MANUAL:
    01..09 = selected mist level
    example: 09 = manual max

  In AUTO/SLEEP:
    00 = output stopped / target reached
    01 = active / minimum / maintain state

Important:
  p17 is selected/requested level or mode-state, not necessarily physical mist.
  Physical mist/output is better represented by p12.

p18 = night light level
  00 = OFF
  32 = LOW
  64 = HIGH

Confirmed by night light tests.

p19 = 0x00
  UNKNOWN / reserved / constant


### STATUS examples

1) Power ON, tank installed, water OK, display ON, auto mode, no mist, night light OFF:

  01 85 40 00 0D 00 01 01 00 00 01 64 00 33 42 16 00 00 00 00

Decoded:
  power=ON
  tank=installed
  noWater=OK
  p10=01
  display=ON
  mistOutput=OFF
  target=51%
  humidity=66%
  mode=AUTO
  level/state=00
  nightLight=OFF

2) Manual max, tank installed, water OK, mist visible:

  01 85 40 00 0D 00 01 01 00 00 01 64 01 32 41 16 01 09 00 00

Decoded:
  power=ON
  tank=installed
  noWater=OK
  p10=01
  display=ON
  mistOutput=ON
  target=50%
  humidity=65%
  mode=MANUAL
  level=09
  nightLight=OFF

3) Manual max, tank installed, no water, mist stopped, before full shutdown:

  01 85 40 00 0D 00 01 01 00 01 01 64 00 32 40 16 01 09 00 00

Decoded:
  power=ON
  tank=installed
  noWater=EMPTY
  p10=01
  display=ON
  mistOutput=OFF
  target=50%
  humidity=64%
  mode=MANUAL
  selectedLevel=09

4) Emergency shutdown by low water:

  01 85 40 00 0D 00 01 00 00 01 00 64 00 32 40 16 01 09 00 00

Decoded:
  power=OFF
  tank=installed
  noWater=EMPTY
  p10=00
  display=ON
  mistOutput=OFF
  target=50%
  humidity=64%
  mode=MANUAL
  selectedLevel=09

5) Tank removed, water OK, manual max selected, output blocked:

  01 85 40 00 0D 00 01 01 01 00 01 64 00 32 3F 16 01 09 00 00

Decoded:
  power=ON
  tank=removed
  noWater=OK
  display=ON
  mistOutput=OFF
  mode=MANUAL
  selectedLevel=09


## MCU -> WiFi REPLY, A5-12

### Short ACK

Most commands return short ACK:

  A5 12 <same_id> 04 00 <checksum> 01 <reg_lo> <reg_hi> 00

Examples:
  Command payload: 01 00 A0 00 01
  Reply payload:   01 00 A0 00

  Command payload: 01 03 A0 00 01 64
  Reply payload:   01 03 A0 00

  Command payload: 01 60 A2 00 00 01 09
  Reply payload:   01 60 A2 00

### Full status reply

Request:
  WiFi -> MCU:
    01 84 40 00

MCU reply:
  A5-12 with len=20:
    01 84 40 00 0D 00 01 <status-like fields...>

Useful for startup sync.


## STARTUP / COLD BOOT BEHAVIOR

Observed after mains power is applied:

1. WiFi module sends:
   01 6A A2 00 00
   Meaning: timer icon OFF

2. WiFi module sends:
   01 84 40 00
   Meaning: request full status

3. MCU replies with A5-12 full status.

4. WiFi module sends several 01 29 A1 startup sync variants:
   01 29 A1 00 00 4B 00 4B 00 00
   01 29 A1 00 00 7D 00 7D 00 00
   01 29 A1 00 01 7D 00 7D 00 00

5. MCU emits A5-02 STATUS.

Physical button behavior:
  When user presses the physical power button, there may be no WiFi->MCU command.
  MCU changes state internally and sends A5-02 STATUS.
  Replacement ESP must always listen for MCU-generated state changes.


## RECOMMENDED INIT FOR REPLACEMENT ESP

Minimum startup sequence:

  1. Init UART 9600 8N1.
  2. Start A5 parser.
  3. nextPacketId = 1 or any uint8 counter.
  4. Send timer icon OFF:
       01 6A A2 00 00
  5. Send request full status:
       01 84 40 00
  6. Wait for A5-12 status reply and/or A5-02 status.
  7. Publish state to HA/MQTT/web.
  8. Keep listening permanently because physical panel buttons are handled by MCU.

Mode switch recommendation:
  To match stock WiFi behavior, send sync preamble before mode commands:

  01 29 A1 00 01 00 00 00 00 00

Then send one of:

  Manual:
    01 60 A2 00 00 01 xx

  Auto:
    01 80 40 00 TT LL HH 09 05 01

  Sleep:
    01 82 40 00 TT LL HH 09 05 01


## COMMAND FRAME BUILD EXAMPLE

Pseudo C/C++:

  uint8_t nextPacketId = 1;

  void sendA5Command(const uint8_t* payload, uint16_t len) {
    uint8_t frame[64];
    uint8_t id = nextPacketId++;

    frame[0] = 0xA5;
    frame[1] = 0x22;
    frame[2] = id;
    frame[3] = len & 0xFF;
    frame[4] = len >> 8;
    frame[5] = 0x00; // checksum placeholder

    memcpy(&frame[6], payload, len);

    uint16_t total = len + 6;
    uint16_t sum = 0;

    for (int i = 0; i < total; i++) {
      if (i != 5) sum += frame[i];
    }

    frame[5] = 0xFF - (sum & 0xFF);

    uart_write_bytes(UART_NUM_1, (const char*)frame, total);
  }

Example payloads:

  const uint8_t powerOn[]      = {0x01,0x00,0xA0,0x00,0x01};
  const uint8_t powerOff[]     = {0x01,0x00,0xA0,0x00,0x00};
  const uint8_t displayOn[]    = {0x01,0x05,0xA1,0x00,0x64};
  const uint8_t displayOff[]   = {0x01,0x05,0xA1,0x00,0x00};
  const uint8_t timerIconOff[] = {0x01,0x6A,0xA2,0x00,0x00};
  const uint8_t requestStatus[]= {0x01,0x84,0x40,0x00};


## REMAINING UNKNOWN / NOT FULLY CLOSED

1. STATUS p10
   Current safe name: p10_raw or readyFlag candidate.
   Do not use it as power, water, tank, or mist output.

2. STATUS p00-p06
   Mostly constant/service prefix:
     01 85 40 00 0D 00 01
   Useful to log, not necessary for user-level control.

3. STATUS p15
   Usually 0x16. Probably constant/separator.

4. STATUS p19
   Usually 0x00. Reserved/unused.

5. Exact meaning of all 01 29 A1 variants
   Mode-change sync is understood practically.
   Startup variants are not fully understood.

6. Whether 01 29 A1 preamble is strictly required
   Stock module sends it before mode changes.
   MVP should copy stock behavior first; later test if it can be omitted.


## SHORT COMMAND SUMMARY

Power:
  01 00 A0 00 00 = OFF
  01 00 A0 00 01 = ON

Night light:
  01 03 A0 00 01 00 = OFF
  01 03 A0 00 01 32 = LOW
  01 03 A0 00 01 64 = HIGH

Display:
  01 05 A1 00 00 = OFF
  01 05 A1 00 64 = ON

Timer icon:
  01 6A A2 00 00 = OFF
  01 6A A2 00 01 = ON

Manual:
  01 29 A1 00 01 00 00 00 00 00 = mode sync/preamble
  01 60 A2 00 00 01 xx          = manual level xx

Auto:
  01 29 A1 00 01 00 00 00 00 00 = mode sync/preamble
  01 80 40 00 TT LL HH 09 05 01 = auto target/band

Sleep:
  01 29 A1 00 01 00 00 00 00 00 = mode sync/preamble
  01 82 40 00 TT LL HH 09 05 01 = sleep target/band

Stop at target / Auto-off:
  01 E5 A5 00 00 = OFF, keep minimum/maintain
  01 E5 A5 00 01 = ON, stop output at target

Request status:
  01 84 40 00 = request full status

Sync/internal:
  01 29 A1 ... = sync/mode preamble/internal state
