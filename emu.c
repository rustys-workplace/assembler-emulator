/*
 * ==============================================================================
 * IITPSx Architectural Emulator (emu.c)
 * 
 * Author: ARNAV RASTOGI
 * User ID: ARNAV RASTOGI
 * Declaration of Authorship:
 * I hereby declare that this codebase is entirely my own original work,
 * written in strict ISO C89 compliance (-std=c89 -pedantic) for the
 * CS2102 IITPSx Processor project.
 * ==============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Ensure 32-bit integer types under ISO C89.
 * Under standard ILP32 and LP64 systems (x86 and x86_64), 'int' is 32 bits wide.
 */
typedef int iit_int32;
typedef unsigned int iit_uint32;

/*
 * Compile-time assertion ensuring 32-bit word width in ISO C89.
 * If sizeof(iit_int32) != 4, array dimension is -1, causing a build failure.
 */
typedef char check_int32_size[sizeof(iit_int32) == 4 ? 1 : -1];

/*
 * Simulated memory capacity:
 * 262,144 32-bit words corresponds to exactly 1 Megabyte of simulated RAM.
 * This comfortably exceeds the 64K-word minimum requirement.
 */
#define MEM_SIZE            262144
#define DEFAULT_MAX_CYCLES  10000000UL
#define MAX_PATH_LEN        512

/*
 * Simulated Machine State:
 * - memory: 32-bit word-addressable memory array.
 * - memory_written: byte array tracking addresses written to (for memory dumps).
 */
static iit_int32 memory[MEM_SIZE];
static unsigned char memory_written[MEM_SIZE];

/*
 * Simulated Architectural Registers:
 * - reg_A: Primary accumulator / top of internal evaluation stack.
 * - reg_B: Secondary accumulator / second element of internal evaluation stack.
 * - reg_PC: Program Counter (points to current instruction address in 32-bit words).
 * - reg_SP: Stack Pointer (points to current stack top in 32-bit words).
 */
static iit_int32 reg_A = 0;
static iit_int32 reg_B = 0;
static iit_int32 reg_PC = 0;
static iit_int32 reg_SP = 0;

/* Execution metrics */
static unsigned long instructions_executed = 0; /* Total instruction cycles completed */
static size_t loaded_words = 0;                 /* Number of program words loaded from binary */

/* Command-line option flags */
static int opt_trace  = 0;  /* 1: print step-by-step register states and instructions */
static int opt_dump   = 0;  /* 1: dump memory after program halts */
static int opt_before = 0;  /* 1: dump memory before execution commences */
static int opt_after  = 0;  /* 1: dump memory after execution terminates */
static int opt_read   = 0;  /* 1: log memory read accesses */
static int opt_write  = 0;  /* 1: log memory write accesses */
static int opt_wipe   = 0;  /* 1: wipe written tracking flags before execution */
static int opt_isa    = 0;  /* 1: display ISA documentation table */
static unsigned long max_cycles = DEFAULT_MAX_CYCLES; /* Maximum instruction cycle trap */

/*
 * Instruction mnemonic and operand metadata table.
 * Used for formatting disassembly in traces and documentation.
 */
typedef struct InstructionInfo {
    const char *mnemonic;  /* Canonical textual mnemonic */
    int has_operand;       /* 1 if instruction utilizes an operand, 0 otherwise */
} InstructionInfo;

/* Static ISA metadata mapping for opcodes 0 through 20 */
static const InstructionInfo ISA_INFO[21] = {
    {"ldc",    1}, /* Opcode 0:  Load constant */
    {"adc",    1}, /* Opcode 1:  Add constant */
    {"ldl",    1}, /* Opcode 2:  Load local from stack offset */
    {"stl",    1}, /* Opcode 3:  Store local to stack offset */
    {"ldnl",   1}, /* Opcode 4:  Load non-local from base pointer */
    {"stnl",   1}, /* Opcode 5:  Store non-local to base pointer */
    {"add",    0}, /* Opcode 6:  Stack addition */
    {"sub",    0}, /* Opcode 7:  Stack subtraction */
    {"shl",    0}, /* Opcode 8:  Bitwise shift left */
    {"shr",    0}, /* Opcode 9:  Arithmetic shift right */
    {"adj",    1}, /* Opcode 10: Adjust stack pointer */
    {"a2sp",   0}, /* Opcode 11: Transfer A to SP */
    {"sp2a",   0}, /* Opcode 12: Transfer SP to A */
    {"call",   1}, /* Opcode 13: Subroutine call */
    {"return", 0}, /* Opcode 14: Subroutine return */
    {"brz",    1}, /* Opcode 15: Conditional branch if zero */
    {"brlz",   1}, /* Opcode 16: Conditional branch if less than zero */
    {"br",     1}, /* Opcode 17: Unconditional relative branch */
    {"HALT",   0}, /* Opcode 18: Virtual halt instruction */
    {"mul",    0}, /* Opcode 19: Stack multiplication */
    {"div",    0}  /* Opcode 20: Signed division with zero check */
};

/*
 * Displays the complete formal Instruction Set Architecture (ISA) table.
 */
static void display_isa(void) {
    printf("Opcode Mnemonic Operand\n");
    printf("0      ldc      value\n");
    printf("1      adc      value\n");
    printf("2      ldl      offset\n");
    printf("3      stl      offset\n");
    printf("4      ldnl     offset\n");
    printf("5      stnl     offset\n");
    printf("6      add\n");
    printf("7      sub\n");
    printf("8      shl\n");
    printf("9      shr\n");
    printf("10     adj      value\n");
    printf("11     a2sp\n");
    printf("12     sp2a\n");
    printf("13     call     offset\n");
    printf("14     return\n");
    printf("15     brz      offset\n");
    printf("16     brlz     offset\n");
    printf("17     br       offset\n");
    printf("18     HALT\n");
    printf("19     mul\n");
    printf("20     div\n");
    printf("       SET      value\n");
}

/*
 * Prints current register states and total execution cycle count.
 */
static void dump_registers(FILE *out) {
    fprintf(out, "Register State:\n");
    fprintf(out, "  PC = 0x%08X (%d)\n", (unsigned int)reg_PC, reg_PC);
    fprintf(out, "  SP = 0x%08X (%d)\n", (unsigned int)reg_SP, reg_SP);
    fprintf(out, "  A  = 0x%08X (%d)\n", (unsigned int)reg_A, reg_A);
    fprintf(out, "  B  = 0x%08X (%d)\n", (unsigned int)reg_B, reg_B);
    fprintf(out, "  Instructions executed: %lu\n", instructions_executed);
}

/*
 * Dumps memory contents in rows of 4 words (16 bytes per line).
 * Automatically compacts output by skipping blocks that remain entirely zero.
 */
static void dump_memory(FILE *out, const char *title) {
    size_t i;
    size_t max_addr = loaded_words;

    /* Scan memory to determine the highest non-zero or written word address */
    for (i = 0; i < MEM_SIZE; i++) {
        if (memory[i] != 0 || memory_written[i]) {
            if (i >= max_addr) {
                max_addr = i + 1;
            }
        }
    }

    /* Ensure a minimal window is displayed */
    if (max_addr == 0) {
        max_addr = 16;
    }

    /* Align max_addr upward to the nearest multiple of 4 words */
    max_addr = (max_addr + 3) & ~3UL;
    if (max_addr > MEM_SIZE) {
        max_addr = MEM_SIZE;
    }

    fprintf(out, "%s\n", title);

    /* Iterate through memory in chunks of 4 words */
    for (i = 0; i < max_addr; i += 4) {
        int non_zero = 0;
        size_t j;

        /* Check if any of the 4 words are non-zero or were written to */
        for (j = 0; j < 4 && (i + j) < MEM_SIZE; j++) {
            if (memory[i + j] != 0 || memory_written[i + j]) {
                non_zero = 1;
                break;
            }
        }

        /* Emit the row if non-zero or part of the initial loaded program */
        if (non_zero || i < loaded_words) {
            fprintf(out, "%08X %08X %08X %08X %08X \n",
                    (unsigned int)i,
                    (unsigned int)memory[i],
                    (unsigned int)((i + 1 < MEM_SIZE) ? memory[i + 1] : 0),
                    (unsigned int)((i + 2 < MEM_SIZE) ? memory[i + 2] : 0),
                    (unsigned int)((i + 3 < MEM_SIZE) ? memory[i + 3] : 0));
        }
    }
}

/*
 * Loads a binary machine code file into simulated memory starting at address 0.
 * Decodes 4 raw bytes per word assuming strict little-endian byte ordering.
 */
static int load_binary(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    unsigned char buf[4];
    size_t addr = 0;

    if (fp == NULL) {
        fprintf(stderr, "Error: cannot open binary file '%s'\n", filename);
        return 0;
    }

    /* Initialize memory space and written flags to zero */
    memset(memory, 0, sizeof(memory));
    memset(memory_written, 0, sizeof(memory_written));

    /* Read 4-byte chunks until EOF */
    while (fread(buf, 1, 4, fp) == 4) {
        if (addr >= MEM_SIZE) {
            fprintf(stderr, "Warning: program exceeds simulated memory size (%d words)\n", MEM_SIZE);
            break;
        }

        /* Assemble 32-bit integer word from little-endian bytes */
        memory[addr] = (iit_int32)((iit_uint32)buf[0] |
                                  ((iit_uint32)buf[1] << 8) |
                                  ((iit_uint32)buf[2] << 16) |
                                  ((iit_uint32)buf[3] << 24));
        addr++;
    }

    fclose(fp);
    loaded_words = addr;
    return 1;
}

/*
 * Formats and outputs an execution trace record.
 * Displays: PC, SP, A, B, mnemonic, and operand (if applicable).
 */
static void print_trace_line(FILE *out, iit_int32 pc, int opcode, iit_int32 operand) {
    if (opcode >= 0 && opcode <= 20) {
        if (ISA_INFO[opcode].has_operand) {
            fprintf(out, "PC=%08X, SP=%08X, A=%08X, B=%08X %s %08X \n",
                    (unsigned int)pc, (unsigned int)reg_SP,
                    (unsigned int)reg_A, (unsigned int)reg_B,
                    ISA_INFO[opcode].mnemonic, (unsigned int)operand);
        } else {
            fprintf(out, "PC=%08X, SP=%08X, A=%08X, B=%08X %s \n",
                    (unsigned int)pc, (unsigned int)reg_SP,
                    (unsigned int)reg_A, (unsigned int)reg_B,
                    ISA_INFO[opcode].mnemonic);
        }
    } else {
        fprintf(out, "PC=%08X, SP=%08X, A=%08X, B=%08X <unknown %d> \n",
                (unsigned int)pc, (unsigned int)reg_SP,
                (unsigned int)reg_A, (unsigned int)reg_B,
                opcode);
    }
}

/*
 * Core Execution Loop.
 * Fetches, decodes, and executes instructions until HALT or an unhandled fault.
 */
static int run_emulator(FILE *log_fp) {
    int halted = 0;

    /* Wipe written flags if -wipe option is set */
    if (opt_wipe) {
        memset(memory_written, 0, sizeof(memory_written));
    }

    /* Perform pre-execution memory dump if requested */
    if (opt_before) {
        dump_memory(stdout, "Memory dump before execution");
        if (log_fp != NULL) {
            dump_memory(log_fp, "Memory dump before execution");
        }
    }

    /* Main CPU fetch-decode-execute cycle */
    while (!halted) {
        iit_int32 current_pc;
        iit_uint32 inst;
        int opcode;
        iit_int32 operand;
        iit_int32 eff_addr;

        /*
         * Fetch stage bounds checking:
         * Verify that PC points to a valid memory location [0, MEM_SIZE).
         */
        if (reg_PC < 0 || (size_t)reg_PC >= MEM_SIZE) {
            fprintf(stderr, "Runtime Error: PC 0x%08X out of bounds [0, %d)\n", (unsigned int)reg_PC, MEM_SIZE);
            dump_registers(stderr);
            if (log_fp != NULL) {
                fprintf(log_fp, "Runtime Error: PC 0x%08X out of bounds\n", (unsigned int)reg_PC);
                dump_registers(log_fp);
            }
            return 1;
        }

        /* Fetch 32-bit instruction word */
        current_pc = reg_PC;
        inst = (iit_uint32)memory[current_pc];

        /*
         * Implicit PC pre-increment:
         * The PC is automatically incremented to point to the next instruction
         * BEFORE the semantics of the current instruction are executed.
         */
        reg_PC = reg_PC + 1;

        /*
         * Instruction decoding:
         * Bits [7:0]:   8-bit opcode.
         * Bits [31:8]: 24-bit operand, sign-extended to 32 bits.
         */
        opcode = (int)(inst & 0xFF);
        operand = (iit_int32)(inst >> 8);
        if (operand & 0x00800000) {
            /* Bit 23 is set: sign-extend negative value with leading 1s */
            operand |= (iit_int32)0xFF000000;
        } else {
            /* Bit 23 is clear: zero-extend positive value */
            operand &= (iit_int32)0x007FFFFF;
        }

        /* Emit execution trace if -trace option was enabled */
        if (opt_trace) {
            print_trace_line(stdout, current_pc, opcode, operand);
            if (log_fp != NULL) {
                print_trace_line(log_fp, current_pc, opcode, operand);
            }
        }

        /*
         * Execution of instruction semantics according to formal ISA specification.
         */
        switch (opcode) {
            case 0: /* ldc value: B = A; A = value; */
                reg_B = reg_A;
                reg_A = operand;
                break;

            case 1: /* adc value: A = A + value; */
                reg_A = reg_A + operand;
                break;

            case 2: /* ldl offset: B = A; A = memory[SP + offset]; */
                eff_addr = reg_SP + operand;
                if (eff_addr < 0 || (size_t)eff_addr >= MEM_SIZE) {
                    fprintf(stderr, "Runtime Error: Memory read out of bounds at 0x%08X (PC=0x%08X)\n",
                            (unsigned int)eff_addr, (unsigned int)current_pc);
                    dump_registers(stderr);
                    return 1;
                }
                if (opt_read) {
                    printf("Memory read [SP%+d] (0x%08X) = 0x%08X\n", operand, (unsigned int)eff_addr, (unsigned int)memory[eff_addr]);
                }
                reg_B = reg_A;
                reg_A = memory[eff_addr];
                break;

            case 3: /* stl offset: memory[SP + offset] = A; A = B; */
                eff_addr = reg_SP + operand;
                if (eff_addr < 0 || (size_t)eff_addr >= MEM_SIZE) {
                    fprintf(stderr, "Runtime Error: Memory write out of bounds at 0x%08X (PC=0x%08X)\n",
                            (unsigned int)eff_addr, (unsigned int)current_pc);
                    dump_registers(stderr);
                    return 1;
                }
                if (opt_write) {
                    printf("Memory write [SP%+d] (0x%08X) = 0x%08X\n", operand, (unsigned int)eff_addr, (unsigned int)reg_A);
                }
                memory[eff_addr] = reg_A;
                memory_written[eff_addr] = 1;
                reg_A = reg_B;
                break;

            case 4: /* ldnl offset: A = memory[A + offset]; */
                eff_addr = reg_A + operand;
                if (eff_addr < 0 || (size_t)eff_addr >= MEM_SIZE) {
                    fprintf(stderr, "Runtime Error: Memory read out of bounds at 0x%08X (PC=0x%08X)\n",
                            (unsigned int)eff_addr, (unsigned int)current_pc);
                    dump_registers(stderr);
                    return 1;
                }
                if (opt_read) {
                    printf("Memory read [A%+d] (0x%08X) = 0x%08X\n", operand, (unsigned int)eff_addr, (unsigned int)memory[eff_addr]);
                }
                reg_A = memory[eff_addr];
                break;

            case 5: /* stnl offset: memory[A + offset] = B; */
                eff_addr = reg_A + operand;
                if (eff_addr < 0 || (size_t)eff_addr >= MEM_SIZE) {
                    fprintf(stderr, "Runtime Error: Memory write out of bounds at 0x%08X (PC=0x%08X)\n",
                            (unsigned int)eff_addr, (unsigned int)current_pc);
                    dump_registers(stderr);
                    return 1;
                }
                if (opt_write) {
                    printf("Memory write [A%+d] (0x%08X) = 0x%08X\n", operand, (unsigned int)eff_addr, (unsigned int)reg_B);
                }
                memory[eff_addr] = reg_B;
                memory_written[eff_addr] = 1;
                break;

            case 6: /* add: A = B + A; */
                reg_A = reg_B + reg_A;
                break;

            case 7: /* sub: A = B - A; */
                reg_A = reg_B - reg_A;
                break;

            case 8: /* shl: A = B << A; */
                reg_A = reg_B << (reg_A & 0x1F);
                break;

            case 9: /* shr: A = B >> A; */
                reg_A = reg_B >> (reg_A & 0x1F);
                break;

            case 10: /* adj value: SP = SP + value; */
                reg_SP = reg_SP + operand;
                break;

            case 11: /* a2sp: SP = A; A = B; */
                reg_SP = reg_A;
                reg_A = reg_B;
                break;

            case 12: /* sp2a: B = A; A = SP; */
                reg_B = reg_A;
                reg_A = reg_SP;
                break;

            case 13: /* call offset: B = A; A = PC; PC = PC + offset; */
                reg_B = reg_A;
                reg_A = reg_PC; /* Return address is current_pc + 1 (already in reg_PC) */
                reg_PC = reg_PC + operand;
                break;

            case 14: /* return: PC = A; A = B; */
                reg_PC = reg_A;
                reg_A = reg_B;
                break;

            case 15: /* brz offset: if (A == 0) PC = PC + offset; */
                if (reg_A == 0) {
                    reg_PC = reg_PC + operand;
                }
                break;

            case 16: /* brlz offset: if (A < 0) PC = PC + offset; */
                if (reg_A < 0) {
                    reg_PC = reg_PC + operand;
                }
                break;

            case 17: /* br offset: PC = PC + offset; */
                reg_PC = reg_PC + operand;
                break;

            case 18: /* HALT: Cleanly terminate processor execution */
                halted = 1;
                break;

            case 19: /* mul: A = B * A; */
                reg_A = reg_B * reg_A;
                break;

            case 20: /* div: if (A == 0) runtime_error(); else A = B / A; */
                if (reg_A == 0) {
                    /* Division by zero fault detection */
                    fprintf(stderr, "Runtime Error: Division by zero at PC=0x%08X\n", (unsigned int)current_pc);
                    dump_registers(stderr);
                    if (log_fp != NULL) {
                        fprintf(log_fp, "Runtime Error: Division by zero at PC=0x%08X\n", (unsigned int)current_pc);
                        dump_registers(log_fp);
                    }
                    if (opt_dump || opt_after) {
                        dump_memory(stderr, "Memory dump upon division by zero");
                    }
                    return 1;
                }
                reg_A = reg_B / reg_A;
                break;

            default:
                /* Illegal or unrecognized instruction opcode */
                fprintf(stderr, "Runtime Error: Illegal opcode %d at PC=0x%08X\n", opcode, (unsigned int)current_pc);
                dump_registers(stderr);
                if (log_fp != NULL) {
                    fprintf(log_fp, "Runtime Error: Illegal opcode %d at PC=0x%08X\n", opcode, (unsigned int)current_pc);
                    dump_registers(log_fp);
                }
                return 1;
        }

        instructions_executed++;

        /* Infinite loop trap / cycle limit cap */
        if (instructions_executed >= max_cycles) {
            fprintf(stderr, "Runtime Error: Execution limit reached (%lu cycles) - possible infinite loop\n", max_cycles);
            dump_registers(stderr);
            if (log_fp != NULL) {
                fprintf(log_fp, "Runtime Error: Execution limit reached (%lu cycles)\n", max_cycles);
                dump_registers(log_fp);
            }
            return 1;
        }
    }

    /* Execution finished cleanly upon HALT */
    printf("%lu instructions executed\n", instructions_executed);
    if (log_fp != NULL) {
        fprintf(log_fp, "%lu instructions executed\n", instructions_executed);
    }

    /* Output memory dump if -dump or -after flag was specified */
    if (opt_dump || opt_after) {
        dump_memory(stdout, "Memory dump after execution");
        if (log_fp != NULL) {
            dump_memory(log_fp, "Memory dump after execution");
        }
    }

    return 0;
}

/*
 * Main Entry Point.
 * Parses CLI options, opens the binary file, initializes emulation,
 * and coordinates output generation.
 */
int main(int argc, char **argv) {
    const char *binary_file = NULL;
    char log_name[MAX_PATH_LEN];
    FILE *log_fp = NULL;
    char *dot;
    int i;
    int exit_code;

    /* Validate basic usage */
    if (argc < 2) {
        fprintf(stderr, "usage: %s [options] file.o\n", argv[0]);
        fprintf(stderr, "-trace  show instruction trace\n");
        fprintf(stderr, "-read   show memory reads\n");
        fprintf(stderr, "-write  show memory writes\n");
        fprintf(stderr, "-before show memory dump before execution\n");
        fprintf(stderr, "-after  show memory dump after execution\n");
        fprintf(stderr, "-dump   show memory dump after execution\n");
        fprintf(stderr, "-wipe   wipe written flags before execution\n");
        fprintf(stderr, "-isa    display ISA\n");
        return 1;
    }

    /* Parse command-line flags */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-trace") == 0) {
            opt_trace = 1;
        } else if (strcmp(argv[i], "-dump") == 0) {
            opt_dump = 1;
        } else if (strcmp(argv[i], "-before") == 0) {
            opt_before = 1;
        } else if (strcmp(argv[i], "-after") == 0) {
            opt_after = 1;
        } else if (strcmp(argv[i], "-read") == 0) {
            opt_read = 1;
        } else if (strcmp(argv[i], "-write") == 0) {
            opt_write = 1;
        } else if (strcmp(argv[i], "-wipe") == 0) {
            opt_wipe = 1;
        } else if (strcmp(argv[i], "-isa") == 0) {
            opt_isa = 1;
        } else if (strcmp(argv[i], "-limit") == 0 && i + 1 < argc) {
            max_cycles = (unsigned long)atol(argv[++i]);
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return 1;
        } else {
            binary_file = argv[i];
        }
    }

    /* Handle standalone -isa display flag */
    if (opt_isa) {
        display_isa();
        if (binary_file == NULL) {
            return 0;
        }
    }

    if (binary_file == NULL) {
        fprintf(stderr, "usage: %s [options] file.o\n", argv[0]);
        return 1;
    }

    /* Open execution log file (<basename>.emu.log) */
    {
        size_t len = strlen(binary_file);
        if (len >= sizeof(log_name)) {
            len = sizeof(log_name) - 1;
        }
        memcpy(log_name, binary_file, len);
        log_name[len] = '\0';
    }
    dot = strrchr(log_name, '.');
    if (dot != NULL) {
        *dot = '\0';
    }
    strcat(log_name, ".emu.log");

    log_fp = fopen(log_name, "w");

    /* Load program binary into simulated memory starting at address 0 */
    if (!load_binary(binary_file)) {
        if (log_fp != NULL) {
            fclose(log_fp);
        }
        return 1;
    }

    /* Execute the emulator */
    exit_code = run_emulator(log_fp);

    /* Close log file handle */
    if (log_fp != NULL) {
        fclose(log_fp);
    }

    return exit_code;
}
