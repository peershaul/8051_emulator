#include "emulator.h"

#define DEBUG_OUT
#define ASSERT_WHEN_UNKNOWN

SFR *check_out_location(uint8_t location, Memory *memory) {
  for (uint8_t i = 0; i < SFR_AMOUNT; i++)
    if (memory->data_regs[i].address == location)
      return &memory->data_regs[i];

  fprintf(stderr, "invalid memory location\n");
  return NULL;
}

void calculate_parity(Memory* memory){
  uint8_t acc = memory->data_regs[SFR_A].value;
  uint8_t number_of_ones = 0;
  
  while(acc != 0){
    number_of_ones += acc & 0x1;
    acc = acc >> 1;
  }

  memory->data_regs[SFR_PSW].value = (memory->data_regs[SFR_PSW].value & 0xfe)
    | (number_of_ones & 0x1);
}

uint8_t add_8bit(uint8_t value, bool carry, Memory *memory) {

  // Calculating 8bit result
  uint8_t acc = memory->data_regs[SFR_A].value;
  uint16_t result = acc + value + carry;

  // Checking for C flag
  if (result & 0x100)
    memory->data_regs[SFR_PSW].value =
        memory->data_regs[SFR_PSW].value | (1 << 7);
  else
    memory->data_regs[SFR_PSW].value =
        memory->data_regs[SFR_PSW].value & (0xff >> 1);

  // Checking for AC flag
  uint8_t value_4b = value & 0x0f;
  uint8_t acc_4b = acc & 0x0f;
  uint8_t sum_4b = acc_4b + value_4b;

  if (sum_4b & 0x10)
    memory->data_regs[SFR_PSW].value =
        memory->data_regs[SFR_PSW].value | (1 << 6);
  else
    memory->data_regs[SFR_PSW].value = memory->data_regs[SFR_PSW].value & 0xbf;

  // Checking for OV flag
  uint8_t value_6b = value & 0x7f;
  uint8_t acc_6b = value & 0x7f;
  uint8_t sum_6b = value_6b + acc_6b;

  if ((sum_6b & 0x80) ^ (result > 0xff))
    memory->data_regs[SFR_PSW].value =
        memory->data_regs[SFR_PSW].value | (1 << 2);
  else
    memory->data_regs[SFR_PSW].value = memory->data_regs[SFR_PSW].value & 0xfb;

  return result;
}

uint8_t sub_8bit(uint8_t value, Memory *memory) {
  uint8_t acc = memory->data_regs[SFR_A].value;
  bool carry = memory->data_regs[SFR_PSW].value & 0x80;
  int16_t result = acc - value - carry;
  
  // Checking if the result is negative
  if(result & 0x8000)
    memory->data_regs[SFR_PSW].value = memory->data_regs[SFR_PSW].value | 0x80;
  else
    memory->data_regs[SFR_PSW].value = memory->data_regs[SFR_PSW].value & 0x7f;

  uint8_t value_4b = value & 0x0f;
  uint8_t acc_4b = acc & 0x0f;
  int8_t result_4b = acc_4b - value_4b;

  // Checking for a borrow in 4 lsb's
  if(result_4b & 0x80)
    memory->data_regs[SFR_PSW].value = memory->data_regs[SFR_PSW].value | (1 << 6);
  else
    memory->data_regs[SFR_PSW].value = memory->data_regs[SFR_PSW].value & ~(1 << 6);

  // Checking for the OV flag
  uint8_t value_6b = value & 0x7f;
  uint8_t acc_6b = acc & 0x7f;
  int8_t result_6b = acc_6b - value_6b;
  if((result_6b & 0x80) ^ (result & 0x8000))
    memory->data_regs[SFR_PSW].value = memory->data_regs[SFR_PSW].value | 0x04;
  else
    memory->data_regs[SFR_PSW].value = memory->data_regs[SFR_PSW].value & ~0x04;

  return result;
}

SFR *get_sfr_by_bit(uint8_t address, uint8_t *offset, Memory *memory) {
  for (uint8_t i = 0; i < SFR_AMOUNT; i++)
    if (memory->data_regs[i].is_bit_addressable)
      for (uint8_t j = 0; j < 8; j++)
        if (address == memory->data_regs[i].bits[j]) {
          *offset = j;
          return &memory->data_regs[i];
        }

  *offset = 8;
  return NULL;
}

void run_program(Memory *memory) {

  memory->instraction_reg = 0;
  bool assert = false;
  uint8_t display_value = 0;
  uint8_t psw_reg = 0;

  while (!assert) {

    printf("instraction location: %x\n", memory->instraction_reg);

    switch (memory->rom[memory->instraction_reg]) {
      // NOP
    case 0x00:
      break;

      // AJMP code address
    case 0x01:
    case 0x21:
    case 0x41:
    case 0x61:
    case 0x81:
    case 0xa1:
    case 0xc1:
    case 0xe1: {
      uint8_t page = (memory->rom[memory->instraction_reg] & 0xe0) >> 5;
      uint8_t lsb = memory->rom[++memory->instraction_reg];

      memory->instraction_reg = (memory->instraction_reg + 2) & 0xf800;
      memory->instraction_reg = memory->instraction_reg | lsb | (page << 8);
      break;
    }

      // LJMP address
    case 0x02: {
      uint8_t addr_msb = memory->rom[++memory->instraction_reg];
      uint8_t addr_lsb = memory->rom[++memory->instraction_reg];
      uint16_t addr = addr_lsb | (addr_msb << 8);
      memory->instraction_reg = addr - 1;
      break;
    }

      // RR A
    case 0x03: {
      uint8_t lsb = memory->data_regs[SFR_A].value & 1;
      memory->data_regs[SFR_A].value = memory->data_regs[SFR_A].value >> 1;
      memory->data_regs[SFR_A].value =
          memory->data_regs[SFR_A].value | (lsb << 7);
      calculate_parity(memory);
      break;
    }

      // INC A
    case 0x04: {
      memory->data_regs[SFR_A].value++;
      calculate_parity(memory);
      break;
    }

      // INC direct
    case 0x05: {
      uint8_t location = memory->rom[++memory->instraction_reg];
      if (location <= 0x7f)
        memory->data_ram[location]++;
      else {
        SFR *reg = check_out_location(location, memory);
        if (reg == NULL)
          assert = true;
        else
          reg->value++;
      }

      break;
    }

      // INC @Ri
    case 0x06:
    case 0x07: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0x06;
      uint8_t location = memory->data_ram[r_index + 8 * page];
      if (location <= 0x7f)
        memory->xdata_ram[location]++;
      else {
        SFR *reg = check_out_location(location, memory);
        if (reg == NULL)
          assert = true;
        else
          reg->value++;
      }
      break;
    }

      // INC Rn
    case 0x08:
    case 0x09:
    case 0x0a:
    case 0x0b:
    case 0x0c:
    case 0x0d:
    case 0x0e:
    case 0x0f: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0x08;
      memory->data_ram[r_index + page * 8]++;
      break;
    }

      // JBC bit_addr, relative_addr
    case 0x10: {
      uint8_t bit_addr = memory->rom[++memory->instraction_reg];
      int8_t relative = memory->rom[++memory->instraction_reg];

      if (bit_addr <= 0x7f) {
        uint8_t location = (bit_addr / 8) + 0x20;
        uint8_t offset = bit_addr % 8;
        if (memory->data_ram[location] & (1 << offset)) {
          memory->data_ram[location] =
              memory->data_ram[location] & ~(1 << offset);
          memory->instraction_reg += relative;
          break;
        } else
          break;
      }

      else {
        uint8_t offset;
        SFR *sfr = get_sfr_by_bit(bit_addr, &offset, memory);
        if (sfr->value & (1 << offset)) {
          sfr->value = sfr->value & ~(1 << offset);
          memory->instraction_reg += relative;
          break;
        } else
          break;
      }
    }

      // ACALL code address
    case 0x11:
    case 0x31:
    case 0x51:
    case 0x71:
    case 0x91:
    case 0xb1:
    case 0xd1:
    case 0xf1:{
      uint8_t page = (memory->rom[memory->instraction_reg] & 0xe0);
      uint8_t jmp_lsb = memory->rom[++memory->instraction_reg];

      memory->instraction_reg++;
      
      memory->data_ram[++memory->data_regs[SFR_SP].value] = memory->instraction_reg & 0x00ff;
      memory->data_ram[++memory->data_regs[SFR_SP].value] =
	(memory->instraction_reg & 0xff00) >> 8; 

      memory->instraction_reg = (memory->instraction_reg & 0xf800) | jmp_lsb | (page << 3);
      break;
    }
      
      // LCALL code address 
    case 0x12:{
      uint8_t lsb = memory->rom[++memory->instraction_reg];
      uint8_t msb = memory->rom[++memory->instraction_reg];

      memory->instraction_reg++;

      memory->data_ram[++memory->data_regs[SFR_SP].value] = memory->instraction_reg & 0x00ff;
      memory->data_ram[++memory->data_regs[SFR_SP].value] =
	(memory->instraction_reg & 0xff00) >> 8;

      memory->instraction_reg = lsb | (msb << 8);
      break;
    }


      // RRC A
    case 0x13: {
      uint8_t acc = memory->data_regs[SFR_A].value;
      uint8_t carry_flag = memory->data_regs[SFR_PSW].value & 0x80;
      bool lsb = acc & 1;

      acc = acc >> 1;
      acc = acc | carry_flag;

      if (lsb)
        memory->data_regs[SFR_PSW].value =
            memory->data_regs[SFR_PSW].value | 0x80;
      else
        memory->data_regs[SFR_PSW].value =
            memory->data_regs[SFR_PSW].value & ~0x80;

      memory->data_regs[SFR_A].value = acc;
      calculate_parity(memory);
      break;
    }

      // DEC A
    case 0x14:
      memory->data_regs[SFR_A].value--;
      calculate_parity(memory);
      break;

      // DEC direct
    case 0x15: {
      uint8_t location = memory->rom[++memory->instraction_reg];
      if (location <= 0x7f)
        memory->data_ram[location]--;
      else {
        SFR *sfr = check_out_location(location, memory);
        if (sfr == NULL)
          assert = true;
        else
          sfr->value--;
      }
    }

      // DEC @Ri
    case 0x16:
    case 0x17: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0x16;
      uint8_t location = memory->data_ram[r_index + 8 * page];
      memory->data_ram[location]--;
    }

      // DEC Rn
    case 0x18:
    case 0x19:
    case 0x1a:
    case 0x1b:
    case 0x1c:
    case 0x1d:
    case 0x1e:
    case 0x1f: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0x18;
      memory->data_ram[r_index + page * 8]--;
    }

      // JB bit_addr, relative
    case 0x20: {
      uint8_t bit = memory->rom[++memory->instraction_reg];
      int8_t relative = memory->rom[++memory->instraction_reg];
      if (bit <= 0x7f) {
        uint8_t location = bit / 8 + 0x20;
        uint8_t offset = bit % 8;
        if (memory->data_ram[location] & (1 << offset))
          memory->instraction_reg += relative;
        else
          break;
      }

      else {
        uint8_t offset;
        SFR *sfr = get_sfr_by_bit(bit, &offset, memory);
        if (sfr == NULL)
          assert = true;
        if (sfr->value & (1 << offset))
          memory->instraction_reg += relative;
      }
      break;
    }


      // RET
    case 0x22:{
      uint8_t msb = memory->data_ram[--memory->data_regs[SFR_SP].value];
      uint8_t lsb = memory->data_ram[--memory->data_regs[SFR_SP].value];
      memory->instraction_reg = lsb | (msb << 8);
    }
      
      // RL A
    case 0x23: {
      uint8_t msb = memory->data_regs[SFR_A].value & 0x80;
      memory->data_regs[SFR_A].value = memory->data_regs[SFR_A].value << 1;
      memory->data_regs[SFR_A].value =
          memory->data_regs[SFR_A].value | (msb >> 7);
      break;
    }

      // ADD A, #immideate
    case 0x24: {
      uint8_t value = memory->rom[++memory->instraction_reg];
      memory->data_regs[SFR_A].value = add_8bit(value, 0, memory);
      calculate_parity(memory);
      break;
    }

      // ADD A, ram_addr
    case 0x25: {
      uint8_t location = memory->rom[++memory->instraction_reg];
      uint8_t value;
      if (location <= 0x7f)
        value = memory->data_ram[location];
      else {
        SFR *sfr = check_out_location(location, memory);
        if (sfr == NULL) {
          assert = true;
          break;
        } else
          value = sfr->value;
      }

      memory->data_regs[SFR_A].value = add_8bit(value, 0, memory);
      break;
    }

      // ADD A, @Ri
    case 0x26:
    case 0x27: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3; 
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0x26;
      uint8_t location = memory->data_ram[r_index + page * 8];
      uint8_t value;

      if (location <= 0x7f)
        value = memory->data_ram[location];
      else {
        SFR *sfr = check_out_location(location, memory);
        if (sfr == NULL) {
          assert = true;
          break;
        } else
          value = sfr->value;
      }

      memory->data_regs[SFR_A].value = add_8bit(value, 0, memory);
      calculate_parity(memory);
      break;
    }

      // ADD A, Rn
    case 0x28:
    case 0x29:
    case 0x2a:
    case 0x2b:
    case 0x2c:
    case 0x2d:
    case 0x2e:
    case 0x2f: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0x28;
      uint8_t value = memory->data_ram[r_index + page * 8];
      memory->data_regs[SFR_A].value = add_8bit(value, 0, memory);
      calculate_parity(memory);
      break;
    }

      // JNB bit, relative
    case 0x30: {
      uint8_t bit = memory->rom[++memory->instraction_reg];
      int8_t relative = memory->rom[++memory->instraction_reg];
      if (bit <= 0x7f) {
        uint8_t location = bit / 8 + 0x20;
        uint8_t offset = bit % 8;
        if (!(memory->data_ram[location] & (1 << offset)))
          memory->instraction_reg += relative;
        else
          break;
      }

      else {
        uint8_t offset;
        SFR *sfr = get_sfr_by_bit(bit, &offset, memory);
        if (sfr == NULL)
          assert = true;
        if (!(sfr->value & (1 << offset)))
          memory->instraction_reg += relative;
      }
      calculate_parity(memory);
      break;
    }

      // RLC A
    case 0x33: {
      uint8_t acc = memory->data_regs[SFR_A].value;
      bool msb = acc & 0x80;
      bool carry_flag = memory->data_regs[SFR_PSW].value & 0x80;

      acc = acc << 1;
      acc = acc | carry_flag;

      if (msb)
        memory->data_regs[SFR_PSW].value =
            memory->data_regs[SFR_PSW].value | 0x80;
      else
        memory->data_regs[SFR_PSW].value =
            memory->data_regs[SFR_PSW].value & ~0x80;

      memory->data_regs[SFR_A].value = acc;
      calculate_parity(memory);
      break;
    }

      // ADDC A, #immideate
    case 0x34: {
      uint8_t value = memory->rom[++memory->instraction_reg];
      bool carry_flag = memory->data_regs[SFR_PSW].value & 0x80;
      memory->data_regs[SFR_A].value = add_8bit(value, carry_flag, memory);
      calculate_parity(memory);
      break;
    }

      // ADDC A, direct
    case 0x35: {
      uint8_t location = memory->rom[++memory->instraction_reg];
      bool carry_flag = memory->data_regs[SFR_PSW].value & 0x80;
      uint8_t value;

      if (location <= 0x7f)
        value = memory->data_ram[location];

      else {
        SFR *sfr = check_out_location(location, memory);
        if (sfr == NULL) {
          assert = true;
          break;
        }
        value = sfr->value;
      }

      memory->data_regs[SFR_A].value = add_8bit(value, carry_flag, memory);
      calculate_parity(memory);
      break;
    }

      // ADDC A, @Ri
    case 0x36:
    case 0x37: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0x36;
      bool carry_flag = memory->data_regs[SFR_PSW].value & 0x80;
      uint8_t location = memory->data_ram[r_index + page * 8];
      uint8_t value;

      if (location <= 0x7f)
        value = memory->data_ram[location];

      else {
        SFR *sfr = check_out_location(location, memory);
        if (sfr == NULL) {
          assert = true;
          break;
        }
        value = sfr->value;
      }

      memory->data_regs[SFR_A].value = add_8bit(value, carry_flag, memory);
      calculate_parity(memory);
      break;
    }

      // ADDC A, Rn
    case 0x38:
    case 0x39:
    case 0x3a:
    case 0x3b:
    case 0x3c:
    case 0x3d:
    case 0x3e:
    case 0x3f: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0x38;
      bool carry_flag = memory->data_regs[SFR_PSW].value & 0x80;
      uint8_t value = memory->data_ram[r_index + page * 8];

      memory->data_regs[SFR_A].value = add_8bit(value, carry_flag, memory);
      calculate_parity(memory);
      break;
    }

      // JC relative
    case 0x40: {
      int8_t relative = memory->rom[++memory->instraction_reg];
      if (memory->data_regs[SFR_PSW].value & 0x80)
        memory->instraction_reg += relative;
      break;
    }

      // ORL direct, A
    case 0x42: {
      uint8_t location = memory->rom[++memory->instraction_reg];

      if (location <= 0x7f)
        memory->data_ram[location] =
            memory->data_ram[location] | memory->data_regs[SFR_A].value;
      else {
        SFR *sfr = check_out_location(location, memory);
        if (sfr == NULL)
          assert = true;
        else
          sfr->value = sfr->value | memory->data_regs[SFR_A].value;
      }
      break;
    }

      // ORL direct, #immideate
    case 0x43: {
      uint8_t location = memory->rom[++memory->instraction_reg];
      uint8_t value = memory->rom[++memory->instraction_reg];

      if (location <= 0x7f)
        memory->data_ram[location] = memory->data_ram[location] | value;
      else {
        SFR *sfr = check_out_location(location, memory);
        if (sfr == NULL)
          assert = true;
        else
          sfr->value = sfr->value | value;
      }

      break;
    }

      // ORL A, #immideate
    case 0x44: {
      memory->data_regs[SFR_A].value = memory->data_regs[SFR_A].value |
                                       memory->rom[++memory->instraction_reg];
      calculate_parity(memory);
      break;
    }

      // ORL A, direct
    case 0x45: {
      uint8_t location = memory->rom[++memory->instraction_reg];

      if (location <= 0x7f)
        memory->data_regs[SFR_A].value =
            memory->data_regs[SFR_A].value | memory->data_ram[location];
      else {
        SFR *sfr = check_out_location(location, memory);
        if (sfr == NULL)
          assert = true;
        else
          memory->data_regs[SFR_A].value =
              memory->data_regs[SFR_A].value | sfr->value;
      }
      break;
    }

      // ORL A, @Ri
    case 0x46:
    case 0x47: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0x46;
      uint8_t location = memory->data_ram[r_index + 8 * page];

      if (location <= 0x7f)
        memory->data_regs[SFR_A].value =
            memory->data_regs[SFR_A].value | memory->data_ram[location];

      else {
        SFR *sfr = check_out_location(location, memory);
        if (sfr == NULL)
          assert = true;
        else
          memory->data_regs[SFR_A].value =
              memory->data_regs[SFR_A].value | sfr->value;
      }

      calculate_parity(memory);
      break;
    }

      // ORL A, Rn
    case 0x48:
    case 0x49:
    case 0x4a:
    case 0x4b:
    case 0x4c:
    case 0x4d:
    case 0x4e:
    case 0x4f: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0x48;
      memory->data_regs[SFR_A].value =
          memory->data_regs[SFR_A].value | memory->data_ram[r_index + page * 8];
      calculate_parity(memory);
      break;
    }

      // JNC relative
    case 0x50: {
      int8_t relative = memory->rom[++memory->instraction_reg];
      if (!(memory->data_regs[SFR_PSW].value & 0x80))
        memory->instraction_reg += relative;
      break;
    }

      // ANL direct, A
    case 0x52: {
      uint8_t location = memory->rom[++memory->instraction_reg];

      if (location <= 0x7f)
        memory->data_ram[location] =
            memory->data_ram[location] & memory->data_regs[SFR_A].value;
      else {
        SFR *sfr = check_out_location(location, memory);
        if (sfr == NULL)
          assert = true;
        else
          sfr->value = sfr->value & memory->data_regs[SFR_A].value;
      }
      break;
    }

      // ANL direct, #immideate
    case 0x53: {
      uint8_t location = memory->rom[++memory->instraction_reg];
      uint8_t value = memory->rom[++memory->instraction_reg];

      if (location <= 0x7f)
        memory->data_ram[location] = memory->data_ram[location] & value;
      else {
        SFR *sfr = check_out_location(location, memory);
        if (sfr == NULL)
          assert = true;
        else
          sfr->value = sfr->value & value;
      }

      break;
    }

      // ANL A, #immideate
    case 0x54: {
      memory->data_regs[SFR_A].value = memory->data_regs[SFR_A].value &
                                       memory->rom[++memory->instraction_reg];
      calculate_parity(memory);
      break;
    }

      // ANL A, direct
    case 0x55: {
      uint8_t location = memory->rom[++memory->instraction_reg];

      if (location <= 0x7f)
        memory->data_regs[SFR_A].value =
            memory->data_regs[SFR_A].value & memory->data_ram[location];
      else {
        SFR *sfr = check_out_location(location, memory);
        if (sfr == NULL)
          assert = true;
        else
          memory->data_regs[SFR_A].value =
              memory->data_regs[SFR_A].value & sfr->value;
      }
      calculate_parity(memory);
      break;
    }

      // ANL A, @Ri
    case 0x56:
    case 0x57: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0x46;
      uint8_t location = memory->data_ram[r_index + page * 8];

      if (location <= 0x7f)
        memory->data_regs[SFR_A].value =
            memory->data_regs[SFR_A].value & memory->data_ram[location];

      else {
        SFR *sfr = check_out_location(location, memory);
        if (sfr == NULL)
          assert = true;
        else
          memory->data_regs[SFR_A].value =
              memory->data_regs[SFR_A].value & sfr->value;
      }

      calculate_parity(memory);
      break;
    }

      // ANL A, Rn
    case 0x58:
    case 0x59:
    case 0x5a:
    case 0x5b:
    case 0x5c:
    case 0x5d:
    case 0x5e:
    case 0x5f: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0x48;
      memory->data_regs[SFR_A].value =
          memory->data_regs[SFR_A].value & memory->data_ram[r_index + page * 8];
      calculate_parity(memory);
      break;
    }

      // JZ relative
    case 0x60: {
      int8_t relative = memory->rom[++memory->instraction_reg];
      if (memory->data_regs[SFR_A].value == 0)
        memory->instraction_reg += relative;
      break;
    }

      // XRL direct, A
    case 0x62: {
      uint8_t location = memory->rom[++memory->instraction_reg];

      if (location <= 0x7f)
        memory->data_ram[location] =
            memory->data_ram[location] ^ memory->data_regs[SFR_A].value;
      else {
        SFR *sfr = check_out_location(location, memory);
        if (sfr == NULL)
          assert = true;
        else
          sfr->value = sfr->value ^ memory->data_regs[SFR_A].value;
      }
      break;
    }

      // XRL direct, #immideate
    case 0x63: {
      uint8_t location = memory->rom[++memory->instraction_reg];
      uint8_t value = memory->rom[++memory->instraction_reg];

      if (location <= 0x7f)
        memory->data_ram[location] = memory->data_ram[location] ^ value;
      else {
        SFR *sfr = check_out_location(location, memory);
        if (sfr == NULL)
          assert = true;
        else
          sfr->value = sfr->value ^ value;
      }
      break;
    }
      // XRL A, direct
    case 0x64: {
      uint8_t location = memory->rom[++memory->instraction_reg];

      if (location <= 0x7f)
        memory->data_regs[SFR_A].value =
            memory->data_regs[SFR_A].value ^ memory->data_ram[location];
      else {
        SFR *sfr = check_out_location(location, memory);
        if (sfr == NULL)
          assert = true;
        else
          memory->data_regs[SFR_A].value =
              memory->data_regs[SFR_A].value ^ sfr->value;
      }
      calculate_parity(memory);
      break;
    }

      // XRL A, @Ri
    case 0x65:
    case 0x66:{
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0x46;
      uint8_t location = memory->data_ram[r_index];

      if (location <= 0x7f)
        memory->data_regs[SFR_A].value =
            memory->data_regs[SFR_A].value ^ memory->data_ram[location];

      else {
        SFR *sfr = check_out_location(location, memory);
        if (sfr == NULL)
          assert = true;
        else
          memory->data_regs[SFR_A].value =
              memory->data_regs[SFR_A].value ^ sfr->value;
      }

      calculate_parity(memory);
      break;
    }

      // XRL A, Rn
    case 0x68:
    case 0x69:
    case 0x6a:
    case 0x6b:
    case 0x6c:
    case 0x6d:
    case 0x6e:
    case 0x6f: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0x48;
      memory->data_regs[SFR_A].value =
          memory->data_regs[SFR_A].value ^ memory->data_ram[r_index];
      calculate_parity(memory);
      break;
    }

      // JNZ relative
    case 0x70: {
      int8_t relative = memory->rom[++memory->instraction_reg];
      if(memory->data_regs[SFR_A].value != 0)
	memory->instraction_reg += relative;
      break;
    }

      // ORL C, bit
    case 0x72: {
      uint8_t bit = memory->rom[++memory->instraction_reg];
      bool bit_status;
      if (bit <= 0x7f) {
        uint8_t location = bit / 8 + 0x20;
        uint8_t offset = bit % 8;
	bit_status = memory->data_ram[location] & (1 << offset);
      } else {
	uint8_t offset;
	SFR* sfr = get_sfr_by_bit(bit, &offset, memory);
	if(sfr == NULL){ assert = true; break; }
	else bit_status = sfr->value & (1 << offset);
      }
      memory->data_regs[SFR_PSW].value = memory->data_regs[SFR_PSW].value | (bit_status << 7);
      break;
    }

      // JMP @A+DPTR
    case 0x73:{
      uint16_t dptr = memory->data_regs[SFR_DPL].value | (memory->data_regs[SFR_DPH].value << 8);
      memory->instraction_reg = memory->data_regs[SFR_A].value + dptr;
      break;
    }

    // MOV A, #n
    case 0x74:
      memory->data_regs[SFR_A].value = memory->rom[++memory->instraction_reg];
      calculate_parity(memory);
      break;

      // MOV location, #value
    case 0x75: {
      uint8_t location = memory->rom[++memory->instraction_reg];
      if (location <= 0x7f)
        memory->data_ram[location] = memory->rom[++memory->instraction_reg];
      else {
        SFR *reg = check_out_location(location, memory);
        if (reg == NULL)
          assert = true;
        else
          reg->value = memory->rom[++memory->instraction_reg];
      }
      break;
    }

      // MOV @Ri, #immideate
    case 0x76:
    case 0x77: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0x76;
      uint8_t location = memory->data_ram[r_index + 8 * page];
      if(location <= 0x7f)
	memory->data_ram[location] = memory->rom[++memory->instraction_reg];
      else {
	SFR* sfr = check_out_location(location, memory);
	if(sfr == NULL) assert = true;
	else sfr->value = memory->rom[++memory->instraction_reg];
      }
      break;
    }

      // MOV Rn, #immideate
    case 0x78:
    case 0x79:
    case 0x7a:
    case 0x7b:
    case 0x7c:
    case 0x7d:
    case 0x7e:
    case 0x7f: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0x78;
      memory->data_ram[r_index + 8 * page] = memory->rom[++memory->instraction_reg];
      break;
    }

      // sjmp relative
    case 0x80: {
      int8_t offset = memory->rom[++memory->instraction_reg];
      memory->instraction_reg += offset;
      break;
    }

      // ANL C, bit 
    case 0x82: {
      uint8_t bit = memory->rom[++memory->instraction_reg];
      bool bit_status;

      if (bit <= 0x7f) {
	uint8_t offset = bit % 8;
	uint8_t location = bit / 8 + 0x20;
	bit_status = memory->data_ram[location] & (1 << offset);
      } else {
	uint8_t offset;
	SFR* sfr = get_sfr_by_bit(bit, &offset, memory);
	if(sfr == NULL) { assert = true; break; } 
	bit_status = sfr->value & (1 << offset);
      }

      if(bit_status && (memory->data_regs[SFR_A].value & 0x80))
	memory->data_regs[SFR_PSW].value = memory->data_regs[SFR_PSW].value | 0x80;
      else
	memory->data_regs[SFR_PSW].value = memory->data_regs[SFR_PSW].value & 0x7f;
      break;
    }

      // MOVC A, @A+PC
    case 0x83: {
      uint16_t location = memory->instraction_reg + 1 + memory->data_regs[SFR_A].value;
      memory->data_regs[SFR_A].value = memory->rom[location];
      calculate_parity(memory);
      break;
    }

      // DIV AB
    case 0x84: {

      // Reseting the C and OV flags 
      memory->data_regs[SFR_PSW].value = memory->data_regs[SFR_PSW].value & ~0x84;

      if (memory->data_regs[SFR_B].value == 0) {
        // Setting the OV flag if a division by 0 is attempted
        memory->data_regs[SFR_PSW].value =
            memory->data_regs[SFR_PSW].value | 0x04;
        break;
      }

      uint8_t result = memory->data_regs[SFR_A].value / memory->data_regs[SFR_B].value;
      uint8_t remainder = memory->data_regs[SFR_A].value % memory->data_regs[SFR_B].value; 
      memory->data_regs[SFR_A].value = result;
      memory->data_regs[SFR_B].value = remainder;
      break;
    }

      // MOV direct, direct
    case 0x85: {
      uint8_t source_loc = memory->rom[++memory->instraction_reg];
      uint8_t source_val;
      if(source_loc <= 0x7f)
	source_val = memory->data_ram[source_loc];
      else {
	SFR* sfr = check_out_location(source_loc, memory);
	if(sfr == NULL) {assert = true; break;}
	source_val = sfr->value;
      }

      uint8_t dest_loc = memory->rom[++memory->instraction_reg];
      if(dest_loc <= 0x7f)
	memory->data_ram[dest_loc] = source_val;
      else {
        SFR *sfr = check_out_location(dest_loc, memory);
        if(sfr == NULL) assert = true;
	else sfr->value = source_val;
      }

      break;
    }

    case 0x86:
    case 0x87: {
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0x86;
      uint8_t source_loc = memory->data_ram[r_index];
      uint8_t source_val;
      if(source_loc <= 0x7f)
	source_val = memory->data_ram[source_loc];
      else {
	SFR* sfr = check_out_location(source_loc, memory);
	if(sfr == NULL) {assert = true; break;}
	source_val = sfr->value;
      }

      uint8_t dest_loc = memory->rom[++memory->instraction_reg];
      if(dest_loc <= 0x7f)
	memory->data_ram[dest_loc] = source_val;
      else {
        SFR *sfr = check_out_location(dest_loc, memory);
        if(sfr == NULL) assert = true;
	else sfr->value = source_val;
      }

      break;
    }

      
      // MOV DPTR, #immideate
    case 0x90: {
      uint8_t msb = memory->rom[++memory->instraction_reg];
      uint8_t lsb = memory->rom[++memory->instraction_reg];
      memory->data_regs[SFR_DPL].value = lsb;
      memory->data_regs[SFR_DPH].value = msb;
      break;
    }

      // MOV bit, C
    case 0x92: {
      uint8_t bit = memory->rom[++memory->instraction_reg];
      bool carry = memory->data_regs[SFR_PSW].value & 0x80;
      if (bit <= 0x7f) {
	uint8_t offset = bit % 8;
	uint8_t location = bit / 8 + 0x20;
	if(carry)
	  memory->data_ram[location] = memory->data_ram[location] | (1 << offset);
	else
	  memory->data_ram[location] = memory->data_ram[location] & ~(1 << offset);
      }
      else{
	uint8_t offset;
	SFR* sfr = get_sfr_by_bit(bit, &offset, memory);
	if(sfr == NULL) assert = true;
	else if(carry) sfr->value = sfr->value | (1 << offset);
	else sfr->value = sfr->value & ~(1 << offset);
      }

      break;
    }

      // MOVC A, @A+DPTR 
    case 0x93:{
      uint16_t dptr = memory->data_regs[SFR_DPL].value | (memory->data_regs[SFR_DPH].value << 8);
      memory->data_regs[SFR_A].value = memory->rom[dptr + memory->data_regs[SFR_A].value];
      calculate_parity(memory);
      break;
    }

      // SUBB A, #immideate
    case 0x94: {
      uint8_t value = memory->rom[++memory->instraction_reg];
      memory->data_regs[SFR_A].value = sub_8bit(value, memory);
      calculate_parity(memory);
      break;
    }

      // SUBB A, direct
    case 0x95: {
      uint8_t location = memory->rom[++memory->instraction_reg];
      uint8_t value;
      if(location <= 0x7f)
	value = memory->data_ram[location];
      else {
	SFR* sfr = check_out_location(location, memory);
	if(sfr == NULL) {assert = true; break;}
	value = sfr->value;
      }

      memory->data_regs[SFR_A].value = sub_8bit(value, memory);
      calculate_parity(memory);
      break;
    }

      // SUBB A, @Ri
    case 0x96:
    case 0x97: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0x96;
      uint8_t location = memory->data_ram[r_index + 8 * page];
      uint8_t value;

      if(location <= 0x7f)
	value = memory->data_ram[location];
      else {
	SFR* sfr = check_out_location(location, memory);
	if(sfr == NULL) {assert = true; break;}
	value = sfr->value;
      }

      memory->data_regs[SFR_A].value = sub_8bit(value, memory);
      calculate_parity(memory);
      break;
    }
      // SUBB A, Rn
    case 0x98:
    case 0x99:
    case 0x9a:
    case 0x9b:
    case 0x9c:
    case 0x9d:
    case 0x9e:
    case 0x9f: {
      uint8_t r_index = memory->rom[memory->instraction_reg];
      uint8_t value = memory->data_ram[r_index];

      memory->data_regs[SFR_A].value = sub_8bit(value, memory);
      break;
    }

      // ORL C, /bit 
    case 0xa0:{
      uint8_t bit_addr = memory->rom[++memory->instraction_reg];
      

      if(!(memory->data_regs[SFR_PSW].value & 0x80)){
	bool inv_state;
	uint8_t offset;
	
	if(bit_addr <= 0x7f){
	  uint8_t location = bit_addr / 8 + 0x20;
	  offset = bit_addr % 8;
	  inv_state = !(memory->data_ram[location] & (1 << offset));
	}

	else{
	  SFR* sfr = get_sfr_by_bit(bit_addr, &offset, memory);
	  inv_state = !(sfr->value & (1 << offset));
	}

	memory->data_regs[SFR_A].value = memory->data_regs[SFR_A].value | (inv_state << 7);
      }

      calculate_parity(memory);
      break;
    }

      // MOV C, bit
    case 0xa2: {
      uint8_t bit = memory->rom[++memory->instraction_reg];
      bool state;
      if (bit <= 0x7f) {
	uint8_t location = bit / 8 + 0x20;
	uint8_t offset = bit % 8;
	state = memory->data_ram[location] & (1 << offset);
      } else {
	uint8_t offset;
	SFR* sfr = get_sfr_by_bit(bit, &offset, memory);
	state = sfr->value & (1 << offset);
      }

      if(state)
	memory->data_regs[SFR_PSW].value = memory->data_regs[SFR_PSW].value | 0x80;
      else
	memory->data_regs[SFR_PSW].value = memory->data_regs[SFR_PSW].value & 0x7f;

      break;
    }

      // INC DPTR
    case 0xa3:{
      uint16_t dpl = memory->data_regs[SFR_DPL].value + 1;
      if(dpl & 0x100) memory->data_regs[SFR_DPH].value++;
      memory->data_regs[SFR_DPL].value = dpl;
      break;
    }

      // MUL AB
    case 0xa4:{
      // Clear the carry bit
      memory->data_regs[SFR_PSW].value = memory->data_regs[SFR_PSW].value & 0x7f;

      uint16_t result = memory->data_regs[SFR_A].value * memory->data_regs[SFR_B].value;
      if (result > 0xff) {
        memory->data_regs[SFR_PSW].value =
            memory->data_regs[SFR_PSW].value | 0x04;
      }
    }

      // MOV @Ri, direct
    case 0xa6:
    case 0xa7: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0xa6;
      uint8_t source_loc = memory->rom[++memory->instraction_reg];
      uint8_t source_val;
      if(source_loc <= 0x7f)
	source_val = memory->data_ram[source_loc];
      else {
	SFR* sfr = check_out_location(source_loc, memory);
	if(sfr == NULL) {assert = true; break;}
	source_val = sfr->value;
      }

      uint8_t dest_loc = memory->data_ram[r_index + page * 8];
      if(dest_loc <= 0x7f)
	memory->data_ram[dest_loc] = source_val;
      else {
        SFR *sfr = check_out_location(dest_loc, memory);
        if(sfr == NULL) assert = true;
	else sfr->value = source_val;
      }

      break;
    }

      
      // MOV Rn, direct
    case 0xa8:
    case 0xa9:
    case 0xaa:
    case 0xab:
    case 0xac:
    case 0xad:
    case 0xae:
    case 0xaf: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0xa8;
      uint8_t location = memory->rom[++memory->instraction_reg];

      if (location <= 0x7f)
        memory->data_ram[r_index + page * 8] = memory->data_ram[location];

      else {
        SFR *sfr = check_out_location(location, memory);

        if (sfr == NULL)
          assert = true;
        else
          memory->data_ram[r_index + page * 8] = sfr->value;
      }
      break;
    }

    case 0xb0:{
      uint8_t bit_addr = memory->rom[++memory->instraction_reg];
      if(memory->data_regs[SFR_PSW].value & 0x80){
	uint8_t offset;
	bool state;
	
	if(bit_addr <= 0x7f){
	  uint8_t location = bit_addr / 8 + 0x20;
	  offset = bit_addr % 8;
	  state = memory->data_ram[location] & (1 << offset);
	}

	else state = get_sfr_by_bit(bit_addr, &offset, memory)->value & (1 << offset);

	memory->data_regs[SFR_PSW].value = memory->data_regs[SFR_PSW].value | (state << 7);
      }

      break;
    }

      // CPL bit
    case 0xb2: {
      uint8_t* bit_location;
      uint8_t offset;
      uint8_t bit = memory->rom[++memory->instraction_reg];

      if (bit <= 0x7f) {
	uint8_t location = bit / 8 + 0x20;
	offset = bit % 8;
	bit_location = &memory->data_ram[location];
      } else {
	SFR* sfr = get_sfr_by_bit(bit, &offset, memory);
	bit_location = &sfr->value;
      }

      if(*bit_location & (1 << offset))
	*bit_location = *bit_location & ~(1 << offset);
      else
	*bit_location = *bit_location | (1 << offset);

      break;
    }

      // CPL C
    case 0xb3: 
      if(memory->data_regs[SFR_PSW].value & 0x80)
	memory->data_regs[SFR_PSW].value = memory->data_regs[SFR_PSW].value & 0x7f;
      else
	memory->data_regs[SFR_PSW].value = memory->data_regs[SFR_PSW].value | 0x80;
      break;

      // CJNE A, #immideate, relative
    case 0xb4: {
      uint8_t data = memory->rom[++memory->instraction_reg];
      int8_t relative = memory->rom[++memory->instraction_reg];
      if(memory->data_regs[SFR_A].value != data)
	memory->instraction_reg += relative;
      break;
    }

      // CJNE A, direct, relative
    case 0xb5: {
      uint8_t location = memory->rom[++memory->instraction_reg];
      int8_t relative = memory->rom[++memory->instraction_reg];
      uint8_t value;
      
      if(location <= 0x7f)
	value = memory->data_ram[location];
      else {
	SFR* sfr = check_out_location(location, memory);
	if(sfr == NULL){ assert = true; break; }
	value = sfr->value;
      }

      if(memory->data_regs[SFR_A].value != value)
	memory->instraction_reg += relative;
      break;
    }

      // CJNE @Ri, #immideate, relative 
    case 0xb6:
    case 0xb7: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0xb6;
      uint8_t location = memory->data_ram[r_index + page * 8];
      uint8_t value1 = memory->rom[++memory->instraction_reg];
      uint8_t value2;
      int8_t relative = memory->rom[++memory->instraction_reg];

      if(location <= 0x7f)
	value2 = memory->data_ram[location];
      else {
	SFR* sfr = check_out_location(location, memory);
        if (sfr == NULL) {
          assert = true;
          break;
        }
        value2 = sfr->value;
      }

      if(value1 != value2)
	memory->instraction_reg += relative;

      break;
    }

      // CJNE Rn, #immideate, relative
    case 0xb8:
    case 0xb9:
    case 0xba:
    case 0xbb:
    case 0xbc:
    case 0xbd:
    case 0xbe:
    case 0xbf: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0xb8;
      uint8_t r_value = memory->data_ram[r_index + 8 * page];
      uint8_t i_value = memory->rom[++memory->instraction_reg];
      int8_t relative = memory->rom[++memory->instraction_reg];

      if(r_value != i_value)
	memory->instraction_reg += relative;

      break;
    }

      // PUSH direct
    case 0xc0:{
      uint8_t location = memory->rom[++memory->instraction_reg];
      uint8_t value;
      
      if(location <= 0x7f)
	value = memory->data_ram[location];
      else {
	SFR* sfr = check_out_location(location, memory);
	if(sfr == NULL) { assert = true; break; }
	value = sfr->value;
      }

      memory->data_ram[++memory->data_regs[SFR_SP].value] = value;
      break;
    }

      // CLR bit
    case 0xc2: {
      uint8_t bit = memory->rom[++memory->instraction_reg];
      uint8_t offset;
      uint8_t* value;
      if (bit <= 0x7f) {
	offset = bit % 8;
	value = &memory->data_ram[bit / 8 + 0x20];
      }
      else 
	value = &get_sfr_by_bit(bit, &offset, memory)->value;

      *value = *value & ~(1 << offset);
      break;
    }

      // CLR C
    case 0xc3:
      memory->data_regs[SFR_PSW].value = memory->data_regs[SFR_PSW].value & 0x7f;
      break;

      // SWAP A
    case 0xc4: {
      uint8_t lsb = memory->data_regs[SFR_A].value & 0x0f;
      uint8_t msb = (memory->data_regs[SFR_A].value & 0xf0) >> 4;
      memory->data_regs[SFR_A].value = (lsb << 4) | msb;
      break;
    }

      // xch A, direct
    case 0xc5: {
      uint8_t location = memory->rom[++memory->instraction_reg];
      uint8_t* l_ptr;

      if(location <= 0x7f)
	l_ptr = &memory->data_ram[location];
      else
	l_ptr = &check_out_location(location, memory)->value;

      uint8_t acc = memory->data_regs[SFR_A].value;
      memory->data_regs[SFR_A].value = *l_ptr;
      *l_ptr = acc;
      calculate_parity(memory);
      break;
    }

      // xch A, @Ri
    case 0xc6:
    case 0xc7: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0xc6;
      uint8_t location = memory->data_ram[r_index + page * 8];
      uint8_t* l_ptr;

      if(location <= 0x7f)
	l_ptr = &memory->data_ram[location];
      else
	l_ptr = &check_out_location(location, memory)->value;

      uint8_t acc = memory->data_regs[SFR_A].value;
      memory->data_regs[SFR_A].value = *l_ptr;
      *l_ptr = acc;
      calculate_parity(memory);
      break;
    }

      // xch A, Rn
    case 0xc8:
    case 0xc9:
    case 0xca:
    case 0xcb:
    case 0xcc:
    case 0xcd:
    case 0xce:
    case 0xcf: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0xc8;
      uint8_t slot = memory->data_ram[r_index + page * 8];
      memory->data_ram[r_index + page * 8] = memory->data_regs[SFR_A].value;
      memory->data_regs[SFR_A].value = slot;

      calculate_parity(memory);
      break;
    }

      // POP direct 
    case 0xd0:{
      uint8_t location = memory->rom[++memory->instraction_reg];
      uint8_t* drop_location;

      if(location <= 0x7f)
	drop_location = &memory->data_ram[location];

      else{
	SFR* sfr = check_out_location(location, memory);
	if(sfr == NULL) { assert = true; break; }
	drop_location = &sfr->value;
      }

      *drop_location = memory->data_ram[memory->data_regs[SFR_SP].value--];
      break;
    }

      // setb bit
    case 0xd2: {
      uint8_t bit = memory->rom[++memory->instraction_reg];
      uint8_t offset;
      uint8_t* slot;

      if (bit <= 0x7f) {
	offset = bit % 8;
	slot = &memory->data_ram[bit / 8 + 0x20];
      }
      else 
       slot = &get_sfr_by_bit(bit, &offset, memory)->value;

      *slot = *slot | (1 << offset);
      break;
    }

      // setb C
    case 0xd3:
      memory->data_regs[SFR_PSW].value = memory->data_regs[SFR_PSW].value | 0x80;
      break;

      // DA A
    case 0xd4: {
      uint8_t check = memory->data_regs[SFR_A].value & 0x0f;

      if((memory->data_regs[SFR_PSW].value & 0x40) || (check > 9))
	memory->data_regs[SFR_A].value += 6;

      check = (memory->data_regs[SFR_A].value & 0xf0) >> 4;
      if((memory->data_regs[SFR_PSW].value & 0x80) || (check > 9))
	memory->data_regs[SFR_A].value += 0x60;

      if(memory->data_regs[SFR_A].value > 0x99)
	memory->data_regs[SFR_PSW].value = memory->data_regs[SFR_PSW].value | 0x80;
      else
	memory->data_regs[SFR_PSW].value = memory->data_regs[SFR_PSW].value & 0x7f;

      calculate_parity(memory);
      break;
    }

      // DJNZ direct, relative 
    case 0xd5:{
      uint8_t location = memory->rom[++memory->instraction_reg];
      int8_t relative = memory->rom[++memory->instraction_reg];
      uint8_t* value;

      if(location <= 0x7f)
	value = &memory->data_ram[location];
      else {
	SFR* sfr = check_out_location(location, memory);
	if(sfr == NULL) { assert = true; break; }
	value = &sfr->value;
      }

      *value -= 1;
      if(*value != 0)
	memory->instraction_reg += relative;

      break;
    }

      // XCHD A, @Ri
    case 0xd6:
    case 0xd7: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0xd6;
      uint8_t location = memory->data_ram[r_index + page * 8];
      uint8_t* replace;

      if(location <= 0x7f)
	replace = &memory->data_ram[location];

      else {
	SFR* sfr = check_out_location(location, memory);
	if(sfr == NULL) { assert = true; break; }
	replace = &sfr->value;
      }

      uint8_t replace_msb = *replace & 0xf0;
      uint8_t replace_lsb = *replace & 0x0f;
      uint8_t acc_msb = memory->data_regs[SFR_A].value & 0xf0;
      uint8_t acc_lsb = memory->data_regs[SFR_A].value & 0x0f;

      memory->data_regs[SFR_A].value = acc_msb | replace_lsb;
      *replace = replace_msb | acc_lsb;

      calculate_parity(memory);
      break;
    }

      // DJNZ Rn, relative
    case 0xd8:
    case 0xd9:
    case 0xda:
    case 0xdb:
    case 0xdc:
    case 0xdd:
    case 0xde:
    case 0xdf: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0xd8;
      int8_t relative = memory->rom[++memory->instraction_reg];

      memory->data_ram[r_index + page * 8]--;
      if(memory->data_ram[r_index + page * 8] != 0)
	memory->instraction_reg += relative;

      break;
    }

      // MOVX A, @DPTR
    case 0xe0:{
      uint16_t dptr = memory->data_regs[SFR_DPL].value | (memory->data_regs[SFR_DPH].value << 8);
      memory->data_regs[SFR_A].value = memory->xdata_ram[dptr];
      calculate_parity(memory);
      break;
    }

      // MOVX A, @Ri
    case 0xe2:
    case 0xe3: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0xe2;
      memory->data_regs[SFR_A].value = memory->xdata_ram[memory->data_ram[r_index + 8 * page]];
      calculate_parity(memory);
      break;
    }

      // CLR A
    case 0xe4: 
      memory->data_regs[SFR_A].value = 0;
      calculate_parity(memory);
      break;

      // MOV A, direct
    case 0xe5: {
      uint8_t location = memory->rom[++memory->instraction_reg];
      if (location <= 0x7f)
        memory->data_regs[SFR_A].value = memory->data_ram[location];

      else {
	SFR* sfr = check_out_location(location, memory);
	if(sfr == NULL) assert = true;
	else memory->data_regs[SFR_A].value = sfr->value;
      }

      calculate_parity(memory);
      break;
    }

      // MOV A, @Ri
    case 0xe6:
    case 0xe7: {
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0xe6;
      uint8_t location = memory->data_ram[r_index];
      if (location <= 0x7f)
        memory->data_regs[SFR_A].value = memory->data_ram[location];

      else {
        SFR *sfr = check_out_location(location, memory);
        if (sfr == NULL)
          assert = true;
        else
          memory->data_regs[SFR_A].value = sfr->value;
      }

      calculate_parity(memory);
      break;
    }
      
      // MOV A, Rn
    case 0xe8:
    case 0xe9:
    case 0xea:
    case 0xeb:
    case 0xec:
    case 0xed:
    case 0xee:
    case 0xef: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0xe8;
      memory->data_regs[SFR_A].value = memory->data_ram[r_index + 8 * page];
      calculate_parity(memory);
      break;
    }

      // MOVX @DPTR, A
    case 0xf0:{
      uint16_t dptr = memory->data_regs[SFR_DPL].value | (memory->data_regs[SFR_DPH].value << 8);
      memory->xdata_ram[dptr] = memory->data_regs[SFR_A].value;
      break;
    }

      // MOVX @Ri, A
    case 0xf2:
    case 0xf3: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0xf2;
      memory->xdata_ram[memory->data_ram[r_index + page * 8]] = memory->data_regs[SFR_A].value;
      break;
    }

      // CPL A
    case 0xf4:
      memory->data_regs[SFR_A].value = ~memory->data_regs[SFR_A].value;
      calculate_parity(memory);
      break;

      // MOV direct, A
    case 0xf5: {
      uint8_t location = memory->rom[++memory->instraction_reg];

      if (location <= 0x7f)
        memory->data_ram[location] = memory->data_regs[SFR_A].value;
      else {
	SFR* sfr = check_out_location(location, memory);
	if(sfr == NULL) assert = true;
	else sfr->value = memory->data_regs[SFR_A].value;
      }

      break;
    }

      // MOV @Ri, A
    case 0xf6:
    case 0xf7: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0xf6;
      uint8_t location = memory->data_ram[r_index + page * 8];

      if (location <= 0x7f)
        memory->data_ram[location] = memory->data_regs[SFR_A].value;
      else {
	SFR* sfr = check_out_location(location, memory);
	if(sfr == NULL) assert = true;
	else sfr->value = memory->data_regs[SFR_A].value;
      }

      break;
    }

      // MOV Rn, A
    case 0xf8:
    case 0xf9:
    case 0xfa:
    case 0xfb:
    case 0xfc:
    case 0xfd:
    case 0xfe:
    case 0xff: {
      uint8_t page = (memory->data_regs[SFR_PSW].value & 0x18) >> 3;
      uint8_t r_index = memory->rom[memory->instraction_reg] - 0xf8;
      memory->data_ram[r_index + 8 * page] = memory->data_regs[SFR_A].value;
      break;
    }

    default:
      fprintf(stderr, "Unknown opcode: %x\n",
              memory->rom[memory->instraction_reg]);

      #ifdef ASSERT_WHEN_UNKNOWN
      assert = true;
      #endif
   
      break;
    }

    if (display_value != memory->data_ram[0x2]) {
      display_value = memory->data_ram[0x2];
      printf("display change: %x\n", display_value);
    }

    if (psw_reg != memory->data_regs[SFR_PSW].value) {
      psw_reg = memory->data_regs[SFR_PSW].value;
      printf("PSW changed: %x\n", psw_reg);
    }

#ifdef DEBUG_OUT
    printf("ACC: %X\n", memory->data_regs[SFR_A].value);
    printf("B: %X\n", memory->data_regs[SFR_B].value);
#endif

    memory->instraction_reg++;
  }
}
