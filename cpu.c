#include <stdint.h>
#include <stdlib.h>

#include "cpu.h"
#include "mem.h"
#include "ansi_color.h"

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
static void prefix_rl(uint8_t *reg) {
    uint8_t result = (*reg << 1) + flag_get_c();
    bool result_c = *reg >> 7;
    flag_set_z(result);
    flag_set_n(0);
    flag_set_h(0);
    flag_set_c(result_c);
    *reg = result;
}

static void prefix_swap(uint8_t *reg) {
    bool result = (*reg >> 4) + ((*reg << 4) & 0x0F);
    flag_set_z(result);
    flag_set_n(0);
    flag_set_h(0);
    flag_set_c(0);
    *reg = result;
}

static void prefix_bit(uint8_t bit, uint8_t reg) {
    bool result = (reg >> bit) & 1;
    flag_set_z(result);
    flag_set_n(0);
    flag_set_h(1);
}

// Execute prefixed instructions
void execute_prefix(uint8_t op) {
    switch(op) {
        case 0x11: // RL C
            prefix_rl(&cpu_next.c);
            break;
        case 0x37: // SWAP A
            prefix_swap(&cpu_next.a);
            break;
        case 0x7C: // BIT 7,H
            prefix_bit(7, cpu.h);
            break;
        default:
            printf("%sUnknown PREFIX OP 0x%X%s\n", ANSI_RED, op, ANSI_CLEAR);
            exit(1);
    }
}

uint8_t cpu_execute() {
    uint8_t t = 0;

    // Fetch instruction
    cpu.op = mem_read(cpu.pc);

    printf("%sPC:0x%X %sOP:0x%X%s ", ANSI_BLUE, cpu.pc, ANSI_BLUE_BOLD, cpu.op, ANSI_CLEAR);

    // Temporary variables
    uint8_t n8;
    uint8_t result;

    // Compute state mutation
    if (!cpu.ime || !mem.ie) { // No interrupt triggered
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
            case 0x08: // LD [a16],SP
                t = 20;
                cpu_next.pc += 3;
                mem_write_next16(mem_read16(cpu.pc+1), cpu.sp);
                break;
            case 0x0B: // DEC BC
                t = 8;
                cpu_next.pc += 1;
                reg_bc_write(reg_bc_read() - 1);
                break;
            case 0x0C: // INC C
                t = 4;
                cpu_next.pc += 1;
                cpu_next.c = cpu.c + 1;
                flag_set_z(cpu_next.c);
                flag_set_n(0);
                flag_set_h((cpu.c & 0x0F) == 0x0F);
                break;
            case 0x0D: // DEC C
                t = 4;
                cpu_next.pc += 1;
                cpu_next.c = cpu.c - 1;
                flag_set_z(cpu_next.c);
                flag_set_n(1);
                flag_set_h((cpu.c & 0x0F) == 0);
                break;
            case 0x0E: // LD C,n8
                t = 8;
                cpu_next.pc += 2;
                cpu_next.c = mem_read(cpu.pc+1);
                break;
            case 0x11: // LD DE,n16
                t = 12;
                cpu_next.pc += 3;
                reg_de_write(mem_read16(cpu.pc+1));
                break;
            case 0x13: // INC DE
                t = 8;
                cpu_next.pc += 1;
                reg_de_write(reg_de_read() + 1);
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
                flag_set_z(0);
                flag_set_n(0);
                flag_set_h(0);
                flag_set_c(cpu.a >> 7);
                break;
            case 0x18: // JR e8
                t = 12;
                cpu_next.pc += 2 + (int8_t)mem_read(cpu.pc+1);
                break;
            case 0x1A: // LD A,[DE];
                t = 8;
                cpu_next.pc += 1;
                cpu_next.a = mem_read(reg_de_read());
                break;
            case 0x1D: // DEC E
                t = 4;
                cpu_next.pc += 1;
                cpu_next.e = cpu.e - 1;
                flag_set_z(cpu_next.e);
                flag_set_n(1);
                flag_set_h((cpu.e & 0x0F) == 0x00);
                break;
            case 0x1E: // LD E,n8
                t = 8;
                cpu_next.pc += 2;
                cpu_next.e = mem_read(cpu.pc+1);
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
                cpu_next.h = cpu.h + 1;
                flag_set_z(cpu_next.h);
                flag_set_n(0);
                flag_set_h((cpu.h & 0x0F) == 0x0F);
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
            case 0x2A: // LD A,[HL+]
                t = 8;
                cpu_next.pc += 1;
                cpu_next.a = mem_read(reg_hl_read());
                reg_hl_write(reg_hl_read()+1);
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
                flag_set_n(0);
                flag_set_h(1);
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
            case 0x36: // LD [HL],n8
                t = 12;
                cpu_next.pc += 2;
                mem_write_next(reg_hl_read(), mem_read(cpu.pc+1));
                break;
            case 0x3D: // DEC A
                t = 4;
                cpu_next.pc += 1;
                cpu_next.a = cpu.a - 1;
                flag_set_z(cpu_next.a);
                flag_set_n(1);
                flag_set_h((cpu.a & 0x0F) == 0);
                break;
            case 0x3E: // LD A,n8
                t = 8;
                cpu_next.pc += 2;
                cpu_next.a = mem_read(cpu.pc+1);
                break;
            case 0x4F: // LD C,A
                t = 4;
                cpu_next.pc += 1;
                cpu_next.c = cpu.a;
                break;
            case 0x57: // LD D,A
                t = 4;
                cpu_next.pc += 1;
                cpu_next.d = cpu.a;
                break;
            case 0x67: // LD H,A
                t = 4;
                cpu_next.pc += 1;
                cpu_next.h = cpu.a;
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
            case 0xAF: // XOR A,A
                t = 4;
                cpu_next.pc += 1;
                cpu_next.a = cpu.a ^ cpu.a;
                flag_set_z(cpu_next.a);
                break;
            case 0x86: // ADD A,[HL]
                t = 8;
                cpu_next.pc += 1;
                n8 = mem_read(reg_hl_read());
                cpu_next.a += n8;
                flag_set_z(cpu_next.a);
                flag_set_n(0);
                flag_set_h(((cpu.a & 0x0F) + (n8 & 0x0F)) > 0x0F);
                flag_set_c(cpu_next.a < cpu.a);
                break;
            case 0x90: // SUB A,B
                t = 4;
                cpu_next.pc += 1;
                cpu_next.a -= cpu.b;
                flag_set_z(cpu_next.a);
                flag_set_n(1);
                flag_set_h((cpu.a & 0x0F) < (cpu.b & 0x0F));
                flag_set_c(cpu.b > cpu.a);
                break;
            case 0x95: // SUB A,L
                t = 4;
                cpu_next.pc += 1;
                cpu_next.a -= cpu.l;
                flag_set_z(cpu_next.a);
                flag_set_n(1);
                flag_set_h((cpu.a & 0x0F) < (cpu.l & 0x0F));
                flag_set_c(cpu.l > cpu.a);
                break;
            case 0xB1: // OR A,C
                t = 4;
                cpu_next.pc += 1;
                cpu_next.a = cpu.a | cpu.c;
                flag_set_z(cpu_next.a);
                flag_set_n(0);
                flag_set_h(0);
                flag_set_c(0);
                break;
            case 0xBE: // CP A,[HL]
                t = 8;
                cpu_next.pc += 1;
                n8 = mem_read(reg_hl_read());
                result = cpu.a - n8;
                flag_set_z(result);
                flag_set_n(1);
                flag_set_h((n8 & 0x0F) > (cpu.a & 0x0F));
                flag_set_c(n8 > cpu.a);
                break;
            case 0xC0: // RET NZ
                if (flag_get_z()) { // Not taken
                    t = 8;
                    cpu_next.pc += 1;
                } else { // Taken
                    t = 20;
                    cpu_next.pc = mem_read16(cpu.sp);
                    cpu.sp += 2;
                }
                break;
            case 0xC1: // POP BC
                t = 12;
                cpu_next.pc += 1;
                reg_bc_write(mem_read16(cpu.sp));
                cpu_next.sp += 2;
                break;
            case 0xC3: // JP a16
                t = 16;
                cpu_next.pc = mem_read16(cpu.pc+1);
                break;
            case 0xC5: // PUSH BC
                t = 16;
                cpu_next.pc += 1;
                cpu_next.sp -= 2;
                mem_write_next16(cpu_next.sp, reg_bc_read());
                break;
            case 0xC9: // RET
                t = 16;
                cpu_next.pc = mem_read16(cpu.sp);
                cpu.sp += 2;
                break;
            case 0xCB: // PREFIX
                t = 4;
                cpu_next.pc += 2;
                execute_prefix(mem_read(cpu.pc+1));
                break;
            case 0xCD: // CALL a16
                t = 24;
                cpu_next.sp -= 2;
                mem_write_next16(cpu_next.sp, cpu.pc+3);
                cpu_next.pc = mem_read16(cpu.pc+1);
                break;
            case 0xE0: // LDH [a8],A
                t = 12;
                cpu_next.pc += 2;
                mem_write_next(0xFF00+(uint16_t)mem_read(cpu.pc+1), cpu.a);
                break;
            case 0xE2: // LDH [C],A
                t = 8;
                cpu_next.pc += 1;
                mem_write_next(0xFF00+(uint16_t)cpu.c, cpu.a);
                break;
            case 0xE6: // AND A,n8
                t = 8;
                cpu_next.pc += 2;
                cpu_next.a = cpu.a & mem_read(cpu.pc + 1);
                flag_set_z(cpu_next.a);
                flag_set_n(0);
                flag_set_h(1);
                flag_set_c(0);
                break;
            case 0xEA: // LD [a16],A
                t = 16;
                cpu_next.pc += 3;
                mem_write_next(mem_read16(cpu.pc+1), cpu.a);
                break;
            case 0xF0: // LDH A,[a8]
                t = 12;
                cpu_next.pc += 2;
                cpu_next.a = mem_read(0xFF00+(uint16_t)mem_read(cpu.pc+1));
                break;
            case 0xF3: // DI
                t = 4;
                cpu_next.pc += 1;
                cpu.ime = 0;
                break;
            case 0xFB: // EI
                t = 4;
                cpu_next.pc += 1;
                cpu.ime = 1;
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
                if (cpu.pc > 0x100) {
                    exit(1);
                }
                break;
            default:
                printf("%sUnknown OP 0x%X%s\n", ANSI_RED, cpu.op, ANSI_CLEAR);
                exit(1);
        }
    } else { // Interrupt triggered
        printf("Time to write your interrupt handler!");
        exit(1);
    }

    //if (cpu.pc > 0x216) { exit(1); }

    printf("%sAF:0x%#02x%02x BC:0x%#02x%02x ", ANSI_GREEN, cpu_next.a,cpu_next.f,cpu_next.b,cpu_next.c);
    printf("DE:0x%#02x%02x HL:0x%#02x%02x SP:%#X%s\n",cpu_next.d,cpu_next.e,cpu_next.h,cpu_next.l,cpu_next.sp, ANSI_CLEAR);

    return t;
}

void cpu_writeback() {
    // Commit mutated state
    cpu = cpu_next;
    mem_writeback();
}
