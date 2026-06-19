// Minimal A5 frame builder for Levoit Classic 300S UART protocol.
// UART parameters observed: 9600 8N1.
// This example only builds command frames (A5-22).
//
// Copyright (c) 2026 Maxim Pivovarov / MaxPi (Taxom)
// SPDX-License-Identifier: CC-BY-NC-SA-4.0

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Replace this with your platform UART write function.
extern void uart_write_bytes(const uint8_t *data, size_t len);

static uint8_t next_packet_id = 1;

static uint8_t a5_checksum(const uint8_t *frame, size_t total_len) {
  uint16_t sum = 0;

  for (size_t i = 0; i < total_len; i++) {
    if (i != 5) {
      sum += frame[i];  // byte 5 is checksum placeholder
    }
  }

  return (uint8_t)(0xFF - (sum & 0xFF));
}

void send_a5_command(const uint8_t *payload, uint16_t payload_len) {
  uint8_t frame[64];

  if (payload_len + 6 > sizeof(frame)) {
    return;
  }

  frame[0] = 0xA5;
  frame[1] = 0x22;                 // command: WiFi/ESP -> MCU
  frame[2] = next_packet_id++;
  frame[3] = payload_len & 0xFF;
  frame[4] = payload_len >> 8;
  frame[5] = 0x00;                 // checksum placeholder

  memcpy(&frame[6], payload, payload_len);

  const uint16_t total_len = payload_len + 6;
  frame[5] = a5_checksum(frame, total_len);

  uart_write_bytes(frame, total_len);
}

static void send_mode_sync(void) {
  const uint8_t payload[] = {0x01, 0x29, 0xA1, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
  send_a5_command(payload, sizeof(payload));
}

void example_usage(void) {
  const uint8_t power_on[] = {0x01, 0x00, 0xA0, 0x00, 0x01};
  send_a5_command(power_on, sizeof(power_on));

  const uint8_t display_on[] = {0x01, 0x05, 0xA1, 0x00, 0x64};
  send_a5_command(display_on, sizeof(display_on));

  const uint8_t night_light_30[] = {0x01, 0x03, 0xA0, 0x00, 0x01, 0x1E};
  send_a5_command(night_light_30, sizeof(night_light_30));

  send_mode_sync();
  const uint8_t manual_level_5[] = {0x01, 0x60, 0xA2, 0x00, 0x00, 0x01, 0x05};
  send_a5_command(manual_level_5, sizeof(manual_level_5));

  send_mode_sync();
  // Auto target 50%, lower 45%, upper 55%.
  const uint8_t auto_target_50[] = {0x01, 0x80, 0x40, 0x00, 0x32, 0x2D, 0x37, 0x09, 0x05, 0x01};
  send_a5_command(auto_target_50, sizeof(auto_target_50));

  const uint8_t stop_at_target_on[] = {0x01, 0xE5, 0xA5, 0x00, 0x01};
  send_a5_command(stop_at_target_on, sizeof(stop_at_target_on));

  const uint8_t request_status[] = {0x01, 0x84, 0x40, 0x00};
  send_a5_command(request_status, sizeof(request_status));
}
