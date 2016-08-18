#include <xc.h>
#include "hardware.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cmds.h"

enum mode {
  mode_gb, mode_dev
};

static int current_mode = mode_dev;

void cmd_ping(size_t payload_size) {
  uint8_t handshake[8];
  memcpy(handshake, nelmax_payload(&NELMAX), payload_size);
  nelmax_write_array(&NELMAX, handshake, payload_size);
}

void cmd_gameboy_mode() {
  __delay_us(1);
  low_GB();
  __delay_us(1);
  low_OE();
  __delay_us(1);
}

void cmd_development_mode() {
  __delay_us(1);
  high_GB();
  __delay_us(1);
  high_OE();
  __delay_us(1);
}

bool cmd_read_block(uint8_t addr_h) {
  if (current_mode != mode_dev) {
    return false;
  }
  cfg_A0_15_output();
  write_A8_15(addr_h);
  low_OE();

  uint8_t addr_l = 0;
  do {
    write_A0_7(addr_l++);
    nelmax_write(&NELMAX, read_D0_D7());
  } while (addr_l != 0);

  high_OE();
  cfg_A0_15_input();
  return true;
}

bool cmd_write_block(uint8_t addr_h) {
  if (current_mode != mode_dev) {
    return false;
  }
  cfg_A0_15_output();
  write_A8_15(addr_h);

  uint8_t addr_l = 0;
  do {
    write_A0_7(addr_l);
    low_WR();
    cfg_D0_7_output();
    write_D0_D7(nelmax_payload(&NELMAX)[addr_l + 1]);
    high_WR();
    cfg_D0_7_input();
    addr_l++;
  } while (addr_l != 0);

  cfg_A0_15_input();
  return true;
}

void cmd_reset() {
  high_RSTCTRL();
  __delay_ms(1);
  low_RSTCTRL();
}

bool dispatch_command(uint8_t command, size_t payload_size) {
  switch (command) {
    case 'P':
      cmd_ping(payload_size);
      return true;
    case 'G':
      cmd_gameboy_mode();
      return true;
    case 'D':
      cmd_development_mode();
      return true;
    case 'R':
      if (payload_size == 1) {
        return cmd_read_block(nelmax_payload(&NELMAX)[0]);
      }
      break;
    case 'W':
      if (payload_size == 257) {
        return cmd_write_block(nelmax_payload(&NELMAX)[0]);
      }
      break;
    case 'B':
      cmd_reset();
      return true;
  }
  return false;
}