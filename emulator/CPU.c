#include "CPU.h"

#define debug_mode

#ifdef debug_mode
#include <stdio.h>
#endif


typedef struct CPU {
  BYTE reg_a;
  BYTE reg_b;
  WORD dptr;
  BYTE r_regs[8];
  BYTE psw;

  EPinout* pinout;
  
  WORD program_counter;
  BYTE stack[256];
  BYTE* external_ram;
} CPU;

CPU cpu;

void begin(EPinout *pinout) {
  cpu.pinout = pinout;

}
