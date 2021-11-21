#ifndef H_8051_CPU
#define H_8051_CPU

#include <stdint.h> 
#include <stdbool.h> 

typedef bool BIT;
typedef uint8_t BYTE;
typedef uint16_t WORD;

typedef struct EPinout {
  BYTE Ports[5];
} EPinout;


void begin(EPinout* pinout);
void step();

#endif 
