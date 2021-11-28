
#include <stdio.h>
#include <sys/stat.h>

#include "emulator.h"
#include "main.h"

void initialize_sfrs(Memory *mem) {
  // The addresses of each sfr
  uint8_t SFR_ARR[21] = {
      0xf0, // B
      0xe0, // A
      0xd0, // PSW
      0xb8, // IP
      0xb0, // P3
      0xa8, // IE
      0xa0, // P2
      0x99, // SBUF
      0x98, // SCON
      0x90, // P1
      0x8d, // TH1
      0x8c, // TH0
      0x8b, // TL1
      0x8a, // TL0
      0x89, // TMOD
      0x88, // TCON
      0x87, // PCON
      0x83, // DPH
      0x82, // DPL
      0x81, // SP
      0x80, // P0
  };

  // Determains if the sfr is bit addressable or not 
  bool bitaddressables[] = {true,  true, true,  true,  true,  true,  true,
                            false, true, true,  false, false, false, false,
                            false, true, false, false, false, false, true};

  // The addresses of the bit on the bitaddressable sfr 
  uint8_t bit_addresses[] = {
      0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xe0, 0xe1, 0xe2,
      0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xd0, 0x00, 0xd2, 0xd3, 0xd4, 0xd5,
      0xd6, 0xd7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0x00, 0x00, 0x00, 0xb0,
      0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xa8, 0xa9, 0xaa, 0xab,
      0xac, 0x00, 0x00, 0xaf, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6,
      0xa7, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0x90, 0x91,
      0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x88, 0x89, 0x8a, 0x8b, 0x8c,
      0x8d, 0x8e, 0x8f, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87};

  uint8_t address_ptr = 0;

  for (uint8_t i = 0; i < 21; i++) {
    mem->data_regs[i].address = SFR_ARR[i];
    mem->data_regs[i].is_bit_addressable = bitaddressables[i];
    if (bitaddressables[i]) {
      for (uint8_t j = 0; j < 8; j++)
        mem->data_regs[i].bits[j] = bit_addresses[address_ptr + j];

      address_ptr += 8;
    }
  }
}

int main(int argc, char *argv[]) {

  // Getting the path to the rom file
  char *rompath = NULL;
  if (argc < 2) {
    fprintf(stderr, "no file specified\n");
    return -1;
  }

  for (int i = 1; i < argc; i++) {
    struct stat buffer;
    int exists = stat(argv[i], &buffer);
    if (exists == 0 && S_ISREG(buffer.st_mode))
      rompath = argv[i];
    else {
      fprintf(stderr, "We cant understand what \"%s\" means\n", argv[i]);
      return -1;
    }
  }

  printf("path chosen \"%s\"\n", rompath);

  // Initializing memory
  Memory memory = {};

  // Openning the rom file
  FILE *rom = fopen(rompath, "r");

  fseek(rom, 0, SEEK_END);
  uint16_t rom_length = ftell(rom);

  printf("File size: %d\n", rom_length);

  fseek(rom, 0, SEEK_SET);

  // Copying the rom to the VM's rom
  for (uint16_t i = 0; i < rom_length; i++) {
    fscanf(rom, "%c", &memory.rom[i]);
    printf("%x ", memory.rom[i]);
  }

  printf("\n");

  // Initializing the sfrs
  initialize_sfrs(&memory);

  for (uint8_t i = 0; i < 21; i++) {
    printf("SFR Address: %x\n", memory.data_regs[i].address);
    printf("Is bit addressable: %s\n", memory.data_regs[i].is_bit_addressable? "Yes" : "No");
    if (memory.data_regs[i].is_bit_addressable) {
      for(uint8_t j = 0; j < 8; j++)
	printf("%x ", memory.data_regs[i].bits[j]);
      printf("\n");
    }
    printf("\n");
  }

  fclose(rom);


  // Running the program
  run_program(&memory);  

}
