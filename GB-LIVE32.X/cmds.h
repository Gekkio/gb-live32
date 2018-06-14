#ifndef CMDS_H
#define	CMDS_H

#include <stdbool.h>
#include <stdint.h>

#include "nelmax.h"

enum StateTag {
  STATE_CMD,
  STATE_RX_STREAM,
  STATE_TX_STREAM,
};

struct State {
  enum StateTag tag;
  struct {
    uint8_t addr_h;
    uint8_t addr_l;
    uint16_t remaining;
  } stream;
};

const uint8_t STATUS_OK = 0xFF;
const uint8_t STATUS_ERR_STR = 0xFE;

typedef uint8_t ResponseCode;

extern ResponseCode dispatch_command(uint8_t command, size_t payload_size, struct State *state);

#endif	/* CMDS_H */
