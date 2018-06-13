#ifndef CMDS_H
#define	CMDS_H

#include <stdbool.h>
#include <stdint.h>

#include "nelmax.h"

const uint8_t STATUS_OK = 0xFF;
const uint8_t STATUS_ERR_STR = 0xFE;

typedef uint8_t ResponseCode;

extern ResponseCode dispatch_command(uint8_t command, size_t payload_size);

#endif	/* CMDS_H */
