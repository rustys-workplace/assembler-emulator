/**
 * IITPSx Instruction Set Architecture (ISA) Specification Table
 * Self-contained for offline / Live Server execution.
 */
(function (root, factory) {
  const isa = factory();
  if (typeof module === 'object' && module.exports) {
    module.exports = isa;
  }
  if (typeof window !== 'undefined') {
    window.IITPSX_ISA = isa;
  }
  if (typeof global !== 'undefined') {
    global.IITPSX_ISA = isa;
  }
}(typeof self !== 'undefined' ? self : (typeof window !== 'undefined' ? window : this), function () {
  return [
    {"mnemonic": "ldc", "opcode": 0, "operand": "value", "semantics": "B = A; A = value;", "desc": "Load accumulator with constant."},
    {"mnemonic": "adc", "opcode": 1, "operand": "value", "semantics": "A = A + value;", "desc": "Add constant to accumulator."},
    {"mnemonic": "ldl", "opcode": 2, "operand": "offset", "semantics": "B = A; A = memory[SP + offset];", "desc": "Load local from stack offset."},
    {"mnemonic": "stl", "opcode": 3, "operand": "offset", "semantics": "memory[SP + offset] = A; A = B;", "desc": "Store accumulator to stack offset, pop."},
    {"mnemonic": "ldnl", "opcode": 4, "operand": "offset", "semantics": "A = memory[A + offset];", "desc": "Load non-local from base address A + offset."},
    {"mnemonic": "stnl", "opcode": 5, "operand": "offset", "semantics": "memory[A + offset] = B;", "desc": "Store B into memory at A + offset."},
    {"mnemonic": "add", "opcode": 6, "operand": "None", "semantics": "A = B + A;", "desc": "Stack addition."},
    {"mnemonic": "sub", "opcode": 7, "operand": "None", "semantics": "A = B - A;", "desc": "Stack subtraction (B - A)."},
    {"mnemonic": "shl", "opcode": 8, "operand": "None", "semantics": "A = B << A;", "desc": "Bitwise shift left (B << A)."},
    {"mnemonic": "shr", "opcode": 9, "operand": "None", "semantics": "A = B >> A;", "desc": "Arithmetic shift right (B >> A)."},
    {"mnemonic": "adj", "opcode": 10, "operand": "value", "semantics": "SP = SP + value;", "desc": "Adjust stack pointer."},
    {"mnemonic": "a2sp", "opcode": 11, "operand": "None", "semantics": "SP = A; A = B;", "desc": "Transfer A to SP, restore A = B."},
    {"mnemonic": "sp2a", "opcode": 12, "operand": "None", "semantics": "B = A; A = SP;", "desc": "Transfer SP to A, push old A to B."},
    {"mnemonic": "call", "opcode": 13, "operand": "offset", "semantics": "B = A; A = PC; PC = PC + offset;", "desc": "Subroutine call (stores return address in A)."},
    {"mnemonic": "return", "opcode": 14, "operand": "None", "semantics": "PC = A; A = B;", "desc": "Pop return address into PC."},
    {"mnemonic": "brz", "opcode": 15, "operand": "offset", "semantics": "if (A == 0) PC = PC + offset;", "desc": "Conditional branch if accumulator is 0."},
    {"mnemonic": "brlz", "opcode": 16, "operand": "offset", "semantics": "if (A < 0) PC = PC + offset;", "desc": "Conditional branch if accumulator is negative."},
    {"mnemonic": "br", "opcode": 17, "operand": "offset", "semantics": "PC = PC + offset;", "desc": "Unconditional relative branch."},
    {"mnemonic": "HALT", "opcode": 18, "operand": "None", "semantics": "Cleanly terminates emulator.", "desc": "Virtual halt instruction."},
    {"mnemonic": "mul", "opcode": 19, "operand": "None", "semantics": "A = B * A;", "desc": "Stack multiplication."},
    {"mnemonic": "div", "opcode": 20, "operand": "None", "semantics": "if (A == 0) runtime_error(); else A = B / A;", "desc": "Signed integer division (B / A)."},
    {"mnemonic": "data", "opcode": null, "operand": "value", "semantics": "memory[current_address] = value;", "desc": "Reserve memory word initialized to value."},
    {"mnemonic": "SET", "opcode": null, "operand": "value", "semantics": "symbol = value;", "desc": "Symbol table directive assigning constant without advancing PC."}
  ];
}));
