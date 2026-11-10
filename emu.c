/*
 * emu.c - simple emulator for IITPSx
 * Author: ARNAV RASTOGI
 * User ID: ARNAV RASTOGI
 * Declaration of Authorship:
 * I hereby declare that this codebase is my own original work,
 * written for the CS2102 IITPSx Processor project.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEM_SIZE            262144
#define DEFAULT_MAX_CYCLES  10000000UL
#define MAX_PATH_LEN        512

/* memory: 256K words */
static int memory[MEM_SIZE];
static unsigned char memory_written[MEM_SIZE];

/* registers */
static int reg_A = 0;
static int reg_B = 0;
static int reg_PC = 0;
static int reg_SP = 0;

static unsigned long instructions_executed = 0;
static size_t loaded_words = 0;

/* options */
static int opt_trace  = 0;
static int opt_dump   = 0;
static int opt_before = 0;
static int opt_after  = 0;
static int opt_read   = 0;
static int opt_write  = 0;
static int opt_wipe   = 0;
static int opt_isa    = 0;
static unsigned long max_cycles = DEFAULT_MAX_CYCLES;

typedef struct InstructionInfo {
    const char *mnemonic;
    int has_operand;
} InstructionInfo;

static const InstructionInfo ISA_INFO[21] = {
    {"ldc", 1},
    {"adc", 1},
    {"ldl", 1},
    {"stl", 1},
    {"ldnl", 1},
    {"stnl", 1},
    {"add", 0},
    {"sub", 0},
    {"shl", 0},
    {"shr", 0},
    {"adj", 1},
    {"a2sp", 0},
    {"sp2a", 0},
    {"call", 1},
    {"return", 0},
    {"brz", 1},
    {"brlz", 1},
    {"br", 1},
    {"HALT", 0},
    {"mul", 0},
    {"div", 0}
};

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

static void dump_registers(FILE *out) {
    fprintf(out, "Register State:\n");
    fprintf(out, "  PC = 0x%08X (%d)\n", (unsigned int)reg_PC, reg_PC);
    fprintf(out, "  SP = 0x%08X (%d)\n", (unsigned int)reg_SP, reg_SP);
    fprintf(out, "  A  = 0x%08X (%d)\n", (unsigned int)reg_A, reg_A);
    fprintf(out, "  B  = 0x%08X (%d)\n", (unsigned int)reg_B, reg_B);
    fprintf(out, "  Instructions executed: %lu\n", instructions_executed);
}

static void dump_memory(FILE *out, const char *title) {
    size_t i, j;
    size_t max_addr = loaded_words;
    int non_zero;

    for (i = 0; i < MEM_SIZE; i++) {
        if (memory[i] != 0 || memory_written[i]) {
            if (i >= max_addr) {
                max_addr = i + 1;
            }
        }
    }

    if (max_addr == 0) {
        max_addr = 16;
    }

    max_addr = (max_addr + 3) & ~3UL;
    if (max_addr > MEM_SIZE) {
        max_addr = MEM_SIZE;
    }

    fprintf(out, "%s\n", title);

    for (i = 0; i < max_addr; i += 4) {
        non_zero = 0;
        for (j = 0; j < 4 && (i + j) < MEM_SIZE; j++) {
            if (memory[i + j] != 0 || memory_written[i + j]) {
                non_zero = 1;
                break;
            }
        }

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

static int load_binary(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    unsigned char buf[4];
    size_t addr = 0;

    if (fp == NULL) {
        fprintf(stderr, "Error: cannot open binary file '%s'\n", filename);
        return 0;
    }

    memset(memory, 0, sizeof(memory));
    memset(memory_written, 0, sizeof(memory_written));

    while (fread(buf, 1, 4, fp) == 4) {
        if (addr >= MEM_SIZE) {
            fprintf(stderr, "Warning: program exceeds simulated memory size (%d words)\n", MEM_SIZE);
            break;
        }

        memory[addr] = (int)((unsigned int)buf[0] |
                            ((unsigned int)buf[1] << 8) |
                            ((unsigned int)buf[2] << 16) |
                            ((unsigned int)buf[3] << 24));
        addr++;
    }

    fclose(fp);
    loaded_words = addr;
    return 1;
}

static void print_trace_line(FILE *out, int pc, int opcode, int operand) {
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

static int run_emulator(FILE *log_fp) {
    int halted = 0;
    int current_pc;
    unsigned int inst;
    int opcode;
    int operand;
    int eff_addr;

    if (opt_wipe) {
        memset(memory_written, 0, sizeof(memory_written));
    }

    if (opt_before) {
        dump_memory(stdout, "Memory dump before execution");
        if (log_fp != NULL) {
            dump_memory(log_fp, "Memory dump before execution");
        }
    }

    while (!halted) {
        if (reg_PC < 0 || (size_t)reg_PC >= MEM_SIZE) {
            fprintf(stderr, "Runtime Error: PC 0x%08X out of bounds [0, %d)\n", (unsigned int)reg_PC, MEM_SIZE);
            dump_registers(stderr);
            if (log_fp != NULL) {
                fprintf(log_fp, "Runtime Error: PC 0x%08X out of bounds\n", (unsigned int)reg_PC);
                dump_registers(log_fp);
            }
            return 1;
        }

        current_pc = reg_PC;
        inst = (unsigned int)memory[current_pc];
        reg_PC = reg_PC + 1;

        opcode = (int)(inst & 0xFF);
        operand = (int)(inst >> 8);
        if (operand & 0x00800000) {
            operand |= (int)0xFF000000;
        } else {
            operand &= 0x007FFFFF;
        }

        if (opt_trace) {
            print_trace_line(stdout, current_pc, opcode, operand);
            if (log_fp != NULL) {
                print_trace_line(log_fp, current_pc, opcode, operand);
            }
        }

        switch (opcode) {
            case 0: /* ldc */
                reg_B = reg_A;
                reg_A = operand;
                break;

            case 1: /* adc */
                reg_A = reg_A + operand;
                break;

            case 2: /* ldl */
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

            case 3: /* stl */
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

            case 4: /* ldnl */
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

            case 5: /* stnl */
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

            case 6: /* add */
                reg_A = reg_B + reg_A;
                break;

            case 7: /* sub */
                reg_A = reg_B - reg_A;
                break;

            case 8: /* shl */
                reg_A = reg_B << (reg_A & 0x1F);
                break;

            case 9: /* shr */
                reg_A = reg_B >> (reg_A & 0x1F);
                break;

            case 10: /* adj */
                reg_SP = reg_SP + operand;
                break;

            case 11: /* a2sp */
                reg_SP = reg_A;
                reg_A = reg_B;
                break;

            case 12: /* sp2a */
                reg_B = reg_A;
                reg_A = reg_SP;
                break;

            case 13: /* call */
                reg_B = reg_A;
                reg_A = reg_PC;
                reg_PC = reg_PC + operand;
                break;

            case 14: /* return */
                reg_PC = reg_A;
                reg_A = reg_B;
                break;

            case 15: /* brz */
                if (reg_A == 0) {
                    reg_PC = reg_PC + operand;
                }
                break;

            case 16: /* brlz */
                if (reg_A < 0) {
                    reg_PC = reg_PC + operand;
                }
                break;

            case 17: /* br */
                reg_PC = reg_PC + operand;
                break;

            case 18: /* HALT */
                halted = 1;
                break;

            case 19: /* mul */
                reg_A = reg_B * reg_A;
                break;

            case 20: /* div */
                if (reg_A == 0) {
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
                fprintf(stderr, "Runtime Error: Illegal opcode %d at PC=0x%08X\n", opcode, (unsigned int)current_pc);
                dump_registers(stderr);
                if (log_fp != NULL) {
                    fprintf(log_fp, "Runtime Error: Illegal opcode %d at PC=0x%08X\n", opcode, (unsigned int)current_pc);
                    dump_registers(log_fp);
                }
                return 1;
        }

        instructions_executed++;

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

    printf("%lu instructions executed\n", instructions_executed);
    if (log_fp != NULL) {
        fprintf(log_fp, "%lu instructions executed\n", instructions_executed);
    }

    if (opt_dump || opt_after) {
        dump_memory(stdout, "Memory dump after execution");
        if (log_fp != NULL) {
            dump_memory(log_fp, "Memory dump after execution");
        }
    }

    return 0;
}

int main(int argc, char **argv) {
    const char *binary_file = NULL;
    char log_name[MAX_PATH_LEN];
    FILE *log_fp = NULL;
    char *dot;
    int i;
    int exit_code;

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

    if (strlen(binary_file) >= sizeof(log_name) - 10) {
        fprintf(stderr, "Error: filename too long\n");
        return 1;
    }
    strcpy(log_name, binary_file);
    dot = strrchr(log_name, '.');
    if (dot != NULL) {
        *dot = '\0';
    }
    strcat(log_name, ".emu.log");

    log_fp = fopen(log_name, "w");

    if (!load_binary(binary_file)) {
        if (log_fp != NULL) {
            fclose(log_fp);
        }
        return 1;
    }

    exit_code = run_emulator(log_fp);

    if (log_fp != NULL) {
        fclose(log_fp);
    }

    return exit_code;
}
