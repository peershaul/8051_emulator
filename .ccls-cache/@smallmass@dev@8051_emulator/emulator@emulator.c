#include "emulator.h"

void run_program(Memory *memory) {

  memory->instraction_reg = 0;
  bool assert = false;
  
  while (!assert) {
    switch (memory->rom[memory->instraction_reg]) {
      // NOP
    case 0x00: break;

      // RR A
    case 0x03: {
      // NEED TO FIX THAT SHIT
      bool lsb = memory->data_regs[SFR_A].value & 1;
      memory->data_regs[SFR_A].value = memory->data_regs[SFR_A].value >> 1;
      memory->data_regs[SFR_A].value =
	memory->data_regs[SFR_A].value | (lsb << 6);

      memory->instraction_reg = 0xffff;
    }

      // MOV A, #n
    case 0x74:
      memory->data_regs[SFR_A].value = memory->rom[++memory->instraction_reg];
      break;

      // MOV location, #value
    case 0x75: {
      uint8_t location = memory->rom[++memory->instraction_reg];
      if(location <= 0x7f)
	memory->data_ram[location] = memory->rom[++memory->instraction_reg];
      else {
	SFR* reg = NULL;
	for(uint8_t i = 0; i < SFR_AMOUNT; i++)
          if (memory->data_regs[i].address == location) {
	    reg = &memory->data_regs[i];
	    break;
          }

        if (reg == NULL) {
	  fprintf(stderr, "invalid memory location\n");
	  assert = true;
        }

	else reg->value = memory->rom[++memory->instraction_reg];
      }
      break;
    }
    }

    printf("Value of ACC: %x\n", memory->data_regs[SFR_A].value);
    printf("Value of B: %x\n", memory->data_regs[SFR_B].value);
    printf("instraction position %x\n", memory->instraction_reg);
    memory->instraction_reg++;
  }
}
