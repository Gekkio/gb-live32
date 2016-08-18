#ifndef CMDS_H
#define	CMDS_H

#include <stdbool.h>
#include <stdint.h>

#include "nelmax.h"

bool dispatch_command(uint8_t command, size_t payload_size);

extern struct NelmaX NELMAX;

#endif	/* CMDS_H */
