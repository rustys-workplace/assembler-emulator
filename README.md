# CS2102: IITPSx Processor Toolchain & Architecture

**Author**: ARNAV RASTOGI  
**User ID**: ARNAV RASTOGI  
**Standard**: Strict ISO C89 (`-std=c89 -pedantic`)  

---

## 1. Project Overview

This repository contains a complete, robust, and verified systems toolchain for the 32-bit **IITPSx Processor**, implemented in strict ISO C89 with zero warnings and zero errors under GCC pedantic flags.

The toolchain consists of:
1. **Two-Pass Assembler (`asm.c`)**: Translates IITPSx assembly source code into 32-bit little-endian binary object files (`.o`), advanced human-readable listing files (`.lst`), and comprehensive diagnostic logs (`.log`).
2. **Architectural Emulator (`emu.c`)**: A cycle-accurate emulator simulating 256K words (1 MB) of RAM, the processor register set, implicit pre-increment execution loop, fault handling, extended arithmetic (`mul` and `div`), and full state inspection facilities.
3. **In-Place Bubble Sort (`bubblesort.asm`)**: A complete, in-place Bubble Sort written in IITPSx assembly sorting 10 signed 32-bit integers in ascending order.
4. **Verification Test Suite**: Spec test programs (`test1.asm` through `test7.asm`), algorithmic suites (`test_sum.asm`, `test_poly.asm`, `test_triangle.asm`), and fault verification (`test_divzero.asm`).
5. **Makefile**: Automated build and test harness compiling both binaries under strict pedantic flags and running the full verification suite.
6. **Rubric Claims (`claims`)**: Comprehensive plaintext document verifying compliance with every item in the project rubric.

---

## 2. Processor Architecture Specification

### 2.1 Registers & Memory Model
The IITPSx processor operates on 32-bit words:
* **Registers**:
  * `A`: Primary accumulator / top of the internal 2-element evaluation stack.
  * `B`: Secondary accumulator / second element of the internal evaluation stack.
  * `PC`: Program Counter (points to the current instruction address in 32-bit words).
  * `SP`: Stack Pointer (points to the current top of stack in 32-bit words).
* **Word Size & Addressing**: 32-bit word-addressable memory. Execution and code loading begin at address `0`.
* **Stack Operation**:
  * Pushing onto the internal stack moves `A` to `B` before updating `A` (e.g. `ldc`, `ldl`).
  * Popping restores `A` from `B` (e.g. `stl`, `a2sp`, `return`).
* **Implicit PC Increment**: For every instruction, the Program Counter is incremented to point to the next instruction (`PC = PC + 1`) **before** the semantic actions of that instruction are executed.

### 2.2 Instruction Encoding Format
Every instruction is encoded as a single 32-bit word:
```text
 31                                       8 7        0
+------------------------------------------+----------+
|      Signed 24-bit Operand Value         |  Opcode  |
+------------------------------------------+----------+
```
* **Bits [7:0]** (Lowest 8 bits): Instruction Opcode (0..20).
* **Bits [31:8]** (Upper 24 bits): Operand, encoded as a signed 2's complement 24-bit integer, sign-extended to 32 bits upon fetch.

---

## 3. Instruction Set Architecture (ISA) Reference

| Mnemonic | Opcode | Operand Type | Formal Semantics | Description |
| :--- | :---: | :--- | :--- | :--- |
| `ldc` | `0` | `value` | `B = A; A = value;` | Load accumulator with constant. |
| `adc` | `1` | `value` | `A = A + value;` | Add constant to accumulator. |
| `ldl` | `2` | `offset` | `B = A; A = memory[SP + offset];` | Load local from stack offset. |
| `stl` | `3` | `offset` | `memory[SP + offset] = A; A = B;` | Store accumulator to stack offset, pop. |
| `ldnl` | `4` | `offset` | `A = memory[A + offset];` | Load non-local from base address `A + offset`. |
| `stnl` | `5` | `offset` | `memory[A + offset] = B;` | Store `B` into memory at `A + offset`. |
| `add` | `6` | *None* | `A = B + A;` | Stack addition. |
| `sub` | `7` | *None* | `A = B - A;` | Stack subtraction (`B - A`). |
| `shl` | `8` | *None* | `A = B << A;` | Bitwise shift left (`B << A`). |
| `shr` | `9` | *None* | `A = B >> A;` | Arithmetic shift right (`B >> A`). |
| `adj` | `10` | `value` | `SP = SP + value;` | Adjust stack pointer. |
| `a2sp` | `11` | *None* | `SP = A; A = B;` | Transfer `A` to `SP`, restore `A = B`. |
| `sp2a` | `12` | *None* | `B = A; A = SP;` | Transfer `SP` to `A`, push old `A` to `B`. |
| `call` | `13` | `offset` | `B = A; A = PC; PC = PC + offset;` | Subroutine call (stores return address in `A`). |
| `return` | `14` | *None* | `PC = A; A = B;` | Pop return address into `PC`. |
| `brz` | `15` | `offset` | `if (A == 0) PC = PC + offset;` | Conditional branch if accumulator is 0. |
| `brlz` | `16` | `offset` | `if (A < 0) PC = PC + offset;` | Conditional branch if accumulator is negative. |
| `br` | `17` | `offset` | `PC = PC + offset;` | Unconditional relative branch. |
| `HALT` | `18` | *None* | Cleanly terminates emulator. | Virtual halt instruction. |
| `mul` | `19` | *None* | `A = B * A;` | Stack multiplication. |
| `div` | `20` | *None* | `if (A == 0) runtime_error(); else A = B / A;` | Signed integer division (`B / A`). |
| `data` | N/A | `value` | `memory[current_address] = value;` | Reserve memory word initialized to `value`. |
| `SET` | N/A | `value` | Symbol table directive: `symbol = value` | Assigns constant to label without advancing `PC`. |

---

## 4. Two-Pass Assembler Design (`asm.c`)

### 4.1 Single Parameterized Routine Architecture
In strict adherence to the project rubric, the assembler utilizes a single core routine:
```c
static int assemble_pass(FILE *in, int pass, AssemblerContext *ctx);
```
* **Pass 1 (`pass = 1`)**:
  * Tracks location counter `PC` starting at `0`.
  * Strips comments (`;`) and whitespace.
  * Validates label identifiers (`[a-zA-Z][a-zA-Z0-9_]*`).
  * Enters labels into the Symbol Table.
  * Identifies duplicate label definitions.
  * Evaluates `SET` pseudo-instructions and assigns constant values.
  * Validates instruction mnemonics and operand syntax.
  * Forward label references are tolerated and deferred.
* **Pass 2 (`pass = 2`)**:
  * Rewinds file stream (`rewind(in)`) and resets `PC = 0`.
  * Resolves forward label references against the populated Symbol Table.
  * Flags undefined labels with line numbers.
  * Calculates PC-relative branch displacements:
    $$\text{offset} = \text{target\_address} - (\text{PC} + 1)$$
  * Encodes 32-bit machine instructions:
    $$\text{word} = ((\text{offset} \ \& \ \text{0x00FFFFFF}) \ll 8) \mid (\text{opcode} \ \& \ \text{0xFF})$$
  * Constructs advanced listing records (`.lst`).
  * Emits little-endian raw binary words to disk (`.o`).

### 4.2 Comprehensive Diagnostic Engine
`asm.c` reports line numbers and explicit diagnostics for:
* **Duplicate Labels**: Flagged in Pass 1 (`ERR: Line 7: duplicate label 'label'`).
* **Undefined Labels**: Flagged in Pass 2 (`ERR: Line 8: undefined label 'nonesuch'`).
* **Unknown Mnemonics**: Flagged in Pass 1 (`ERR: Line 14: unknown mnemonic 'fibble'`).
* **Missing Operands**: Flagged for instructions requiring an operand (`ERR: Line 10: missing operand`).
* **Unexpected Operands**: Flagged for 0-operand instructions (`ERR: Line 11: unexpected operand`).
* **Extra Characters**: Flagged when garbage or multiple tokens follow an operand (`ERR: Line 12: extra on end of line ', 6'`).
* **Invalid Label Names**: Flagged when labels begin with numbers or invalid symbols (`ERR: Line 13: invalid label '0def'`).
* **Malformed Numbers**: Flagged when numeric constants contain invalid characters (`ERR: Line 9: undefined label '08ge'`).
* **Unused Labels Warning**: At the end of Pass 2, scans symbol table and flags symbols with `used == 0`.
* **Backward Branch Warning**: Detects unconditional branches jumping backward (`offset < 0`), warning of potential infinite loops.

### 4.3 Advanced Listing Format (`.lst`)
The listing file presents an 8-digit hexadecimal address, 8-digit machine word, and decoded instruction. If a relative branch resolves to an address matching a known label, the listing prints the label name:
```text
00000000          label:
00000000 00000000 ldc 0
00000001 FFFFFB00 ldc -5
00000002 00000500 ldc +5
00000003 FFFFFF11 loop: br loop
00000004 00000011 br next
00000005          next:
00000005 00000300 ldc loop
00000006 00000700 ldc var1
00000007 00000000 var1: data 0
00000008 00000012 HALT
```

Passing `-hex` (e.g. `./asm test1.asm -hex`) switches the listing to the spec's "simple memory dump" form: only the 32-bit machine words in hex, one per line, with label-only lines omitted.

---

## 5. Architectural Emulator Design (`emu.c`)

### 5.1 Memory & Execution Model
* **Simulated Memory**: 262,144 32-bit words (1 MB) initialized to `0`.
* **Binary Object Loader**: Reads 32-bit binary words in little-endian format:
  $$\text{word} = \text{buf}[0] \mid (\text{buf}[1] \ll 8) \mid (\text{buf}[2] \ll 16) \mid (\text{buf}[3] \ll 24)$$
* **Implicit Pre-Increment**:
  ```c
  current_pc = reg_PC;
  inst = memory[current_pc];
  reg_PC = reg_PC + 1; /* Pre-increment before execution */
  opcode = inst & 0xFF;
  operand = (iit_int32)(inst >> 8);
  if (operand & 0x00800000) operand |= 0xFF000000; /* Sign-extend */
  ```
* **Division by Zero Protection**:
  When executing `div` (Opcode 20), if $A == 0$, the emulator halts immediately, prints:
  `Runtime Error: Division by zero at PC=0x...`, outputs the register dump, and exits with code 1.
* **Fault Handling**: Traps out-of-bounds PC or memory access, unknown opcodes, and infinite loop cycles.

### 5.2 CLI Flags
* `-trace`: Outputs step-by-step register states (`PC`, `SP`, `A`, `B`) and instructions.
* `-dump` / `-after`: Outputs 4-word memory dump after execution.
* `-before`: Outputs memory dump before execution begins.
* `-read` / `-write`: Logs individual memory read and write events.
* `-wipe`: Resets memory written flags before running.
* `-isa`: Displays ISA table.
* `-limit <N>`: Sets maximum cycle cap (default: 10,000,000).

---

## 6. Bubble Sort Implementation (`bubblesort.asm`)

### 6.1 Algorithm & Memory Layout
An in-place Bubble Sort is implemented in `bubblesort.asm`:
* **Input Array**: 10 signed integers `[25, -4, 13, 0, 99, -50, 42, 8, 1, 7]` at label `array`.
* **Array Length**: Word initialized to `10` at label `count`.
* **Stack Setup**: `SP` initialized to `0x1000` (4096) using `a2sp`.
* **Stack Frame Allocation**: 6 local words allocated via `adj -6`:
  * `SP+0`: Outer loop index $i$ (runs from $n-1$ down to 1).
  * `SP+1`: Inner loop index $j$ (runs from 0 up to $i-1$).
  * `SP+2`: Array length $n$ (10).
  * `SP+3`: Element value $array[j]$.
  * `SP+4`: Element value $array[j+1]$.
  * `SP+5`: Memory pointer $\&array[j]$.

### 6.2 Comparison & In-Place Swap Logic
* Adjacent elements are compared by evaluating $(array[j+1] - array[j])$.
* If negative (`brlz do_swap`), elements are swapped in memory using `stnl 0` and `stnl 1`.
* **Sorted Output**:
  ```text
  [-50, -4, 0, 1, 7, 8, 13, 25, 42, 99]
  ```
* Clean termination with `HALT` after 1,339 instruction cycles.

---

## 7. Test Suite Summary

| Test Program | Purpose | Expected Outcome |
| :--- | :--- | :--- |
| `test1.asm` | Tests negative numbers, forward/backward branches, labels as pointers. | Assembles cleanly, generates warning on unused label and loop. |
| `test2.asm` | Error-checking suite exercising syntax and semantic errors. | Diagnoses duplicate labels, undefined labels, bogus mnemonics, operand errors. |
| `test3.asm` | Demonstrates the `SET` directive. | $A = 75 + 66 = 141$ (`0x8D`). |
| `test4.asm` | Demonstrates extended arithmetic (`mul` and `div`). | $(6 \times 7) / 2 = 21$ (`0x15`). |
| `test_sum.asm` | Sum of first 10 integers ($1+2+\dots+10$). | $A = 55$ (`0x37`), stored in `result`. |
| `test_poly.asm` | Evaluates polynomial $P(4) = 3(4^2) + 5(4) + 7$. | $A = 75$ (`0x4B`), stored in `result`. |
| `test_divzero.asm` | Deliberate division by zero ($100 / 0$). | Traps error, writes message to stderr, dumps registers. |
| `test_triangle.asm` | Computes Pascal's triangle row 10. | Recursive subroutine test, executes 47,654 cycles cleanly. |
| `bubblesort.asm` | In-place Bubble Sort of 10 signed integers. | Sorts array ascending, verified via memory dump. |
| `test5.asm` | Demonstrates `SET`, `data`, local stack operations, loop sum ($1..10$). | Assembles cleanly ($0$ errors); executes loop to compute $55$ (`0x37`). |
| `test6.asm` | Demonstrates consecutive empty labels, arithmetic, infinite backward jump, unused labels. | Assembles cleanly ($0$ errors); emits warnings for infinite loop and unused labels. |
| `test7.asm` | Comprehensive error-checking suite verifying parser and semantic diagnostics. | Diagnoses 10 syntax/semantic errors (bogus labels, missing/unexpected operands, comments, duplicate labels, SET). |

---

## 8. Compilation & Verification Instructions

### 8.1 Strict ISO C89 Compilation
Both binaries compile with zero warnings and zero errors under:
```bash
gcc -std=c89 -pedantic -Wall -Wextra -Wpointer-arith -Wwrite-strings -Wstrict-prototypes -O2 -o asm asm.c
gcc -std=c89 -pedantic -Wall -Wextra -Wpointer-arith -Wwrite-strings -Wstrict-prototypes -O2 -o emu emu.c
```

### 8.2 Automated Build and Testing
To compile both tools and run the full test suite:
```bash
make test
```

To clean generated binaries and logs:
```bash
make clean
```
