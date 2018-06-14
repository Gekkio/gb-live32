#include "system.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cmds.h"
#include "hardware.h"
#include "nelmax.h"
#include "usb.h"
#include "usb_device_cdc.h"

struct NelmaX NELMAX = {{{0}}};

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

void clear_sram(uint8_t value)
{
  cfg_A0_15_output();
  cfg_D0_7_output();
  write_D0_D7(value);
  for (uint8_t addr_h = 0; addr_h < 0x80; addr_h++) {
    write_A8_15(addr_h);
    uint8_t addr_l = 0;
    do {
      write_A0_7(addr_l);
      low_WR();
      high_WR();
      addr_l++;
    } while (addr_l != 0);
  }
  cfg_D0_7_input();
  cfg_A0_15_input();
}

static uint8_t rx_buffer[CDC_DATA_OUT_EP_SIZE];
static uint8_t tx_buffer[CDC_DATA_IN_EP_SIZE];

struct RxState {
  const uint8_t *buf;
  size_t remaining;
};

struct TxState {
  const uint8_t *buf;
  size_t remaining;
};

static struct RxState rx_state = {0};
static struct TxState tx_state = {0};

struct State state = {0};

union Events {
  struct {
    uint8_t reset: 1;
    uint8_t sof: 1;
  };
  uint8_t byte;
};

static volatile union Events events;

void reset()
{
  cfg_D0_7_input();
  cfg_A0_15_input();
  high_OE();
  high_WR();
  high_GB_RES();
  high_GB_EN();
  rx_state.remaining = 0;
  tx_state.remaining = 0;
  events.byte = 0;
  memset(&state, 0, sizeof(struct State));
}

void check_blocked()
{
  if (!events.sof) {
    return;
  }
  events.sof = false;
  state.blocked_ticks += 1;
  if (state.blocked_ticks > 1000) {
    reset();
  }
}

void tick_state()
{
  if (events.reset) {
    events.reset = 0;
    reset();
  }
  switch (state.tag) {
    case STATE_CMD: {
      if (tx_state.remaining > 0) {
        check_blocked();
        return;
      }
      while (rx_state.remaining > 0) {
        uint8_t byte = *(rx_state.buf++);
        rx_state.remaining -= 1;

        uint8_t command;
        size_t payload_size;

        if (nelmax_read(&NELMAX, byte, &command, &payload_size)) {
          nelmax_write(&NELMAX, dispatch_command(command, payload_size));
          state.blocked_ticks = 0;
          tx_state.buf = nelmax_encoded_packet(&NELMAX);
          tx_state.remaining = nelmax_encode_response(&NELMAX);
          return;
        }
      }
      return;
    }
    case STATE_RX_STREAM: {
      if (rx_state.remaining <= 0) {
        check_blocked();
        return;
      }
      state.blocked_ticks = 0;
      if (state.stream.remaining <= 0) {
        cfg_D0_7_input();
        cfg_A0_15_input();
        state.tag = STATE_CMD;
        return;
      }
      while (state.stream.remaining > 0 && rx_state.remaining > 0) {
        uint8_t byte = *(rx_state.buf++);
        rx_state.remaining -= 1;

        write_A0_7(state.stream.addr_l);
        write_D0_D7(byte);
        low_WR();
        high_WR();

        state.stream.remaining -= 1;
        state.stream.addr_l += 1;
        if (state.stream.addr_l == 0x00) {
          state.stream.addr_h += 1;
          write_A8_15(state.stream.addr_h);
        }
      }
      return;
    }
    case STATE_TX_STREAM: {
      if (tx_state.remaining > 0) {
        check_blocked();
        return;
      }
      state.blocked_ticks = 0;
      if (state.stream.remaining <= 0) {
        high_OE();
        cfg_A0_15_input();
        state.tag = STATE_CMD;
        return;
      }
      size_t len = 0;
      while (state.stream.remaining > 0 && len < CDC_DATA_IN_EP_SIZE) {
        state.stream.remaining -= 1;
        write_A0_7(state.stream.addr_l);
        tx_buffer[len++] = read_D0_D7();
        state.stream.addr_l += 1;
        if (state.stream.addr_l == 0x00) {
          state.stream.addr_h += 1;
          write_A8_15(state.stream.addr_h);
        }
      }
      tx_state.buf = tx_buffer;
      tx_state.remaining = len;
    }
  }
}

void tick_rx()
{
  if (rx_state.remaining > 0) {
    return;
  }
  uint8_t remaining = getsUSBUSART(rx_buffer, CDC_DATA_OUT_EP_SIZE);
  if (remaining > 0) {
    rx_state.buf = rx_buffer;
    rx_state.remaining = remaining;
  }
}

void tick_tx()
{
  if (tx_state.remaining <= 0 || !USBUSARTIsTxTrfReady()) {
    return;
  }
  size_t chunk_len = MIN(tx_state.remaining, CDC_DATA_IN_EP_SIZE);
  putUSBUSART((uint8_t *) tx_state.buf, chunk_len);
  tx_state.buf += chunk_len;
  tx_state.remaining -= chunk_len;
}

void main()
{
  state.tag = STATE_CMD;
  events.byte = 0;
  configure_hardware();
  clear_sram(0xFF);

  USBDeviceInit();

  while (1) {
    CLRWDT();

    USB_DEVICE_STATE device_state = USBGetDeviceState();

    if (device_state == DETACHED_STATE) {
      USBDeviceAttach();
      continue;
    } else if (device_state < CONFIGURED_STATE) {
      continue;
    } else if (USBIsDeviceSuspended()) {
      SLEEP();
      continue;
    }

    tick_state();
    tick_rx();
    tick_tx();

    CDCTxService();
  }
}

void interrupt high_priority isr()
{
  USBDeviceTasks();
}

void suspend()
{
  ACTCONbits.ACTEN = 0;
  OSCCON2bits.INTSRC = 0;
  OSCCONbits.IRCF = 0b000;
}

void resume()
{
  OSCCONbits.IRCF = 0b111;
  OSCCON2bits.INTSRC = 1;
  while (!OSCCONbits.HFIOFS || !OSCCON2bits.PLLRDY) {
  }
  ACTCONbits.ACTEN = 1;
}

bool USER_USB_CALLBACK_EVENT_HANDLER(USB_EVENT event, void *pdata, uint16_t size)
{
  if (USBCDCEventHandler(event, pdata, size)) {
    return true;
  }
  switch (event) {
    case EVENT_SOF:
      events.sof = true;
      break;
    case EVENT_CONFIGURED:
      CDCInitEP();
      break;
    case EVENT_EP0_REQUEST:
      USBCheckCDCRequest();
      break;
    case EVENT_RESET:
      events.reset = true;
      break;
    case EVENT_SUSPEND:
      suspend();
      break;
    case EVENT_RESUME:
      resume();
      break;
    default:
      break;
  }
  return true;
}
