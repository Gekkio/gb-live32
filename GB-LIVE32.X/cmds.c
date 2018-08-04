#include "system.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cmds.h"
#include "hardware.h"

extern struct NelmaX NELMAX;

static const uint8_t NIBBLE_ASCII[] = {
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

ResponseCode string_error_response(const char *str)
{
  NELMAX.nelma.response_size = 0;
  const char *ptr = str;
  while (*ptr != 0x00) {
    nelmax_write(&NELMAX, *(ptr++));
  }
  return STATUS_ERR_STR;
}

ResponseCode cmd_ping(size_t payload_size)
{
  uint8_t handshake[8];
  memcpy(handshake, nelmax_payload(&NELMAX), payload_size);
  nelmax_write_array(&NELMAX, handshake, payload_size);
  return STATUS_OK;
}

ResponseCode cmd_version(void)
{
  // 2.1
  nelmax_write(&NELMAX, 0x02);
  nelmax_write(&NELMAX, 0x01);
  return STATUS_OK;
}

ResponseCode cmd_status(void)
{
  nelmax_write(&NELMAX, state.unlocked);
  nelmax_write(&NELMAX, state.passthrough);
  nelmax_write(&NELMAX, state.reset);
  return STATUS_OK;
}

ResponseCode cmd_set_unlocked(bool value)
{
  state.unlocked = value;
  return STATUS_OK;
}

ResponseCode cmd_set_passthrough(bool value)
{
  if (value) {
    low_GB_EN();
    low_OE();
    state.passthrough = true;
  } else {
    high_OE();
    high_GB_EN();
    state.passthrough = false;
  }
  return STATUS_OK;
}

ResponseCode cmd_set_reset(bool value)
{
  if (value) {
    low_GB_RES();
    state.reset = true;
  } else {
    high_GB_RES();
    state.reset = false;
  }
  return STATUS_OK;
}

ResponseCode cmd_read_block(uint8_t addr_h)
{
  if (!state.unlocked) {
    return string_error_response("Locked: block reads not allowed");
  } else if (state.passthrough) {
    return string_error_response("Pass-through mode: block reads not allowed");
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
  return STATUS_OK;
}

ResponseCode cmd_write_block(uint8_t addr_h)
{
  if (!state.unlocked) {
    return string_error_response("Locked: block writes not allowed");
  } else if (state.passthrough) {
    return string_error_response("Pass-through mode: block writes not allowed");
  }
  cfg_A0_15_output();
  write_A8_15(addr_h);
  cfg_D0_7_output();

  uint8_t addr_l = 0;
  do {
    write_A0_7(addr_l);
    write_D0_D7(nelmax_payload(&NELMAX)[addr_l + 1]);
    low_WR();
    high_WR();
    addr_l++;
  } while (addr_l != 0);

  cfg_D0_7_input();
  cfg_A0_15_input();
  return STATUS_OK;
}

ResponseCode cmd_rx_stream(void)
{
  if (!state.unlocked) {
    return string_error_response("Locked: rx stream not allowed");
  } else if (state.passthrough) {
    return string_error_response("Pass-through mode: rx stream not allowed");
  }
  state.tag = STATE_RX_STREAM;
  state.stream.addr_h = 0x00;
  state.stream.addr_l = 0x00;
  state.stream.remaining = 0x8000;
  cfg_A0_15_output();
  write_A8_15(0x00);
  cfg_D0_7_output();

  return STATUS_OK;
}

ResponseCode cmd_tx_stream(void)
{
  if (!state.unlocked) {
    return string_error_response("Locked: tx stream not allowed");
  } else if (state.passthrough) {
    return string_error_response("Pass-through mode: tx stream not allowed");
  }
  state.tag = STATE_TX_STREAM;
  state.stream.addr_h = 0x00;
  state.stream.addr_l = 0x00;
  state.stream.remaining = 0x8000;
  cfg_A0_15_output();
  write_A8_15(0x00);
  low_OE();

  return STATUS_OK;
}

ResponseCode dispatch_command(uint8_t command, size_t payload_size)
{
  switch (command) {
    case 0x01:
      if (payload_size <= 8) {
        return cmd_ping(payload_size);
      }
      break;
    case 0x02:
      if (payload_size == 0) {
        return cmd_version();
      }
      break;
    case 0x03:
      if (payload_size == 0) {
        return cmd_status();
      }
      break;
    case 0x04:
      if (payload_size == 1) {
        return cmd_set_unlocked(nelmax_payload(&NELMAX)[0]);
      }
      break;
    case 0x05:
      if (payload_size == 1) {
        return cmd_set_passthrough(nelmax_payload(&NELMAX)[0]);
      }
      break;
    case 0x06:
      if (payload_size == 1) {
        return cmd_set_reset(nelmax_payload(&NELMAX)[0]);
      }
      break;
    case 0x07:
      if (payload_size == 1) {
        return cmd_read_block(nelmax_payload(&NELMAX)[0]);
      }
      break;
    case 0x08:
      if (payload_size == 257) {
        return cmd_write_block(nelmax_payload(&NELMAX)[0]);
      }
      break;
    case 0x09:
      if (payload_size == 0) {
        return cmd_rx_stream();
      }
      break;
    case 0x0A:
      if (payload_size == 0) {
        return cmd_tx_stream();
      }
      break;
  }
  string_error_response("Unsupported command: 0x");
  nelmax_write(&NELMAX, NIBBLE_ASCII[(uint8_t)(command >> 4)]);
  nelmax_write(&NELMAX, NIBBLE_ASCII[(uint8_t)(command & 0xf)]);
  return STATUS_ERR_STR;
}
