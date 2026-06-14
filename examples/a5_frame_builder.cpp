// Minimal A5 frame builder for Levoit Classic 300S UART protocol.
// UART parameters observed: 9600 8N1.
// This example only builds command frames (A5-22).

#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Replace this with your platform UART write function.
extern void uart_write_bytes(const uint8_t *data, size_t len);

static uint8_t next_packet_id = 1;

static uint8_t a5_checksum(const uint8_t *frame, size_t total_len) {
  uint16_t sum = 0;
  for (size_t i = 0; i < total_len; i++) {
    if (i != 5) sum += frame[i];  // byte 5 is checksum placeholder
  }
  return (uint8_t)(0xFF - (sum & 0xFF));
}

void send_a5_command(const uint8_t *payload, uint16_t payload_len) {
  uint8_t frame[64];
  if (payload_len + 6 > sizeof(frame)) return;

  frame[0] = 0xA5;
  frame[1] = 0x22;                 // command: WiFi/ESP -> MCU
  frame[2] = next_packet_id++;
  frame[3] = payload_len & 0xFF;
  frame[4] = payload_len >> 8;
  frame[5] = 0x00;                 // checksum placeholder

  memcpy(&frame[6], payload, payload_len);

  const size_t total_len = payload_len + 6;
  frame[5] = a5_checksum(frame, total_len);

  uart_write_bytes(frame, total_len);
}

// Example payloads.
static const uint8_t CMD_POWER_OFF[]      = {0x01,0x00,0xA0,0x00,0x00};
static const uint8_t CMD_POWER_ON[]       = {0x01,0x00,0xA0,0x00,0x01};
static const uint8_t CMD_DISPLAY_OFF[]    = {0x01,0x05,0xA1,0x00,0x00};
static const uint8_t CMD_DISPLAY_ON[]     = {0x01,0x05,0xA1,0x00,0x64};
static const uint8_t CMD_TIMER_ICON_OFF[] = {0x01,0x6A,0xA2,0x00,0x00};
static const uint8_t CMD_REQUEST_STATUS[] = {0x01,0x84,0x40,0x00};

void example_usage() {
  send_a5_command(CMD_REQUEST_STATUS, sizeof(CMD_REQUEST_STATUS));
}
