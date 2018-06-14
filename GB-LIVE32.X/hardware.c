#include "system.h"
#include <stddef.h>

#include "hardware.h"

inline void low_GB_EN()
{
  LATCbits.LC0 = 0;
}

inline void high_GB_EN()
{
  LATCbits.LC0 = 1;
}

inline void low_OE()
{
  LATCbits.LC1 = 0;
}

inline void high_OE()
{
  LATCbits.LC1 = 1;
}

inline void low_WR()
{
  LATCbits.LC2 = 0;
}

inline void high_WR()
{
  LATCbits.LC2 = 1;
}

inline void low_GB_RES()
{
  LATEbits.LE2 = 0;
}

inline void high_GB_RES()
{
  LATEbits.LE2 = 1;
}

inline void write_A0_7(uint8_t value)
{
  LATB = value;
}

inline void write_A8_15(uint8_t value)
{
  LATA = value;
}

inline void write_D0_D7(uint8_t value)
{
  LATD = value;
}

inline uint8_t read_D0_D7()
{
  return PORTD;
}

inline void cfg_A0_15_input()
{
  TRISA = 0xFF;
  TRISB = 0xFF;
}

inline void cfg_A0_15_output()
{
  TRISA = 0x00;
  TRISB = 0x00;
}

inline void cfg_D0_7_input()
{
  TRISD = 0xFF;
}

inline void cfg_D0_7_output()
{
  TRISD = 0x00;
}

void configure_hardware()
{
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
  while (!OSCCONbits.HFIOFS || !OSCCON2bits.PLLRDY) {
  }

  // Enable interrupts for USB
  INTCONbits.PEIE = 1;
  INTCONbits.GIE = 1;

  // Enable active clock tuning based on USB
  ACTCONbits.ACTSRC = 1;
  ACTCONbits.ACTEN = 1;

  TRISCbits.RC0 = 0; // GB_EN
  TRISCbits.RC1 = 0; // OE
  TRISCbits.RC2 = 0; // WR
  TRISEbits.RE2 = 0; // GB_RES

  // Unused pins -> output low
  LATCbits.LC6 = 0;
  LATCbits.LC7 = 0;
  TRISCbits.RC6 = 0;
  TRISCbits.RC7 = 0;
  LATEbits.LE0 = 0;
  LATEbits.LE1 = 0;
  TRISEbits.RE0 = 0;
  TRISEbits.RE1 = 0;
}
