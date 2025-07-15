#include <stdint.h>
#include <stdlib.h>

#include "cpu.h"
#include "mem.h"
#include "log.h"

cpu_t cpu = {};

// CPU state is mutated here to be written back once t = 0
// This ensures things are mostly accurate
// Reads are always at fetch, writes always at end of instruction
cpu_t cpu_next;

void cpu_reset() {
    cpu.pc = 0x00;
    cpu_next = cpu;
}

// Flag helper functions
static const uint8_t CPU_FLAG_Z = 0b10000000;
static const uint8_t CPU_FLAG_N = 0b01000000;
static const uint8_t CPU_FLAG_H = 0b00100000;
static const uint8_t CPU_FLAG_C = 0b00010000;

static bool flag_get_z() {
    return (cpu.f & CPU_FLAG_Z) != 0;
}

static void flag_set_z(uint8_t value) {
    if (value == 0) {
        cpu_next.f |= CPU_FLAG_Z;
    } else {
        cpu_next.f &= ~CPU_FLAG_Z;
    }
}

static bool flag_get_n() {
    return (cpu.f & CPU_FLAG_N) != 0;
}

static void flag_set_n(bool value) {
    if (value) {
        cpu_next.f |= CPU_FLAG_N;
    } else {
        cpu_next.f &= ~CPU_FLAG_N;
    }
}

static bool flag_get_h() {
    return (cpu.f & CPU_FLAG_H) != 0;
}

static void flag_set_h(bool value) {
    if (value) {
        cpu_next.f |= CPU_FLAG_H;
    } else {
        cpu_next.f &= ~CPU_FLAG_H;
    }
}

static bool flag_get_c() {
    return (cpu.f & CPU_FLAG_C) != 0;
}

static void flag_set_c(bool value) {
    if (value) {
        cpu_next.f |= CPU_FLAG_C;
    } else {
        cpu_next.f &= ~CPU_FLAG_C;
    }
}

// Register helper functions

static inline uint16_t bytes_to_16(uint8_t high, uint8_t low) {
    return ((uint16_t)high << 8) + (uint16_t)low;
}

static inline uint16_t reg_af_read() {
    return bytes_to_16(cpu.a, cpu.f);
}

static inline void reg_af_write(uint16_t value) {
    cpu_next.a = value >> 8;
    cpu_next.f = value & 0x00FF;
}

static inline uint16_t reg_bc_read() {
    return bytes_to_16(cpu.b, cpu.c);
}

static inline void reg_bc_write(uint16_t value) {
    cpu_next.b = value >> 8;
    cpu_next.c = value & 0x00FF;
}

static inline uint16_t reg_de_read() {
    return bytes_to_16(cpu.d, cpu.e);
}

static inline void reg_de_write(uint16_t value) {
    cpu_next.d = value >> 8;
    cpu_next.e = value & 0x00FF;
}

static inline uint16_t reg_hl_read() {
    return bytes_to_16(cpu.h, cpu.l);
}

static inline void reg_hl_write(uint16_t value) {
    cpu_next.h = value >> 8;
    cpu_next.l = value & 0x00FF;
}

// Prefixed instruction helper functions
static void prefix_rlc(uint8_t *reg) {
    uint8_t result = (*reg << 1) | (*reg >> 7);
    flag_set_z(result);
    flag_set_n(0);
    flag_set_h(0);
    flag_set_c(*reg >> 7);
    *reg = result;
}

static void prefix_rrc(uint8_t *reg) {
    uint8_t result = (*reg >> 1) | (*reg << 7);
    flag_set_z(result);
    flag_set_n(0);
    flag_set_h(0);
    flag_set_c(*reg & 1);
    *reg = result;
}

static void prefix_rl(uint8_t *reg) {
    uint8_t result = (*reg << 1) | flag_get_c();
    flag_set_z(result);
    flag_set_n(0);
    flag_set_h(0);
    flag_set_c(*reg >> 7);
    *reg = result;
}

static void prefix_rr(uint8_t *reg) {
    uint8_t result = (*reg >> 1) + ((uint8_t)flag_get_c() << 7);
    flag_set_z(result);
    flag_set_n(0);
    flag_set_h(0);
    flag_set_c(*reg & 1);
    *reg = result;
}

static void prefix_sla(uint8_t *reg) {
    uint8_t result = (*reg << 1);
    flag_set_z(result);
    flag_set_n(0);
    flag_set_h(0);
    flag_set_c(*reg >> 7);
    *reg = result;
}

static void prefix_sra(uint8_t *reg) {
    uint8_t result = (*reg >> 1) | (*reg & 0b10000000);
    flag_set_z(result);
    flag_set_n(0);
    flag_set_h(0);
    flag_set_c(*reg & 1);
    *reg = result;
}

static void prefix_swap(uint8_t *reg) {
    uint8_t result = (*reg >> 4) | (*reg << 4);
    flag_set_z(result);
    flag_set_n(0);
    flag_set_h(0);
    flag_set_c(0);
    *reg = result;
}

static void prefix_srl(uint8_t *reg) {
    uint8_t result = *reg >> 1;
    flag_set_z(result);
    flag_set_n(0);
    flag_set_h(0);
    flag_set_c(*reg & 1);
    *reg = result;
}

static void prefix_bit(uint8_t bit, uint8_t *reg) {
    bool result = (*reg >> bit) & 1;
    flag_set_z(result);
    flag_set_n(0);
    flag_set_h(1);
}

static void prefix_res(uint8_t bit, uint8_t *reg) {
    *reg &= ~(1 << bit);
}

static void prefix_set(uint8_t bit, uint8_t *reg) {
    *reg |= (1 << bit);
}

// Execute prefixed instructions
bool execute_prefix(uint8_t op) {
    uint8_t op_high = op >> 4;
    uint8_t op_low = op & 0xF;

    uint8_t *reg;
    uint8_t n8;
    bool hl_mem = 0;
    switch(op_low%8) {
        case 0: reg = &cpu_next.b; break;
        case 1: reg = &cpu_next.c; break;
        case 2: reg = &cpu_next.d; break;
        case 3: reg = &cpu_next.e; break;
        case 4: reg = &cpu_next.h; break;
        case 5: reg = &cpu_next.l; break;
        case 6:
            n8 = mem_read(reg_hl_read());
            reg = &n8;
            hl_mem = 1;
            break;
        case 7: reg = &cpu_next.a; break;
    }

    bool bank = op_low / 8;
    switch(((uint8_t)bank << 4) | op_high) {
        case 0x00: prefix_rlc(reg); break;
        case 0x10: prefix_rrc(reg); break;
        case 0x01: prefix_rl(reg); break;
        case 0x11: prefix_rr(reg); break;
        case 0x02: prefix_sla(reg); break;
        case 0x12: prefix_sra(reg); break;
        case 0x03: prefix_swap(reg); break;
        case 0x13: prefix_srl(reg); break;
        case 0x04: prefix_bit(0, reg); break;
        case 0x14: prefix_bit(1, reg); break;
        case 0x05: prefix_bit(2, reg); break;
        case 0x15: prefix_bit(3, reg); break;
        case 0x06: prefix_bit(4, reg); break;
        case 0x16: prefix_bit(5, reg); break;
        case 0x07: prefix_bit(6, reg); break;
        case 0x17: prefix_bit(7, reg); break;
        case 0x08: prefix_res(0, reg); break;
        case 0x18: prefix_res(1, reg); break;
        case 0x09: prefix_res(2, reg); break;
        case 0x19: prefix_res(3, reg); break;
        case 0x0A: prefix_res(4, reg); break;
        case 0x1A: prefix_res(5, reg); break;
        case 0x0B: prefix_res(6, reg); break;
        case 0x1B: prefix_res(7, reg); break;
        case 0x0C: prefix_set(0, reg); break;
        case 0x1C: prefix_set(1, reg); break;
        case 0x0D: prefix_set(2, reg); break;
        case 0x1D: prefix_set(3, reg); break;
        case 0x0E: prefix_set(4, reg); break;
        case 0x1E: prefix_set(5, reg); break;
        case 0x0F: prefix_set(6, reg); break;
        case 0x1F: prefix_set(7, reg); break;
        default:
            printf("Unknown PREFIX OP:0x%X\n", op);
            exit(1);
    }

    if (hl_mem) {
        mem_write_next(reg_hl_read(), n8);
    }

    return hl_mem; // Instruction needs an additonal t += 4
}

// Opcode helper functions
static void add_a(uint8_t *reg) {
    cpu_next.a += *reg;
    flag_set_z(cpu_next.a);
    flag_set_n(0);
    flag_set_h(((cpu.a & 0x0F) + (*reg & 0x0F)) > 0x0F);
    flag_set_c(cpu_next.a < cpu.a);
}

static void adc_a(uint8_t *reg) {
    uint8_t carry = flag_get_c();
    uint16_t result = cpu.a + *reg + carry;
    flag_set_z((uint8_t)result);
    flag_set_n(0);
    flag_set_h(((cpu.a & 0x0F) + (*reg & 0x0F) + carry) > 0x0F);
    flag_set_c(result > 0xFF);
    cpu_next.a = (uint8_t)result;
}

static void sub_a(uint8_t *reg) {
    cpu_next.a -= *reg;
    flag_set_z(cpu_next.a);
    flag_set_n(1);
    flag_set_h((cpu.a & 0x0F) < (*reg & 0x0F));
    flag_set_c(*reg > cpu.a);
}

static void sbc_a(uint8_t *reg) {
    uint8_t carry = flag_get_c();
    uint16_t result = cpu.a - *reg - carry;
    flag_set_z((uint8_t)result);
    flag_set_n(1);
    flag_set_h((cpu.a & 0x0F) < ((*reg & 0x0F) + carry));
    flag_set_c(result > 0xFF);
    cpu_next.a = (uint8_t)result;
}

static void and_a(uint8_t *reg) {
    cpu_next.a &= *reg;
    flag_set_z(cpu_next.a);
    flag_set_n(0);
    flag_set_h(1);
    flag_set_c(0);
}

static void xor_a(uint8_t *reg) {
    cpu_next.a ^= *reg;
    flag_set_z(cpu_next.a);
    flag_set_n(0);
    flag_set_h(0);
    flag_set_c(0);
}

static void or_a(uint8_t *reg) {
    cpu_next.a |= *reg;
    flag_set_z(cpu_next.a);
    flag_set_n(0);
    flag_set_h(0);
    flag_set_c(0);
}

static void cp_a(uint8_t *reg) {
    uint8_t result = cpu.a - *reg;
    flag_set_z(result);
    flag_set_n(1);
    flag_set_h((*reg & 0x0F) > (cpu.a & 0x0F));
    flag_set_c(*reg > cpu.a);
}

uint8_t cpu_execute() {
    uint8_t t = 0;

    // Fetch instruction
    cpu.op = mem_read(cpu.pc);

    DEBUG_PRINTF_CPU("PC:0x%X OP:0x%X ", cpu.pc, cpu.op);

    // Temporary variables
    uint8_t n8;
    uint8_t result;
    uint16_t result16;

    // Calculate if an interrupt should be handled
    uint8_t *iflag = &mem.io_reg[0x0F];
    bool interrupt = cpu.ime && (mem.ie & *iflag);

    // Compute state mutation
    if (!interrupt && !cpu.halt) { // No interrupt triggered
        switch(cpu.op) {
            case 0x00: // NOP
                t = 4;
                cpu_next.pc += 1;
                break;
            case 0x01: // LD BC,n16
                t = 12;
                cpu_next.pc += 3;
                reg_bc_write(mem_read16(cpu.pc+1));
                break;
            case 0x02: // LD [BC], A
                t = 8;
                cpu_next.pc += 1;
                mem_write_next(reg_bc_read(), cpu.a);
                break;
            case 0x03: // INC BC
                t = 8;
                cpu_next.pc += 1;
                reg_bc_write(reg_bc_read() + 1);
                break;
            case 0x04: // INC B
                t = 4;
                cpu_next.pc += 1;
                cpu_next.b = cpu.b + 1;
                flag_set_z(cpu_next.b);
                flag_set_n(0);
                flag_set_h((cpu.b & 0x0F) == 0x0F);
                break;
            case 0x05: // DEC B
                t = 4;
                cpu_next.pc += 1;
                cpu_next.b = cpu.b - 1;
                flag_set_z(cpu_next.b);
                flag_set_n(1);
                flag_set_h((cpu.b & 0x0F) == 0x00);
                break;
            case 0x06: // LD B,n8
                t = 8;
                cpu_next.pc += 2;
                cpu_next.b = mem_read(cpu.pc+1);
                break;
            case 0x07: // RLCA
                t = 4;
                cpu_next.pc += 1;
                cpu_next.a = (cpu.a << 1) | (cpu.a >> 7);
                flag_set_z(1);
                flag_set_n(0);
                flag_set_h(0);
                flag_set_c(cpu.a >> 7);
                break;
            case 0x08: // LD [a16],SP
                t = 20;
                cpu_next.pc += 3;
                mem_write_next16(mem_read16(cpu.pc+1), cpu.sp);
                break;
            case 0x09: // ADD HL,BC
                t = 8;
                cpu_next.pc += 1;
                result16 = reg_hl_read() + reg_bc_read();
                reg_hl_write(result16);
                flag_set_n(0);
                flag_set_h(((reg_hl_read() & 0x0FFF) + (reg_bc_read() & 0x0FFF)) > 0x0FFF);
                flag_set_c(result16 < reg_hl_read());
                break;
            case 0x0A: // LD A,[BC];
                t = 8;
                cpu_next.pc += 1;
                cpu_next.a = mem_read(reg_bc_read());
                break;
            case 0x0B: // DEC BC
                t = 8;
                cpu_next.pc += 1;
                reg_bc_write(reg_bc_read() - 1);
                break;
            case 0x0C: // INC C
                t = 4;
                cpu_next.pc += 1;
                cpu_next.c += 1;
                flag_set_z(cpu_next.c);
                flag_set_n(0);
                flag_set_h((cpu.c & 0x0F) == 0x0F);
                break;
            case 0x0D: // DEC C
                t = 4;
                cpu_next.pc += 1;
                cpu_next.c -= 1;
                flag_set_z(cpu_next.c);
                flag_set_n(1);
                flag_set_h((cpu.c & 0x0F) == 0);
                break;
            case 0x0E: // LD C,n8
                t = 8;
                cpu_next.pc += 2;
                cpu_next.c = mem_read(cpu.pc+1);
                break;
            case 0x0F: // RRCA
                t = 4;
                cpu_next.pc += 1;
                cpu_next.a = (cpu.a >> 1) | (cpu.a << 7);
                flag_set_z(1);
                flag_set_n(0);
                flag_set_h(0);
                flag_set_c(cpu.a & 1);
                break;
            case 0x10: // STOP
                t = 4;
                cpu_next.pc += 2;
                cpu_next.stop = 1;
                break;
            case 0x11: // LD DE,n16
                t = 12;
                cpu_next.pc += 3;
                reg_de_write(mem_read16(cpu.pc+1));
                break;
            case 0x12: // LD [DE], A
                t = 8;
                cpu_next.pc += 1;
                mem_write_next(reg_de_read(), cpu.a);
                break;
            case 0x13: // INC DE
                t = 8;
                cpu_next.pc += 1;
                reg_de_write(reg_de_read() + 1);
                break;
            case 0x14: // INC D
                t = 4;
                cpu_next.pc += 1;
                cpu_next.d += 1;
                flag_set_z(cpu_next.d);
                flag_set_n(0);
                flag_set_h((cpu.d & 0x0F) == 0x0F);
                break;
            case 0x15: // DEC D
                t = 4;
                cpu_next.pc += 1;
                cpu_next.d = cpu.d - 1;
                flag_set_z(cpu_next.d);
                flag_set_n(1);
                flag_set_h((cpu.d & 0x0F) == 0);
                break;
            case 0x16: // LD D,n8
                t = 8;
                cpu_next.pc += 2;
                cpu_next.d = mem_read(cpu.pc+1);
                break;
            case 0x17: // RLA
                t = 4;
                cpu_next.pc += 1;
                cpu_next.a = (cpu.a << 1) + flag_get_c();
                flag_set_z(1);
                flag_set_n(0);
                flag_set_h(0);
                flag_set_c(cpu.a >> 7);
                break;
            case 0x18: // JR e8
                t = 12;
                cpu_next.pc += 2 + (int8_t)mem_read(cpu.pc+1);
                break;
            case 0x19: // ADD HL,DE
                t = 8;
                cpu_next.pc += 1;
                result16 = reg_hl_read() + reg_de_read();
                reg_hl_write(result16);
                flag_set_n(0);
                flag_set_h(((reg_hl_read() & 0x0FFF) + (reg_de_read() & 0x0FFF)) > 0x0FFF);
                flag_set_c(result16 < reg_hl_read());
                break;
            case 0x1A: // LD A,[DE];
                t = 8;
                cpu_next.pc += 1;
                cpu_next.a = mem_read(reg_de_read());
                break;
            case 0x1B: // DEC DE
                t = 8;
                cpu_next.pc += 1;
                reg_de_write(reg_de_read() - 1);
                break;
            case 0x1C: // INC E
                t = 4;
                cpu_next.pc += 1;
                cpu_next.e += 1;
                flag_set_z(cpu_next.e);
                flag_set_n(0);
                flag_set_h((cpu.e & 0x0F) == 0x0F);
                break;
            case 0x1D: // DEC E
                t = 4;
                cpu_next.pc += 1;
                cpu_next.e -= 1;
                flag_set_z(cpu_next.e);
                flag_set_n(1);
                flag_set_h((cpu.e & 0x0F) == 0x00);
                break;
            case 0x1E: // LD E,n8
                t = 8;
                cpu_next.pc += 2;
                cpu_next.e = mem_read(cpu.pc+1);
                break;
            case 0x1F: // RRA
                t = 4;
                cpu_next.pc += 1;
                cpu_next.a = (cpu.a >> 1) + ((uint8_t)flag_get_c() << 7);
                flag_set_z(1);
                flag_set_n(0);
                flag_set_h(0);
                flag_set_c(cpu.a & 1);
                break;
            case 0x20: // JR NZ,e8
                if (flag_get_z()) { // Not taken
                    t = 8;
                    cpu_next.pc += 2;
                } else { // Taken
                    t = 12;
                    cpu_next.pc += 2 + (int8_t)mem_read(cpu.pc+1);
                }
                break;
            case 0x21: // LD HL,n16
                t = 12;
                cpu_next.pc += 3;
                reg_hl_write(mem_read16(cpu.pc+1));
                break;
            case 0x22: // LD [HL+], A
                t = 8;
                cpu_next.pc += 1;
                mem_write_next(reg_hl_read(), cpu.a);
                reg_hl_write(reg_hl_read() + 1);
                break;
            case 0x23: // INC HL
                t = 8;
                cpu_next.pc += 1;
                reg_hl_write(reg_hl_read() + 1);
                break;
            case 0x24: // INC H
                t = 4;
                cpu_next.pc += 1;
                cpu_next.h += 1;
                flag_set_z(cpu_next.h);
                flag_set_n(0);
                flag_set_h((cpu.h & 0x0F) == 0x0F);
                break;
            case 0x25: // DEC H
                t = 4;
                cpu_next.pc += 1;
                cpu_next.h -= 1;
                flag_set_z(cpu_next.h);
                flag_set_n(1);
                flag_set_h((cpu.h & 0x0F) == 0x00);
                break;
            case 0x26: // LD H,n8
                t = 8;
                cpu_next.pc += 2;
                cpu_next.h = mem_read(cpu.pc+1);
                break;
            case 0x27: // DAA
                t = 4;
                cpu_next.pc += 1;
                uint8_t adj = 0;
                if (flag_get_n()) {
                    if (flag_get_h()) { adj += 0x6; }
                    if (flag_get_c()) { adj += 0x60; }
                    cpu_next.a -= adj;
                } else {
                    if (flag_get_h() || ((cpu.a & 0xF) > 0x9)) { adj += 0x6; }
                    if (flag_get_c() || (cpu.a > 0x99)) {
                        adj += 0x60;
                        flag_set_c(1);
                    }
                    cpu_next.a += adj;
                }
                flag_set_z(cpu_next.a);
                flag_set_h(0);
                break;
            case 0x28: // JR Z,e8
                if (!flag_get_z()) { // Not taken
                    t = 8;
                    cpu_next.pc += 2;
                } else { // Taken
                    t = 12;
                    cpu_next.pc += 2 + (int8_t)mem_read(cpu.pc+1);
                }
                break;
            case 0x29: // ADD HL,HL
                t = 8;
                cpu_next.pc += 1;
                result16 = reg_hl_read() + reg_hl_read();
                reg_hl_write(result16);
                flag_set_n(0);
                flag_set_h(((reg_hl_read() & 0x0FFF) + (reg_hl_read() & 0x0FFF)) > 0x0FFF);
                flag_set_c(result16 < reg_hl_read());
                break;
            case 0x2A: // LD A,[HL+]
                t = 8;
                cpu_next.pc += 1;
                cpu_next.a = mem_read(reg_hl_read());
                reg_hl_write(reg_hl_read()+1);
                break;
            case 0x2B: // DEC HL
                t = 8;
                cpu_next.pc += 1;
                reg_hl_write(reg_hl_read() - 1);
                break;
            case 0x2C: // INC L
                t = 4;
                cpu_next.pc += 1;
                cpu_next.l += 1;
                flag_set_z(cpu_next.l);
                flag_set_n(0);
                flag_set_h((cpu.l & 0x0F) == 0x0F);
                break;
            case 0x2D: // DEC L
                t = 4;
                cpu_next.pc += 1;
                cpu_next.l -= 1;
                flag_set_z(cpu_next.l);
                flag_set_n(1);
                flag_set_h((cpu.l & 0x0F) == 0x00);
                break;
            case 0x2E: // LD L,n8
                t = 8;
                cpu_next.pc += 2;
                cpu_next.l = mem_read(cpu.pc+1);
                break;
            case 0x2F: // CPL
                t = 4;
                cpu_next.pc += 1;
                cpu_next.a = ~cpu.a;
                flag_set_n(1);
                flag_set_h(1);
                break;
            case 0x30: // JR NC,e8
                if (flag_get_c()) { // Not taken
                    t = 8;
                    cpu_next.pc += 2;
                } else { // Taken
                    t = 12;
                    cpu_next.pc += 2 + (int8_t)mem_read(cpu.pc+1);
                }
                break;
            case 0x31: // LD SP,n16
                t = 12;
                cpu_next.pc += 3;
                cpu_next.sp = mem_read16(cpu.pc+1);
                break;
            case 0x32: // LD [HL-],A
                t = 8;
                cpu_next.pc += 1;
                mem_write_next(reg_hl_read(), cpu.a);
                reg_hl_write(reg_hl_read() - 1);
                break;
            case 0x33: // INC SP
                t = 8;
                cpu_next.pc += 1;
                cpu_next.sp += 1;
                break;
            case 0x34: // INC [HL]
                t = 12;
                cpu_next.pc += 1;
                n8 = mem_read(reg_hl_read());
                result = n8 + 1;
                mem_write_next(reg_hl_read(), result);
                flag_set_z(result);
                flag_set_n(0);
                flag_set_h((n8 & 0x0F) == 0x0F);
                break;
            case 0x35: // DEC [HL]
                t = 12;
                cpu_next.pc += 1;
                n8 = mem_read(reg_hl_read());
                result = n8 - 1;
                mem_write_next(reg_hl_read(), result);
                flag_set_z(result);
                flag_set_n(1);
                flag_set_h((n8 & 0x0F) == 0x00);
                break;
            case 0x36: // LD [HL],n8
                t = 12;
                cpu_next.pc += 2;
                mem_write_next(reg_hl_read(), mem_read(cpu.pc+1));
                break;
            case 0x37: // SCF
                t = 4;
                cpu_next.pc += 1;
                flag_set_n(0);
                flag_set_h(0);
                flag_set_c(1);
                break;
            case 0x38: // JR C,e8
                if (!flag_get_c()) { // Not taken
                    t = 8;
                    cpu_next.pc += 2;
                } else { // Taken
                    t = 12;
                    cpu_next.pc += 2 + (int8_t)mem_read(cpu.pc+1);
                }
                break;
            case 0x39: // ADD HL,SP
                t = 8;
                cpu_next.pc += 1;
                result16 = reg_hl_read() + cpu.sp;
                reg_hl_write(result16);
                flag_set_n(0);
                flag_set_h(((reg_hl_read() & 0x0FFF) + (cpu.sp & 0x0FFF)) > 0x0FFF);
                flag_set_c(result16 < reg_hl_read());
                break;
            case 0x3A: // LD A,[HL-]
                t = 8;
                cpu_next.pc += 1;
                cpu_next.a = mem_read(reg_hl_read());
                reg_hl_write(reg_hl_read()-1);
                break;
            case 0x3B: // DEC SP
                t = 8;
                cpu_next.pc += 1;
                cpu_next.sp -= 1;
                break;
            case 0x3C: // INC A
                t = 4;
                cpu_next.pc += 1;
                cpu_next.a += 1;
                flag_set_z(cpu_next.a);
                flag_set_n(0);
                flag_set_h((cpu.a & 0x0F) == 0x0F);
                break;
            case 0x3D: // DEC A
                t = 4;
                cpu_next.pc += 1;
                cpu_next.a -= 1;
                flag_set_z(cpu_next.a);
                flag_set_n(1);
                flag_set_h((cpu.a & 0x0F) == 0);
                break;
            case 0x3E: // LD A,n8
                t = 8;
                cpu_next.pc += 2;
                cpu_next.a = mem_read(cpu.pc+1);
                break;
            case 0x3F: // CCF
                t = 4;
                cpu_next.pc += 1;
                flag_set_n(0);
                flag_set_h(0);
                flag_set_c(!flag_get_c());
                break;
            case 0x40: // LD B,B
                t = 4;
                cpu_next.pc += 1;
                cpu_next.b = cpu.b;
                break;
            case 0x41: // LD B,C
                t = 4;
                cpu_next.pc += 1;
                cpu_next.b = cpu.c;
                break;
            case 0x42: // LD B,D
                t = 4;
                cpu_next.pc += 1;
                cpu_next.b = cpu.d;
                break;
            case 0x43: // LD B,E
                t = 4;
                cpu_next.pc += 1;
                cpu_next.b = cpu.e;
                break;
            case 0x44: // LD B,H
                t = 4;
                cpu_next.pc += 1;
                cpu_next.b = cpu.h;
                break;
            case 0x45: // LD B,L
                t = 4;
                cpu_next.pc += 1;
                cpu_next.b = cpu.l;
                break;
            case 0x46: // LD B,[HL];
                t = 8;
                cpu_next.pc += 1;
                cpu_next.b = mem_read(reg_hl_read());
                break;
            case 0x47: // LD B,A
                t = 4;
                cpu_next.pc += 1;
                cpu_next.b = cpu.a;
                break;
            case 0x48: // LD C,B
                t = 4;
                cpu_next.pc += 1;
                cpu_next.c = cpu.b;
                break;
            case 0x49: // LD C,C
                t = 4;
                cpu_next.pc += 1;
                cpu_next.c = cpu.c;
                break;
            case 0x4A: // LD C,D
                t = 4;
                cpu_next.pc += 1;
                cpu_next.c = cpu.d;
                break;
            case 0x4B: // LD C,E
                t = 4;
                cpu_next.pc += 1;
                cpu_next.c = cpu.e;
                break;
            case 0x4C: // LD C,H
                t = 4;
                cpu_next.pc += 1;
                cpu_next.c = cpu.h;
                break;
            case 0x4D: // LD C,L
                t = 4;
                cpu_next.pc += 1;
                cpu_next.c = cpu.l;
                break;
            case 0x4E: // LD C,[HL];
                t = 8;
                cpu_next.pc += 1;
                cpu_next.c = mem_read(reg_hl_read());
                break;
            case 0x4F: // LD C,A
                t = 4;
                cpu_next.pc += 1;
                cpu_next.c = cpu.a;
                break;
            case 0x50: // LD D,B
                t = 4;
                cpu_next.pc += 1;
                cpu_next.d = cpu.b;
                break;
            case 0x51: // LD D,C
                t = 4;
                cpu_next.pc += 1;
                cpu_next.d = cpu.c;
                break;
            case 0x52: // LD D,D
                t = 4;
                cpu_next.pc += 1;
                cpu_next.d = cpu.d;
                break;
            case 0x53: // LD D,E
                t = 4;
                cpu_next.pc += 1;
                cpu_next.d = cpu.e;
                break;
            case 0x54: // LD D,H
                t = 4;
                cpu_next.pc += 1;
                cpu_next.d = cpu.h;
                break;
            case 0x55: // LD D,L
                t = 4;
                cpu_next.pc += 1;
                cpu_next.d = cpu.l;
                break;
            case 0x56: // LD D,[HL];
                t = 8;
                cpu_next.pc += 1;
                cpu_next.d = mem_read(reg_hl_read());
                break;
            case 0x57: // LD D,A
                t = 4;
                cpu_next.pc += 1;
                cpu_next.d = cpu.a;
                break;
            case 0x58: // LD E,B
                t = 4;
                cpu_next.pc += 1;
                cpu_next.e = cpu.b;
                break;
            case 0x59: // LD E,C
                t = 4;
                cpu_next.pc += 1;
                cpu_next.e = cpu.c;
                break;
            case 0x5A: // LD E,D
                t = 4;
                cpu_next.pc += 1;
                cpu_next.e = cpu.d;
                break;
            case 0x5B: // LD E,E
                t = 4;
                cpu_next.pc += 1;
                cpu_next.e = cpu.e;
                break;
            case 0x5C: // LD E,H
                t = 4;
                cpu_next.pc += 1;
                cpu_next.e = cpu.h;
                break;
            case 0x5D: // LD E,L
                t = 4;
                cpu_next.pc += 1;
                cpu_next.e = cpu.l;
                break;
            case 0x5E: // LD E,[HL];
                t = 8;
                cpu_next.pc += 1;
                cpu_next.e = mem_read(reg_hl_read());
                break;
            case 0x5F: // LD E,A
                t = 4;
                cpu_next.pc += 1;
                cpu_next.e = cpu.a;
                break;
            case 0x60: // LD H,B
                t = 4;
                cpu_next.pc += 1;
                cpu_next.h = cpu.b;
                break;
            case 0x61: // LD H,C
                t = 4;
                cpu_next.pc += 1;
                cpu_next.h = cpu.c;
                break;
            case 0x62: // LD H,D
                t = 4;
                cpu_next.pc += 1;
                cpu_next.h = cpu.d;
                break;
            case 0x63: // LD H,E
                t = 4;
                cpu_next.pc += 1;
                cpu_next.h = cpu.e;
                break;
            case 0x64: // LD H,H
                t = 4;
                cpu_next.pc += 1;
                cpu_next.h = cpu.h;
                break;
            case 0x65: // LD H,L
                t = 4;
                cpu_next.pc += 1;
                cpu_next.h = cpu.l;
                break;
            case 0x66: // LD H,[HL];
                t = 8;
                cpu_next.pc += 1;
                cpu_next.h = mem_read(reg_hl_read());
                break;
            case 0x67: // LD H,A
                t = 4;
                cpu_next.pc += 1;
                cpu_next.h = cpu.a;
                break;
            case 0x68: // LD L,B
                t = 4;
                cpu_next.pc += 1;
                cpu_next.l = cpu.b;
                break;
            case 0x69: // LD L,C
                t = 4;
                cpu_next.pc += 1;
                cpu_next.l = cpu.c;
                break;
            case 0x6A: // LD L,D
                t = 4;
                cpu_next.pc += 1;
                cpu_next.l = cpu.d;
                break;
            case 0x6B: // LD L,E
                t = 4;
                cpu_next.pc += 1;
                cpu_next.l = cpu.e;
                break;
            case 0x6C: // LD L,H
                t = 4;
                cpu_next.pc += 1;
                cpu_next.l = cpu.h;
                break;
            case 0x6D: // LD L,L
                t = 4;
                cpu_next.pc += 1;
                cpu_next.l = cpu.l;
                break;
            case 0x6E: // LD L,[HL];
                t = 8;
                cpu_next.pc += 1;
                cpu_next.l = mem_read(reg_hl_read());
                break;
            case 0x6F: // LD L,A
                t = 4;
                cpu_next.pc += 1;
                cpu_next.l = cpu.a;
                break;
            case 0x70: // LD [HL],B
                t = 8;
                cpu_next.pc += 1;
                mem_write_next(reg_hl_read(), cpu.b);
                break;
            case 0x71: // LD [HL],C
                t = 8;
                cpu_next.pc += 1;
                mem_write_next(reg_hl_read(), cpu.c);
                break;
            case 0x72: // LD [HL],D
                t = 8;
                cpu_next.pc += 1;
                mem_write_next(reg_hl_read(), cpu.d);
                break;
            case 0x73: // LD [HL],E
                t = 8;
                cpu_next.pc += 1;
                mem_write_next(reg_hl_read(), cpu.e);
                break;
            case 0x74: // LD [HL],H
                t = 8;
                cpu_next.pc += 1;
                mem_write_next(reg_hl_read(), cpu.h);
                break;
            case 0x75: // LD [HL],L
                t = 8;
                cpu_next.pc += 1;
                mem_write_next(reg_hl_read(), cpu.l);
                break;
            case 0x76: // HALT
                t = 4;
                cpu_next.pc += 1;
                cpu_next.halt = 1;
                break;
            case 0x77: // LD [HL],A
                t = 8;
                cpu_next.pc += 1;
                mem_write_next(reg_hl_read(), cpu.a);
                break;
            case 0x78: // LD A,B
                t = 4;
                cpu_next.pc += 1;
                cpu_next.a = cpu.b;
                break;
            case 0x79: // LD A,C
                t = 4;
                cpu_next.pc += 1;
                cpu_next.a = cpu.c;
                break;
            case 0x7A: // LD A,D
                t = 4;
                cpu_next.pc += 1;
                cpu_next.a = cpu.d;
                break;
            case 0x7B: // LD A,E
                t = 4;
                cpu_next.pc += 1;
                cpu_next.a = cpu.e;
                break;
            case 0x7C: // LD A,H
                t = 4;
                cpu_next.pc += 1;
                cpu_next.a = cpu.h;
                break;
            case 0x7D: // LD A,L
                t = 4;
                cpu_next.pc += 1;
                cpu_next.a = cpu.l;
                break;
            case 0x7E: // LD A,[HL];
                t = 8;
                cpu_next.pc += 1;
                cpu_next.a = mem_read(reg_hl_read());
                break;
            case 0x7F: // LD A,A
                t = 4;
                cpu_next.pc += 1;
                cpu_next.a = cpu.a;
                break;
            case 0x80: // ADD A,B
                t = 4;
                cpu_next.pc += 1;
                add_a(&cpu.b);
                break;
            case 0x81: // ADD A,C
                t = 4;
                cpu_next.pc += 1;
                add_a(&cpu.c);
                break;
            case 0x82: // ADD A,D
                t = 4;
                cpu_next.pc += 1;
                add_a(&cpu.d);
                break;
            case 0x83: // ADD A,E
                t = 4;
                cpu_next.pc += 1;
                add_a(&cpu.e);
                break;
            case 0x84: // ADD A,H
                t = 4;
                cpu_next.pc += 1;
                add_a(&cpu.h);
                break;
            case 0x85: // ADD A,L
                t = 4;
                cpu_next.pc += 1;
                add_a(&cpu.l);
                break;
            case 0x86: // ADD A,[HL]
                t = 8;
                cpu_next.pc += 1;
                n8 = mem_read(reg_hl_read());
                add_a(&n8);
                break;
            case 0x87: // ADD A,A
                t = 4;
                cpu_next.pc += 1;
                add_a(&cpu.a);
                break;
            case 0x88: // ADC A,B
                t = 4;
                cpu_next.pc += 1;
                adc_a(&cpu.b);
                break;
            case 0x89: // ADC A,C
                t = 4;
                cpu_next.pc += 1;
                adc_a(&cpu.c);
                break;
            case 0x8A: // ADC A,D
                t = 4;
                cpu_next.pc += 1;
                adc_a(&cpu.d);
                break;
            case 0x8B: // ADC A,E
                t = 4;
                cpu_next.pc += 1;
                adc_a(&cpu.e);
                break;
            case 0x8C: // ADC A,H
                t = 4;
                cpu_next.pc += 1;
                adc_a(&cpu.h);
                break;
            case 0x8D: // ADC A,L
                t = 4;
                cpu_next.pc += 1;
                adc_a(&cpu.l);
                break;
            case 0x8E: // ADC A,[HL]
                t = 8;
                cpu_next.pc += 1;
                n8 = mem_read(reg_hl_read());
                adc_a(&n8);
                break;
            case 0x8F: // ADC A,A
                t = 4;
                cpu_next.pc += 1;
                adc_a(&cpu.a);
                break;
            case 0x90: // SUB A,B
                t = 4;
                cpu_next.pc += 1;
                sub_a(&cpu.b);
                break;
            case 0x91: // SUB A,C
                t = 4;
                cpu_next.pc += 1;
                sub_a(&cpu.c);
                break;
            case 0x92: // SUB A,D
                t = 4;
                cpu_next.pc += 1;
                sub_a(&cpu.d);
                break;
            case 0x93: // SUB A,E
                t = 4;
                cpu_next.pc += 1;
                sub_a(&cpu.e);
                break;
            case 0x94: // SUB A,H
                t = 4;
                cpu_next.pc += 1;
                sub_a(&cpu.h);
                break;
            case 0x95: // SUB A,L
                t = 4;
                cpu_next.pc += 1;
                sub_a(&cpu.l);
                break;
            case 0x96: // SUB A,[HL]
                t = 8;
                cpu_next.pc += 1;
                n8 = mem_read(reg_hl_read());
                sub_a(&n8);
                break;
            case 0x97: // SUB A,A
                t = 4;
                cpu_next.pc += 1;
                sub_a(&cpu.a);
                break;
            case 0x98: // SBC A,B
                t = 4;
                cpu_next.pc += 1;
                sbc_a(&cpu.b);
                break;
            case 0x99: // SBC A,C
                t = 4;
                cpu_next.pc += 1;
                sbc_a(&cpu.c);
                break;
            case 0x9A: // SBC A,D
                t = 4;
                cpu_next.pc += 1;
                sbc_a(&cpu.d);
                break;
            case 0x9B: // SBC A,E
                t = 4;
                cpu_next.pc += 1;
                sbc_a(&cpu.e);
                break;
            case 0x9C: // SBC A,H
                t = 4;
                cpu_next.pc += 1;
                sbc_a(&cpu.h);
                break;
            case 0x9D: // SBC A,L
                t = 4;
                cpu_next.pc += 1;
                sbc_a(&cpu.l);
                break;
            case 0x9E: // SBC A,[HL]
                t = 8;
                cpu_next.pc += 1;
                n8 = mem_read(reg_hl_read());
                sbc_a(&n8);
                break;
            case 0x9F: // SBC A,A
                t = 4;
                cpu_next.pc += 1;
                sbc_a(&cpu.a);
                break;
            case 0xA0: // AND A,B
                t = 4;
                cpu_next.pc += 1;
                and_a(&cpu.b);
                break;
            case 0xA1: // AND A,C
                t = 4;
                cpu_next.pc += 1;
                and_a(&cpu.c);
                break;
            case 0xA2: // AND A,D
                t = 4;
                cpu_next.pc += 1;
                and_a(&cpu.d);
                break;
            case 0xA3: // AND A,E
                t = 4;
                cpu_next.pc += 1;
                and_a(&cpu.e);
                break;
            case 0xA4: // AND A,H
                t = 4;
                cpu_next.pc += 1;
                and_a(&cpu.h);
                break;
            case 0xA5: // AND A,L
                t = 4;
                cpu_next.pc += 1;
                and_a(&cpu.l);
                break;
            case 0xA6: // AND A,[HL]
                t = 8;
                cpu_next.pc += 1;
                n8 = mem_read(reg_hl_read());
                and_a(&n8);
                break;
            case 0xA7: // AND A,A
                t = 4;
                cpu_next.pc += 1;
                and_a(&cpu.a);
                break;
            case 0xA8: // XOR A,B
                t = 4;
                cpu_next.pc += 1;
                xor_a(&cpu.b);
                break;
            case 0xA9: // XOR A,C
                t = 4;
                cpu_next.pc += 1;
                xor_a(&cpu.c);
                break;
            case 0xAA: // XOR A,D
                t = 4;
                cpu_next.pc += 1;
                xor_a(&cpu.d);
                break;
            case 0xAB: // XOR A,E
                t = 4;
                cpu_next.pc += 1;
                xor_a(&cpu.e);
                break;
            case 0xAC: // XOR A,H
                t = 4;
                cpu_next.pc += 1;
                xor_a(&cpu.h);
                break;
            case 0xAD: // XOR A,L
                t = 4;
                cpu_next.pc += 1;
                xor_a(&cpu.l);
                break;
            case 0xAE: // XOR A,[HL]
                t = 8;
                cpu_next.pc += 1;
                n8 = mem_read(reg_hl_read());
                xor_a(&n8);
                break;
            case 0xAF: // XOR A,A
                t = 4;
                cpu_next.pc += 1;
                xor_a(&cpu.a);
                break;
            case 0xB0: // OR A,B
                t = 4;
                cpu_next.pc += 1;
                or_a(&cpu.b);
                break;
            case 0xB1: // OR A,C
                t = 4;
                cpu_next.pc += 1;
                or_a(&cpu.c);
                break;
            case 0xB2: // OR A,D
                t = 4;
                cpu_next.pc += 1;
                or_a(&cpu.d);
                break;
            case 0xB3: // OR A,E
                t = 4;
                cpu_next.pc += 1;
                or_a(&cpu.e);
                break;
            case 0xB4: // OR A,H
                t = 4;
                cpu_next.pc += 1;
                or_a(&cpu.h);
                break;
            case 0xB5: // OR A,L
                t = 4;
                cpu_next.pc += 1;
                or_a(&cpu.l);
                break;
            case 0xB6: // OR A,[HL]
                t = 8;
                cpu_next.pc += 1;
                n8 = mem_read(reg_hl_read());
                or_a(&n8);
                break;
            case 0xB7: // OR A,A
                t = 4;
                cpu_next.pc += 1;
                or_a(&cpu.a);
                break;
            case 0xB8: // CP A,B
                t = 4;
                cpu_next.pc += 1;
                cp_a(&cpu.b);
                break;
            case 0xB9: // CP A,C
                t = 4;
                cpu_next.pc += 1;
                cp_a(&cpu.c);
                break;
            case 0xBA: // CP A,D
                t = 4;
                cpu_next.pc += 1;
                cp_a(&cpu.d);
                break;
            case 0xBB: // CP A,E
                t = 4;
                cpu_next.pc += 1;
                cp_a(&cpu.e);
                break;
            case 0xBC: // CP A,H
                t = 4;
                cpu_next.pc += 1;
                cp_a(&cpu.h);
                break;
            case 0xBD: // CP A,L
                t = 4;
                cpu_next.pc += 1;
                cp_a(&cpu.l);
                break;
            case 0xBE: // CP A,[HL]
                t = 8;
                cpu_next.pc += 1;
                n8 = mem_read(reg_hl_read());
                cp_a(&n8);
                break;
            case 0xBF: // CP A,A
                t = 4;
                cpu_next.pc += 1;
                cp_a(&cpu.a);
                break;
            case 0xC0: // RET NZ
                if (flag_get_z()) { // Not taken
                    t = 8;
                    cpu_next.pc += 1;
                } else { // Taken
                    t = 20;
                    cpu_next.pc = mem_read16(cpu.sp);
                    cpu_next.sp += 2;
                }
                break;
            case 0xC1: // POP BC
                t = 12;
                cpu_next.pc += 1;
                reg_bc_write(mem_read16(cpu.sp));
                cpu_next.sp += 2;
                break;
            case 0xC2: // JP NZ,a16
                if (flag_get_z()) { // Not taken
                    t = 12;
                    cpu_next.pc += 3;
                } else {
                    t = 16;
                    cpu_next.pc = mem_read16(cpu.pc+1);
                }
                break;
            case 0xC3: // JP a16
                t = 16;
                cpu_next.pc = mem_read16(cpu.pc+1);
                break;
            case 0xC4: // CALL NZ,a16
                if (flag_get_z()) { // Not taken
                    t = 12;
                    cpu_next.pc += 3;
                } else {
                    t = 24;
                    cpu_next.sp -= 2;
                    mem_write_next16(cpu_next.sp, cpu.pc+3);
                    cpu_next.pc = mem_read16(cpu.pc+1);
                }
                break;
            case 0xC5: // PUSH BC
                t = 16;
                cpu_next.pc += 1;
                cpu_next.sp -= 2;
                mem_write_next16(cpu_next.sp, reg_bc_read());
                break;
            case 0xC6: // ADD A,n8
                t = 8;
                cpu_next.pc += 2;
                n8 = mem_read(cpu.pc+1);
                cpu_next.a += n8;
                flag_set_z(cpu_next.a);
                flag_set_n(0);
                flag_set_h(((cpu.a & 0x0F) + (n8 & 0x0F)) > 0x0F);
                flag_set_c(cpu_next.a < cpu.a);
                break;
            case 0xC7: // RST $00
                t = 16;
                cpu_next.sp -= 2;
                mem_write_next16(cpu_next.sp, cpu.pc+1);
                cpu_next.pc = 0x0000;
                break;
            case 0xC8: // RET Z
                if (!flag_get_z()) { // Not taken
                    t = 8;
                    cpu_next.pc += 1;
                } else { // Taken
                    t = 20;
                    cpu_next.pc = mem_read16(cpu.sp);
                    cpu_next.sp += 2;
                }
                break;
            case 0xC9: // RET
                t = 16;
                cpu_next.pc = mem_read16(cpu.sp);
                cpu_next.sp += 2;
                break;
            case 0xCA: // JP Z,a16
                if (!flag_get_z()) { // Not taken
                    t = 12;
                    cpu_next.pc += 3;
                } else {
                    t = 16;
                    cpu_next.pc = mem_read16(cpu.pc+1);
                }
                break;
            case 0xCB: // PREFIX
                cpu_next.pc += 2;
                if (execute_prefix(mem_read(cpu.pc+1))) {
                    t = 12;
                } else {
                    t = 8;
                }
                break;
            case 0xCC: // CALL Z,a16
                if (!flag_get_z()) { // Not taken
                    t = 12;
                    cpu_next.pc += 3;
                } else {
                    t = 24;
                    cpu_next.sp -= 2;
                    mem_write_next16(cpu_next.sp, cpu.pc+3);
                    cpu_next.pc = mem_read16(cpu.pc+1);
                }
                break;
            case 0xCD: // CALL a16
                t = 24;
                cpu_next.sp -= 2;
                mem_write_next16(cpu_next.sp, cpu.pc+3);
                cpu_next.pc = mem_read16(cpu.pc+1);
                break;
            case 0xCE: // ADC A,n8
                t = 8;
                cpu_next.pc += 2;
                n8 = mem_read(cpu.pc+1);
                adc_a(&n8);
                break;
            case 0xCF: // RST $08
                t = 16;
                cpu_next.sp -= 2;
                mem_write_next16(cpu_next.sp, cpu.pc+1);
                cpu_next.pc = 0x0008;
                break;
            case 0xD0: // RET NC
                if (flag_get_c()) { // Not taken
                    t = 8;
                    cpu_next.pc += 1;
                } else { // Taken
                    t = 20;
                    cpu_next.pc = mem_read16(cpu.sp);
                    cpu_next.sp += 2;
                }
                break;
            case 0xD1: // POP DE
                t = 12;
                cpu_next.pc += 1;
                reg_de_write(mem_read16(cpu.sp));
                cpu_next.sp += 2;
                break;
            case 0xD2: // JP NC,a16
                if (flag_get_c()) { // Not taken
                    t = 12;
                    cpu_next.pc += 3;
                } else {
                    t = 16;
                    cpu_next.pc = mem_read16(cpu.pc+1);
                }
                break;
            case 0xD4: // CALL NC,a16
                if (flag_get_c()) { // Not taken
                    t = 12;
                    cpu_next.pc += 3;
                } else {
                    t = 24;
                    cpu_next.sp -= 2;
                    mem_write_next16(cpu_next.sp, cpu.pc+3);
                    cpu_next.pc = mem_read16(cpu.pc+1);
                }
                break;
            case 0xD5: // PUSH DE
                t = 16;
                cpu_next.pc += 1;
                cpu_next.sp -= 2;
                mem_write_next16(cpu_next.sp, reg_de_read());
                break;
            case 0xD6: // SUB A,n8
                t = 8;
                cpu_next.pc += 2;
                n8 = mem_read(cpu.pc+1);
                sub_a(&n8);
                break;
            case 0xD7: // RST $10
                t = 16;
                cpu_next.sp -= 2;
                mem_write_next16(cpu_next.sp, cpu.pc+1);
                cpu_next.pc = 0x0010;
                break;
            case 0xD8: // RET C
                if (!flag_get_c()) { // Not taken
                    t = 8;
                    cpu_next.pc += 1;
                } else { // Taken
                    t = 20;
                    cpu_next.pc = mem_read16(cpu.sp);
                    cpu_next.sp += 2;
                }
                break;
            case 0xD9: // RETI
                t = 16;
                cpu_next.pc = mem_read16(cpu.sp);
                cpu_next.sp += 2;
                cpu_next.ime = 1;
                break;
            case 0xDA: // JP C,a16
                if (!flag_get_c()) { // Not taken
                    t = 12;
                    cpu_next.pc += 3;
                } else {
                    t = 16;
                    cpu_next.pc = mem_read16(cpu.pc+1);
                }
                break;
            case 0xDC: // CALL C,a16
                if (!flag_get_c()) { // Not taken
                    t = 12;
                    cpu_next.pc += 3;
                } else {
                    t = 24;
                    cpu_next.sp -= 2;
                    mem_write_next16(cpu_next.sp, cpu.pc+3);
                    cpu_next.pc = mem_read16(cpu.pc+1);
                }
                break;
            case 0xDE: // SBC A,n8
                t = 8;
                cpu_next.pc += 2;
                n8 = mem_read(cpu.pc+1);
                sbc_a(&n8);
                break;
            case 0xDF: // RST $18
                t = 16;
                cpu_next.sp -= 2;
                mem_write_next16(cpu_next.sp, cpu.pc+1);
                cpu_next.pc = 0x0018;
                break;
            case 0xE0: // LDH [a8],A
                t = 12;
                cpu_next.pc += 2;
                mem_write_next(0xFF00+(uint16_t)mem_read(cpu.pc+1), cpu.a);
                break;
            case 0xE1: // POP HL
                t = 12;
                cpu_next.pc += 1;
                reg_hl_write(mem_read16(cpu.sp));
                cpu_next.sp += 2;
                break;
            case 0xE2: // LDH [C],A
                t = 8;
                cpu_next.pc += 1;
                mem_write_next(0xFF00+(uint16_t)cpu.c, cpu.a);
                break;
            case 0xE5: // PUSH HL
                t = 16;
                cpu_next.pc += 1;
                cpu_next.sp -= 2;
                mem_write_next16(cpu_next.sp, reg_hl_read());
                break;
            case 0xE6: // AND A,n8
                t = 8;
                cpu_next.pc += 2;
                n8 = mem_read(cpu.pc + 1);
                and_a(&n8);
                break;
            case 0xE7: // RST $20
                t = 16;
                cpu_next.sp -= 2;
                mem_write_next16(cpu_next.sp, cpu.pc+1);
                cpu_next.pc = 0x0020;
                break;
            case 0xE8: // ADD SP,e8
                t = 16;
                cpu_next.pc += 2;
                n8 = mem_read(cpu.pc+1);
                cpu_next.sp += (int8_t)n8;
                flag_set_z(1);
                flag_set_n(0);
                flag_set_h(((cpu.sp & 0x0F) + (n8 & 0x0F)) > 0x0F);
                flag_set_c(((cpu.sp & 0xFF) + n8) > 0xFF);
                break;
            case 0xE9: // JP HL
                t = 4;
                cpu_next.pc = reg_hl_read();
                break;
            case 0xEA: // LD [a16],A
                t = 16;
                cpu_next.pc += 3;
                mem_write_next(mem_read16(cpu.pc+1), cpu.a);
                break;
            case 0xEE: // XOR A,n8
                t = 8;
                cpu_next.pc += 2;
                n8 = mem_read(cpu.pc+1);
                xor_a(&n8);
                break;
            case 0xEF: // RST $28
                t = 16;
                cpu_next.sp -= 2;
                mem_write_next16(cpu_next.sp, cpu.pc+1);
                cpu_next.pc = 0x0028;
                break;
            case 0xF0: // LDH A,[a8]
                t = 12;
                cpu_next.pc += 2;
                cpu_next.a = mem_read(0xFF00+(uint16_t)mem_read(cpu.pc+1));
                break;
            case 0xF1: // POP AF
                t = 12;
                cpu_next.pc += 1;
                reg_af_write(mem_read16(cpu.sp) & 0xFFF0);
                cpu_next.sp += 2;
                break;
            case 0xF2: // LDH A,[C]
                t = 8;
                cpu_next.pc += 1;
                cpu_next.a = mem_read(0xFF00+(uint16_t)cpu.c);
                break;
            case 0xF3: // DI
                t = 4;
                cpu_next.pc += 1;
                cpu_next.ime = 0;
                break;
            case 0xF5: // PUSH AF
                t = 16;
                cpu_next.pc += 1;
                cpu_next.sp -= 2;
                mem_write_next16(cpu_next.sp, reg_af_read() & 0xFFF0);
                break;
            case 0xF6: // OR A,n8
                t = 8;
                cpu_next.pc += 2;
                n8 = mem_read(cpu.pc + 1);
                or_a(&n8);
                break;
            case 0xF7: // RST $30
                t = 16;
                cpu_next.sp -= 2;
                mem_write_next16(cpu_next.sp, cpu.pc+1);
                cpu_next.pc = 0x0030;
                break;
            case 0xF8: // LD HL,SP+e8
                t = 12;
                cpu_next.pc += 2;
                n8 = mem_read(cpu.pc+1);
                reg_hl_write(cpu.sp + (int8_t)n8);
                flag_set_z(1);
                flag_set_n(0);
                flag_set_h(((cpu.sp & 0x0F) + (n8 & 0x0F)) > 0x0F);
                flag_set_c(((cpu.sp & 0xFF) + n8) > 0xFF);
                break;
            case 0xF9: // LD SP,HL
                t = 8;
                cpu_next.pc += 1;
                cpu_next.sp = reg_hl_read();
                break;
            case 0xFA: // LD A,[a16]
                t = 16;
                cpu_next.pc += 3;
                cpu_next.a = mem_read(mem_read16(cpu.pc+1));
                break;
            case 0xFB: // EI
                t = 4;
                cpu_next.pc += 1;
                cpu_next.ime_pending = 1;
                break;
            case 0xFE: // CP A,n8
                t = 8;
                cpu_next.pc += 2;
                n8 = mem_read(cpu.pc+1);
                result = cpu.a - n8;
                flag_set_z(result);
                flag_set_n(1);
                flag_set_h((n8 & 0x0F) > (cpu.a & 0x0F));
                flag_set_c(n8 > cpu.a);
                break;
            case 0xFF: // RST $38
                t = 16;
                cpu_next.sp -= 2;
                mem_write_next16(cpu_next.sp, cpu.pc+1);
                cpu_next.pc = 0x0038;
                break;
            default:
                printf("Unknown OP 0x%X\n", cpu.op);
                exit(1);
        }
    } else if (interrupt) { // Interrupt triggered
        DEBUG_PRINTF_CPU("INT 0b%08b ", *iflag);

        cpu_next.halt = 0;
        cpu_next.stop = 0;
        cpu_next.ime = 0;
        t = 20;

        // Determine call address
        uint16_t iaddress;
        if (*iflag & mem.ie & INT_VBLANK) {
            iaddress = 0x0040;
            *iflag &= ~INT_VBLANK;
        } else if (*iflag & mem.ie & INT_STAT) {
            iaddress = 0x0048;
            *iflag &= ~INT_STAT;
        } else if (*iflag & mem.ie & INT_TIMER) {
            iaddress = 0x0050;
            *iflag &= ~INT_TIMER;
        } else if (*iflag & mem.ie & INT_SERIAL) {
            iaddress = 0x0058;
            *iflag &= ~INT_SERIAL;
        } else if (*iflag & mem.ie & INT_JOYPAD) {
            iaddress = 0x0060;
            *iflag &= ~INT_JOYPAD;
        } else {
            printf("Unknown INT 0x%X\n", *iflag);
            exit(1);
        }

        cpu_next.sp -= 2;
        mem_write_next16(cpu_next.sp, cpu.pc);
        cpu_next.pc = iaddress;
    } else if (cpu.halt) {
        DEBUG_PRINTF_CPU("HALTED");
        t = 4;
        cpu_next.halt = !(bool)(*iflag & mem.ie); // TODO emulate HALT bug
    } else if (cpu.stop) {
        DEBUG_PRINTF_CPU("STOPPED");
        t = 0;
    }

    DEBUG_PRINTF_CPU("AF:0x%02X%02X BC:0x%02X%02X ", cpu_next.a,cpu_next.f,cpu_next.b,cpu_next.c);
    DEBUG_PRINTF_CPU("DE:0x%02X%02X HL:0x%02X%02X ",cpu_next.d,cpu_next.e,cpu_next.h,cpu_next.l);
    DEBUG_PRINTF_CPU("SP:0x%04X\n",cpu_next.sp);

    return t;
}

void cpu_writeback() {
    // Commit mutated state
    cpu = cpu_next;
    if (cpu.ime_pending) {
        cpu_next.ime = 1;
        cpu_next.ime_pending = 0;
    }
    mem_writeback();
}
