/*
 * asm.c - simple two-pass assembler
 * Author: ARNAV RASTOGI
 * User ID: ARNAV RASTOGI
 * Declaration of Authorship:
 * I hereby declare that this codebase is my own original work,
 * written for the CS2102 IITPSx Processor project.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE_LEN    512
#define MAX_TEXT_LEN    1024
#define MAX_LABEL_LEN   64
#define MAX_SYMBOLS     4096
#define MAX_DIAGS       2048
#define MAX_LISTING     8192

typedef enum {
    OP_TYPE_NONE = 0,
    OP_TYPE_VALUE,
    OP_TYPE_OFFSET,
    OP_TYPE_BRANCH
} OperandType;

typedef struct InstructionDef {
    const char *mnemonic;
    int opcode;
    OperandType op_type;
} InstructionDef;

static const InstructionDef INSTRUCTION_TABLE[] = {
    {"ldc", 0, OP_TYPE_VALUE},
    {"adc", 1, OP_TYPE_VALUE},
    {"ldl", 2, OP_TYPE_OFFSET},
    {"stl", 3, OP_TYPE_OFFSET},
    {"ldnl", 4, OP_TYPE_OFFSET},
    {"stnl", 5, OP_TYPE_OFFSET},
    {"add", 6, OP_TYPE_NONE},
    {"sub", 7, OP_TYPE_NONE},
    {"shl", 8, OP_TYPE_NONE},
    {"shr", 9, OP_TYPE_NONE},
    {"adj", 10, OP_TYPE_VALUE},
    {"a2sp", 11, OP_TYPE_NONE},
    {"sp2a", 12, OP_TYPE_NONE},
    {"call", 13, OP_TYPE_BRANCH},
    {"return", 14, OP_TYPE_NONE},
    {"brz", 15, OP_TYPE_BRANCH},
    {"brlz", 16, OP_TYPE_BRANCH},
    {"br", 17, OP_TYPE_BRANCH},
    {"HALT", 18, OP_TYPE_NONE},
    {"halt", 18, OP_TYPE_NONE},
    {"mul", 19, OP_TYPE_NONE},
    {"div", 20, OP_TYPE_NONE},
    {"data", -1, OP_TYPE_VALUE},
    {"SET", -2, OP_TYPE_VALUE},
    {"set", -2, OP_TYPE_VALUE},
    {NULL, 0, OP_TYPE_NONE}
};

typedef struct Symbol {
    char name[MAX_LABEL_LEN];
    int value;
    int defined;
    int defined_line;
    int used;
    int is_set;
} Symbol;

typedef struct ListingRecord {
    int address;
    unsigned int code;
    int has_code;
    char text[MAX_TEXT_LEN];
    char error_text[MAX_TEXT_LEN];
} ListingRecord;

typedef struct AssemblerContext {
    Symbol symbols[MAX_SYMBOLS];
    int symbol_count;
    char errors[MAX_DIAGS][MAX_TEXT_LEN];
    int error_count;
    char warnings[MAX_DIAGS][MAX_TEXT_LEN];
    int warning_count;
    ListingRecord listing[MAX_LISTING];
    int listing_count;
    unsigned int object_code[MAX_LISTING];
    int object_count;
    int pc;
} AssemblerContext;

static void copy_str(char *dest, const char *src, int max_len) {
    int i = 0;
    while (src[i] != '\0' && i < max_len - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static char *trim_whitespace(char *str) {
    char *end;
    while (*str && isspace((unsigned char)*str)) {
        str++;
    }
    if (*str == '\0') {
        return str;
    }
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    return str;
}

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

static void add_error(AssemblerContext *ctx, int line, const char *msg) {
    char buf[MAX_TEXT_LEN];
    int i;

    sprintf(buf, "ERR: Line %d: %s", line, msg);
    for (i = 0; i < ctx->error_count; i++) {
        if (strcmp(ctx->errors[i], buf) == 0) {
            return;
        }
    }
    if (ctx->error_count < MAX_DIAGS) {
        copy_str(ctx->errors[ctx->error_count++], buf, MAX_TEXT_LEN);
    }
}

static void add_warning(AssemblerContext *ctx, const char *msg) {
    int i;
    for (i = 0; i < ctx->warning_count; i++) {
        if (strcmp(ctx->warnings[i], msg) == 0) {
            return;
        }
    }
    if (ctx->warning_count < MAX_DIAGS) {
        copy_str(ctx->warnings[ctx->warning_count++], msg, MAX_TEXT_LEN);
    }
}

static Symbol *find_symbol(AssemblerContext *ctx, const char *name) {
    int i;
    for (i = 0; i < ctx->symbol_count; i++) {
        if (strcmp(ctx->symbols[i].name, name) == 0) {
            return &ctx->symbols[i];
        }
    }
    return NULL;
}

static Symbol *add_symbol(AssemblerContext *ctx, const char *name, int value, int line, int is_set) {
    Symbol *sym = find_symbol(ctx, name);
    if (sym != NULL) {
        return sym;
    }
    if (ctx->symbol_count >= MAX_SYMBOLS) {
        return NULL;
    }
    sym = &ctx->symbols[ctx->symbol_count++];
    copy_str(sym->name, name, sizeof(sym->name));
    sym->value = value;
    sym->defined = 1;
    sym->defined_line = line;
    sym->used = 0;
    sym->is_set = is_set;
    return sym;
}

static const char *find_label_at_address(const AssemblerContext *ctx, int addr) {
    int i;
    for (i = 0; i < ctx->symbol_count; i++) {
        if (!ctx->symbols[i].is_set && ctx->symbols[i].value == addr) {
            return ctx->symbols[i].name;
        }
    }
    return NULL;
}

static int is_valid_label(const char *name) {
    int i;
    if (name == NULL || *name == '\0') {
        return 0;
    }
    if (!isalpha((unsigned char)name[0])) {
        return 0;
    }
    for (i = 1; name[i] != '\0'; i++) {
        if (!isalnum((unsigned char)name[i]) && name[i] != '_') {
            return 0;
        }
    }
    return 1;
}

static const InstructionDef *lookup_instruction(const char *mnemonic) {
    int i;
    for (i = 0; INSTRUCTION_TABLE[i].mnemonic != NULL; i++) {
        if (str_case_cmp(INSTRUCTION_TABLE[i].mnemonic, mnemonic) == 0) {
            return &INSTRUCTION_TABLE[i];
        }
    }
    return NULL;
}

static int parse_number(const char *str, int *out_val, char **out_endptr) {
    char *endptr;
    long val;

    val = strtol(str, &endptr, 0);
    if (endptr == str) {
        return 0;
    }
    *out_val = (int)val;
    if (out_endptr != NULL) {
        *out_endptr = endptr;
    }
    return 1;
}

static int assemble_pass(FILE *in, int pass, AssemblerContext *ctx) {
    char line_buffer[MAX_LINE_LEN];
    char label[MAX_LABEL_LEN];
    char stmt[MAX_LINE_LEN];
    char mnemonic[MAX_LABEL_LEN];
    char operand_str[MAX_LINE_LEN];
    char op_token[MAX_LABEL_LEN];
    char listing_op[MAX_LINE_LEN];
    char err_msg[MAX_LINE_LEN];
    char warn_msg[MAX_LINE_LEN];
    char first_tok[MAX_LABEL_LEN];
    char *p, *colon_ptr, *comma_ptr, *extra_ptr, *endptr;
    int line_num = 0;
    int has_label, has_stmt;
    int is_num, line_had_error, initial_error_count;
    int current_pc, op_val, enc_operand, set_val, is_set_stmt;
    int target_addr, i;
    unsigned int code, machine_word;
    const InstructionDef *inst_def;
    Symbol *sym;
    ListingRecord *rec;
    const char *target_lbl;

    ctx->pc = 0;
    if (pass == 2) {
        ctx->object_count = 0;
        ctx->listing_count = 0;
    }

    while (fgets(line_buffer, sizeof(line_buffer), in) != NULL) {
        line_num++;
        current_pc = ctx->pc;
        line_had_error = 0;
        initial_error_count = ctx->error_count;
        has_label = 0;
        has_stmt = 0;
        label[0] = '\0';
        stmt[0] = '\0';
        enc_operand = 0;
        op_val = 0;
        target_addr = 0;
        machine_word = 0;

        p = strchr(line_buffer, ';');
        if (p != NULL) {
            *p = '\0';
        }

        p = trim_whitespace(line_buffer);
        if (*p == '\0') {
            continue;
        }

        colon_ptr = strchr(p, ':');
        if (colon_ptr != NULL) {
            *colon_ptr = '\0';
            copy_str(label, trim_whitespace(p), sizeof(label));
            has_label = 1;
            p = trim_whitespace(colon_ptr + 1);
            if (*p != '\0') {
                copy_str(stmt, p, sizeof(stmt));
                has_stmt = 1;
            }
        } else {
            copy_str(stmt, p, sizeof(stmt));
            has_stmt = 1;
        }

        if (has_label) {
            if (!is_valid_label(label)) {
                if (pass == 1) {
                    sprintf(err_msg, "invalid label '%s'", label);
                    add_error(ctx, line_num, err_msg);
                }
            } else {
                if (pass == 1) {
                    sym = find_symbol(ctx, label);
                    if (sym != NULL && sym->defined) {
                        sprintf(err_msg, "duplicate label '%s'", label);
                        add_error(ctx, line_num, err_msg);
                    } else {
                        is_set_stmt = 0;
                        if (has_stmt) {
                            if (sscanf(stmt, "%s", first_tok) == 1) {
                                if (str_case_cmp(first_tok, "SET") == 0) {
                                    is_set_stmt = 1;
                                }
                            }
                        }
                        add_symbol(ctx, label, ctx->pc, line_num, is_set_stmt);
                    }
                }
            }
        }

        if (!has_stmt) {
            if (pass == 2 && has_label && ctx->listing_count < MAX_LISTING) {
                rec = &ctx->listing[ctx->listing_count++];
                rec->address = current_pc;
                rec->code = 0;
                rec->has_code = 0;
                sprintf(rec->text, "%08X          %s:", (unsigned int)current_pc, label);
                rec->error_text[0] = '\0';
            }
            continue;
        }

        mnemonic[0] = '\0';
        operand_str[0] = '\0';

        i = 0;
        while (stmt[i] != '\0' && !isspace((unsigned char)stmt[i])) {
            if (i < MAX_LABEL_LEN - 1) {
                mnemonic[i] = stmt[i];
            }
            i++;
        }
        mnemonic[i < MAX_LABEL_LEN ? i : MAX_LABEL_LEN - 1] = '\0';

        copy_str(operand_str, trim_whitespace(stmt + i), sizeof(operand_str));

        if (strncmp(mnemonic, "//", 2) == 0) {
            if (pass == 1) {
                add_error(ctx, line_num, "unknown mnemonic '//'");
            }
            continue;
        }

        inst_def = lookup_instruction(mnemonic);
        if (inst_def == NULL) {
            if (pass == 1) {
                sprintf(err_msg, "unknown mnemonic '%s'", mnemonic);
                add_error(ctx, line_num, err_msg);
            }
            if (pass == 2 && ctx->listing_count < MAX_LISTING) {
                rec = &ctx->listing[ctx->listing_count++];
                rec->address = current_pc;
                rec->code = 0;
                rec->has_code = 1;
                sprintf(rec->text, "%08X 00000000 %s", (unsigned int)current_pc, stmt);
                sprintf(rec->error_text, "ERR: Line %d: unknown mnemonic '%s'", line_num, mnemonic);
            }
            ctx->pc++;
            continue;
        }

        if (inst_def->opcode == -2) {
            set_val = 0;
            endptr = NULL;

            if (!has_label && pass == 1) {
                add_error(ctx, line_num, "SET must follow a label");
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
                    sym = find_symbol(ctx, label);
                    if (sym != NULL) {
                        sym->value = set_val;
                        sym->is_set = 1;
                    }
                }
            }
            continue;
        }

        if (inst_def->op_type == OP_TYPE_NONE) {
            if (operand_str[0] != '\0') {
                if (pass == 1) {
                    add_error(ctx, line_num, "unexpected operand");
                }
                line_had_error = 1;
            }
            if (pass == 2) {
                code = (unsigned int)(inst_def->opcode & 0xFF);
                if (ctx->object_count < MAX_LISTING) {
                    ctx->object_code[ctx->object_count++] = code;
                }
                if (ctx->listing_count < MAX_LISTING) {
                    rec = &ctx->listing[ctx->listing_count++];
                    rec->address = current_pc;
                    rec->code = code;
                    rec->has_code = 1;
                    if (has_label) {
                        sprintf(rec->text, "%08X %08X %s: %s", (unsigned int)current_pc, code, label, inst_def->mnemonic);
                    } else {
                        sprintf(rec->text, "%08X %08X %s", (unsigned int)current_pc, code, inst_def->mnemonic);
                    }
                    if (line_had_error && ctx->error_count > initial_error_count) {
                        copy_str(rec->error_text, ctx->errors[ctx->error_count - 1], sizeof(rec->error_text));
                    } else {
                        rec->error_text[0] = '\0';
                    }
                }
            }
            ctx->pc++;
            continue;
        }

        if (operand_str[0] == '\0') {
            if (pass == 1) {
                add_error(ctx, line_num, "missing operand");
            }
            line_had_error = 1;
            if (pass == 2 && ctx->listing_count < MAX_LISTING) {
                rec = &ctx->listing[ctx->listing_count++];
                rec->address = current_pc;
                rec->code = 0;
                rec->has_code = 1;
                sprintf(rec->text, "%08X 00000000 %s", (unsigned int)current_pc, inst_def->mnemonic);
                sprintf(rec->error_text, "ERR: Line %d: missing operand", line_num);
            }
            ctx->pc++;
            continue;
        }

        op_val = 0;
        is_num = 0;
        endptr = NULL;
        op_token[0] = '\0';

        comma_ptr = strchr(operand_str, ',');
        if (comma_ptr != NULL) {
            if (pass == 1) {
                sprintf(err_msg, "extra on end of line '%s'", trim_whitespace(comma_ptr));
                add_error(ctx, line_num, err_msg);
            }
            line_had_error = 1;
        }

        p = operand_str;
        i = 0;
        while (*p && !isspace((unsigned char)*p) && *p != ',') {
            if (i < MAX_LABEL_LEN - 1) {
                op_token[i++] = *p;
            }
            p++;
        }
        op_token[i] = '\0';

        extra_ptr = trim_whitespace(p);
        if (*extra_ptr != '\0' && comma_ptr == NULL) {
            if (pass == 1) {
                sprintf(err_msg, "extra on end of line '%s'", extra_ptr);
                add_error(ctx, line_num, err_msg);
            }
            line_had_error = 1;
        }

        if (parse_number(op_token, &op_val, &endptr) && *endptr == '\0') {
            is_num = 1;
        } else {
            if (isdigit((unsigned char)op_token[0]) || op_token[0] == '+' || op_token[0] == '-') {
                if (pass == 1) {
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
                    sprintf(err_msg, "invalid label '%s'", op_token);
                    add_error(ctx, line_num, err_msg);
                }
                line_had_error = 1;
            }
        }

        if (pass == 2) {
            machine_word = 0;
            copy_str(listing_op, operand_str, sizeof(listing_op));

            if (inst_def->opcode == -1) {
                if (is_num) {
                    machine_word = (unsigned int)op_val;
                } else {
                    sym = find_symbol(ctx, op_token);
                    if (sym == NULL || !sym->defined) {
                        sprintf(err_msg, "undefined label '%s'", op_token);
                        add_error(ctx, line_num, err_msg);
                        line_had_error = 1;
                    } else {
                        sym->used = 1;
                        machine_word = (unsigned int)sym->value;
                    }
                }
            } else {
                enc_operand = 0;

                if (inst_def->op_type == OP_TYPE_BRANCH) {
                    if (is_num) {
                        enc_operand = op_val;
                    } else {
                        sym = find_symbol(ctx, op_token);
                        if (sym == NULL || !sym->defined) {
                            sprintf(err_msg, "undefined label '%s'", op_token);
                            add_error(ctx, line_num, err_msg);
                            line_had_error = 1;
                        } else {
                            sym->used = 1;
                            enc_operand = sym->value - (current_pc + 1);
                            copy_str(listing_op, sym->name, sizeof(listing_op));
                        }
                    }

                    if (strcmp(inst_def->mnemonic, "br") == 0 && enc_operand < 0) {
                        sprintf(warn_msg, "WARN: Line %d: possible infinite loop (backward jump)", line_num);
                        add_warning(ctx, warn_msg);
                    }
                } else {
                    if (is_num) {
                        enc_operand = op_val;
                    } else {
                        sym = find_symbol(ctx, op_token);
                        if (sym == NULL || !sym->defined) {
                            sprintf(err_msg, "undefined label '%s'", op_token);
                            add_error(ctx, line_num, err_msg);
                            line_had_error = 1;
                        } else {
                            sym->used = 1;
                            enc_operand = sym->value;
                        }
                    }
                }

                machine_word = (((unsigned int)enc_operand & 0x00FFFFFF) << 8) | ((unsigned int)inst_def->opcode & 0xFF);
            }

            if (ctx->object_count < MAX_LISTING) {
                ctx->object_code[ctx->object_count++] = machine_word;
            }

            if (ctx->listing_count < MAX_LISTING) {
                rec = &ctx->listing[ctx->listing_count++];
                rec->address = current_pc;
                rec->code = machine_word;
                rec->has_code = 1;

                if (inst_def->op_type == OP_TYPE_BRANCH) {
                    target_addr = current_pc + 1 + enc_operand;
                    target_lbl = find_label_at_address(ctx, target_addr);
                    if (target_lbl != NULL) {
                        copy_str(listing_op, target_lbl, sizeof(listing_op));
                    }
                }

                if (has_label) {
                    sprintf(rec->text, "%08X %08X %s: %s %s", (unsigned int)current_pc, machine_word, label, inst_def->mnemonic, listing_op);
                } else {
                    sprintf(rec->text, "%08X %08X %s %s", (unsigned int)current_pc, machine_word, inst_def->mnemonic, listing_op);
                }

                if (line_had_error && ctx->error_count > initial_error_count) {
                    copy_str(rec->error_text, ctx->errors[ctx->error_count - 1], sizeof(rec->error_text));
                } else {
                    rec->error_text[0] = '\0';
                }
            }
        }
        ctx->pc++;
    }

    return ctx->error_count;
}

static void check_unused_labels(AssemblerContext *ctx) {
    char warn_msg[MAX_LINE_LEN];
    int i;
    for (i = 0; i < ctx->symbol_count; i++) {
        if (!ctx->symbols[i].used) {
            sprintf(warn_msg, "WARN: Label '%s' defined but never used", ctx->symbols[i].name);
            add_warning(ctx, warn_msg);
        }
    }
}

static int write_object_file(const char *filename, const AssemblerContext *ctx) {
    FILE *fp = fopen(filename, "wb");
    int i;
    unsigned int w;

    if (fp == NULL) {
        fprintf(stderr, "Error: cannot open output object file '%s'\n", filename);
        return 0;
    }
    for (i = 0; i < ctx->object_count; i++) {
        w = ctx->object_code[i];
        fputc(w & 0xFF, fp);
        fputc((w >> 8) & 0xFF, fp);
        fputc((w >> 16) & 0xFF, fp);
        fputc((w >> 24) & 0xFF, fp);
    }
    fclose(fp);
    return 1;
}

static int write_listing_file(const char *filename, const AssemblerContext *ctx) {
    FILE *fp = fopen(filename, "w");
    int i;

    if (fp == NULL) {
        fprintf(stderr, "Error: cannot open listing file '%s'\n", filename);
        return 0;
    }
    for (i = 0; i < ctx->listing_count; i++) {
        if (ctx->listing[i].error_text[0] != '\0') {
            fprintf(fp, "%s  %s\n", ctx->listing[i].text, ctx->listing[i].error_text);
        } else {
            fprintf(fp, "%s\n", ctx->listing[i].text);
        }
    }
    fclose(fp);
    return 1;
}

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
        fprintf(fp, "%s : %d\n", ctx->symbols[i].name, ctx->symbols[i].value);
    }
    if (ctx->error_count == 0) {
        fprintf(fp, "Program assembled.\n");
    }
    fclose(fp);
    return 1;
}

int main(int argc, char **argv) {
    static AssemblerContext ctx;
    FILE *in;
    char base_name[MAX_LINE_LEN];
    char obj_name[MAX_TEXT_LEN];
    char lst_name[MAX_TEXT_LEN];
    char log_name[MAX_TEXT_LEN];
    const char *src_file = NULL;
    char *dot;
    int i;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <source.asm> [-o output.o] [-l listing.lst] [-log file.log]\n", argv[0]);
        return 1;
    }

    src_file = argv[1];
    copy_str(base_name, src_file, sizeof(base_name));
    dot = strrchr(base_name, '.');
    if (dot != NULL) {
        *dot = '\0';
    }

    sprintf(obj_name, "%s.o", base_name);
    sprintf(lst_name, "%s.lst", base_name);
    sprintf(log_name, "%s.log", base_name);

    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            copy_str(obj_name, argv[++i], sizeof(obj_name));
        } else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            copy_str(lst_name, argv[++i], sizeof(lst_name));
        } else if (strcmp(argv[i], "-log") == 0 && i + 1 < argc) {
            copy_str(log_name, argv[++i], sizeof(log_name));
        }
    }

    in = fopen(src_file, "r");
    if (in == NULL) {
        fprintf(stderr, "Error: cannot open source file '%s'\n", src_file);
        return 1;
    }

    memset(&ctx, 0, sizeof(ctx));
    assemble_pass(in, 1, &ctx);

    rewind(in);
    assemble_pass(in, 2, &ctx);
    fclose(in);

    check_unused_labels(&ctx);
    write_listing_file(lst_name, &ctx);
    write_log_file(log_name, &ctx);

    if (ctx.error_count > 0) {
        for (i = 0; i < ctx.error_count; i++) {
            fprintf(stderr, "%s\n", ctx.errors[i]);
        }
        for (i = 0; i < ctx.warning_count; i++) {
            fprintf(stderr, "%s\n", ctx.warnings[i]);
        }
        return 1;
    }

    if (!write_object_file(obj_name, &ctx)) {
        return 1;
    }

    for (i = 0; i < ctx.warning_count; i++) {
        printf("%s\n", ctx.warnings[i]);
    }

    printf("Program assembled successfully -> %s, %s, %s\n", obj_name, lst_name, log_name);
    return 0;
}
