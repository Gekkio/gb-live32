#ifndef HARDWARE_H
#define	HARDWARE_H

#include "system.h"
#include <stdint.h>

extern inline void low_GB_EN();
extern inline void high_GB_EN();
extern inline void low_OE();
extern inline void high_OE();
extern inline void low_WR();
extern inline void high_WR();
extern inline void low_GB_RES();
extern inline void high_GB_RES();

extern inline void cfg_A0_15_input();
extern inline void cfg_A0_15_output();
extern inline void cfg_D0_7_input();
extern inline void cfg_D0_7_output();

extern inline void write_A0_7(uint8_t value);
extern inline void write_A8_15(uint8_t value);
extern inline void write_D0_D7(uint8_t value);
extern inline uint8_t read_D0_D7();

void configure_hardware();

#endif	/* HARDWARE_H */
