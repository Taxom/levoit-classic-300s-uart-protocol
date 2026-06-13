// Minimal A5 UART command frame builder for Levoit Classic 300S protocol notes.
// UART: 9600 8N1.
// Frame checksum rule:
//   sum(all frame bytes including checksum) & 0xFF == 0xFF

#include <stdint.h>
#include <string.h>

// Replace this with Serial/UART write code for your platform.
static void uart_write_bytes_stub(const uint8_t* data, uint16_t len) {
  (void)data;
  (void)len;
}

static uint8_t nextPacketId = 1;

void sendA5Command(const uint8_t* payload, uint16_t len) {
  // Current known payloads are short; increase if you add longer payloads.
  uint8_t frame[64];

  if (payload == nullptr || len > sizeof(frame) - 6) {
    return;
  }

  const uint8_t id = nextPacketId++;

  frame[0] = 0xA5;             // magic/header
  frame[1] = 0x22;             // WiFi -> MCU command
  frame[2] = id;               // packet id
  frame[3] = len & 0xFF;       // payload length low
  frame[4] = (len >> 8) & 0xFF;// payload length high
  frame[5] = 0x00;             // checksum placeholder

  memcpy(&frame[6], payload, len);

  const uint16_t total = len + 6;
  uint16_t sum = 0;

  for (uint16_t i = 0; i < total; i++) {
    if (i != 5) {
      sum += frame[i];
    }
  }

  frame[5] = 0xFF - (sum & 0xFF);

  uart_write_bytes_stub(frame, total);
}

// Example payloads.
const uint8_t powerOn[]       = {0x01, 0x00, 0xA0, 0x00, 0x01};
const uint8_t powerOff[]      = {0x01, 0x00, 0xA0, 0x00, 0x00};
const uint8_t displayOn[]     = {0x01, 0x05, 0xA1, 0x00, 0x64};
const uint8_t displayOff[]    = {0x01, 0x05, 0xA1, 0x00, 0x00};
const uint8_t timerIconOff[]  = {0x01, 0x6A, 0xA2, 0x00, 0x00};
const uint8_t requestStatus[] = {0x01, 0x84, 0x40, 0x00};

void example() {
  sendA5Command(timerIconOff, sizeof(timerIconOff));
  sendA5Command(requestStatus, sizeof(requestStatus));
  sendA5Command(powerOn, sizeof(powerOn));
}
