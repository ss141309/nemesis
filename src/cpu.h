#pragma once

#include <stdint.h>
#include <stdlib.h>

#ifdef CPU_TESTS
//  The https://github.com/SingleStepTests/65x02 tests expect full 64KiB memory mapped to the CPU
constexpr uint32_t INTERNAL_RAM_SIZE = 64 * 1024;
#else
constexpr uint32_t INTERNAL_RAM_SIZE = 2 * 1024;
#endif

typedef enum {
  ADDRESSING_ABSOLUTE,
  ADDRESSING_ABSOLUTE_X,
  ADDRESSING_ABSOLUTE_X_W,
  ADDRESSING_ABSOLUTE_Y,
  ADDRESSING_ABSOLUTE_Y_W,
  ADDRESSING_ACCUMULATOR,
  ADDRESSING_IMMEDIATE,
  ADDRESSING_IMPLICIT,
  ADDRESSING_INDIRECT,
  ADDRESSING_INDIRECT_X,
  ADDRESSING_INDIRECT_Y,
  ADDRESSING_INDIRECT_Y_W,
  ADDRESSING_NONE,
  ADDRESSING_RELATIVE,
  ADDRESSING_ZERO_PAGE,
  ADDRESSING_ZERO_PAGE_X,
  ADDRESSING_ZERO_PAGE_Y
} addressing_modes_t;

typedef union {
  struct {
    bool carry : 1;
    bool zero : 1;
    bool interrupt_disable : 1;
    bool decimal : 1;
    bool b : 1;
    bool unused : 1;
    bool overflow : 1;
    bool negative : 1;
  } bits;
  uint8_t val;
} status_flag_t;

typedef struct {
  uint16_t pc;
  uint8_t sp;
  uint8_t ac;
  uint8_t x;
  uint8_t y;
  status_flag_t s;
  size_t cycles;  // FIXME: what should be its data type?
  uint8_t mem[INTERNAL_RAM_SIZE];
  addressing_modes_t current_addr_mode;
} cpu_t;

cpu_t cpu_power_on(void);
void cpu_reset(cpu_t *cpu);
void cpu_step(cpu_t *cpu);
