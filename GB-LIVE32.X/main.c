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

static uint8_t out_buffer[CDC_DATA_OUT_EP_SIZE];

void handle_rx_data(uint8_t out_buf_len)
{
  uint8_t command;
  size_t payload_size;

  for (size_t i = 0; i < out_buf_len; i++) {
    if (nelmax_read(&NELMAX, out_buffer[i], &command, &payload_size)) {
      nelmax_write(&NELMAX, dispatch_command(command, payload_size));
      size_t response_len = nelmax_encode_response(&NELMAX);
      const uint8_t *encoded_packet = nelmax_encoded_packet(&NELMAX);

      size_t offset = 0;
      size_t chunk_len;
      while (offset < response_len) {
        chunk_len = MIN(response_len - offset, UINT8_MAX);
        const uint8_t *encoded_chunk = encoded_packet + offset;
        offset += chunk_len;

        while (!USBUSARTIsTxTrfReady()) {
          if (USBGetDeviceState() < CONFIGURED_STATE) {
            return;
          } else if (USBIsDeviceSuspended()) {
            continue;
          }
          CDCTxService();
        }

        putUSBUSART((uint8_t *) encoded_chunk, chunk_len);
      }
    }
  }
}

void main()
{
  configure_hardware();
  clear_sram(0xFF);

  USBDeviceInit();

  while (1) {
    CLRWDT();

    USB_DEVICE_STATE device_state = USBGetDeviceState();

    if (device_state == DETACHED_STATE) {
      USBDeviceAttach();
    } else if (device_state < CONFIGURED_STATE || USBIsDeviceSuspended()) {
      continue;
    }

    uint8_t out_buf_len = getsUSBUSART(out_buffer, CDC_DATA_OUT_EP_SIZE);
    if (out_buf_len > 0) {
      handle_rx_data(out_buf_len);
    }

    CDCTxService();
  }
}

void interrupt high_priority isr()
{
  USBDeviceTasks();
}

bool USER_USB_CALLBACK_EVENT_HANDLER(USB_EVENT event, void *pdata, uint16_t size)
{
  if (USBCDCEventHandler(event, pdata, size)) {
    return true;
  }
  switch (event) {
    case EVENT_CONFIGURED:
      CDCInitEP();
      break;
    case EVENT_EP0_REQUEST:
      USBCheckCDCRequest();
      break;
    default:
      break;
  }
  return true;
}
