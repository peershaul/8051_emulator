
#ifndef MAIN8051_H
#define MAIN8051_H


#include <stdint.h>
#include <stdbool.h>

#define SFR_B 0x0
#define SFR_A 0x1
#define SFR_PSW 0x2
#define SFR_IP 0x3
#define SFR_P3 0x4
#define SFR_IE 0x5
#define SFR_P2 0x6
#define SFR_SBUF 0x7
#define SFR_SCON 0x8
#define SFR_P1 0x9
#define SFR_TH1 0xa
#define SFR_TH0 0xb
#define SFR_TL1 0xc
#define SFR_TL0 0xd
#define SFR_TMOD 0xe
#define SFR_TCON 0xf
#define SFR_PCON 0x10
#define SFR_DPH 0x11
#define SFR_DPL 0x12
#define SFR_SP 0x13
#define SFR_P0 0x14

#define SFR_AMOUNT 21 

typedef struct SFR {
  uint8_t address;
  uint8_t value;
  uint8_t bits[8];
  bool is_bit_addressable;
} SFR;

typedef struct Memory {
  uint8_t data_ram[127];
  uint16_t instraction_reg;
  SFR data_regs[21];
  uint8_t xdata_ram[0x10000];
  uint8_t rom[0x10000];
} Memory;


#endif 
