/*
Copyright 2025 समीर सिंह Sameer Singh

This file is part of nemesis.

nemesis is free software: you can redistribute it and/or modify it under the terms of the GNU
General Public License as published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

nemesis is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along with nemesis. If not, see
<https://www.gnu.org/licenses/>. */

#include "cpu.h"

#include <stdint.h>

#include "utils.h"

#define STACK_ADDR ((uint16_t)cpu->sp + 0x100)

static constexpr uint8_t LXA_XAA_MAGIC = 0xEE;

static constexpr uint8_t INTERRUPT_DISABLE = 1 << 2;
static constexpr uint8_t B = 1 << 4;
static constexpr uint8_t UNUSED = 1 << 5;

static constexpr uint8_t ZP_FIRST_ADDR = 0x00;
static constexpr uint8_t ZP_LAST_ADDR = 0xFF;
static constexpr uint8_t ZP_WRAPAROUND_ADDR = 0xFF;

static constexpr uint16_t NMI_VECTOR = 0xFFFA;
static constexpr uint16_t RESET_VECTOR = 0xFFFC;
static constexpr uint16_t IRQ_VECTOR = 0xFFFE;

static constexpr addressing_modes_t addr_mode_table[256] = {
    // clang-format off
	  //+00                 +01                      +02                   +03                      +04                     +05                     +06                     +07                     +08                  +09                      +0A                     +0B                      +0C                      +0D                      +0E                      +0F
	  ADDRESSING_IMPLICIT,  ADDRESSING_INDIRECT_X,   ADDRESSING_NONE,      ADDRESSING_INDIRECT_X,   ADDRESSING_ZERO_PAGE,   ADDRESSING_ZERO_PAGE,   ADDRESSING_ZERO_PAGE,   ADDRESSING_ZERO_PAGE,   ADDRESSING_IMPLICIT, ADDRESSING_IMMEDIATE,    ADDRESSING_ACCUMULATOR, ADDRESSING_IMMEDIATE,    ADDRESSING_ABSOLUTE,     ADDRESSING_ABSOLUTE,     ADDRESSING_ABSOLUTE,     ADDRESSING_ABSOLUTE,     // 00
	  ADDRESSING_RELATIVE,  ADDRESSING_INDIRECT_Y,   ADDRESSING_NONE,      ADDRESSING_INDIRECT_Y_W, ADDRESSING_ZERO_PAGE_X, ADDRESSING_ZERO_PAGE_X, ADDRESSING_ZERO_PAGE_X, ADDRESSING_ZERO_PAGE_X, ADDRESSING_IMPLICIT, ADDRESSING_ABSOLUTE_Y,   ADDRESSING_IMPLICIT,    ADDRESSING_ABSOLUTE_Y_W,   ADDRESSING_ABSOLUTE_X,   ADDRESSING_ABSOLUTE_X,   ADDRESSING_ABSOLUTE_X_W, ADDRESSING_ABSOLUTE_X_W,   // 01
	  ADDRESSING_ABSOLUTE,  ADDRESSING_INDIRECT_X,   ADDRESSING_NONE,      ADDRESSING_INDIRECT_X,   ADDRESSING_ZERO_PAGE,   ADDRESSING_ZERO_PAGE,   ADDRESSING_ZERO_PAGE,   ADDRESSING_ZERO_PAGE,   ADDRESSING_IMPLICIT, ADDRESSING_IMMEDIATE,    ADDRESSING_ACCUMULATOR, ADDRESSING_IMMEDIATE,    ADDRESSING_ABSOLUTE,     ADDRESSING_ABSOLUTE,     ADDRESSING_ABSOLUTE,     ADDRESSING_ABSOLUTE,     // 20
	  ADDRESSING_RELATIVE,  ADDRESSING_INDIRECT_Y,   ADDRESSING_NONE,      ADDRESSING_INDIRECT_Y_W, ADDRESSING_ZERO_PAGE_X, ADDRESSING_ZERO_PAGE_X, ADDRESSING_ZERO_PAGE_X, ADDRESSING_ZERO_PAGE_X, ADDRESSING_IMPLICIT, ADDRESSING_ABSOLUTE_Y,   ADDRESSING_IMPLICIT,    ADDRESSING_ABSOLUTE_Y_W, ADDRESSING_ABSOLUTE_X,   ADDRESSING_ABSOLUTE_X,   ADDRESSING_ABSOLUTE_X_W, ADDRESSING_ABSOLUTE_X_W, // 30
	  ADDRESSING_IMPLICIT,  ADDRESSING_INDIRECT_X,   ADDRESSING_NONE,      ADDRESSING_INDIRECT_X,   ADDRESSING_ZERO_PAGE,   ADDRESSING_ZERO_PAGE,   ADDRESSING_ZERO_PAGE,   ADDRESSING_ZERO_PAGE,   ADDRESSING_IMPLICIT, ADDRESSING_IMMEDIATE,    ADDRESSING_ACCUMULATOR, ADDRESSING_IMMEDIATE,    ADDRESSING_ABSOLUTE,     ADDRESSING_ABSOLUTE,     ADDRESSING_ABSOLUTE,     ADDRESSING_ABSOLUTE,     // 40
	  ADDRESSING_RELATIVE,  ADDRESSING_INDIRECT_Y,   ADDRESSING_NONE,      ADDRESSING_INDIRECT_Y_W, ADDRESSING_ZERO_PAGE_X, ADDRESSING_ZERO_PAGE_X, ADDRESSING_ZERO_PAGE_X, ADDRESSING_ZERO_PAGE_X, ADDRESSING_IMPLICIT, ADDRESSING_ABSOLUTE_Y,   ADDRESSING_IMPLICIT,    ADDRESSING_ABSOLUTE_Y_W, ADDRESSING_ABSOLUTE_X,   ADDRESSING_ABSOLUTE_X,   ADDRESSING_ABSOLUTE_X_W, ADDRESSING_ABSOLUTE_X_W, // 50
	  ADDRESSING_IMPLICIT,  ADDRESSING_INDIRECT_X,   ADDRESSING_NONE,      ADDRESSING_INDIRECT_X,   ADDRESSING_ZERO_PAGE,   ADDRESSING_ZERO_PAGE,   ADDRESSING_ZERO_PAGE,   ADDRESSING_ZERO_PAGE,   ADDRESSING_IMPLICIT, ADDRESSING_IMMEDIATE,    ADDRESSING_ACCUMULATOR, ADDRESSING_IMMEDIATE,    ADDRESSING_INDIRECT,     ADDRESSING_ABSOLUTE,     ADDRESSING_ABSOLUTE,     ADDRESSING_ABSOLUTE,     // 60
	  ADDRESSING_RELATIVE,  ADDRESSING_INDIRECT_Y,   ADDRESSING_NONE,      ADDRESSING_INDIRECT_Y_W, ADDRESSING_ZERO_PAGE_X, ADDRESSING_ZERO_PAGE_X, ADDRESSING_ZERO_PAGE_X, ADDRESSING_ZERO_PAGE_X, ADDRESSING_IMPLICIT, ADDRESSING_ABSOLUTE_Y,   ADDRESSING_IMPLICIT,    ADDRESSING_ABSOLUTE_Y_W, ADDRESSING_ABSOLUTE_X,   ADDRESSING_ABSOLUTE_X,   ADDRESSING_ABSOLUTE_X_W, ADDRESSING_ABSOLUTE_X_W, // 70
	  ADDRESSING_IMMEDIATE, ADDRESSING_INDIRECT_X,   ADDRESSING_IMMEDIATE, ADDRESSING_INDIRECT_X,   ADDRESSING_ZERO_PAGE,   ADDRESSING_ZERO_PAGE,   ADDRESSING_ZERO_PAGE,   ADDRESSING_ZERO_PAGE,   ADDRESSING_IMPLICIT, ADDRESSING_IMMEDIATE,    ADDRESSING_ACCUMULATOR, ADDRESSING_IMMEDIATE,    ADDRESSING_ABSOLUTE,     ADDRESSING_ABSOLUTE,     ADDRESSING_ABSOLUTE,     ADDRESSING_ABSOLUTE,     // 80
	  ADDRESSING_RELATIVE,  ADDRESSING_INDIRECT_Y_W, ADDRESSING_NONE,      ADDRESSING_INDIRECT_Y_W, ADDRESSING_ZERO_PAGE_X, ADDRESSING_ZERO_PAGE_X, ADDRESSING_ZERO_PAGE_Y, ADDRESSING_ZERO_PAGE_Y, ADDRESSING_IMPLICIT, ADDRESSING_ABSOLUTE_Y_W, ADDRESSING_IMPLICIT,    ADDRESSING_ABSOLUTE_Y_W, ADDRESSING_ABSOLUTE_X_W, ADDRESSING_ABSOLUTE_X_W, ADDRESSING_ABSOLUTE_Y_W, ADDRESSING_ABSOLUTE_X,   // 90
	  ADDRESSING_IMMEDIATE, ADDRESSING_INDIRECT_X,   ADDRESSING_IMMEDIATE, ADDRESSING_INDIRECT_X,   ADDRESSING_ZERO_PAGE,   ADDRESSING_ZERO_PAGE,   ADDRESSING_ZERO_PAGE,   ADDRESSING_ZERO_PAGE,   ADDRESSING_IMPLICIT, ADDRESSING_IMMEDIATE,    ADDRESSING_ACCUMULATOR, ADDRESSING_IMMEDIATE,    ADDRESSING_ABSOLUTE,     ADDRESSING_ABSOLUTE,     ADDRESSING_ABSOLUTE,     ADDRESSING_ABSOLUTE,     // A0
	  ADDRESSING_RELATIVE,  ADDRESSING_INDIRECT_Y,   ADDRESSING_NONE,      ADDRESSING_INDIRECT_Y,   ADDRESSING_ZERO_PAGE_X, ADDRESSING_ZERO_PAGE_X, ADDRESSING_ZERO_PAGE_Y, ADDRESSING_ZERO_PAGE_Y, ADDRESSING_IMPLICIT, ADDRESSING_ABSOLUTE_Y,   ADDRESSING_IMPLICIT,    ADDRESSING_ABSOLUTE_Y,   ADDRESSING_ABSOLUTE_X,   ADDRESSING_ABSOLUTE_X,   ADDRESSING_ABSOLUTE_Y,   ADDRESSING_ABSOLUTE_Y,   // B0
	  ADDRESSING_IMMEDIATE, ADDRESSING_INDIRECT_X,   ADDRESSING_IMMEDIATE, ADDRESSING_INDIRECT_X,   ADDRESSING_ZERO_PAGE,   ADDRESSING_ZERO_PAGE,   ADDRESSING_ZERO_PAGE,   ADDRESSING_ZERO_PAGE,   ADDRESSING_IMPLICIT, ADDRESSING_IMMEDIATE,    ADDRESSING_ACCUMULATOR, ADDRESSING_IMMEDIATE,    ADDRESSING_ABSOLUTE,     ADDRESSING_ABSOLUTE,     ADDRESSING_ABSOLUTE,     ADDRESSING_ABSOLUTE,     // C0
	  ADDRESSING_RELATIVE,  ADDRESSING_INDIRECT_Y,   ADDRESSING_NONE,      ADDRESSING_INDIRECT_Y_W, ADDRESSING_ZERO_PAGE_X, ADDRESSING_ZERO_PAGE_X, ADDRESSING_ZERO_PAGE_X, ADDRESSING_ZERO_PAGE_X, ADDRESSING_IMPLICIT, ADDRESSING_ABSOLUTE_Y,   ADDRESSING_IMPLICIT,    ADDRESSING_ABSOLUTE_Y_W, ADDRESSING_ABSOLUTE_X,   ADDRESSING_ABSOLUTE_X,   ADDRESSING_ABSOLUTE_X_W, ADDRESSING_ABSOLUTE_X_W, // D0
	  ADDRESSING_IMMEDIATE, ADDRESSING_INDIRECT_X,   ADDRESSING_IMMEDIATE, ADDRESSING_INDIRECT_X,   ADDRESSING_ZERO_PAGE,   ADDRESSING_ZERO_PAGE,   ADDRESSING_ZERO_PAGE,   ADDRESSING_ZERO_PAGE,   ADDRESSING_IMPLICIT, ADDRESSING_IMMEDIATE,    ADDRESSING_ACCUMULATOR, ADDRESSING_IMMEDIATE,    ADDRESSING_ABSOLUTE,     ADDRESSING_ABSOLUTE,     ADDRESSING_ABSOLUTE,     ADDRESSING_ABSOLUTE,     // E0
	  ADDRESSING_RELATIVE,  ADDRESSING_INDIRECT_Y,   ADDRESSING_NONE,      ADDRESSING_INDIRECT_Y_W, ADDRESSING_ZERO_PAGE_X, ADDRESSING_ZERO_PAGE_X, ADDRESSING_ZERO_PAGE_X, ADDRESSING_ZERO_PAGE_X, ADDRESSING_IMPLICIT, ADDRESSING_ABSOLUTE_Y,   ADDRESSING_IMPLICIT,    ADDRESSING_ABSOLUTE_Y_W, ADDRESSING_ABSOLUTE_X,   ADDRESSING_ABSOLUTE_X,   ADDRESSING_ABSOLUTE_X_W, ADDRESSING_ABSOLUTE_X_W  // F0
    // clang-format on
};
static const char *addressing_modes_string[] = {
    "ABSOLUTE",  "ABSOLUTE_X", "ABSOLUTE_X", "ABSOLUTE_Y",  "ABSOLUTE_Y", "ACCUMULATOR",
    "IMMEDIATE", "IMPLICIT",   "INDIRECT",   "INDIRECT_X",  "INDIRECT_Y", "INDIRECT_Y",
    "NONE",      "RELATIVE",   "ZERO_PAGE",  "ZERO_PAGE_X", "ZERO_PAGE_Y"};

private
uint8_t read(cpu_t *cpu, uint16_t addr) { return cpu->mem[addr]; }
private
void write(cpu_t *cpu, uint16_t addr, uint8_t val) { cpu->mem[addr] = val; }

#define GET_MACRO(_1, _2, NAME, ...) NAME

#define mem_read_byte(...) \
  GET_MACRO(__VA_ARGS__, mem_read_byte_from_addr, mem_read_byte_from_pc)(__VA_ARGS__)

#define mem_read_word(...) \
  GET_MACRO(__VA_ARGS__, mem_read_word_from_addr, mem_read_word_from_pc)(__VA_ARGS__)

private
uint8_t mem_read_byte_from_pc(cpu_t *cpu) {
  uint8_t val = read(cpu, cpu->pc);
  log_info("MEM[%d]=%d CYC:%ld", cpu->pc++, val, ++cpu->cycles);
  return val;
}

private
uint16_t mem_read_word_from_pc(cpu_t *cpu) {
  uint8_t lo = mem_read_byte(cpu);
  uint8_t hi = mem_read_byte(cpu);

  return (uint16_t)(hi << 8) | lo;
}

private
uint8_t mem_read_byte_from_addr(cpu_t *cpu, uint16_t addr) {
  uint8_t val = read(cpu, addr);
  log_info("MEM[%d]=%d CYC:%ld", addr, val, ++cpu->cycles);
  return val;
}

private
uint16_t mem_read_word_from_addr(cpu_t *cpu, uint16_t addr) {
  uint8_t lo = mem_read_byte(cpu, addr);
  uint8_t hi = mem_read_byte(cpu, addr + 1);

  return (uint16_t)(hi << 8) | lo;
}

private
void mem_write_byte(cpu_t *cpu, uint16_t addr, uint8_t val) {
  write(cpu, addr, val);
  log_info("MEM[%d]=%d CYC:%ld", addr, val, ++cpu->cycles);
}

private
void push_byte(cpu_t *cpu, uint8_t val) {
  uint16_t addr = STACK_ADDR;
  write(cpu, addr, val);

  cpu->sp--;  // stack grows downwards
  log_info("MEM[%d]=%d CYC:%ld", addr, val, ++cpu->cycles);
}

private
void push_word(cpu_t *cpu, uint16_t val) {
  uint8_t lo = (uint8_t)get_lower_byte(val);
  uint8_t hi = (uint8_t)get_upper_byte(val);

  push_byte(cpu, hi);
  push_byte(cpu, lo);
}

private
uint8_t peek_byte(cpu_t *cpu) {
  uint16_t addr = STACK_ADDR;
  uint8_t val = read(cpu, addr);

  log_info("MEM[%d]=%d CYC:%ld", addr, val, ++cpu->cycles);
  return val;
}

private
uint8_t pop_byte(cpu_t *cpu) {
  cpu->sp++;
  uint16_t addr = STACK_ADDR;
  uint8_t val = read(cpu, addr);
  log_info("MEM[%d]=%d CYC:%ld", addr, val, ++cpu->cycles);
  return val;
}

private
uint16_t pop_word(cpu_t *cpu) {
  uint8_t lo = pop_byte(cpu);
  uint8_t hi = pop_byte(cpu);

  return (uint16_t)(hi << 8) | lo;
}

private
bool check_page_crossed(uint16_t val_a, uint16_t val_b) {
  return get_upper_byte(val_a + val_b) != get_upper_byte(val_a);
}

private
uint16_t fetch_absolute(cpu_t *cpu) { return mem_read_word(cpu); }

private
uint16_t fetch_absolute_x(cpu_t *cpu, bool dummy_read) {
  uint16_t base_addr = mem_read_word(cpu);
  bool page_crossed = check_page_crossed(base_addr, cpu->x);

  if (page_crossed || dummy_read) {
    mem_read_byte(cpu, (uint16_t)(base_addr + cpu->x - (page_crossed << 8)));
  }

  return base_addr + cpu->x;
}

private
uint16_t fetch_absolute_y(cpu_t *cpu, bool dummy_read) {
  uint16_t base_addr = mem_read_word(cpu);
  bool page_crossed = check_page_crossed(base_addr, cpu->y);

  if (page_crossed || dummy_read) {
    mem_read_byte(cpu, (uint16_t)(base_addr + cpu->y - (page_crossed << 8)));
  }

  return base_addr + cpu->y;
}

private
uint16_t fetch_accumulator(cpu_t *cpu) {
  mem_read_byte(cpu, cpu->pc);  // dummy read
  return 0;
}

private
uint16_t fetch_immediate(cpu_t *cpu) { return mem_read_byte(cpu); }

private
uint16_t fetch_implicit(cpu_t *cpu) {
  mem_read_byte(cpu, cpu->pc);  // dummy read
  return 0;
}

private
uint16_t fetch_indirect(cpu_t *cpu) {
  uint16_t base_addr = mem_read_word(cpu);

  if (get_lower_byte(base_addr) == ZP_LAST_ADDR) {
    uint8_t lo = mem_read_byte(cpu, base_addr);
    uint8_t hi = mem_read_byte(cpu, base_addr - ZP_LAST_ADDR);

    return (uint16_t)(hi << 8) | lo;
  }

  return mem_read_word(cpu, base_addr);
}

private
uint16_t fetch_indirect_x(cpu_t *cpu) {
  uint16_t zp_addr = mem_read_byte(cpu);
  mem_read_byte(cpu, zp_addr);  // dummy read

  uint16_t old_addr = (zp_addr + cpu->x) & ZP_WRAPAROUND_ADDR;

  if (old_addr == ZP_LAST_ADDR) {
    uint8_t lo = mem_read_byte(cpu, ZP_LAST_ADDR);
    uint8_t hi = mem_read_byte(cpu, ZP_FIRST_ADDR);
    return (uint16_t)(hi << 8) | lo;
  }

  return mem_read_word(cpu, old_addr);
}

private
uint16_t fetch_indirect_y(cpu_t *cpu, bool dummy_read) {
  uint16_t zp_addr = mem_read_byte(cpu);
  uint16_t addr;

  if (zp_addr == ZP_LAST_ADDR) {
    uint8_t lo = mem_read_byte(cpu, ZP_LAST_ADDR);
    uint8_t hi = mem_read_byte(cpu, ZP_FIRST_ADDR);
    addr = (uint16_t)(hi << 8) | lo;
  } else {
    addr = mem_read_word(cpu, zp_addr);
  }

  bool page_crossed = check_page_crossed(addr, cpu->y);
  if (page_crossed || dummy_read) {
    mem_read_byte(cpu, (uint16_t)(addr + cpu->y - (page_crossed << 8)));
  }

  return addr + cpu->y;
}

private
uint8_t fetch_zero_page(cpu_t *cpu) { return mem_read_byte(cpu); }

private
uint8_t fetch_zero_page_x(cpu_t *cpu) {
  uint8_t zp_addr = mem_read_byte(cpu);
  mem_read_byte(cpu, zp_addr);
  return zp_addr + cpu->x;
}

private
uint8_t fetch_zero_page_y(cpu_t *cpu) {
  uint8_t zp_addr = mem_read_byte(cpu);
  mem_read_byte(cpu, zp_addr);
  return zp_addr + cpu->y;
}

private
uint16_t fetch_address(cpu_t *cpu) {
  switch (cpu->current_addr_mode) {
    case ADDRESSING_ABSOLUTE:
      return fetch_absolute(cpu);
    case ADDRESSING_ABSOLUTE_X:
      return fetch_absolute_x(cpu, false);
    case ADDRESSING_ABSOLUTE_X_W:
      return fetch_absolute_x(cpu, true);
    case ADDRESSING_ABSOLUTE_Y:
      return fetch_absolute_y(cpu, false);
    case ADDRESSING_ABSOLUTE_Y_W:
      return fetch_absolute_y(cpu, true);
    case ADDRESSING_ACCUMULATOR:
      return fetch_accumulator(cpu);
    case ADDRESSING_IMMEDIATE:
      return fetch_immediate(cpu);
    case ADDRESSING_IMPLICIT:
      return fetch_implicit(cpu);
    case ADDRESSING_INDIRECT:
      return fetch_indirect(cpu);
    case ADDRESSING_INDIRECT_X:
      return fetch_indirect_x(cpu);
    case ADDRESSING_INDIRECT_Y:
      return fetch_indirect_y(cpu, false);
    case ADDRESSING_INDIRECT_Y_W:
      return fetch_indirect_y(cpu, true);
    case ADDRESSING_NONE:
      return fetch_implicit(cpu);
    case ADDRESSING_RELATIVE:
      return fetch_immediate(cpu);
    case ADDRESSING_ZERO_PAGE:
      return fetch_zero_page(cpu);
    case ADDRESSING_ZERO_PAGE_X:
      return fetch_zero_page_x(cpu);
    case ADDRESSING_ZERO_PAGE_Y:
      return fetch_zero_page_y(cpu);
    default:
      return 0;  // silence compiler warning
  };
}

#define is_direct_value_mode(mode)                                    \
  ((mode) == ADDRESSING_IMMEDIATE || (mode) == ADDRESSING_RELATIVE || \
   (mode) == ADDRESSING_IMPLICIT || (mode) == ADDRESSING_ACCUMULATOR)

#define fetch_operand(...) \
  GET_MACRO(__VA_ARGS__, fetch_operand_from_addr, fetch_operand_auto)(__VA_ARGS__)

private
uint8_t fetch_operand_auto(cpu_t *cpu) {
  uint16_t addr = fetch_address(cpu);
  if (is_direct_value_mode(cpu->current_addr_mode)) {
    return (uint8_t)addr;
  } else {
    return mem_read_byte(cpu, addr);
  }
}

private
uint8_t fetch_operand_from_addr(cpu_t *cpu, uint16_t addr) {
  if (is_direct_value_mode(cpu->current_addr_mode)) {
    return (uint8_t)addr;
  } else {
    return mem_read_byte(cpu, addr);
  }
}

private
void set_zero_negative(cpu_t *cpu, uint8_t reg) {
  cpu->s.bits.zero = reg == 0;
  cpu->s.bits.negative = check_if_bit7_set(reg);
}

private
void ADD(cpu_t *cpu, uint8_t val) {
  uint16_t ac_16 = cpu->ac;
  uint16_t val_16 = val;
  uint16_t carry_16 = cpu->s.bits.carry;
  uint16_t sum = (uint16_t)(ac_16 + val_16 + carry_16);

  cpu->s.bits.carry = sum > 0xFF;
  cpu->s.bits.overflow = get_7th_bit(~(cpu->ac ^ val) & (cpu->ac ^ sum));
  cpu->ac = (uint8_t)sum;

  set_zero_negative(cpu, cpu->ac);
}

private
void ADC(cpu_t *cpu) { ADD(cpu, fetch_operand(cpu)); }

// FIXME: later
private
void AHX(cpu_t *cpu) {
  uint16_t addr = fetch_address(cpu);
  log_info("addr: %d", addr);
  mem_write_byte(cpu, addr, (uint8_t)(get_upper_byte(addr) + 1) & (cpu->x & cpu->ac));
}

private
void ANC(cpu_t *cpu) {
  cpu->ac &= fetch_operand(cpu);
  cpu->s.bits.carry = check_if_bit7_set(cpu->ac);
  set_zero_negative(cpu, cpu->ac);
}

private
void AND(cpu_t *cpu) {
  cpu->ac &= fetch_operand(cpu);
  set_zero_negative(cpu, cpu->ac);
}

private
void ARR(cpu_t *cpu) {
  cpu->ac &= fetch_operand(cpu);
  uint8_t carry_byte = (uint8_t)(cpu->s.bits.carry << 7);

  cpu->s.bits.carry = check_if_bit7_set(cpu->ac);
  cpu->ac = (cpu->ac >> 1) | carry_byte;
  cpu->s.bits.overflow = cpu->s.bits.carry != check_if_bit5_set(cpu->ac);
  set_zero_negative(cpu, cpu->ac);
}

private
uint8_t ASL(cpu_t *cpu, uint8_t old_val) {
  cpu->s.bits.carry = check_if_bit7_set(old_val);

  uint8_t shifted_val = (uint8_t)(old_val << 1);
  set_zero_negative(cpu, shifted_val);

  return shifted_val;
}

private
void ASLa(cpu_t *cpu) {
  fetch_operand(cpu);
  cpu->ac = ASL(cpu, cpu->ac);
}

private
void ASLm(cpu_t *cpu) {
  uint16_t addr = fetch_address(cpu);
  uint8_t val = fetch_operand(cpu, addr);

  mem_write_byte(cpu, addr, val);  // dummy write
  mem_write_byte(cpu, addr, ASL(cpu, val));
}

private
void AXS(cpu_t *cpu) {
  uint8_t val = fetch_operand(cpu);
  cpu->x &= cpu->ac;

  cpu->s.bits.carry = cpu->x >= val;

  cpu->x -= val;
  set_zero_negative(cpu, cpu->x);
}

private
void BIT(cpu_t *cpu) {
  uint8_t val = fetch_operand(cpu);

  cpu->s.bits.zero = (cpu->ac & val) == 0;
  cpu->s.bits.overflow = check_if_bit6_set(val);
  cpu->s.bits.negative = check_if_bit7_set(val);
}

private
void BRA(cpu_t *cpu, bool branch) {
  int8_t signed_addr = fetch_operand(cpu);

  if (branch) {
    uint16_t old_pc = cpu->pc;
    uint16_t new_pc = (uint16_t)(old_pc + signed_addr);

    if (check_page_crossed(old_pc, signed_addr)) {
      mem_read_byte(cpu);
      uint16_t dummy_addr = clear_lower_byte(old_pc) | get_lower_byte(new_pc);
      mem_read_byte(cpu, dummy_addr);
    } else {
      mem_read_byte(cpu);
    }
    cpu->pc = new_pc;
  }
}

private
void BCC(cpu_t *cpu) { BRA(cpu, !cpu->s.bits.carry); }

private
void BCS(cpu_t *cpu) { BRA(cpu, cpu->s.bits.carry); }

private
void BEQ(cpu_t *cpu) { BRA(cpu, cpu->s.bits.zero); }

private
void BMI(cpu_t *cpu) { BRA(cpu, cpu->s.bits.negative); }

private
void BNE(cpu_t *cpu) { BRA(cpu, !cpu->s.bits.zero); }

private
void BPL(cpu_t *cpu) { BRA(cpu, !cpu->s.bits.negative); }

private
void BVC(cpu_t *cpu) { BRA(cpu, !cpu->s.bits.overflow); }

private
void BVS(cpu_t *cpu) { BRA(cpu, cpu->s.bits.overflow); }

private
void BRK(cpu_t *cpu) {
  fetch_operand(cpu);  // dummy read
  push_word(cpu, cpu->pc + 1);
  push_byte(cpu, cpu->s.val | B);
  cpu->s.bits.interrupt_disable = true;

  cpu->pc = mem_read_word(cpu, IRQ_VECTOR);
}

private
void CLC(cpu_t *cpu) {
  fetch_operand(cpu);
  cpu->s.bits.carry = false;
}

private
void CLD(cpu_t *cpu) {
  fetch_operand(cpu);
  cpu->s.bits.decimal = false;
}

private
void CLI(cpu_t *cpu) {
  fetch_operand(cpu);
  cpu->s.bits.interrupt_disable = false;
}

private
void CLV(cpu_t *cpu) {
  fetch_operand(cpu);
  cpu->s.bits.overflow = false;
}

private
void CMP(cpu_t *cpu, uint8_t reg) {
  uint8_t val = fetch_operand(cpu);
  uint8_t res = reg - val;

  cpu->s.bits.carry = reg >= val;
  cpu->s.bits.zero = reg == val;
  cpu->s.bits.negative = check_if_bit7_set(res);
}

private
void CPA(cpu_t *cpu) { CMP(cpu, cpu->ac); }

private
void CPX(cpu_t *cpu) { CMP(cpu, cpu->x); }

private
void CPY(cpu_t *cpu) { CMP(cpu, cpu->y); }

private
void DCP(cpu_t *cpu) {
  uint16_t addr = fetch_address(cpu);
  uint8_t val = fetch_operand(cpu, addr);

  mem_write_byte(cpu, addr, val--);
  uint8_t diff = cpu->ac - val;

  cpu->s.bits.carry = cpu->ac >= val;
  cpu->s.bits.zero = cpu->ac == val;
  cpu->s.bits.negative = check_if_bit7_set(diff);
  mem_write_byte(cpu, addr, val);
}

private
void DEC(cpu_t *cpu) {
  uint16_t addr = fetch_address(cpu);
  uint8_t val = fetch_operand(cpu, addr);

  mem_write_byte(cpu, addr, val);
  mem_write_byte(cpu, addr, --val);

  set_zero_negative(cpu, val);
}

private
void DEX(cpu_t *cpu) {
  fetch_operand(cpu);
  set_zero_negative(cpu, --cpu->x);
}

private
void DEY(cpu_t *cpu) {
  fetch_operand(cpu);
  set_zero_negative(cpu, --cpu->y);
}

private
void EOR(cpu_t *cpu) {
  cpu->ac ^= fetch_operand(cpu);
  set_zero_negative(cpu, cpu->ac);
}

private
void INX(cpu_t *cpu) {
  fetch_operand(cpu);
  set_zero_negative(cpu, ++cpu->x);
}

private
void INC(cpu_t *cpu) {
  uint16_t addr = fetch_address(cpu);
  uint8_t val = fetch_operand(cpu, addr);

  mem_write_byte(cpu, addr, val);
  mem_write_byte(cpu, addr, ++val);

  set_zero_negative(cpu, val);
}

private
void INY(cpu_t *cpu) {
  fetch_operand(cpu);
  set_zero_negative(cpu, ++cpu->y);
}

private
void ISC(cpu_t *cpu) {
  uint16_t addr = fetch_address(cpu);
  uint8_t val = fetch_operand(cpu, addr);

  mem_write_byte(cpu, addr, val++);
  ADD(cpu, ~val);
  mem_write_byte(cpu, addr, val);
}

private
void JMP(cpu_t *cpu) { cpu->pc = fetch_address(cpu); }

private
void JSR(cpu_t *cpu) {
  uint8_t lo = mem_read_byte(cpu);
  peek_byte(cpu);
  push_word(cpu, cpu->pc);
  cpu->pc = (uint16_t)(mem_read_byte(cpu) << 8) | lo;
}

private
void LAS(cpu_t *cpu) {
  cpu->ac = cpu->x = cpu->sp = fetch_operand(cpu) & cpu->sp;
  set_zero_negative(cpu, cpu->ac);
}

private
void LAX(cpu_t *cpu) {
  cpu->ac = cpu->x = fetch_operand(cpu);
  set_zero_negative(cpu, cpu->ac);
}

private
void LOD(cpu_t *cpu, uint8_t *reg) {
  *reg = fetch_operand(cpu);
  set_zero_negative(cpu, *reg);
}

private
void LDA(cpu_t *cpu) { LOD(cpu, &cpu->ac); }

private
void LDX(cpu_t *cpu) { LOD(cpu, &cpu->x); }

private
void LDY(cpu_t *cpu) { LOD(cpu, &cpu->y); }

private
uint8_t LSR(cpu_t *cpu, uint8_t val) {
  cpu->s.bits.carry = check_if_bit0_set(val);
  uint8_t shifted_val = val >> 1;
  set_zero_negative(cpu, shifted_val);
  return shifted_val;
}

private
void ALR(cpu_t *cpu) {
  cpu->ac &= fetch_operand(cpu);
  cpu->ac = LSR(cpu, cpu->ac);
}

private
void LSRa(cpu_t *cpu) {
  fetch_operand(cpu);
  cpu->ac = LSR(cpu, cpu->ac);
}

private
void LSRm(cpu_t *cpu) {
  uint16_t addr = fetch_address(cpu);
  uint8_t val = fetch_operand(cpu, addr);
  uint8_t shifted_val = LSR(cpu, val);

  mem_write_byte(cpu, addr, val);  // dummy write
  mem_write_byte(cpu, addr, shifted_val);
}

private
void LXA(cpu_t *cpu) {
  uint8_t val = fetch_operand(cpu);
  cpu->ac = cpu->x = (cpu->ac | LXA_XAA_MAGIC) & val;
  set_zero_negative(cpu, cpu->ac);
}

private
void NOP(cpu_t *cpu) { fetch_operand(cpu); }

private
void ORA(cpu_t *cpu) {
  cpu->ac |= fetch_operand(cpu);
  set_zero_negative(cpu, cpu->ac);
}

private
void PHA(cpu_t *cpu) {
  fetch_operand(cpu);
  push_byte(cpu, cpu->ac);
}

private
void PHP(cpu_t *cpu) {
  fetch_operand(cpu);
  push_byte(cpu, cpu->s.val | B);
}

private
void PLA(cpu_t *cpu) {
  fetch_operand(cpu);
  peek_byte(cpu);
  cpu->ac = pop_byte(cpu);
  set_zero_negative(cpu, cpu->ac);
}

private
void PLP(cpu_t *cpu) {
  fetch_operand(cpu);
  peek_byte(cpu);
  cpu->s.val &= UNUSED | B;
  cpu->s.val |= pop_byte(cpu) & ~(UNUSED | B);
}

private
void RLA(cpu_t *cpu) {
  uint16_t addr = fetch_address(cpu);
  uint8_t val = fetch_operand(cpu, addr);

  mem_write_byte(cpu, addr, val);  // dummy write

  bool old_carry = cpu->s.bits.carry;
  uint8_t shifted_val = (uint8_t)(val << 1) | old_carry;
  cpu->s.bits.carry = check_if_bit7_set(val);

  cpu->ac &= shifted_val;
  set_zero_negative(cpu, cpu->ac);
  mem_write_byte(cpu, addr, shifted_val);
}

private
uint8_t ROL(cpu_t *cpu, uint8_t old_val) {
  bool old_carry = cpu->s.bits.carry;
  cpu->s.bits.carry = check_if_bit7_set(old_val);

  uint8_t new_val = (uint8_t)(old_val << 1) | old_carry;
  set_zero_negative(cpu, new_val);
  return new_val;
}

private
void ROLa(cpu_t *cpu) {
  fetch_operand(cpu);
  uint8_t shifted_val = ROL(cpu, cpu->ac);
  cpu->ac = shifted_val;
}

private
void ROLm(cpu_t *cpu) {
  uint16_t addr = fetch_address(cpu);
  uint8_t val = fetch_operand(cpu, addr);
  uint8_t shifted_val = ROL(cpu, val);

  mem_write_byte(cpu, addr, val);  // dummy write
  mem_write_byte(cpu, addr, shifted_val);
}

private
uint8_t ROR(cpu_t *cpu, uint8_t old_val) {
  uint8_t old_carry = (uint8_t)(cpu->s.bits.carry << 7);
  cpu->s.bits.carry = check_if_bit0_set(old_val);

  uint8_t new_val = (old_val >> 1) | old_carry;
  set_zero_negative(cpu, new_val);
  return new_val;
}

private
void RORa(cpu_t *cpu) {
  fetch_operand(cpu);
  cpu->ac = ROR(cpu, cpu->ac);
}

private
void RORm(cpu_t *cpu) {
  uint16_t addr = fetch_address(cpu);
  uint8_t val = fetch_operand(cpu, addr);
  uint8_t shifted_val = ROR(cpu, val);

  mem_write_byte(cpu, addr, val);  // dummy write
  mem_write_byte(cpu, addr, shifted_val);
}

private
void RRA(cpu_t *cpu) {
  uint16_t addr = fetch_address(cpu);
  uint8_t val = fetch_operand(cpu, addr);
  uint8_t carry = (uint8_t)(cpu->s.bits.carry << 7);

  uint8_t shifted_val = (val >> 1) | carry;
  cpu->s.bits.carry = check_if_bit0_set(val);
  ADD(cpu, shifted_val);

  mem_write_byte(cpu, addr, val);
  mem_write_byte(cpu, addr, shifted_val);
}

private
void RTI(cpu_t *cpu) {
  fetch_operand(cpu);
  peek_byte(cpu);

  cpu->s.val &= UNUSED | B;
  cpu->s.val |= pop_byte(cpu) & ~(UNUSED | B);
  cpu->pc = pop_word(cpu);
}

private
void RTS(cpu_t *cpu) {
  fetch_operand(cpu);
  peek_byte(cpu);
  uint16_t addr = pop_word(cpu);
  mem_read_byte(cpu, addr);
  cpu->pc = addr + 1;
}

private
void SAX(cpu_t *cpu) { mem_write_byte(cpu, fetch_address(cpu), cpu->ac & cpu->x); }

private
void SBC(cpu_t *cpu) { ADD(cpu, ~fetch_operand(cpu)); }

private
void SEC(cpu_t *cpu) {
  fetch_operand(cpu);
  cpu->s.bits.carry = true;
}

private
void SED(cpu_t *cpu) {
  fetch_operand(cpu);
  cpu->s.bits.decimal = true;
}

private
void SEI(cpu_t *cpu) {
  fetch_operand(cpu);
  cpu->s.bits.interrupt_disable = true;
}

// FIXME: later
private
void SHX(cpu_t *cpu) { return; }

// FIXME: later
private
void SHY(cpu_t *cpu) { return; }

private
void SLO(cpu_t *cpu) {
  uint16_t addr = fetch_address(cpu);

  uint8_t old_val = fetch_operand(cpu, addr);
  uint8_t shifted_val = ASL(cpu, old_val);

  cpu->ac |= shifted_val;

  mem_write_byte(cpu, addr, old_val);  // dummy write
  mem_write_byte(cpu, addr, shifted_val);
  set_zero_negative(cpu, cpu->ac);
}

private
void SRE(cpu_t *cpu) {
  uint16_t addr = fetch_address(cpu);
  uint8_t val = fetch_operand(cpu, addr);
  mem_write_byte(cpu, addr, val);

  cpu->s.bits.carry = get_0th_bit(val);
  uint8_t shifted_val = val >> 1;
  cpu->ac ^= shifted_val;
  set_zero_negative(cpu, cpu->ac);

  mem_write_byte(cpu, addr, shifted_val);
}

// FIXME: Implement this opcode later
private
void STP(cpu_t *cpu) { fetch_operand(cpu); }

private
void STR(cpu_t *cpu, uint8_t reg) { mem_write_byte(cpu, fetch_address(cpu), reg); }

private
void STA(cpu_t *cpu) { STR(cpu, cpu->ac); }

private
void STX(cpu_t *cpu) { STR(cpu, cpu->x); }

private
void STY(cpu_t *cpu) { STR(cpu, cpu->y); }

// FIXME: later
private
void TAS(cpu_t *cpu) { return; }

private
void TRA(cpu_t *cpu, uint8_t *reg_a, uint8_t reg_b) {
  fetch_operand(cpu);
  *reg_a = reg_b;
  set_zero_negative(cpu, *reg_a);
}

private
void TAX(cpu_t *cpu) { TRA(cpu, &cpu->x, cpu->ac); }

private
void TAY(cpu_t *cpu) { TRA(cpu, &cpu->y, cpu->ac); }

private
void TSX(cpu_t *cpu) { TRA(cpu, &cpu->x, cpu->sp); }

private
void TXA(cpu_t *cpu) { TRA(cpu, &cpu->ac, cpu->x); }

private
void TYA(cpu_t *cpu) { TRA(cpu, &cpu->ac, cpu->y); }

private
void TXS(cpu_t *cpu) {
  fetch_operand(cpu);
  cpu->sp = cpu->x;
}

private
void XAA(cpu_t *cpu) {
  uint8_t val = fetch_operand(cpu);

  cpu->ac = ((cpu->ac | LXA_XAA_MAGIC) & cpu->x & val);
  set_zero_negative(cpu, cpu->ac);
}

// clang-format off
typedef void (*opcode_func_t)(cpu_t *cpu);
static const opcode_func_t opcode_table[256] = {
  //+00 +01  +02  +03  +04  +05  +06   +07  +08  +09  +0A   +0B  +0C  +0D  +0E   +0F
  BRK,  ORA, STP, SLO, NOP, ORA, ASLm, SLO, PHP, ORA, ASLa, ANC, NOP, ORA, ASLm, SLO, // 00
  BPL,  ORA, STP, SLO, NOP, ORA, ASLm, SLO, CLC, ORA, NOP,  SLO, NOP, ORA, ASLm, SLO, // 10
  JSR,  AND, STP, RLA, BIT, AND, ROLm, RLA, PLP, AND, ROLa, ANC, BIT, AND, ROLm, RLA, // 20
  BMI,  AND, STP, RLA, NOP, AND, ROLm, RLA, SEC, AND, NOP,  RLA, NOP, AND, ROLm, RLA, // 30
  RTI,  EOR, STP, SRE, NOP, EOR, LSRm, SRE, PHA, EOR, LSRa, ALR, JMP, EOR, LSRm, SRE, // 40
  BVC,  EOR, STP, SRE, NOP, EOR, LSRm, SRE, CLI, EOR, NOP,  SRE, NOP, EOR, LSRm, SRE, // 50
  RTS,  ADC, STP, RRA, NOP, ADC, RORm, RRA, PLA, ADC, RORa, ARR, JMP, ADC, RORm, RRA, // 60
  BVS,  ADC, STP, RRA, NOP, ADC, RORm, RRA, SEI, ADC, NOP,  RRA, NOP, ADC, RORm, RRA, // 70
  NOP,  STA, NOP, SAX, STY, STA, STX,  SAX, DEY, NOP, TXA,  XAA, STY, STA, STX,  SAX, // 80
  BCC,  STA, STP, AHX, STY, STA, STX,  SAX, TYA, STA, TXS,  TAS, SHY, STA, SHX,  AHX, // 90
  LDY,  LDA, LDX, LAX, LDY, LDA, LDX,  LAX, TAY, LDA, TAX,  LXA, LDY, LDA, LDX,  LAX, // A0
  BCS,  LDA, STP, LAX, LDY, LDA, LDX,  LAX, CLV, LDA, TSX,  LAS, LDY, LDA, LDX,  LAX, // B0
  CPY,  CPA, NOP, DCP, CPY, CPA, DEC,  DCP, INY, CPA, DEX,  AXS, CPY, CPA, DEC,  DCP, // C0
  BNE,  CPA, STP, DCP, NOP, CPA, DEC,  DCP, CLD, CPA, NOP,  DCP, NOP, CPA, DEC,  DCP, // D0
  CPX,  SBC, NOP, ISC, CPX, SBC, INC,  ISC, INX, SBC, NOP,  SBC, CPX, SBC, INC,  ISC, // E0
  BEQ,  SBC, STP, ISC, NOP, SBC, INC,  ISC, SED, SBC, NOP,  ISC, NOP, SBC, INC,  ISC, // F0
};
static const char *opcode_table_string[256] = {
  //+00   +01   +02    +03    +04    +05    +06    +07    +08    +09    +0A    +0B    +0C    +0D    +0E    +0F
  "BRK", "ORA", "STP", "SLO", "NOP", "ORA", "ASL", "SLO", "PHP", "ORA", "ASL", "ANC", "NOP", "ORA", "ASL", "SLO", // 00
  "BPL", "ORA", "STP", "SLO", "NOP", "ORA", "ASL", "SLO", "CLC", "ORA", "NOP", "SLO", "NOP", "ORA", "ASL", "SLO", // 10
  "JSR", "AND", "STP", "RLA", "BIT", "AND", "ROL", "RLA", "PLP", "AND", "ROL", "ANC", "BIT", "AND", "ROL", "RLA", // 20
  "BMI", "AND", "STP", "RLA", "NOP", "AND", "ROL", "RLA", "SEC", "AND", "NOP", "RLA", "NOP", "AND", "ROL", "RLA", // 30
  "RTI", "EOR", "STP", "SRE", "NOP", "EOR", "LSR", "SRE", "PHA", "EOR", "LSR", "ALR", "JMP", "EOR", "LSR", "SRE", // 40
  "BVC", "EOR", "STP", "SRE", "NOP", "EOR", "LSR", "SRE", "CLI", "EOR", "NOP", "SRE", "NOP", "EOR", "LSR", "SRE", // 50
  "RTS", "ADC", "STP", "RRA", "NOP", "ADC", "ROR", "RRA", "PLA", "ADC", "ROR", "ARR", "JMP", "ADC", "ROR", "RRA", // 60
  "BVS", "ADC", "STP", "RRA", "NOP", "ADC", "ROR", "RRA", "SEI", "ADC", "NOP", "RRA", "NOP", "ADC", "ROR", "RRA", // 70
  "NOP", "STA", "NOP", "SAX", "STY", "STA", "STX", "SAX", "DEY", "NOP", "TXA", "XAA", "STY", "STA", "STX", "SAX", // 80
  "BCC", "STA", "STP", "AHX", "STY", "STA", "STX", "SAX", "TYA", "STA", "TXS", "TAS", "SHY", "STA", "SHX", "AHX", // 90
  "LDY", "LDA", "LDX", "LAX", "LDY", "LDA", "LDX", "LAX", "TAY", "LDA", "TAX", "LXA", "LDY", "LDA", "LDX", "LAX", // A0
  "BCS", "LDA", "STP", "LAX", "LDY", "LDA", "LDX", "LAX", "CLV", "LDA", "TSX", "LAS", "LDY", "LDA", "LDX", "LAX", // B0
  "CPY", "CPA", "NOP", "DCP", "CPY", "CPA", "DEC", "DCP", "INY", "CPA", "DEX", "AXS", "CPY", "CPA", "DEC", "DCP", // C0
  "BNE", "CPA", "STP", "DCP", "NOP", "CPA", "DEC", "DCP", "CLD", "CPA", "NOP", "DCP", "NOP", "CPA", "DEC", "DCP", // D0
  "CPX", "SBC", "NOP", "ISC", "CPX", "SBC", "INC", "ISC", "INX", "SBC", "NOP", "SBC", "CPX", "SBC", "INC", "ISC", // E0
  "BEQ", "SBC", "STP", "ISC", "NOP", "SBC", "INC", "ISC", "SED", "SBC", "NOP", "ISC", "NOP", "SBC", "INC", "ISC", // F0
};
// clang-format on

cpu_t cpu_power_on(void) {
  return (cpu_t){
      .pc = RESET_VECTOR,
      .sp = 0xFD,
      .ac = 0,
      .x = 0,
      .y = 0,
      .s.val = UNUSED | INTERRUPT_DISABLE,
      .cycles = 0,
      .mem = {},
  };
}

void cpu_reset(cpu_t *cpu) {
  cpu->pc = RESET_VECTOR;
  cpu->sp -= 3;
  cpu->s.bits.interrupt_disable = true;
  cpu->cycles = 0;
}

void step_cpu(cpu_t *cpu) {
  uint8_t op = mem_read_byte(cpu);
  cpu->current_addr_mode = addr_mode_table[op];
  opcode_table[op](cpu);
  log_info("ADDRESSING:%s INST:%s PC:%d AC:%d X:%d Y:%d S:%d SP:%d CYC:%ld",
           addressing_modes_string[cpu->current_addr_mode], opcode_table_string[op], cpu->pc,
           cpu->ac, cpu->x, cpu->y, cpu->s.val, cpu->sp, cpu->cycles);
}
