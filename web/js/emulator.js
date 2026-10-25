/**
 * IITPSx Architectural Emulator Engine (Client-Side JavaScript)
 * Fully conforms to the IITPSx processor architecture specification.
 * Simulates 1 Megabyte word-addressable RAM (262,144 words),
 * architectural registers (PC, SP, A, B), cycle-accurate instruction
 * execution, memory access tracking, execution trace, and register state dumps.
 */

(function (root, factory) {
  if (typeof module === 'object' && module.exports) {
    module.exports = factory();
  } else {
    root.IITPSXEmulator = factory();
  }
}(typeof self !== 'undefined' ? self : this, function () {

  const MEM_SIZE = 262144;
  const DEFAULT_MAX_CYCLES = 10000000;

  const ISA_INFO = [
    { mnemonic: 'ldc',    has_operand: true },  // 0
    { mnemonic: 'adc',    has_operand: true },  // 1
    { mnemonic: 'ldl',    has_operand: true },  // 2
    { mnemonic: 'stl',    has_operand: true },  // 3
    { mnemonic: 'ldnl',   has_operand: true },  // 4
    { mnemonic: 'stnl',   has_operand: true },  // 5
    { mnemonic: 'add',    has_operand: false }, // 6
    { mnemonic: 'sub',    has_operand: false }, // 7
    { mnemonic: 'shl',    has_operand: false }, // 8
    { mnemonic: 'shr',    has_operand: false }, // 9
    { mnemonic: 'adj',    has_operand: true },  // 10
    { mnemonic: 'a2sp',   has_operand: false }, // 11
    { mnemonic: 'sp2a',   has_operand: false }, // 12
    { mnemonic: 'call',   has_operand: true },  // 13
    { mnemonic: 'return', has_operand: false }, // 14
    { mnemonic: 'brz',    has_operand: true },  // 15
    { mnemonic: 'brlz',   has_operand: true },  // 16
    { mnemonic: 'br',     has_operand: true },  // 17
    { mnemonic: 'HALT',   has_operand: false }, // 18
    { mnemonic: 'mul',    has_operand: false }, // 19
    { mnemonic: 'div',    has_operand: false }  // 20
  ];

  function formatHex8(val) {
    return (val >>> 0).toString(16).toUpperCase().padStart(8, '0');
  }

  function formatHexSignedOffset(val) {
    const sign = val >= 0 ? '+' : '-';
    return `${sign}${Math.abs(val)}`;
  }

  function run(codeOrBinary, flags = ['-trace'], limit = DEFAULT_MAX_CYCLES) {
    const opt_trace = flags.includes('-trace');
    const opt_dump = flags.includes('-dump');
    const opt_read = flags.includes('-read');
    const opt_write = flags.includes('-write');
    const opt_before = flags.includes('-before');
    const opt_after = flags.includes('-after');
    const opt_wipe = flags.includes('-wipe');

    const max_cycles = limit > 0 ? limit : DEFAULT_MAX_CYCLES;

    const memory = new Int32Array(MEM_SIZE);
    const memory_written = new Uint8Array(MEM_SIZE);

    let reg_A = 0;
    let reg_B = 0;
    let reg_PC = 0;
    let reg_SP = 0;
    let instructions_executed = 0;
    let loaded_words = 0;

    const stdoutLines = [];
    const stderrLines = [];

    function printTraceLine(pc, sp, a, b, opcode, operand) {
      const pcStr = formatHex8(pc);
      const spStr = formatHex8(sp);
      const aStr = formatHex8(a);
      const bStr = formatHex8(b);

      if (opcode >= 0 && opcode <= 20) {
        const info = ISA_INFO[opcode];
        if (info.has_operand) {
          stdoutLines.push(`PC=${pcStr}, SP=${spStr}, A=${aStr}, B=${bStr} ${info.mnemonic} ${formatHex8(operand)} `);
        } else {
          stdoutLines.push(`PC=${pcStr}, SP=${spStr}, A=${aStr}, B=${bStr} ${info.mnemonic} `);
        }
      } else {
        stdoutLines.push(`PC=${pcStr}, SP=${spStr}, A=${aStr}, B=${bStr} <unknown ${opcode}> `);
      }
    }

    function dumpRegisters(targetList) {
      targetList.push("Register State:");
      targetList.push(`  PC = 0x${formatHex8(reg_PC)} (${reg_PC})`);
      targetList.push(`  SP = 0x${formatHex8(reg_SP)} (${reg_SP})`);
      targetList.push(`  A  = 0x${formatHex8(reg_A)} (${reg_A})`);
      targetList.push(`  B  = 0x${formatHex8(reg_B)} (${reg_B})`);
      targetList.push(`  Instructions executed: ${instructions_executed}`);
    }

    function dumpMemory(title, targetList) {
      let max_addr = loaded_words;
      for (let i = 0; i < MEM_SIZE; i++) {
        if (memory[i] !== 0 || memory_written[i]) {
          if (i + 1 > max_addr) {
            max_addr = i + 1;
          }
        }
      }
      max_addr = (max_addr + 3) & ~3;
      if (max_addr > MEM_SIZE) max_addr = MEM_SIZE;

      targetList.push(title);
      for (let i = 0; i < max_addr; i += 4) {
        let non_zero = false;
        for (let j = 0; j < 4 && (i + j) < MEM_SIZE; j++) {
          if (memory[i + j] !== 0 || memory_written[i + j]) {
            non_zero = true;
            break;
          }
        }
        if (non_zero || i < loaded_words) {
          const w0 = formatHex8(memory[i]);
          const w1 = formatHex8(i + 1 < MEM_SIZE ? memory[i + 1] : 0);
          const w2 = formatHex8(i + 2 < MEM_SIZE ? memory[i + 2] : 0);
          const w3 = formatHex8(i + 3 < MEM_SIZE ? memory[i + 3] : 0);
          targetList.push(`${formatHex8(i)} ${w0} ${w1} ${w2} ${w3} `);
        }
      }
    }

    // Load binary words into memory
    let words = [];
    if (Array.isArray(codeOrBinary)) {
      words = codeOrBinary;
    } else if (codeOrBinary instanceof Uint8Array || (typeof Buffer !== 'undefined' && Buffer.isBuffer(codeOrBinary))) {
      for (let i = 0; i < codeOrBinary.length; i += 4) {
        const b0 = codeOrBinary[i] || 0;
        const b1 = codeOrBinary[i + 1] || 0;
        const b2 = codeOrBinary[i + 2] || 0;
        const b3 = codeOrBinary[i + 3] || 0;
        const w = (b0 | (b1 << 8) | (b2 << 16) | (b3 << 24)) | 0;
        words.push(w);
      }
    } else if (typeof codeOrBinary === 'string') {
      // Base64 string
      const binaryStr = (typeof atob === 'function')
        ? atob(codeOrBinary)
        : Buffer.from(codeOrBinary, 'base64').toString('binary');
      for (let i = 0; i < binaryStr.length; i += 4) {
        const b0 = binaryStr.charCodeAt(i) || 0;
        const b1 = binaryStr.charCodeAt(i + 1) || 0;
        const b2 = binaryStr.charCodeAt(i + 2) || 0;
        const b3 = binaryStr.charCodeAt(i + 3) || 0;
        const w = (b0 | (b1 << 8) | (b2 << 16) | (b3 << 24)) | 0;
        words.push(w);
      }
    }

    for (let i = 0; i < words.length && i < MEM_SIZE; i++) {
      memory[i] = words[i] | 0;
    }
    loaded_words = Math.min(words.length, MEM_SIZE);

    if (opt_wipe) {
      memory_written.fill(0);
    }

    if (opt_before) {
      dumpMemory("Memory dump before execution", stdoutLines);
    }

    let halted = false;
    let fault = false;

    while (!halted) {
      if (reg_PC < 0 || reg_PC >= MEM_SIZE) {
        stderrLines.push(`Runtime Error: PC 0x${formatHex8(reg_PC)} out of bounds [0, ${MEM_SIZE})`);
        dumpRegisters(stderrLines);
        fault = true;
        break;
      }

      const current_pc = reg_PC;
      const inst = memory[current_pc];

      // Implicit PC pre-increment
      reg_PC = (reg_PC + 1) | 0;

      // Instruction decoding
      const opcode = inst & 0xFF;
      let operand = (inst >> 8);
      if (operand & 0x00800000) {
        operand |= 0xFF000000;
      } else {
        operand &= 0x007FFFFF;
      }
      operand = operand | 0;

      if (opt_trace) {
        if (stdoutLines.length < 50000) {
          printTraceLine(current_pc, reg_SP, reg_A, reg_B, opcode, operand);
        } else if (stdoutLines.length === 50000) {
          stdoutLines.push("... [trace output capped at 50,000 instructions to prevent browser UI lag] ...");
        }
      }

      let eff_addr = 0;
      switch (opcode) {
        case 0: // ldc
          reg_B = reg_A;
          reg_A = operand;
          break;

        case 1: // adc
          reg_A = (reg_A + operand) | 0;
          break;

        case 2: // ldl
          eff_addr = (reg_SP + operand) | 0;
          if (eff_addr < 0 || eff_addr >= MEM_SIZE) {
            stderrLines.push(`Runtime Error: Memory read out of bounds at 0x${formatHex8(eff_addr)} (PC=0x${formatHex8(current_pc)})`);
            dumpRegisters(stderrLines);
            fault = true;
            break;
          }
          if (opt_read && stdoutLines.length < 50000) {
            stdoutLines.push(`Memory read [SP${formatHexSignedOffset(operand)}] (0x${formatHex8(eff_addr)}) = 0x${formatHex8(memory[eff_addr])}`);
          }
          reg_B = reg_A;
          reg_A = memory[eff_addr];
          break;

        case 3: // stl
          eff_addr = (reg_SP + operand) | 0;
          if (eff_addr < 0 || eff_addr >= MEM_SIZE) {
            stderrLines.push(`Runtime Error: Memory write out of bounds at 0x${formatHex8(eff_addr)} (PC=0x${formatHex8(current_pc)})`);
            dumpRegisters(stderrLines);
            fault = true;
            break;
          }
          if (opt_write && stdoutLines.length < 50000) {
            stdoutLines.push(`Memory write [SP${formatHexSignedOffset(operand)}] (0x${formatHex8(eff_addr)}) = 0x${formatHex8(reg_A)}`);
          }
          memory[eff_addr] = reg_A;
          memory_written[eff_addr] = 1;
          reg_A = reg_B;
          break;

        case 4: // ldnl
          eff_addr = (reg_A + operand) | 0;
          if (eff_addr < 0 || eff_addr >= MEM_SIZE) {
            stderrLines.push(`Runtime Error: Memory read out of bounds at 0x${formatHex8(eff_addr)} (PC=0x${formatHex8(current_pc)})`);
            dumpRegisters(stderrLines);
            fault = true;
            break;
          }
          if (opt_read) {
            stdoutLines.push(`Memory read [A${formatHexSignedOffset(operand)}] (0x${formatHex8(eff_addr)}) = 0x${formatHex8(memory[eff_addr])}`);
          }
          reg_A = memory[eff_addr];
          break;

        case 5: // stnl
          eff_addr = (reg_A + operand) | 0;
          if (eff_addr < 0 || eff_addr >= MEM_SIZE) {
            stderrLines.push(`Runtime Error: Memory write out of bounds at 0x${formatHex8(eff_addr)} (PC=0x${formatHex8(current_pc)})`);
            dumpRegisters(stderrLines);
            fault = true;
            break;
          }
          if (opt_write) {
            stdoutLines.push(`Memory write [A${formatHexSignedOffset(operand)}] (0x${formatHex8(eff_addr)}) = 0x${formatHex8(reg_B)}`);
          }
          memory[eff_addr] = reg_B;
          memory_written[eff_addr] = 1;
          break;

        case 6: // add
          reg_A = (reg_B + reg_A) | 0;
          break;

        case 7: // sub
          reg_A = (reg_B - reg_A) | 0;
          break;

        case 8: // shl
          reg_A = (reg_B << (reg_A & 0x1F)) | 0;
          break;

        case 9: // shr
          reg_A = (reg_B >> (reg_A & 0x1F)) | 0;
          break;

        case 10: // adj
          reg_SP = (reg_SP + operand) | 0;
          break;

        case 11: // a2sp
          reg_SP = reg_A;
          reg_A = reg_B;
          break;

        case 12: // sp2a
          reg_B = reg_A;
          reg_A = reg_SP;
          break;

        case 13: // call
          reg_B = reg_A;
          reg_A = reg_PC;
          reg_PC = (reg_PC + operand) | 0;
          break;

        case 14: // return
          reg_PC = reg_A;
          reg_A = reg_B;
          break;

        case 15: // brz
          if (reg_A === 0) {
            reg_PC = (reg_PC + operand) | 0;
          }
          break;

        case 16: // brlz
          if (reg_A < 0) {
            reg_PC = (reg_PC + operand) | 0;
          }
          break;

        case 17: // br
          reg_PC = (reg_PC + operand) | 0;
          break;

        case 18: // HALT
          halted = true;
          break;

        case 19: // mul
          reg_A = Math.imul(reg_B, reg_A) | 0;
          break;

        case 20: // div
          if (reg_A === 0) {
            stderrLines.push(`Runtime Error: Division by zero at PC=0x${formatHex8(current_pc)}`);
            dumpRegisters(stderrLines);
            if (opt_dump || opt_after) {
              dumpMemory("Memory dump upon division by zero", stderrLines);
            }
            fault = true;
            break;
          }
          reg_A = Math.trunc(reg_B / reg_A) | 0;
          break;

        default:
          stderrLines.push(`Runtime Error: Illegal opcode ${opcode} at PC=0x${formatHex8(current_pc)}`);
          dumpRegisters(stderrLines);
          fault = true;
          break;
      }

      if (fault) break;

      instructions_executed++;

      if (instructions_executed >= max_cycles) {
        stderrLines.push(`Runtime Error: Execution limit reached (${max_cycles} cycles) - possible infinite loop`);
        dumpRegisters(stderrLines);
        fault = true;
        break;
      }
    }

    if (!fault) {
      stdoutLines.push(`${instructions_executed} instructions executed`);
      if (opt_dump || opt_after) {
        dumpMemory("Memory dump after execution", stdoutLines);
      }
    }

    const stdoutText = stdoutLines.join('\n') + (stdoutLines.length ? '\n' : '');
    const stderrText = stderrLines.join('\n') + (stderrLines.length ? '\n' : '');

    return {
      success: !fault,
      return_code: fault ? 1 : 0,
      stdout: stdoutText,
      stderr: stderrText,
      instructions_executed: instructions_executed,
      registers: {
        PC: { hex: `0x${formatHex8(reg_PC)}`, dec: reg_PC },
        SP: { hex: `0x${formatHex8(reg_SP)}`, dec: reg_SP },
        A:  { hex: `0x${formatHex8(reg_A)}`,  dec: reg_A },
        B:  { hex: `0x${formatHex8(reg_B)}`,  dec: reg_B }
      }
    };
  }

  return {
    run: run,
    MEM_SIZE: MEM_SIZE,
    ISA_INFO: ISA_INFO
  };
}));
