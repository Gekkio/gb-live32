#ifndef HARDWARE_H
#define	HARDWARE_H

#include "system.h"
#include <stdint.h>

extern inline void low_GB_EN(void);
extern inline void high_GB_EN(void);
extern inline void low_OE(void);
extern inline void high_OE(void);
extern inline void low_WR(void);
extern inline void high_WR(void);
extern inline void low_GB_RES(void);
extern inline void high_GB_RES(void);

extern inline void cfg_A0_15_input(void);
extern inline void cfg_A0_15_output(void);
extern inline void cfg_D0_7_input(void);
extern inline void cfg_D0_7_output(void);

extern inline void write_A0_7(uint8_t value);
extern inline void write_A8_15(uint8_t value);
extern inline void write_D0_D7(uint8_t value);
extern inline uint8_t read_D0_D7(void);

void configure_hardware(void);

#endif	/* HARDWARE_H */
