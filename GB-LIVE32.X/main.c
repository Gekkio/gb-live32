#include <xc.h>
#include "hardware.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "usb.h"

#include "nelmax.h"
#include "cmds.h"

struct NelmaX NELMAX = {{{0}}};

void clear_sram(uint8_t value) {
  cfg_A0_15_output();
  for (uint8_t addr_h = 0; addr_h < 0x80; addr_h++) {
    write_A8_15(addr_h);
    uint8_t addr_l = 0;
    do {
      write_A0_7(addr_l);
      low_WR();
      cfg_D0_7_output();
      write_D0_D7(value);
      high_WR();
      cfg_D0_7_input();
      addr_l++;
    } while (addr_l != 0);
  }
  cfg_A0_15_input();
}

void main() {
  configure_hardware();
  clear_sram(0xFF);

  // Cache serial number in RAM
  print_serial_number_utf16(serial_number.chars);

  usb_init();
  while (1) {
    CLRWDT();
    if (usb_is_configured() &&
        !usb_out_endpoint_halted(2) &&
        usb_out_endpoint_has_data(2)) {
      const uint8_t *out_buf;
      size_t out_buf_len = usb_get_out_buffer(2, &out_buf);

      if (out_buf_len > 0) {
        uint8_t command;
        size_t payload_size;

        for (size_t i = 0; i < out_buf_len; i++) {
          if (nelmax_read(&NELMAX, out_buf[i], &command, &payload_size)) {
            if (dispatch_command(command, payload_size)) {
              size_t response_len = nelmax_encode_response(&NELMAX);
              const uint8_t *encoded_packet = nelmax_encoded_packet(&NELMAX);

              size_t offset = 0;
              size_t chunk_len;
              while (offset < response_len) {
                chunk_len = min(response_len - offset, EP_2_LEN);
                const uint8_t *encoded_chunk = encoded_packet + offset;
                offset += chunk_len;

                while (usb_in_endpoint_busy(2)) {
                }
                memcpy(usb_get_in_buffer(2), encoded_chunk, chunk_len);
                usb_send_in_buffer(2, chunk_len);
              }

              if (chunk_len == EP_2_LEN) {
                while (usb_in_endpoint_busy(2)) {
                }
                usb_send_in_buffer(2, 0);
              }
            }
          }
        }
      }
      usb_arm_out_endpoint(2);
    }
  }
}

void interrupt high_priority isr() {
  usb_service();
}