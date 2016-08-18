#include <xc.h>
#include "hardware.h"

#include <stddef.h>

inline void low_GB() {
  PORTCbits.RC0 = 0;
}

inline void high_GB() {
  PORTCbits.RC0 = 1;
}

inline void low_OE() {
  PORTCbits.RC1 = 0;
}

inline void high_OE() {
  PORTCbits.RC1 = 1;
}

inline void low_WR() {
  PORTCbits.RC2 = 0;
}

inline void high_WR() {
  PORTCbits.RC2 = 1;
}

inline void low_RSTCTRL() {
  PORTEbits.RE2 = 0;
}

inline void high_RSTCTRL() {
  PORTEbits.RE2 = 1;
}

inline void write_A0_7(uint8_t value) {
  PORTB = value;
}

inline void write_A8_15(uint8_t value) {
  PORTA = value;
}

inline void write_D0_D7(uint8_t value) {
  PORTD = value;
}

inline uint8_t read_D0_D7() {
  return PORTD;
}

inline void cfg_A0_15_input() {
  TRISA = 0xFF;
  TRISB = 0xFF;
}

inline void cfg_A0_15_output() {
  TRISA = 0x00;
  TRISB = 0x00;
}

inline void cfg_D0_7_input() {
  TRISD = 0xFF;
}

inline void cfg_D0_7_output() {
  TRISD = 0x00;
}

static const uint8_t HEX[] = "0123456789ABCDEF";

uint8_t read_eeprom(uint8_t addr) {
  EEADR = addr;
  EEPGD = 0;
  CFGS = 0;
  RD = 1;
  return EEDATA;
}

void print_serial_number_utf16(uint16_t *out) {
  for (size_t i = 0; i < 8; i++) {
    uint8_t data = read_eeprom(i);
    out[i] = HEX[(data >> 4) & 15];
    out[i + 1] = HEX[(data >> 0) & 15];
  }
}

void configure_hardware() {
  // All I/O should be digital
  ANSELA = 0x00;
  ANSELB = 0x00;
  ANSELC = 0x00;
  ANSELD = 0x00;
  ANSELE = 0x00;

  // All I/O should be inputs
  TRISA = 0xFF;
  TRISB = 0xFF;
  TRISC = 0xFF;
  TRISD = 0xFF;
  TRISE = 0xFF;

  // All I/O latches should output high
  LATA = 0xFF;
  LATB = 0xFF;
  LATC = 0xFF;
  LATD = 0xFF;
  LATE = 0xFF;

  // By default, the system clock is based on the internal oscillator
  OSCCONbits.IRCF = 0b111; // Set internal oscillator to 16MHz
  // Wait for a stable clock
  while (!OSCCONbits.HFIOFS) {
  }

  // Enable interrupts for USB
  INTCONbits.PEIE = 1;
  INTCONbits.GIE = 1;

  // Enable active clock tuning based on USB
  ACTCONbits.ACTSRC = 1;
  ACTCONbits.ACTEN = 1;

  LATE2 = 0; // RSTCTRL, default to low
  TRISC0 = 0; // GB
  TRISC1 = 0; // OE
  TRISC2 = 0; // WR
  TRISE2 = 0; // RSTCTRL
  TRISC6 = 0; // TX
}