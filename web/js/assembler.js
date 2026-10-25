/**
 * IITPSx Two-Pass Assembler Engine (Client-Side JavaScript)
 * Fully conforms to the IITPSx processor architecture specification.
 * Generates exact listing (.lst), diagnostics (.log), symbol tables,
 * and 32-bit little-endian binary object representations.
 */

(function (root, factory) {
  if (typeof module === 'object' && module.exports) {
    module.exports = factory();
  } else {
    root.IITPSXAssembler = factory();
  }
}(typeof self !== 'undefined' ? self : this, function () {

  const OP_TYPE_NONE = 0;
  const OP_TYPE_VALUE = 1;
  const OP_TYPE_OFFSET = 2;
  const OP_TYPE_BRANCH = 3;

  const INSTRUCTIONS = {
    'ldc':    { opcode: 0,  type: OP_TYPE_VALUE,  mnemonic: 'ldc' },
    'adc':    { opcode: 1,  type: OP_TYPE_VALUE,  mnemonic: 'adc' },
    'ldl':    { opcode: 2,  type: OP_TYPE_OFFSET, mnemonic: 'ldl' },
    'stl':    { opcode: 3,  type: OP_TYPE_OFFSET, mnemonic: 'stl' },
    'ldnl':   { opcode: 4,  type: OP_TYPE_OFFSET, mnemonic: 'ldnl' },
    'stnl':   { opcode: 5,  type: OP_TYPE_OFFSET, mnemonic: 'stnl' },
    'add':    { opcode: 6,  type: OP_TYPE_NONE,   mnemonic: 'add' },
    'sub':    { opcode: 7,  type: OP_TYPE_NONE,   mnemonic: 'sub' },
    'shl':    { opcode: 8,  type: OP_TYPE_NONE,   mnemonic: 'shl' },
    'shr':    { opcode: 9,  type: OP_TYPE_NONE,   mnemonic: 'shr' },
    'adj':    { opcode: 10, type: OP_TYPE_VALUE,  mnemonic: 'adj' },
    'a2sp':   { opcode: 11, type: OP_TYPE_NONE,   mnemonic: 'a2sp' },
    'sp2a':   { opcode: 12, type: OP_TYPE_NONE,   mnemonic: 'sp2a' },
    'call':   { opcode: 13, type: OP_TYPE_BRANCH, mnemonic: 'call' },
    'return': { opcode: 14, type: OP_TYPE_NONE,   mnemonic: 'return' },
    'brz':    { opcode: 15, type: OP_TYPE_BRANCH, mnemonic: 'brz' },
    'brlz':   { opcode: 16, type: OP_TYPE_BRANCH, mnemonic: 'brlz' },
    'br':     { opcode: 17, type: OP_TYPE_BRANCH, mnemonic: 'br' },
    'HALT':   { opcode: 18, type: OP_TYPE_NONE,   mnemonic: 'HALT' },
    'halt':   { opcode: 18, type: OP_TYPE_NONE,   mnemonic: 'HALT' },
    'mul':    { opcode: 19, type: OP_TYPE_NONE,   mnemonic: 'mul' },
    'div':    { opcode: 20, type: OP_TYPE_NONE,   mnemonic: 'div' },
    'data':   { opcode: -1, type: OP_TYPE_VALUE,  mnemonic: 'data' },
    'SET':    { opcode: -2, type: OP_TYPE_VALUE,  mnemonic: 'SET' },
    'set':    { opcode: -2, type: OP_TYPE_VALUE,  mnemonic: 'SET' }
  };

  const OPCODE_MAP = {
    0: 'ldc', 1: 'adc', 2: 'ldl', 3: 'stl', 4: 'ldnl', 5: 'stnl',
    6: 'add', 7: 'sub', 8: 'shl', 9: 'shr', 10: 'adj', 11: 'a2sp',
    12: 'sp2a', 13: 'call', 14: 'return', 15: 'brz', 16: 'brlz',
    17: 'br', 18: 'HALT', 19: 'mul', 20: 'div'
  };

  function isValidLabel(lbl) {
    if (!lbl || lbl.length === 0) return false;
    const first = lbl.charCodeAt(0);
    const isLetter = (first >= 65 && first <= 90) || (first >= 97 && first <= 122);
    if (!isLetter) return false;
    for (let i = 1; i < lbl.length; i++) {
      const c = lbl.charCodeAt(i);
      const ok = (c >= 65 && c <= 90) || (c >= 97 && c <= 122) || (c >= 48 && c <= 57) || c === 95; // _
      if (!ok) return false;
    }
    return true;
  }

  function parseNumber(str) {
    if (!str || typeof str !== 'string') return null;
    const s = str.trim();
    if (s.length === 0) return null;

    let sign = 1;
    let rest = s;
    if (rest.startsWith('+')) {
      rest = rest.substring(1);
    } else if (rest.startsWith('-')) {
      sign = -1;
      rest = rest.substring(1);
    }

    if (rest.length === 0) return null;

    let val = 0;
    if (rest.startsWith('0x') || rest.startsWith('0X')) {
      const hexPart = rest.substring(2);
      if (!hexPart || !/^[0-9a-fA-F]+$/.test(hexPart)) return null;
      val = parseInt(hexPart, 16);
    } else if (rest.length > 1 && rest.startsWith('0') && /^[0-7]+$/.test(rest)) {
      val = parseInt(rest, 8);
    } else if (/^\d+$/.test(rest)) {
      val = parseInt(rest, 10);
    } else {
      return null;
    }

    val = (sign * val) | 0;
    return val;
  }

  function formatHex8(num) {
    return (num >>> 0).toString(16).toUpperCase().padStart(8, '0');
  }

  function formatHex2(num) {
    return (num & 0xFF).toString(16).toUpperCase().padStart(2, '0');
  }

  function assemble(srcCode) {
    const lines = (srcCode || '').split('\n');
    const symbols = new Map(); // name -> { name, value, defined, defined_line, used, is_set, type }
    const errors = [];
    const warnings = [];
    const errorSet = new Set();
    const warningSet = new Set();

    function addError(line, msg) {
      const raw = `ERR: Line ${line}: ${msg}`;
      if (!errorSet.has(raw)) {
        errorSet.add(raw);
        errors.push({ line, message: msg, raw });
      }
    }

    function addWarning(line, msg) {
      const raw = line ? `WARN: Line ${line}: ${msg}` : `WARN: ${msg}`;
      if (!warningSet.has(raw)) {
        warningSet.add(raw);
        warnings.push({ line, message: msg, raw });
      }
    }

    // ==========================================
    // PASS 1: Symbol Table & Syntax Verification
    // ==========================================
    let pc = 0;
    for (let i = 0; i < lines.length; i++) {
      const lineNum = i + 1;
      let line = lines[i];

      // Strip comments
      const commentIdx = line.indexOf(';');
      if (commentIdx !== -1) {
        line = line.substring(0, commentIdx);
      }
      line = line.trim();
      if (!line) continue;

      let label = '';
      let stmt = '';

      const colonIdx = line.indexOf(':');
      if (colonIdx !== -1) {
        label = line.substring(0, colonIdx).trim();
        stmt = line.substring(colonIdx + 1).trim();
      } else {
        stmt = line;
      }

      if (label) {
        if (!isValidLabel(label)) {
          addError(lineNum, `invalid label '${label}'`);
        } else {
          const existing = symbols.get(label);
          if (existing && existing.defined) {
            addError(lineNum, `duplicate label '${label}'`);
          } else {
            const firstTok = stmt.split(/\s+/)[0] || '';
            const isSet = firstTok.toUpperCase() === 'SET';
            symbols.set(label, {
              name: label,
              value: pc,
              defined: true,
              defined_line: lineNum,
              used: false,
              is_set: isSet,
              type: isSet ? 'SET Directive' : 'Label / Code'
            });
          }
        }
      }

      if (!stmt) continue;

      // Extract mnemonic and operand
      const spaceIdx = stmt.search(/\s/);
      let mnemonic = '';
      let operandStr = '';
      if (spaceIdx === -1) {
        mnemonic = stmt;
        operandStr = '';
      } else {
        mnemonic = stmt.substring(0, spaceIdx).trim();
        operandStr = stmt.substring(spaceIdx).trim();
      }

      if (mnemonic.startsWith('//')) {
        addError(lineNum, "unknown mnemonic '//'");
        continue;
      }

      const instDef = INSTRUCTIONS[mnemonic];
      if (!instDef) {
        addError(lineNum, `unknown mnemonic '${mnemonic}'`);
        pc++;
        continue;
      }

      // SET Directive
      if (instDef.opcode === -2) {
        if (!label) {
          addError(lineNum, "SET must follow a label");
        }
        if (!operandStr) {
          addError(lineNum, "missing operand");
        } else {
          const numVal = parseNumber(operandStr);
          if (numVal === null) {
            addError(lineNum, "invalid SET value");
          } else if (label && symbols.has(label)) {
            const sym = symbols.get(label);
            sym.value = numVal;
            sym.is_set = true;
            sym.type = 'SET Directive';
          }
        }
        continue;
      }

      // 0-operand instructions
      if (instDef.type === OP_TYPE_NONE) {
        if (operandStr) {
          addError(lineNum, "unexpected operand");
        }
        pc++;
        continue;
      }

      // Requires operand
      if (!operandStr) {
        addError(lineNum, "missing operand");
        pc++;
        continue;
      }

      // Operand parsing validation
      const commaIdx = operandStr.indexOf(',');
      if (commaIdx !== -1) {
        addError(lineNum, `extra on end of line '${operandStr.substring(commaIdx).trim()}'`);
      }

      const firstTokenMatch = operandStr.match(/^[^\s,]+/);
      const opToken = firstTokenMatch ? firstTokenMatch[0] : '';
      const afterToken = operandStr.substring(opToken.length).trim();
      if (afterToken && commaIdx === -1) {
        addError(lineNum, `extra on end of line '${afterToken}'`);
      }

      const numVal = parseNumber(opToken);
      if (numVal === null) {
        const firstChar = opToken.charAt(0);
        const isDigitOrSign = (firstChar >= '0' && firstChar <= '9') || firstChar === '+' || firstChar === '-';
        if (isDigitOrSign) {
          if (instDef.opcode === -1) {
            addError(lineNum, `invalid DATA value '${opToken}'`);
          } else {
            addError(lineNum, `undefined label '${opToken}'`);
          }
        } else if (!isValidLabel(opToken)) {
          addError(lineNum, `invalid label '${opToken}'`);
        }
      }

      pc++;
    }

    // ==========================================
    // PASS 2: Code Generation, Resolution & Listing
    // ==========================================
    pc = 0;
    const objectCode = [];
    const listingRecords = [];

    function findLabelAtAddress(targetAddr) {
      for (const [name, sym] of symbols.entries()) {
        if (!sym.is_set && sym.value === targetAddr) {
          return name;
        }
      }
      return null;
    }

    for (let i = 0; i < lines.length; i++) {
      const lineNum = i + 1;
      let line = lines[i];

      const commentIdx = line.indexOf(';');
      if (commentIdx !== -1) {
        line = line.substring(0, commentIdx);
      }
      line = line.trim();
      if (!line) continue;

      let label = '';
      let stmt = '';

      const colonIdx = line.indexOf(':');
      if (colonIdx !== -1) {
        label = line.substring(0, colonIdx).trim();
        stmt = line.substring(colonIdx + 1).trim();
      } else {
        stmt = line;
      }

      const currentPc = pc;

      // Label-only line
      if (!stmt) {
        if (label) {
          listingRecords.push({
            address: formatHex8(currentPc),
            code: '',
            statement: `${label}:`,
            raw: `${formatHex8(currentPc)}          ${label}:`
          });
        }
        continue;
      }

      const spaceIdx = stmt.search(/\s/);
      let mnemonic = '';
      let operandStr = '';
      if (spaceIdx === -1) {
        mnemonic = stmt;
        operandStr = '';
      } else {
        mnemonic = stmt.substring(0, spaceIdx).trim();
        operandStr = stmt.substring(spaceIdx).trim();
      }

      const instDef = INSTRUCTIONS[mnemonic];
      if (!instDef) {
        listingRecords.push({
          address: formatHex8(currentPc),
          code: '00000000',
          statement: stmt,
          raw: `${formatHex8(currentPc)} 00000000 ${stmt}`
        });
        pc++;
        continue;
      }

      // SET Directive (no machine word emitted)
      if (instDef.opcode === -2) {
        continue;
      }

      // 0-operand instructions
      if (instDef.type === OP_TYPE_NONE) {
        const code = instDef.opcode & 0xFF;
        objectCode.push(code);

        const stmtDisplay = label ? `${label}: ${instDef.mnemonic}` : instDef.mnemonic;
        listingRecords.push({
          address: formatHex8(currentPc),
          code: formatHex8(code),
          statement: stmtDisplay,
          raw: `${formatHex8(currentPc)} ${formatHex8(code)} ${stmtDisplay}`
        });
        pc++;
        continue;
      }

      // Instructions requiring operands
      if (!operandStr) {
        listingRecords.push({
          address: formatHex8(currentPc),
          code: '00000000',
          statement: instDef.mnemonic,
          raw: `${formatHex8(currentPc)} 00000000 ${instDef.mnemonic}`
        });
        pc++;
        continue;
      }

      const firstTokenMatch = operandStr.match(/^[^\s,]+/);
      const opToken = firstTokenMatch ? firstTokenMatch[0] : '';
      let listingOpDisplay = operandStr;

      let machineWord = 0;
      const numVal = parseNumber(opToken);
      const isNum = numVal !== null;

      if (instDef.opcode === -1) {
        // 'data' pseudo-instruction
        if (isNum) {
          machineWord = numVal >>> 0;
        } else {
          const sym = symbols.get(opToken);
          if (!sym || !sym.defined) {
            addError(lineNum, `undefined label '${opToken}'`);
          } else {
            sym.used = true;
            machineWord = sym.value >>> 0;
          }
        }
      } else {
        let encOperand = 0;
        if (instDef.type === OP_TYPE_BRANCH) {
          if (isNum) {
            encOperand = numVal;
          } else {
            const sym = symbols.get(opToken);
            if (!sym || !sym.defined) {
              addError(lineNum, `undefined label '${opToken}'`);
            } else {
              sym.used = true;
              encOperand = sym.value - (currentPc + 1);
              listingOpDisplay = sym.name;
            }
          }

          if (instDef.mnemonic === 'br' && encOperand < 0) {
            addWarning(lineNum, "possible infinite loop (backward jump)");
          }
        } else {
          if (isNum) {
            encOperand = numVal;
          } else {
            const sym = symbols.get(opToken);
            if (!sym || !sym.defined) {
              addError(lineNum, `undefined label '${opToken}'`);
            } else {
              sym.used = true;
              encOperand = sym.value;
            }
          }
        }

        // Pack 24-bit operand and 8-bit opcode
        machineWord = (((encOperand & 0x00FFFFFF) << 8) | (instDef.opcode & 0xFF)) >>> 0;
      }

      objectCode.push(machineWord);

      if (instDef.type === OP_TYPE_BRANCH) {
        let disp = (machineWord >> 8);
        if (disp & 0x00800000) disp |= 0xFF000000;
        const targetAddr = currentPc + 1 + disp;
        const targetLbl = findLabelAtAddress(targetAddr);
        if (targetLbl) {
          listingOpDisplay = targetLbl;
        }
      }

      const stmtDisplay = label
        ? `${label}: ${instDef.mnemonic} ${listingOpDisplay}`
        : `${instDef.mnemonic} ${listingOpDisplay}`;

      listingRecords.push({
        address: formatHex8(currentPc),
        code: formatHex8(machineWord),
        statement: stmtDisplay,
        raw: `${formatHex8(currentPc)} ${formatHex8(machineWord)} ${stmtDisplay}`
      });

      pc++;
    }

    // Check for unused labels
    for (const [name, sym] of symbols.entries()) {
      if (!sym.used) {
        addWarning(null, `Label '${name}' defined but never used`);
      }
    }

    // Symbol table enrichments
    const symbolList = [];
    for (const sym of symbols.values()) {
      const uval = sym.value >>> 0;
      symbolList.push({
        name: sym.name,
        value_dec: sym.value,
        value_hex: `0x${formatHex8(uval)}`,
        type: sym.type,
        line: sym.defined_line,
        used: sym.used
      });
    }
    symbolList.sort((a, b) => (a.value_dec - b.value_dec) || a.name.localeCompare(b.name));

    // Construct raw listing text
    const lstText = listingRecords.map(r => r.raw).join('\n') + (listingRecords.length ? '\n' : '');

    // Construct diagnostic log text
    let logText = "IITPSx Assembler Log\n------------------------------------------------------------\n";
    if (symbolList.length > 0) {
      logText += "--- labels ---\n";
      symbolList.forEach(s => {
        logText += `${s.name}: ${s.value_dec}\n`;
      });
    }
    if (warnings.length > 0) {
      logText += "--- warnings ---\n";
      warnings.forEach(w => {
        logText += `${w.raw}\n`;
      });
    }
    if (errors.length > 0) {
      logText += "ERRORS:\n";
      errors.forEach(e => {
        logText += `${e.raw}\n`;
      });
    } else {
      logText += `Program assembled successfully: ${objectCode.length} words generated.\n`;
    }

    // Generate binary byte array & hexdump
    const byteLength = objectCode.length * 4;
    const byteBuffer = new Uint8Array(byteLength);
    for (let i = 0; i < objectCode.length; i++) {
      const w = objectCode[i];
      byteBuffer[i * 4 + 0] = w & 0xFF;
      byteBuffer[i * 4 + 1] = (w >> 8) & 0xFF;
      byteBuffer[i * 4 + 2] = (w >> 16) & 0xFF;
      byteBuffer[i * 4 + 3] = (w >> 24) & 0xFF;
    }

    // Canonical hexdump
    const hexLines = [];
    for (let i = 0; i < byteLength; i += 16) {
      const chunk = byteBuffer.subarray(i, Math.min(i + 16, byteLength));
      const p1 = Array.from(chunk.subarray(0, 8)).map(b => formatHex2(b)).join(' ');
      const p2 = Array.from(chunk.subarray(8, 16)).map(b => formatHex2(b)).join(' ');
      let hexPart = '';
      if (chunk.length > 8) {
        hexPart = `${p1.padEnd(23, ' ')}  ${p2.padEnd(23, ' ')}`;
      } else {
        hexPart = `${p1.padEnd(48, ' ')}`;
      }
      const asciiPart = Array.from(chunk).map(b => (b >= 32 && b <= 126) ? String.fromCharCode(b) : '.').join('');
      hexLines.push(`${formatHex8(i)}  ${hexPart}  |${asciiPart}|`);
    }
    const hexdumpText = hexLines.join('\n');

    // Decoded 32-bit machine words
    const decodedWords = [];
    for (let i = 0; i < objectCode.length; i++) {
      const rawWord = objectCode[i];
      const opcode = rawWord & 0xFF;
      let operandRaw = (rawWord >> 8) & 0xFFFFFF;
      let operandSigned = (operandRaw & 0x800000) ? (operandRaw - 0x1000000) : operandRaw;
      const mnemonic = OPCODE_MAP[opcode] || 'data';
      const bytes = [
        formatHex2(rawWord & 0xFF),
        formatHex2((rawWord >> 8) & 0xFF),
        formatHex2((rawWord >> 16) & 0xFF),
        formatHex2((rawWord >> 24) & 0xFF)
      ];
      decodedWords.push({
        index: i,
        address: formatHex8(i),
        machine_hex: formatHex8(rawWord),
        machine_dec: rawWord,
        opcode_hex: formatHex2(opcode),
        opcode_dec: opcode,
        mnemonic: mnemonic,
        operand_hex: `0x${operandRaw.toString(16).toUpperCase().padStart(6, '0')}`,
        operand_dec: operandSigned,
        bytes: bytes
      });
    }

    // Base64 conversion
    let binaryStr = '';
    for (let i = 0; i < byteLength; i++) {
      binaryStr += String.fromCharCode(byteBuffer[i]);
    }
    const base64Str = (typeof btoa === 'function')
      ? btoa(binaryStr)
      : (typeof Buffer !== 'undefined' ? Buffer.from(byteBuffer).toString('base64') : '');

    const success = errors.length === 0;

    return {
      success: success,
      return_code: success ? 0 : 1,
      stdout: success ? `Assembly successful: ${objectCode.length} words emitted.\n` : '',
      stderr: errors.map(e => e.raw).join('\n'),
      log: logText,
      lst: lstText,
      lst_lines: listingRecords,
      symbols: symbolList,
      errors: errors,
      warnings: warnings,
      object: {
        byte_size: byteLength,
        word_count: objectCode.length,
        hexdump: hexdumpText,
        words: decodedWords,
        base64: base64Str,
        raw_words: objectCode
      },
      stats: {
        lines_of_code: lines.length,
        instruction_count: decodedWords.length,
        symbol_count: symbolList.length,
        error_count: errors.length,
        warning_count: warnings.length,
        binary_bytes: byteLength
      }
    };
  }

  return {
    assemble: assemble,
    isValidLabel: isValidLabel,
    parseNumber: parseNumber,
    INSTRUCTIONS: INSTRUCTIONS,
    OPCODE_MAP: OPCODE_MAP
  };
}));
