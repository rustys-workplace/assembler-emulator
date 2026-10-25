/*
 * ==============================================================================
 * IITPSx Two-Pass Assembler (asm.c)
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
#include <ctype.h>

/*
 * Type definitions for 32-bit signed and unsigned integers in ISO C89.
 * Under standard ILP32 and LP64 architectures (such as x86 and x86_64),
 * 'int' is guaranteed to be 32 bits wide.
 */
typedef int iit_int32;
typedef unsigned int iit_uint32;

/*
 * Static assertion to verify at compile time that iit_int32 is exactly 4 bytes (32 bits).
 * In C89, an array of negative size triggers a compilation error if the condition is false.
 */
typedef char check_int32_size[sizeof(iit_int32) == 4 ? 1 : -1];

/* Buffer capacity and limits */
#define MAX_LINE_LEN    512   /* Maximum characters read from a single source line */
#define MAX_TEXT_LEN    1024  /* Maximum characters for diagnostic and listing buffers */
#define MAX_LABEL_LEN   64    /* Maximum allowed length for label identifiers */
#define MAX_SYMBOLS     4096  /* Capacity of the symbol table */
#define MAX_DIAGS       2048  /* Maximum diagnostics (errors and warnings) stored */
#define MAX_LISTING     8192  /* Maximum assembly listing entries */

/*
 * Operand requirement classifications for IITPSx instructions:
 * - OP_TYPE_NONE:   0 operands (e.g., add, sub, mul, div, return, HALT, a2sp, sp2a)
 * - OP_TYPE_VALUE:  1 operand constant/value (e.g., ldc, adc, adj, data, SET)
 * - OP_TYPE_OFFSET: 1 operand stack/memory offset (e.g., ldl, stl, ldnl, stnl)
 * - OP_TYPE_BRANCH: 1 operand PC-relative branch target (e.g., call, brz, brlz, br)
 */
typedef enum {
    OP_TYPE_NONE = 0,   /* Instruction takes zero operands */
    OP_TYPE_VALUE,      /* Instruction takes an immediate or absolute value */
    OP_TYPE_OFFSET,     /* Instruction takes a signed offset relative to SP or A */
    OP_TYPE_BRANCH      /* Instruction takes a signed PC-relative displacement */
} OperandType;

/*
 * Static instruction metadata structure.
 * Maps mnemonic strings to their corresponding numeric opcodes and operand types.
 */
typedef struct InstructionDef {
    const char *mnemonic;  /* Canonical textual mnemonic string */
    int opcode;            /* Opcode value (0..20, -1 for data, -2 for SET) */
    OperandType op_type;   /* Expected operand classification */
} InstructionDef;

/*
 * Static ISA lookup table covering all 21 machine instructions and pseudo-directives.
 * Sentinelled by a NULL mnemonic at the end of the array.
 */
static const InstructionDef INSTRUCTION_TABLE[] = {
    {"ldc",    0, OP_TYPE_VALUE},    /* Load constant: B = A; A = value */
    {"adc",    1, OP_TYPE_VALUE},    /* Add constant: A = A + value */
    {"ldl",    2, OP_TYPE_OFFSET},   /* Load local: B = A; A = memory[SP + offset] */
    {"stl",    3, OP_TYPE_OFFSET},   /* Store local: memory[SP + offset] = A; A = B */
    {"ldnl",   4, OP_TYPE_OFFSET},   /* Load non-local: A = memory[A + offset] */
    {"stnl",   5, OP_TYPE_OFFSET},   /* Store non-local: memory[A + offset] = B */
    {"add",    6, OP_TYPE_NONE},     /* Stack addition: A = B + A */
    {"sub",    7, OP_TYPE_NONE},     /* Stack subtraction: A = B - A */
    {"shl",    8, OP_TYPE_NONE},     /* Bitwise shift left: A = B << A */
    {"shr",    9, OP_TYPE_NONE},     /* Arithmetic shift right: A = B >> A */
    {"adj",   10, OP_TYPE_VALUE},    /* Adjust stack pointer: SP = SP + value */
    {"a2sp",  11, OP_TYPE_NONE},     /* Move accumulator to SP: SP = A; A = B */
    {"sp2a",  12, OP_TYPE_NONE},     /* Move SP to accumulator: B = A; A = SP */
    {"call",  13, OP_TYPE_BRANCH},   /* Subroutine call: B = A; A = PC; PC = PC + offset */
    {"return",14, OP_TYPE_NONE},     /* Return from call: PC = A; A = B */
    {"brz",   15, OP_TYPE_BRANCH},   /* Branch if zero: if (A == 0) PC = PC + offset */
    {"brlz",  16, OP_TYPE_BRANCH},   /* Branch if less than zero: if (A < 0) PC = PC + offset */
    {"br",    17, OP_TYPE_BRANCH},   /* Unconditional relative branch: PC = PC + offset */
    {"HALT",  18, OP_TYPE_NONE},     /* Virtual halt instruction */
    {"halt",  18, OP_TYPE_NONE},     /* Lowercase alias for HALT */
    {"mul",   19, OP_TYPE_NONE},     /* Stack multiplication: A = B * A */
    {"div",   20, OP_TYPE_NONE},     /* Stack division: A = B / A */
    {"data",  -1, OP_TYPE_VALUE},    /* Pseudo-instruction: reserve initialized memory word */
    {"SET",   -2, OP_TYPE_VALUE},    /* Pseudo-instruction: assign constant value to label */
    {"set",   -2, OP_TYPE_VALUE},    /* Lowercase alias for SET */
    {NULL,     0, OP_TYPE_NONE}      /* Sentinelling terminator */
};

/*
 * Symbol table entry structure.
 * Stores symbol identifier, resolved address/value, definition line, and usage flag.
 */
typedef struct Symbol {
    char name[MAX_LABEL_LEN]; /* Identifier name */
    iit_int32 value;          /* Memory address (PC) or SET literal value */
    int defined;              /* Flag: 1 if defined, 0 otherwise */
    int defined_line;         /* 1-indexed source line where defined */
    int used;                 /* Flag: 1 if referenced by an operand, 0 otherwise */
    int is_set;               /* Flag: 1 if defined via SET pseudo-op, 0 if memory label */
} Symbol;

/*
 * Listing record structure.
 * Retains formatted output lines for generating human-readable .lst listing files.
 */
typedef struct ListingRecord {
    iit_int32 address;              /* 32-bit memory location counter (PC) */
    iit_uint32 code;                /* 32-bit emitted machine instruction word */
    int has_code;                   /* 1 if code word was emitted, 0 if label only or SET */
    char text[MAX_TEXT_LEN];        /* Formatted address and instruction text */
    char error_text[MAX_TEXT_LEN];  /* Diagnostic error message attached to this line */
} ListingRecord;

/*
 * Complete Assembler state context.
 * Encapsulates the symbol table, diagnostics lists, emitted binary buffer, and listings.
 */
typedef struct AssemblerContext {
    Symbol symbols[MAX_SYMBOLS];         /* Symbol table storage array */
    int symbol_count;                    /* Current number of defined symbols */

    char errors[MAX_DIAGS][MAX_TEXT_LEN];   /* List of formatted error diagnostic strings */
    int error_count;                        /* Total errors encountered */

    char warnings[MAX_DIAGS][MAX_TEXT_LEN]; /* List of formatted warning diagnostic strings */
    int warning_count;                      /* Total warnings encountered */

    ListingRecord listing[MAX_LISTING];  /* Listing records table for .lst generation */
    int listing_count;                   /* Current count of listing entries */

    iit_uint32 object_code[MAX_LISTING]; /* Sequential buffer of emitted machine code words */
    int object_count;                    /* Total machine code words generated */

    iit_int32 pc;                        /* Assembler location counter (PC) in 32-bit words */
} AssemblerContext;

/*
 * Trims leading and trailing whitespace characters (spaces, tabs, newlines, returns)
 * from a mutable string in place, returning a pointer to the first non-space character.
 */
static char *trim_whitespace(char *str) {
    char *end;

    /* Advance pointer past all leading whitespace characters */
    while (*str && isspace((unsigned char)*str)) {
        str++;
    }

    /* If string is empty or completely whitespace, return immediately */
    if (*str == '\0') {
        return str;
    }

    /* Locate the end of the string and back up over trailing whitespace */
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }

    return str;
}

/*
 * Safe string copy utility function conforming to ISO C89.
 * Ensures the destination buffer is always null-terminated and prevents
 * truncation warnings from GCC compiler heuristics.
 */
static void safe_strcpy(char *dest, const char *src, size_t dest_size) {
    size_t len;

    /* If destination size is 0, no writing can occur */
    if (dest_size == 0) {
        return;
    }

    /* Compute source length and cap at dest_size - 1 */
    len = strlen(src);
    if (len >= dest_size) {
        len = dest_size - 1;
    }

    /* Copy bounded characters and write null terminator */
    memcpy(dest, src, len);
    dest[len] = '\0';
}

/*
 * Case-insensitive string comparison conforming strictly to ISO C89.
 * Compares two strings character-by-character using tolower().
 */
static int str_case_cmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        if (tolower((unsigned char)*s1) != tolower((unsigned char)*s2)) {
            return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
        }
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

/*
 * Appends a formatted error message to the assembler context.
 * Performs duplicate checking to avoid redundant reports for the same line.
 */
static void add_error(AssemblerContext *ctx, int line, const char *msg) {
    char buf[MAX_TEXT_LEN];
    int i;

    /* Format error string with line number */
    sprintf(buf, "ERR: Line %d: %s", line, msg);

    /* Check if an identical error message was already logged */
    for (i = 0; i < ctx->error_count; i++) {
        if (strcmp(ctx->errors[i], buf) == 0) {
            return;
        }
    }

    /* Append error string to diagnostic list */
    if (ctx->error_count < MAX_DIAGS) {
        safe_strcpy(ctx->errors[ctx->error_count++], buf, MAX_TEXT_LEN);
    }
}

/*
 * Appends a warning message to the assembler context.
 * Performs duplicate checking to ensure warnings are unique.
 */
static void add_warning(AssemblerContext *ctx, const char *msg) {
    int i;

    /* Check for duplicate warning */
    for (i = 0; i < ctx->warning_count; i++) {
        if (strcmp(ctx->warnings[i], msg) == 0) {
            return;
        }
    }

    /* Append warning string to diagnostic list */
    if (ctx->warning_count < MAX_DIAGS) {
        safe_strcpy(ctx->warnings[ctx->warning_count], msg, MAX_TEXT_LEN);
        ctx->warning_count++;
    }
}

/*
 * Searches the symbol table for a symbol with the specified identifier name.
 * Returns pointer to the Symbol entry if found, NULL otherwise.
 */
static Symbol *find_symbol(AssemblerContext *ctx, const char *name) {
    int i;
    for (i = 0; i < ctx->symbol_count; i++) {
        if (strcmp(ctx->symbols[i].name, name) == 0) {
            return &ctx->symbols[i];
        }
    }
    return NULL;
}

/*
 * Registers a new symbol into the symbol table.
 * If the symbol already exists, returns the existing pointer.
 */
static Symbol *add_symbol(AssemblerContext *ctx, const char *name, iit_int32 value, int line, int is_set) {
    Symbol *sym = find_symbol(ctx, name);
    if (sym != NULL) {
        return sym;
    }
    if (ctx->symbol_count >= MAX_SYMBOLS) {
        return NULL;
    }

    /* Initialize new symbol entry */
    sym = &ctx->symbols[ctx->symbol_count++];
    safe_strcpy(sym->name, name, sizeof(sym->name));
    sym->value = value;
    sym->defined = 1;
    sym->defined_line = line;
    sym->used = 0;
    sym->is_set = is_set;
    return sym;
}

/*
 * Looks up a label name defined at a specific memory address (excluding SET symbols).
 * Used by advanced listing generation to display branch target label names.
 */
static const char *find_label_at_address(const AssemblerContext *ctx, iit_int32 addr) {
    int i;
    for (i = 0; i < ctx->symbol_count; i++) {
        if (!ctx->symbols[i].is_set && ctx->symbols[i].value == addr) {
            return ctx->symbols[i].name;
        }
    }
    return NULL;
}

/*
 * Validates identifier syntax according to the IITPSx specification:
 * Must begin with an alphabetical letter ([a-zA-Z]) followed by alphanumerics or underscores.
 */
static int is_valid_label(const char *name) {
    int i;

    /* Reject NULL or empty label names */
    if (name == NULL || *name == '\0') {
        return 0;
    }

    /* First character must be an alphabetical letter */
    if (!isalpha((unsigned char)name[0])) {
        return 0;
    }

    /* Subsequent characters must be alphanumeric or underscore */
    for (i = 1; name[i] != '\0'; i++) {
        if (!isalnum((unsigned char)name[i]) && name[i] != '_') {
            return 0;
        }
    }

    return 1;
}

/*
 * Searches the static ISA instruction table for a mnemonic.
 * Case-insensitive comparison supports both upper and lower case variants (e.g. HALT/halt, SET/set).
 */
static const InstructionDef *lookup_instruction(const char *mnemonic) {
    const InstructionDef *def = INSTRUCTION_TABLE;
    while (def->mnemonic != NULL) {
        if (str_case_cmp(def->mnemonic, mnemonic) == 0) {
            return def;
        }
        def++;
    }
    return NULL;
}

/*
 * Parses a numeric constant strictly using strtol().
 * Handles decimal ('123'), hexadecimal ('0x1A'), and octal ('077') representations.
 * Sets out_endptr to the first character past the parsed number.
 */
static int parse_number(const char *str, iit_int32 *out_val, char **out_endptr) {
    char *endptr;
    long val;

    val = strtol(str, &endptr, 0);

    /* If no conversion took place, string is not a valid number */
    if (endptr == str) {
        return 0;
    }

    *out_val = (iit_int32)val;
    if (out_endptr != NULL) {
        *out_endptr = endptr;
    }
    return 1;
}

/*
 * Core Parameterized Assembly Pass Routine.
 * This single routine executes both Pass 1 and Pass 2 as mandated by the project rubric:
 * - Pass 1 (pass == 1): Builds symbol table, checks syntax, tracks PC, emits no code.
 * - Pass 2 (pass == 2): Resolves forward references and branch displacements, emits binary
 *                       words, and constructs listing records.
 */
static int assemble_pass(FILE *in, int pass, AssemblerContext *ctx) {
    char line_buffer[MAX_LINE_LEN];
    char raw_line[MAX_LINE_LEN];
    int line_num = 0;

    /* Initialize program location counter to 0 */
    ctx->pc = 0;

    /* Reset object code and listing counts at the start of Pass 2 */
    if (pass == 2) {
        ctx->object_count = 0;
        ctx->listing_count = 0;
    }

    /* Read source file line by line */
    while (fgets(line_buffer, sizeof(line_buffer), in) != NULL) {
        char *p;
        char *colon_ptr;
        char label[MAX_LABEL_LEN];
        char stmt[MAX_LINE_LEN];
        char mnemonic[MAX_LABEL_LEN];
        char operand_str[MAX_LINE_LEN];
        int has_label = 0;
        int has_stmt = 0;
        const InstructionDef *inst_def = NULL;
        iit_int32 current_pc = ctx->pc;
        int line_had_error = 0;
        int initial_error_count = ctx->error_count;

        line_num++;
        safe_strcpy(raw_line, line_buffer, sizeof(raw_line));

        /* Strip comments beginning with ';' */
        p = strchr(line_buffer, ';');
        if (p != NULL) {
            *p = '\0';
        }

        /* Remove surrounding whitespace */
        p = trim_whitespace(line_buffer);
        if (*p == '\0') {
            /* Skip blank lines and lines containing only comments */
            continue;
        }

        label[0] = '\0';
        stmt[0] = '\0';

        /*
         * Check for label definition containing a colon ':'.
         * Supports both 'label: instruction' and 'label:instruction' (no space after ':').
         */
        colon_ptr = strchr(p, ':');
        if (colon_ptr != NULL) {
            *colon_ptr = '\0';
            safe_strcpy(label, trim_whitespace(p), sizeof(label));
            has_label = 1;

            p = trim_whitespace(colon_ptr + 1);
            if (*p != '\0') {
                safe_strcpy(stmt, p, sizeof(stmt));
                has_stmt = 1;
            }
        } else {
            safe_strcpy(stmt, p, sizeof(stmt));
            has_stmt = 1;
        }

        /*
         * Label syntax verification and symbol table registration.
         */
        if (has_label) {
            if (!is_valid_label(label)) {
                if (pass == 1) {
                    char err_msg[MAX_LINE_LEN];
                    sprintf(err_msg, "invalid label '%s'", label);
                    add_error(ctx, line_num, err_msg);
                }
            } else {
                if (pass == 1) {
                    Symbol *sym = find_symbol(ctx, label);
                    if (sym != NULL && sym->defined) {
                        /* Duplicate label definition detected */
                        char err_msg[MAX_LINE_LEN];
                        sprintf(err_msg, "duplicate label '%s'", label);
                        add_error(ctx, line_num, err_msg);
                    } else {
                        /* Check whether this label is followed by a SET directive */
                        int is_set_stmt = 0;
                        if (has_stmt) {
                            char first_tok[MAX_LABEL_LEN];
                            if (sscanf(stmt, "%s", first_tok) == 1) {
                                if (str_case_cmp(first_tok, "SET") == 0) {
                                    is_set_stmt = 1;
                                }
                            }
                        }
                        /* Register label in symbol table */
                        add_symbol(ctx, label, ctx->pc, line_num, is_set_stmt);
                    }
                }
            }
        }

        /*
         * If the line contains only a label definition without an instruction,
         * record the label in the listing during Pass 2 and advance to next line.
         */
        if (!has_stmt) {
            if (pass == 2 && has_label && ctx->listing_count < MAX_LISTING) {
                ListingRecord *rec = &ctx->listing[ctx->listing_count++];
                rec->address = current_pc;
                rec->code = 0;
                rec->has_code = 0;
                sprintf(rec->text, "%08X          %s:", (unsigned int)current_pc, label);
                rec->error_text[0] = '\0';
            }
            continue;
        }

        /*
         * Tokenize statement into mnemonic and optional operand text.
         */
        mnemonic[0] = '\0';
        operand_str[0] = '\0';

        p = stmt;
        /* Extract mnemonic identifier token */
        while (*p && !isspace((unsigned char)*p)) {
            if ((size_t)(p - stmt) < sizeof(mnemonic) - 1) {
                mnemonic[p - stmt] = *p;
            }
            p++;
        }
        mnemonic[p - stmt] = '\0';

        /* Extract remaining operand text */
        p = trim_whitespace(p);
        if (*p != '\0') {
            safe_strcpy(operand_str, p, sizeof(operand_str));
        }

        /* Check for C++ style comment errors '//' */
        if (strncmp(mnemonic, "//", 2) == 0) {
            if (pass == 1) {
                add_error(ctx, line_num, "unknown mnemonic '//'");
            }
            continue;
        }

        /* Look up instruction definition in static table */
        inst_def = lookup_instruction(mnemonic);
        if (inst_def == NULL) {
            if (pass == 1) {
                char err_msg[MAX_LINE_LEN];
                sprintf(err_msg, "unknown mnemonic '%s'", mnemonic);
                add_error(ctx, line_num, err_msg);
            }
            if (pass == 2 && ctx->listing_count < MAX_LISTING) {
                ListingRecord *rec = &ctx->listing[ctx->listing_count++];
                rec->address = current_pc;
                rec->code = 0;
                rec->has_code = 1;
                sprintf(rec->text, "%08X 00000000 %s", (unsigned int)current_pc, stmt);
                sprintf(rec->error_text, "ERR: Line %d: unknown mnemonic '%s'", line_num, mnemonic);
            }
            ctx->pc++;
            continue;
        }

        /*
         * Handle SET pseudo-instruction directive.
         * Assigns a constant numeric value to the preceding label on the same line.
         * SET does NOT increment the PC or generate machine code words.
         */
        if (inst_def->opcode == -2) {
            iit_int32 set_val = 0;
            char *endptr = NULL;

            if (!has_label) {
                if (pass == 1) {
                    add_error(ctx, line_num, "SET must follow a label");
                }
            }
            if (operand_str[0] == '\0') {
                if (pass == 1) {
                    add_error(ctx, line_num, "missing operand");
                }
            } else if (!parse_number(operand_str, &set_val, &endptr) || *trim_whitespace(endptr) != '\0') {
                if (pass == 1) {
                    add_error(ctx, line_num, "invalid SET value");
                }
            } else {
                if (pass == 1 && has_label) {
                    Symbol *sym = find_symbol(ctx, label);
                    if (sym != NULL) {
                        sym->value = set_val;
                        sym->is_set = 1;
                    }
                }
            }
            continue;
        }

        /*
         * Validate 0-operand instructions (add, sub, mul, div, return, HALT, a2sp, sp2a).
         * Flag syntax error if an extraneous operand was provided.
         */
        if (inst_def->op_type == OP_TYPE_NONE) {
            if (operand_str[0] != '\0') {
                if (pass == 1) {
                    add_error(ctx, line_num, "unexpected operand");
                }
                line_had_error = 1;
            }
            if (pass == 2) {
                iit_uint32 code = (iit_uint32)(inst_def->opcode & 0xFF);
                if (ctx->object_count < MAX_LISTING) {
                    ctx->object_code[ctx->object_count++] = code;
                }
                if (ctx->listing_count < MAX_LISTING) {
                    ListingRecord *rec = &ctx->listing[ctx->listing_count++];
                    rec->address = current_pc;
                    rec->code = code;
                    rec->has_code = 1;
                    if (has_label) {
                        sprintf(rec->text, "%08X %08X %s: %s", (unsigned int)current_pc, (unsigned int)code, label, inst_def->mnemonic);
                    } else {
                        sprintf(rec->text, "%08X %08X %s", (unsigned int)current_pc, (unsigned int)code, inst_def->mnemonic);
                    }
                    if (line_had_error && ctx->error_count > initial_error_count) {
                        safe_strcpy(rec->error_text, ctx->errors[ctx->error_count - 1], sizeof(rec->error_text));
                    } else {
                        rec->error_text[0] = '\0';
                    }
                }
            }
            ctx->pc++;
            continue;
        }

        /*
         * Instructions requiring an operand: flag error if operand is missing.
         */
        if (operand_str[0] == '\0') {
            if (pass == 1) {
                add_error(ctx, line_num, "missing operand");
            }
            line_had_error = 1;
            if (pass == 2 && ctx->listing_count < MAX_LISTING) {
                ListingRecord *rec = &ctx->listing[ctx->listing_count++];
                rec->address = current_pc;
                rec->code = 0;
                rec->has_code = 1;
                sprintf(rec->text, "%08X 00000000 %s", (unsigned int)current_pc, inst_def->mnemonic);
                sprintf(rec->error_text, "ERR: Line %d: missing operand", line_num);
            }
            ctx->pc++;
            continue;
        }

        /*
         * Parse operand token and detect trailing garbage characters.
         */
        {
            iit_int32 op_val = 0;
            int is_num = 0;
            char *endptr = NULL;
            char op_token[MAX_LABEL_LEN];
            char *extra_ptr = NULL;

            /* Check for comma separating extra arguments (e.g. 'ldc 5, 6') */
            char *comma_ptr = strchr(operand_str, ',');
            if (comma_ptr != NULL) {
                if (pass == 1) {
                    char err_msg[MAX_LINE_LEN];
                    sprintf(err_msg, "extra on end of line '%s'", trim_whitespace(comma_ptr));
                    add_error(ctx, line_num, err_msg);
                }
                line_had_error = 1;
            }

            /* Extract first whitespace/comma delimited token */
            p = operand_str;
            while (*p && !isspace((unsigned char)*p) && *p != ',') {
                if ((size_t)(p - operand_str) < sizeof(op_token) - 1) {
                    op_token[p - operand_str] = *p;
                }
                p++;
            }
            op_token[p - operand_str] = '\0';

            /* Check for trailing characters after the token */
            extra_ptr = trim_whitespace(p);
            if (*extra_ptr != '\0' && comma_ptr == NULL) {
                if (pass == 1) {
                    char err_msg[MAX_LINE_LEN];
                    sprintf(err_msg, "extra on end of line '%s'", extra_ptr);
                    add_error(ctx, line_num, err_msg);
                }
                line_had_error = 1;
            }

            /* Test if operand is a valid numeric literal */
            if (parse_number(op_token, &op_val, &endptr) && *endptr == '\0') {
                is_num = 1;
            } else {
                /* Malformed number starting with digit or sign */
                if (isdigit((unsigned char)op_token[0]) || op_token[0] == '+' || op_token[0] == '-') {
                    if (pass == 1) {
                        char err_msg[MAX_LINE_LEN];
                        if (inst_def->opcode == -1) {
                            sprintf(err_msg, "invalid DATA value '%s'", op_token);
                        } else {
                            sprintf(err_msg, "undefined label '%s'", op_token);
                        }
                        add_error(ctx, line_num, err_msg);
                    }
                    line_had_error = 1;
                } else if (!is_valid_label(op_token)) {
                    if (pass == 1) {
                        char err_msg[MAX_LINE_LEN];
                        sprintf(err_msg, "invalid label '%s'", op_token);
                        add_error(ctx, line_num, err_msg);
                    }
                    line_had_error = 1;
                }
            }

            /*
             * Pass 2: Resolve symbols, calculate branch displacements, and encode words.
             */
            if (pass == 2) {
                iit_uint32 machine_word = 0;
                char listing_op_display[MAX_LINE_LEN];
                safe_strcpy(listing_op_display, operand_str, sizeof(listing_op_display));

                if (inst_def->opcode == -1) {
                    /*
                     * 'data' directive: emits full 32-bit initialized value into memory.
                     */
                    if (is_num) {
                        machine_word = (iit_uint32)op_val;
                    } else {
                        Symbol *sym = find_symbol(ctx, op_token);
                        if (sym == NULL || !sym->defined) {
                            char err_msg[MAX_LINE_LEN];
                            sprintf(err_msg, "undefined label '%s'", op_token);
                            add_error(ctx, line_num, err_msg);
                            line_had_error = 1;
                        } else {
                            sym->used = 1;
                            machine_word = (iit_uint32)sym->value;
                        }
                    }
                } else {
                    /*
                     * Standard instructions: encode into 32-bit instruction word:
                     * Bits [7:0]:   8-bit opcode
                     * Bits [31:8]: 24-bit signed 2's complement operand
                     */
                    iit_int32 enc_operand = 0;

                    if (inst_def->op_type == OP_TYPE_BRANCH) {
                        /*
                         * PC-relative branch instruction: displacement calculation.
                         * offset = target_address - (current_instruction_PC + 1)
                         */
                        if (is_num) {
                            enc_operand = op_val;
                        } else {
                            Symbol *sym = find_symbol(ctx, op_token);
                            if (sym == NULL || !sym->defined) {
                                char err_msg[MAX_LINE_LEN];
                                sprintf(err_msg, "undefined label '%s'", op_token);
                                add_error(ctx, line_num, err_msg);
                                line_had_error = 1;
                            } else {
                                sym->used = 1;
                                enc_operand = sym->value - (current_pc + 1);
                                safe_strcpy(listing_op_display, sym->name, sizeof(listing_op_display));
                            }
                        }

                        /* Check for possible infinite loop (backward jump) warning */
                        if (strcmp(inst_def->mnemonic, "br") == 0 && enc_operand < 0) {
                            char warn_msg[MAX_LINE_LEN];
                            sprintf(warn_msg, "WARN: Line %d: possible infinite loop (backward jump)", line_num);
                            add_warning(ctx, warn_msg);
                        }
                    } else {
                        /*
                         * Non-branch instruction: absolute value/address of constant or symbol.
                         */
                        if (is_num) {
                            enc_operand = op_val;
                        } else {
                            Symbol *sym = find_symbol(ctx, op_token);
                            if (sym == NULL || !sym->defined) {
                                char err_msg[MAX_LINE_LEN];
                                sprintf(err_msg, "undefined label '%s'", op_token);
                                add_error(ctx, line_num, err_msg);
                                line_had_error = 1;
                            } else {
                                sym->used = 1;
                                enc_operand = sym->value;
                            }
                        }
                    }

                    /* Pack 24-bit operand and 8-bit opcode into 32-bit machine word */
                    machine_word = (((iit_uint32)enc_operand & 0x00FFFFFF) << 8) | ((iit_uint32)inst_def->opcode & 0xFF);
                }

                /* Record emitted machine word */
                if (ctx->object_count < MAX_LISTING) {
                    ctx->object_code[ctx->object_count++] = machine_word;
                }

                /* Construct human-readable listing entry */
                if (ctx->listing_count < MAX_LISTING) {
                    ListingRecord *rec = &ctx->listing[ctx->listing_count++];
                    rec->address = current_pc;
                    rec->code = machine_word;
                    rec->has_code = 1;

                    /* If branch target matches a known label address, print the label name */
                    if (inst_def->op_type == OP_TYPE_BRANCH) {
                        iit_int32 target_addr = current_pc + 1 + (iit_int32)((machine_word >> 8) | ((machine_word & 0x80000000) ? 0xFF000000 : 0));
                        const char *target_lbl = find_label_at_address(ctx, target_addr);
                        if (target_lbl != NULL) {
                            safe_strcpy(listing_op_display, target_lbl, sizeof(listing_op_display));
                        }
                    }

                    if (has_label) {
                        sprintf(rec->text, "%08X %08X %s: %s %s", (unsigned int)current_pc, (unsigned int)machine_word, label, inst_def->mnemonic, listing_op_display);
                    } else {
                        sprintf(rec->text, "%08X %08X %s %s", (unsigned int)current_pc, (unsigned int)machine_word, inst_def->mnemonic, listing_op_display);
                    }

                    if (line_had_error && ctx->error_count > initial_error_count) {
                        safe_strcpy(rec->error_text, ctx->errors[ctx->error_count - 1], sizeof(rec->error_text));
                    } else {
                        rec->error_text[0] = '\0';
                    }
                }
            }
            ctx->pc++;
        }
    }

    return ctx->error_count;
}

/*
 * Checks the symbol table for declared labels that were never referenced.
 * Generates warning diagnostics as required by the specification rubric.
 */
static void check_unused_labels(AssemblerContext *ctx) {
    int i;
    for (i = 0; i < ctx->symbol_count; i++) {
        if (!ctx->symbols[i].used) {
            char warn_msg[MAX_LINE_LEN];
            sprintf(warn_msg, "WARN: Label '%s' defined but never used", ctx->symbols[i].name);
            add_warning(ctx, warn_msg);
        }
    }
}

/*
 * Writes the raw 32-bit machine code words in little-endian binary format to disk (.o).
 */
static int write_object_file(const char *filename, const AssemblerContext *ctx) {
    FILE *fp = fopen(filename, "wb");
    int i;
    if (fp == NULL) {
        fprintf(stderr, "Error: cannot open output object file '%s'\n", filename);
        return 0;
    }
    for (i = 0; i < ctx->object_count; i++) {
        iit_uint32 w = ctx->object_code[i];
        /* Write four bytes in strict little-endian byte ordering */
        fputc(w & 0xFF, fp);
        fputc((w >> 8) & 0xFF, fp);
        fputc((w >> 16) & 0xFF, fp);
        fputc((w >> 24) & 0xFF, fp);
    }
    fclose(fp);
    return 1;
}

/*
 * Writes the formatted human-readable listing file (.lst) showing addresses,
 * machine words, instructions, labels, and diagnostics.
 * If hex_only is set, writes only the 32-bit machine words in hex, one per line.
 */
static int write_listing_file(const char *filename, const AssemblerContext *ctx, int hex_only) {
    FILE *fp = fopen(filename, "w");
    int i;
    if (fp == NULL) {
        fprintf(stderr, "Error: cannot open listing file '%s'\n", filename);
        return 0;
    }
    for (i = 0; i < ctx->listing_count; i++) {
        const ListingRecord *rec = &ctx->listing[i];
        if (hex_only) {
            /* Skip label-only lines: they emit no machine word */
            if (rec->has_code) {
                fprintf(fp, "%08X\n", (unsigned int)rec->code);
            }
        } else if (rec->error_text[0] != '\0') {
            fprintf(fp, "%s  %s\n", rec->text, rec->error_text);
        } else {
            fprintf(fp, "%s\n", rec->text);
        }
    }
    fclose(fp);
    return 1;
}

/*
 * Writes the diagnostic log file (.log) detailing errors, warnings,
 * the complete symbol table, and final assembly status.
 */
static int write_log_file(const char *filename, const AssemblerContext *ctx) {
    FILE *fp = fopen(filename, "w");
    int i;
    if (fp == NULL) {
        fprintf(stderr, "Error: cannot open log file '%s'\n", filename);
        return 0;
    }
    fprintf(fp, "=== Assemble output ===\n");
    if (ctx->error_count > 0) {
        fprintf(fp, "ERRORS:\n");
        for (i = 0; i < ctx->error_count; i++) {
            fprintf(fp, "%s\n", ctx->errors[i]);
        }
    }
    if (ctx->warning_count > 0) {
        fprintf(fp, "--- warnings ---\n");
        for (i = 0; i < ctx->warning_count; i++) {
            fprintf(fp, "%s\n", ctx->warnings[i]);
        }
    }
    fprintf(fp, "--- labels ---\n");
    for (i = 0; i < ctx->symbol_count; i++) {
        fprintf(fp, "%s : %d\n", ctx->symbols[i].name, (int)ctx->symbols[i].value);
    }
    if (ctx->error_count == 0) {
        fprintf(fp, "Program assembled.\n");
    }
    fclose(fp);
    return 1;
}

/*
 * Main Entry Point.
 * Coordinates command-line argument parsing, two-pass assembly execution,
 * and generation of .o, .lst, and .log files.
 */
int main(int argc, char **argv) {
    static AssemblerContext ctx;  /* Allocated in static storage to prevent stack overflow */
    FILE *in;
    char base_name[MAX_LINE_LEN];
    char obj_name[MAX_TEXT_LEN];
    char lst_name[MAX_TEXT_LEN];
    char log_name[MAX_TEXT_LEN];
    const char *src_file = NULL;
    char *dot;
    int hex_only = 0;  /* 1: .lst contains only hex machine words (-hex flag) */
    int i;

    /* Validate minimal command-line usage */
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <source.asm> [-o output.o] [-l listing.lst] [-log file.log] [-hex]\n", argv[0]);
        return 1;
    }

    src_file = argv[1];

    /* Extract base filename by stripping optional extension */
    safe_strcpy(base_name, src_file, sizeof(base_name));
    dot = strrchr(base_name, '.');
    if (dot != NULL) {
        *dot = '\0';
    }

    /* Set default output filenames based on source name */
    sprintf(obj_name, "%s.o", base_name);
    sprintf(lst_name, "%s.lst", base_name);
    sprintf(log_name, "%s.log", base_name);

    /* Parse optional command-line override flags */
    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            safe_strcpy(obj_name, argv[++i], sizeof(obj_name));
        } else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            safe_strcpy(lst_name, argv[++i], sizeof(lst_name));
        } else if (strcmp(argv[i], "-log") == 0 && i + 1 < argc) {
            safe_strcpy(log_name, argv[++i], sizeof(log_name));
        } else if (strcmp(argv[i], "-hex") == 0) {
            hex_only = 1;
        }
    }

    /* Open input assembly source file */
    in = fopen(src_file, "r");
    if (in == NULL) {
        fprintf(stderr, "Error: cannot open source file '%s'\n", src_file);
        return 1;
    }

    /* Zero-initialize the complete assembler context */
    memset(&ctx, 0, sizeof(ctx));

    /*
     * PASS 1: Build symbol table, process SET directives, check syntax.
     */
    assemble_pass(in, 1, &ctx);

    /* Rewind input file stream back to beginning for Pass 2 */
    rewind(in);

    /*
     * PASS 2: Resolve forward references, encode instructions, emit code and listing.
     */
    assemble_pass(in, 2, &ctx);

    /* Close source file handle */
    fclose(in);

    /* Perform declared but unused labels inspection */
    check_unused_labels(&ctx);

    /* Generate human-readable listing and diagnostic log files */
    write_listing_file(lst_name, &ctx, hex_only);
    write_log_file(log_name, &ctx);

    /* If assembly errors were encountered, report and terminate with failure status */
    if (ctx.error_count > 0) {
        for (i = 0; i < ctx.error_count; i++) {
            fprintf(stderr, "%s\n", ctx.errors[i]);
        }
        for (i = 0; i < ctx.warning_count; i++) {
            fprintf(stderr, "%s\n", ctx.warnings[i]);
        }
        return 1;
    }

    /* Write 32-bit binary object file upon successful error-free assembly */
    if (!write_object_file(obj_name, &ctx)) {
        return 1;
    }

    /* Print warnings to stdout if any exist */
    for (i = 0; i < ctx.warning_count; i++) {
        printf("%s\n", ctx.warnings[i]);
    }

    printf("Program assembled successfully -> %s, %s, %s\n", obj_name, lst_name, log_name);
    return 0;
}
