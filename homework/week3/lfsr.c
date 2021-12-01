#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define POSITION_16 0
#define POSITION_14 2
#define POSITION_13 3
#define POSITION_11 5
#define POSITION_1 15

unsigned get_bit(uint16_t x,
                 unsigned n) {
    return 1 & (x >> n);
}    

void set_bit(uint16_t * x,
             unsigned n,
             unsigned v) {
    *x = (*x & ~(1 << n)) | (v << n);
}

unsigned get_new_lfsr_bit(uint16_t *reg) {
    return get_bit(*reg, POSITION_16) ^ get_bit(*reg, POSITION_14) ^ get_bit(*reg, POSITION_13) ^ get_bit(*reg, POSITION_11);
}

void lfsr_calculate(uint16_t *reg) {
    unsigned new_bit = get_new_lfsr_bit(reg);
    *reg >>= 1;
    set_bit(reg, POSITION_1, new_bit);
}

int main() {
  int8_t *numbers = (int8_t*) malloc(sizeof(int8_t) * 65535);
  if (numbers == NULL) {
    printf("Memory allocation failed!");
    exit(1);
  }

  memset(numbers, 0, sizeof(int8_t) * 65535);
  uint16_t reg = 0x1;
  uint32_t count = 0;
  int i;

  do {
    count++;
    numbers[reg] = 1;
    if (count < 24) {
      printf("My number is: %u\n", reg);
    } else if (count == 24) {
      printf(" ... etc etc ... \n");
    }
    for (i = 0; i < 32; i++)
      lfsr_calculate(&reg);
  } while (numbers[reg] != 1);

  printf("Got %u numbers before cycling!\n", count);

  if (count == 65535) {
    printf("Congratulations! It works!\n");
  } else {
    printf("Did I miss something?\n");
  }

  free(numbers);

  return 0;
}
