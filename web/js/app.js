/**
 * IITPSx Processor Studio - Frontend Application Controller
 * Handles Assembly, Compilation, Symbol Table analysis, Listing/Log/Object views,
 * Cycle-Accurate Emulation, and File exports.
 */

document.addEventListener('DOMContentLoaded', () => {
  // Application State
  const state = {
    editor: null,
    activeTab: 'symbols',
    lastAssemblyResult: null,
    lastEmulationResult: null,
    examples: [],
    symbolsFilter: '',
    objectViewMode: 'hexdump', // 'hexdump' or 'words'
    isAssembling: false,
    isEmulating: false
  };

  // DOM Elements
  const el = {
    // Toolbar & Controls
    btnAssemble: document.getElementById('btnAssemble'),
    btnEmulate: document.getElementById('btnEmulate'),
    selectExample: document.getElementById('selectExample'),
    btnNew: document.getElementById('btnNew'),
    btnUpload: document.getElementById('btnUpload'),
    fileInput: document.getElementById('fileInput'),
    btnDownloadMenu: document.getElementById('btnDownloadMenu'),
    downloadDropdown: document.getElementById('downloadDropdown'),
    btnIsa: document.getElementById('btnIsa'),
    statusBadge: document.getElementById('statusBadge'),
    statusText: document.getElementById('statusText'),

    // Tabs
    tabButtons: document.querySelectorAll('.tab-button'),
    tabPanes: document.querySelectorAll('.tab-pane'),
    badgeSymbols: document.getElementById('badgeSymbols'),
    badgeErrors: document.getElementById('badgeErrors'),
    badgeWarnings: document.getElementById('badgeWarnings'),

    // Symbol Table Elements
    symbolSearch: document.getElementById('symbolSearch'),
    symbolTableBody: document.getElementById('symbolTableBody'),
    symbolEmptyState: document.getElementById('symbolEmptyState'),
    symbolTotalCount: document.getElementById('symbolTotalCount'),
    btnCopySymbolsJson: document.getElementById('btnCopySymbolsJson'),
    btnExportSymbolsCsv: document.getElementById('btnExportSymbolsCsv'),

    // Listing View Elements
    lstContainer: document.getElementById('lstContainer'),
    lstRawText: document.getElementById('lstRawText'),
    lstStructured: document.getElementById('lstStructured'),
    toggleLstView: document.getElementById('toggleLstView'),
    btnCopyLst: document.getElementById('btnCopyLst'),
    btnDownloadLst: document.getElementById('btnDownloadLst'),

    // Log View Elements
    logContainer: document.getElementById('logContainer'),
    logStatusBanner: document.getElementById('logStatusBanner'),
    logDiagnosticsList: document.getElementById('logDiagnosticsList'),
    logRawText: document.getElementById('logRawText'),
    btnCopyLog: document.getElementById('btnCopyLog'),
    btnDownloadLog: document.getElementById('btnDownloadLog'),

    // Object File View Elements
    objByteSize: document.getElementById('objByteSize'),
    objWordCount: document.getElementById('objWordCount'),
    objHexdumpText: document.getElementById('objHexdumpText'),
    objWordsTable: document.getElementById('objWordsTable'),
    objWordsTableBody: document.getElementById('objWordsTableBody'),
    btnToggleObjView: document.getElementById('btnToggleObjView'),
    btnDownloadObj: document.getElementById('btnDownloadObj'),
    btnCopyObjHex: document.getElementById('btnCopyObjHex'),

    // Emulator Elements
    emuBtnRun: document.getElementById('emuBtnRun'),
    emuTraceToggle: document.getElementById('emuTraceToggle'),
    emuDumpToggle: document.getElementById('emuDumpToggle'),
    emuReadToggle: document.getElementById('emuReadToggle'),
    emuWriteToggle: document.getElementById('emuWriteToggle'),
    emuCycleLimit: document.getElementById('emuCycleLimit'),
    regPC: document.getElementById('regPC'),
    regPC_dec: document.getElementById('regPC_dec'),
    regSP: document.getElementById('regSP'),
    regSP_dec: document.getElementById('regSP_dec'),
    regA: document.getElementById('regA'),
    regA_dec: document.getElementById('regA_dec'),
    regB: document.getElementById('regB'),
    regB_dec: document.getElementById('regB_dec'),
    emuInstCount: document.getElementById('emuInstCount'),
    emuTerminalOutput: document.getElementById('emuTerminalOutput'),
    btnCopyEmuOutput: document.getElementById('btnCopyEmuOutput'),

    // ISA Modal
    isaModal: document.getElementById('isaModal'),
    btnCloseIsa: document.getElementById('btnCloseIsa'),
    isaSearch: document.getElementById('isaSearch'),
    isaTableBody: document.getElementById('isaTableBody'),

    // Toasts
    toastContainer: document.getElementById('toastContainer')
  };

  // Initialize Code Editor
  state.editor = new AssemblyEditor('codeEditor', 'editorGutter', 'editorStats');

  // Load Examples List (Client-Side)
  function loadExamples() {
    state.examples = window.IITPSX_EXAMPLES || [];

    el.selectExample.innerHTML = '<option value="" disabled selected>📂 Load Test Example...</option>';
    state.examples.forEach(ex => {
      const opt = document.createElement('option');
      opt.value = ex.id;
      opt.textContent = `${ex.name} [${ex.category}]`;
      el.selectExample.appendChild(opt);
    });
  }

  // Load specific example file content (Client-Side)
  function loadExampleFile(name) {
    try {
      showToast(`Loading ${name}...`, 'info');
      const examples = window.IITPSX_EXAMPLES || [];
      const ex = examples.find(e => e.id === name);
      if (!ex) throw new Error(`Example '${name}' not found.`);
      state.editor.setValue(ex.content);
      showToast(`Loaded ${name}`, 'success');
      // Auto assemble on load
      assembleCurrentCode();
    } catch (e) {
      showToast(`Error loading example: ${e.message}`, 'error');
    }
  }

  // Load ISA Table (Client-Side)
  function loadIsaTable() {
    const isa = window.IITPSX_ISA || [];
    renderIsaTable(isa);
  }

  function renderIsaTable(isaList) {
    if (!el.isaTableBody) return;
    el.isaTableBody.innerHTML = '';
    isaList.forEach(item => {
      const tr = document.createElement('tr');
      const opcDisplay = item.opcode !== null ? `<span class="badge badge-indigo">0x${item.opcode.toString(16).toUpperCase().padStart(2, '0')} (${item.opcode})</span>` : '<span class="badge badge-slate">Directive</span>';
      tr.innerHTML = `
        <td class="font-mono font-bold text-emerald-400">${item.mnemonic}</td>
        <td>${opcDisplay}</td>
        <td class="font-mono text-amber-300 text-xs">${item.operand}</td>
        <td class="font-mono text-cyan-300 text-xs">${escapeHtml(item.semantics)}</td>
        <td class="text-slate-300 text-xs">${escapeHtml(item.desc)}</td>
      `;
      el.isaTableBody.appendChild(tr);
    });
  }

  // Assembly Core Logic (100% Client-Side Engine)
  function assembleCurrentCode() {
    if (state.isAssembling) return;
    const code = state.editor.getValue();
    if (!code.trim()) {
      showToast('Please enter some assembly code to compile.', 'warning');
      return;
    }

    state.isAssembling = true;
    updateStatus('assembling', 'Assembling...');
    el.btnAssemble.disabled = true;

    try {
      const data = IITPSXAssembler.assemble(code);

      state.lastAssemblyResult = data;
      state.editor.setDiagnostics(data.errors, data.warnings);

      // Render all output panels
      renderSymbolTable(data.symbols);
      renderListing(data.lst, data.lst_lines);
      renderLog(data);
      renderObject(data.object);

      // Update Badges
      el.badgeSymbols.textContent = data.symbols ? data.symbols.length : 0;
      el.badgeErrors.textContent = data.errors ? data.errors.length : 0;
      el.badgeErrors.style.display = data.errors && data.errors.length ? 'inline-flex' : 'none';
      el.badgeWarnings.textContent = data.warnings ? data.warnings.length : 0;
      el.badgeWarnings.style.display = data.warnings && data.warnings.length ? 'inline-flex' : 'none';

      if (data.success) {
        updateStatus('success', 'Assembled Cleanly');
        showToast(`Assembled successfully (${data.object.word_count} words)`, 'success');
      } else {
        updateStatus('error', `${data.errors.length} Error(s)`);
        showToast(`Assembly finished with ${data.errors.length} error(s)`, 'error');
        // If there are errors, automatically switch to Log or stay on current
        if (state.activeTab === 'object') {
          switchTab('log');
        }
      }
    } catch (e) {
      updateStatus('error', 'Assembly Failed');
      showToast(e.message, 'error');
    } finally {
      state.isAssembling = false;
      el.btnAssemble.disabled = false;
    }
  }

  // Emulation Core Logic (100% Client-Side Cycle-Accurate Simulator)
  function runEmulation() {
    if (state.isEmulating) return;

    const code = state.editor.getValue();
    if (!code.trim()) {
      showToast('No assembly code to emulate.', 'warning');
      return;
    }

    state.isEmulating = true;
    el.btnEmulate.disabled = true;
    el.emuBtnRun.disabled = true;
    el.emuTerminalOutput.textContent = 'Executing binary in IITPSx emulator...';

    const flags = [];
    if (el.emuTraceToggle.checked) flags.push('-trace');
    if (el.emuDumpToggle.checked) flags.push('-dump');
    if (el.emuReadToggle.checked) flags.push('-read');
    if (el.emuWriteToggle.checked) flags.push('-write');

    const limit = parseInt(el.emuCycleLimit.value, 10) || 10000000;

    try {
      // Assemble first
      const asmRes = IITPSXAssembler.assemble(code);
      if (!asmRes.success) {
        el.emuTerminalOutput.innerHTML = `<span class="text-rose-400 font-bold">Cannot emulate: Assembly failed with ${asmRes.errors.length} error(s):</span>\n${escapeHtml(asmRes.errors.map(e => e.raw).join('\n'))}`;
        showToast('Assembly failed before emulation', 'error');
        switchTab('emulator');
        return;
      }

      // Execute in cycle-accurate JavaScript emulator
      const data = IITPSXEmulator.run(asmRes.object.raw_words, flags, limit);
      state.lastEmulationResult = data;

      if (!data.success) {
        const errMsg = data.stderr || 'Emulation error occurred.';
        el.emuTerminalOutput.innerHTML = `<span class="text-rose-400 font-bold">Execution Terminated with Error:</span>\n${escapeHtml(errMsg)}\n\n${escapeHtml(data.stdout || '')}`;
        showToast('Emulation error detected (see terminal output)', 'error');
      } else {
        el.emuTerminalOutput.textContent = data.stdout || '(Clean execution with no trace output)';
        showToast(`Execution finished: ${data.instructions_executed} cycles`, 'success');
      }

      // Update register cards
      updateRegisterDisplay(data.registers || {}, data.instructions_executed || 0);

      // Switch to Emulator tab
      switchTab('emulator');
    } catch (e) {
      el.emuTerminalOutput.textContent = `Emulation Exception: ${e.message}`;
      showToast(e.message, 'error');
    } finally {
      state.isEmulating = false;
      el.btnEmulate.disabled = false;
      el.emuBtnRun.disabled = false;
    }
  }

  function updateRegisterDisplay(regs, cycles) {
    const pc = regs.PC || { hex: '0x00000000', dec: 0 };
    const sp = regs.SP || { hex: '0x00000000', dec: 0 };
    const a = regs.A || { hex: '0x00000000', dec: 0 };
    const b = regs.B || { hex: '0x00000000', dec: 0 };

    el.regPC.textContent = pc.hex;
    el.regPC_dec.textContent = `(${pc.dec})`;

    el.regSP.textContent = sp.hex;
    el.regSP_dec.textContent = `(${sp.dec})`;

    el.regA.textContent = a.hex;
    el.regA_dec.textContent = `(${a.dec})`;

    el.regB.textContent = b.hex;
    el.regB_dec.textContent = `(${b.dec})`;

    el.emuInstCount.textContent = cycles.toLocaleString();
  }

  // 1. Render Symbol Table
  function renderSymbolTable(symbols) {
    if (!el.symbolTableBody) return;
    el.symbolTableBody.innerHTML = '';

    if (!symbols || symbols.length === 0) {
      el.symbolEmptyState.classList.remove('hidden');
      el.symbolTotalCount.textContent = '0 symbols';
      return;
    }

    el.symbolEmptyState.classList.add('hidden');
    el.symbolTotalCount.textContent = `${symbols.length} symbol${symbols.length === 1 ? '' : 's'}`;

    const filter = state.symbolsFilter.trim().toLowerCase();
    const filtered = symbols.filter(s => s.name.toLowerCase().includes(filter));

    filtered.forEach((sym, idx) => {
      const tr = document.createElement('tr');
      tr.className = 'transition-colors hover:bg-slate-800';

      let typeBadge = '';
      if (sym.type.includes('SET')) {
        typeBadge = '<span class="badge badge-purple">SET Directive</span>';
      } else if (sym.type.includes('Data')) {
        typeBadge = '<span class="badge badge-amber">Data Word</span>';
      } else {
        typeBadge = '<span class="badge badge-cyan">Label / Code</span>';
      }

      let usedBadge = sym.used
        ? '<span class="badge badge-emerald">Referenced</span>'
        : '<span class="badge badge-rose" title="Declared but never used in instructions">Unreferenced</span>';

      let lineDisplay = sym.line !== '-'
        ? `<button class="text-indigo-400 hover:text-indigo-300 font-mono underline cursor-pointer jump-line-btn" data-line="${sym.line}">Ln ${sym.line}</button>`
        : '<span class="text-slate-500">-</span>';

      tr.innerHTML = `
        <td class="text-slate-500 font-mono text-xs">${idx + 1}</td>
        <td class="font-mono font-bold text-slate-100 flex items-center gap-2">
          <span>${escapeHtml(sym.name)}</span>
          <button class="text-slate-500 hover:text-slate-300 copy-sym-btn" title="Copy symbol name" data-copy="${escapeHtml(sym.name)}">📋</button>
        </td>
        <td class="font-mono text-indigo-300">${sym.value_hex}</td>
        <td class="font-mono text-slate-300">${sym.value_dec}</td>
        <td>${typeBadge}</td>
        <td>${lineDisplay}</td>
        <td>${usedBadge}</td>
      `;

      el.symbolTableBody.appendChild(tr);
    });

    // Attach jump-to-line event handlers
    el.symbolTableBody.querySelectorAll('.jump-line-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        const line = parseInt(btn.getAttribute('data-line'), 10);
        if (line) state.editor.jumpToLine(line);
      });
    });

    // Attach copy button handlers
    el.symbolTableBody.querySelectorAll('.copy-sym-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        const text = btn.getAttribute('data-copy');
        copyToClipboard(text, `Copied '${text}'`);
      });
    });
  }

  // 2. Render Listing File (.lst)
  function renderListing(rawLst, structuredLines) {
    if (!el.lstRawText) return;
    el.lstRawText.textContent = rawLst || '(No listing generated)';

    if (!el.lstStructured) return;
    el.lstStructured.innerHTML = '';

    if (!structuredLines || structuredLines.length === 0) {
      el.lstStructured.innerHTML = '<div class="p-6 text-slate-500 text-center">No listing data available.</div>';
      return;
    }

    structuredLines.forEach((item, index) => {
      const lineDiv = document.createElement('div');
      lineDiv.className = 'flex items-baseline font-mono text-xs py-0.5 px-3 hover:bg-slate-800 transition-colors border-b border-slate-850';

      const addrSpan = item.address
        ? `<span class="w-24 text-cyan-400 shrink-0 font-bold select-none">${item.address}</span>`
        : '<span class="w-24 text-slate-700 shrink-0 select-none">        </span>';

      const codeSpan = item.code
        ? `<span class="w-28 text-purple-400 shrink-0 font-bold select-none">${item.code}</span>`
        : '<span class="w-28 text-slate-700 shrink-0 select-none">        </span>';

      const stmtSpan = `<span class="text-slate-200 flex-1">${highlightAssemblyStatement(item.statement)}</span>`;

      lineDiv.innerHTML = `${addrSpan} ${codeSpan} ${stmtSpan}`;
      el.lstStructured.appendChild(lineDiv);
    });
  }

  function highlightAssemblyStatement(stmt) {
    if (!stmt) return '';
    // Highlight labels (label:)
    let html = escapeHtml(stmt);
    html = html.replace(/^([a-zA-Z][a-zA-Z0-9_]*:)/, '<span class="text-amber-400 font-bold">$1</span>');
    // Highlight mnemonics
    const mnemonics = ['ldc', 'adc', 'ldl', 'stl', 'ldnl', 'stnl', 'add', 'sub', 'shl', 'shr', 'adj', 'a2sp', 'sp2a', 'call', 'return', 'brz', 'brlz', 'br', 'HALT', 'mul', 'div', 'data', 'SET'];
    const mneRegex = new RegExp(`\\b(${mnemonics.join('|')})\\b`, 'g');
    html = html.replace(mneRegex, '<span class="text-emerald-400 font-semibold">$1</span>');
    // Highlight hex & dec numbers
    html = html.replace(/\b(0x[0-9A-Fa-f]+|-?\d+)\b/g, '<span class="text-cyan-300">$1</span>');
    return html;
  }

  // 3. Render Diagnostic Log (.log)
  function renderLog(assembleResult) {
    if (!el.logRawText) return;
    el.logRawText.textContent = assembleResult.log || assembleResult.stderr || '(No log available)';

    // Status Banner
    if (el.logStatusBanner) {
      if (assembleResult.success) {
        el.logStatusBanner.className = 'p-3 rounded-lg bg-emerald-950 border border-emerald-700 text-emerald-300 text-sm flex items-center justify-between mb-4';
        el.logStatusBanner.innerHTML = `
          <div class="flex items-center gap-2">
            <span class="text-lg">✓</span>
            <span class="font-semibold">Assembly Successful:</span>
            <span>Zero errors encountered. All symbols resolved.</span>
          </div>
          <span class="badge badge-emerald">PASS</span>
        `;
      } else {
        el.logStatusBanner.className = 'p-3 rounded-lg bg-rose-950 border border-rose-700 text-rose-300 text-sm flex items-center justify-between mb-4';
        el.logStatusBanner.innerHTML = `
          <div class="flex items-center gap-2">
            <span class="text-lg">✕</span>
            <span class="font-semibold">Assembly Failed:</span>
            <span>${assembleResult.errors.length} diagnostic error(s) flagged by assembler.</span>
          </div>
          <span class="badge badge-rose">ERROR</span>
        `;
      }
    }

    // Diagnostics List
    if (el.logDiagnosticsList) {
      el.logDiagnosticsList.innerHTML = '';

      if (assembleResult.errors.length === 0 && assembleResult.warnings.length === 0) {
        el.logDiagnosticsList.innerHTML = '<div class="text-xs text-slate-500 py-2">No errors or warnings to display.</div>';
        return;
      }

      // Render Errors
      assembleResult.errors.forEach(err => {
        const card = document.createElement('div');
        card.className = 'p-2.5 rounded bg-rose-950/40 border border-rose-800/80 text-xs flex items-center justify-between gap-3 mb-2';
        const jumpBtn = err.line
          ? `<button class="px-2 py-1 rounded bg-rose-900 hover:bg-rose-800 text-rose-200 font-mono text-xs cursor-pointer jump-line-btn" data-line="${err.line}">Jump to Ln ${err.line}</button>`
          : '';
        card.innerHTML = `
          <div class="flex items-center gap-2 text-rose-300">
            <span class="font-bold text-rose-400">● ERR</span>
            <span>${escapeHtml(err.message)}</span>
          </div>
          ${jumpBtn}
        `;
        el.logDiagnosticsList.appendChild(card);
      });

      // Render Warnings
      assembleResult.warnings.forEach(warn => {
        const card = document.createElement('div');
        card.className = 'p-2.5 rounded bg-amber-950/40 border border-amber-800/80 text-xs flex items-center justify-between gap-3 mb-2';
        const jumpBtn = warn.line
          ? `<button class="px-2 py-1 rounded bg-amber-900 hover:bg-amber-800 text-amber-200 font-mono text-xs cursor-pointer jump-line-btn" data-line="${warn.line}">Jump to Ln ${warn.line}</button>`
          : '';
        card.innerHTML = `
          <div class="flex items-center gap-2 text-amber-300">
            <span class="font-bold text-amber-400">▲ WARN</span>
            <span>${escapeHtml(warn.message)}</span>
          </div>
          ${jumpBtn}
        `;
        el.logDiagnosticsList.appendChild(card);
      });

      // Attach jump handlers
      el.logDiagnosticsList.querySelectorAll('.jump-line-btn').forEach(btn => {
        btn.addEventListener('click', () => {
          const line = parseInt(btn.getAttribute('data-line'), 10);
          if (line) state.editor.jumpToLine(line);
        });
      });
    }
  }

  // 4. Render Object File (.o)
  function renderObject(obj) {
    if (!obj) return;

    el.objByteSize.textContent = `${obj.byte_size} bytes`;
    el.objWordCount.textContent = `${obj.word_count} words (32-bit)`;

    // Hexdump
    if (el.objHexdumpText) {
      if (!obj.hexdump) {
        el.objHexdumpText.textContent = '(No binary object file generated due to assembly errors)';
      } else {
        el.objHexdumpText.textContent = obj.hexdump;
      }
    }

    // Decoded 32-bit Words
    if (el.objWordsTableBody) {
      el.objWordsTableBody.innerHTML = '';
      if (!obj.words || obj.words.length === 0) {
        el.objWordsTableBody.innerHTML = '<tr><td colspan="6" class="text-center text-slate-500 py-6">No binary words available.</td></tr>';
        return;
      }

      obj.words.forEach(w => {
        const tr = document.createElement('tr');
        tr.className = 'hover:bg-slate-800 transition-colors font-mono text-xs';
        const rawBytesStr = w.bytes.join(' ');
        tr.innerHTML = `
          <td class="text-slate-500">#${w.index}</td>
          <td class="text-cyan-400 font-bold">${w.address}</td>
          <td class="text-purple-400 font-bold">${w.machine_hex}</td>
          <td><span class="badge badge-emerald">${w.mnemonic}</span> <span class="text-slate-400 text-xs">(0x${w.opcode_hex})</span></td>
          <td class="text-amber-300">${w.operand_dec} <span class="text-slate-500 text-xs">(${w.operand_hex})</span></td>
          <td class="text-slate-400">${rawBytesStr}</td>
        `;
        el.objWordsTableBody.appendChild(tr);
      });
    }
  }

  // Tab Switching
  function switchTab(tabId) {
    state.activeTab = tabId;
    el.tabButtons.forEach(btn => {
      if (btn.getAttribute('data-tab') === tabId) {
        btn.classList.add('active');
      } else {
        btn.classList.remove('active');
      }
    });

    el.tabPanes.forEach(pane => {
      if (pane.id === `tab-${tabId}`) {
        pane.classList.remove('hidden');
      } else {
        pane.classList.add('hidden');
      }
    });
  }

  // Update Status Pill
  function updateStatus(type, text) {
    el.statusText.textContent = text;
    el.statusBadge.className = 'flex items-center gap-2 px-3 py-1 rounded-full text-xs font-semibold';

    if (type === 'assembling') {
      el.statusBadge.classList.add('bg-indigo-950', 'text-indigo-300', 'border', 'border-indigo-700');
    } else if (type === 'success') {
      el.statusBadge.classList.add('bg-emerald-950', 'text-emerald-300', 'border', 'border-emerald-700');
    } else if (type === 'error') {
      el.statusBadge.classList.add('bg-rose-950', 'text-rose-300', 'border', 'border-rose-700');
    } else {
      el.statusBadge.classList.add('bg-slate-800', 'text-slate-400', 'border', 'border-slate-700');
    }
  }

  // Toast Notification System
  function showToast(msg, type = 'info') {
    if (!el.toastContainer) return;
    const toast = document.createElement('div');
    toast.className = 'px-4 py-2.5 rounded-lg shadow-xl text-xs font-medium flex items-center gap-2 transition-all duration-200';

    if (type === 'success') {
      toast.classList.add('bg-emerald-900', 'border', 'border-emerald-500', 'text-emerald-100');
      toast.innerHTML = `<span>✓</span> <span>${escapeHtml(msg)}</span>`;
    } else if (type === 'error') {
      toast.classList.add('bg-rose-900', 'border', 'border-rose-500', 'text-rose-100');
      toast.innerHTML = `<span>✕</span> <span>${escapeHtml(msg)}</span>`;
    } else if (type === 'warning') {
      toast.classList.add('bg-amber-900', 'border', 'border-amber-500', 'text-amber-100');
      toast.innerHTML = `<span>▲</span> <span>${escapeHtml(msg)}</span>`;
    } else {
      toast.classList.add('bg-slate-800', 'border', 'border-slate-600', 'text-slate-200');
      toast.innerHTML = `<span>ℹ</span> <span>${escapeHtml(msg)}</span>`;
    }

    el.toastContainer.appendChild(toast);
    setTimeout(() => {
      toast.style.opacity = '0';
      setTimeout(() => toast.remove(), 250);
    }, 3200);
  }

  // Clipboard Helper
  function copyToClipboard(text, successMsg = 'Copied to clipboard!') {
    if (!navigator.clipboard) {
      const textarea = document.createElement('textarea');
      textarea.value = text;
      document.body.appendChild(textarea);
      textarea.select();
      document.execCommand('copy');
      document.body.removeChild(textarea);
      showToast(successMsg, 'success');
      return;
    }
    navigator.clipboard.writeText(text).then(() => {
      showToast(successMsg, 'success');
    }).catch(err => {
      showToast(`Copy failed: ${err.message}`, 'error');
    });
  }

  // HTML Entity Escaper
  function escapeHtml(text) {
    if (!text) return '';
    return text.toString()
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#039;');
  }

  // File Download Triggers
  function downloadBlob(filename, blob) {
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }

  function downloadText(filename, content) {
    const blob = new Blob([content], { type: 'text/plain;charset=utf-8' });
    downloadBlob(filename, blob);
  }

  function downloadBase64Binary(filename, base64) {
    const byteCharacters = atob(base64);
    const byteNumbers = new Array(byteCharacters.length);
    for (let i = 0; i < byteCharacters.length; i++) {
      byteNumbers[i] = byteCharacters.charCodeAt(i);
    }
    const byteArray = new Uint8Array(byteNumbers);
    const blob = new Blob([byteArray], { type: 'application/octet-stream' });
    downloadBlob(filename, blob);
  }

  // Event Listeners Wiring

  // 1. Toolbar Actions
  el.btnAssemble.addEventListener('click', () => assembleCurrentCode());
  el.btnEmulate.addEventListener('click', () => runEmulation());
  el.emuBtnRun.addEventListener('click', () => runEmulation());

  el.selectExample.addEventListener('change', (e) => {
    if (e.target.value) {
      loadExampleFile(e.target.value);
    }
  });

  el.btnNew.addEventListener('click', () => {
    if (confirm('Clear editor and create a new assembly file?')) {
      state.editor.setValue('; IITPSx Assembly Program\n\nstart:\n    ldc 0\n    HALT\n');
      assembleCurrentCode();
    }
  });

  el.btnUpload.addEventListener('click', () => el.fileInput.click());
  el.fileInput.addEventListener('change', (e) => {
    const file = e.target.files[0];
    if (file) {
      const reader = new FileReader();
      reader.onload = (event) => {
        state.editor.setValue(event.target.result);
        showToast(`Loaded ${file.name}`, 'success');
        assembleCurrentCode();
      };
      reader.readAsText(file);
    }
  });

  // Download Dropdown Toggle
  el.btnDownloadMenu.addEventListener('click', (e) => {
    e.stopPropagation();
    el.downloadDropdown.classList.toggle('hidden');
  });

  document.addEventListener('click', () => {
    el.downloadDropdown.classList.add('hidden');
  });

  document.getElementById('dlAsm').addEventListener('click', () => {
    downloadText('program.asm', state.editor.getValue());
  });

  document.getElementById('dlObj').addEventListener('click', () => {
    if (state.lastAssemblyResult && state.lastAssemblyResult.object && state.lastAssemblyResult.object.base64) {
      downloadBase64Binary('program.o', state.lastAssemblyResult.object.base64);
    } else {
      showToast('No compiled .o binary file available to download', 'warning');
    }
  });

  document.getElementById('dlLst').addEventListener('click', () => {
    if (state.lastAssemblyResult && state.lastAssemblyResult.lst) {
      downloadText('program.lst', state.lastAssemblyResult.lst);
    } else {
      showToast('No .lst listing file available to download', 'warning');
    }
  });

  document.getElementById('dlLog').addEventListener('click', () => {
    if (state.lastAssemblyResult && state.lastAssemblyResult.log) {
      downloadText('program.log', state.lastAssemblyResult.log);
    } else {
      showToast('No .log diagnostic file available to download', 'warning');
    }
  });

  // 2. Tab Navigation
  el.tabButtons.forEach(btn => {
    btn.addEventListener('click', () => {
      const tabId = btn.getAttribute('data-tab');
      switchTab(tabId);
    });
  });

  // 3. Symbol Table Controls
  el.symbolSearch.addEventListener('input', (e) => {
    state.symbolsFilter = e.target.value;
    if (state.lastAssemblyResult && state.lastAssemblyResult.symbols) {
      renderSymbolTable(state.lastAssemblyResult.symbols);
    }
  });

  el.btnCopySymbolsJson.addEventListener('click', () => {
    if (state.lastAssemblyResult && state.lastAssemblyResult.symbols) {
      copyToClipboard(JSON.stringify(state.lastAssemblyResult.symbols, null, 2), 'Copied symbols as JSON');
    }
  });

  el.btnExportSymbolsCsv.addEventListener('click', () => {
    if (state.lastAssemblyResult && state.lastAssemblyResult.symbols) {
      const syms = state.lastAssemblyResult.symbols;
      let csv = 'Name,Decimal_Value,Hex_Value,Type,Source_Line,Used\n';
      syms.forEach(s => {
        csv += `"${s.name}",${s.value_dec},"${s.value_hex}","${s.type}",${s.line},${s.used}\n`;
      });
      downloadText('symbols.csv', csv);
      showToast('Exported symbols.csv', 'success');
    }
  });

  // 4. Listing File Controls
  el.btnCopyLst.addEventListener('click', () => {
    if (state.lastAssemblyResult && state.lastAssemblyResult.lst) {
      copyToClipboard(state.lastAssemblyResult.lst, 'Copied .lst file contents');
    }
  });

  el.btnDownloadLst.addEventListener('click', () => {
    if (state.lastAssemblyResult && state.lastAssemblyResult.lst) {
      downloadText('program.lst', state.lastAssemblyResult.lst);
    }
  });

  el.toggleLstView.addEventListener('click', () => {
    const isRaw = el.lstRawText.classList.contains('hidden');
    if (isRaw) {
      el.lstRawText.classList.remove('hidden');
      el.lstStructured.classList.add('hidden');
      el.toggleLstView.textContent = 'Structured View';
    } else {
      el.lstRawText.classList.add('hidden');
      el.lstStructured.classList.remove('hidden');
      el.toggleLstView.textContent = 'Raw Text View';
    }
  });

  // 5. Log File Controls
  el.btnCopyLog.addEventListener('click', () => {
    if (state.lastAssemblyResult && state.lastAssemblyResult.log) {
      copyToClipboard(state.lastAssemblyResult.log, 'Copied .log file contents');
    }
  });

  el.btnDownloadLog.addEventListener('click', () => {
    if (state.lastAssemblyResult && state.lastAssemblyResult.log) {
      downloadText('program.log', state.lastAssemblyResult.log);
    }
  });

  // 6. Object File Controls
  el.btnDownloadObj.addEventListener('click', () => {
    if (state.lastAssemblyResult && state.lastAssemblyResult.object && state.lastAssemblyResult.object.base64) {
      downloadBase64Binary('program.o', state.lastAssemblyResult.object.base64);
    } else {
      showToast('No compiled .o binary file to download', 'warning');
    }
  });

  el.btnCopyObjHex.addEventListener('click', () => {
    if (state.lastAssemblyResult && state.lastAssemblyResult.object && state.lastAssemblyResult.object.hexdump) {
      copyToClipboard(state.lastAssemblyResult.object.hexdump, 'Copied object hexdump');
    }
  });

  el.btnToggleObjView.addEventListener('click', () => {
    if (state.objectViewMode === 'hexdump') {
      state.objectViewMode = 'words';
      el.objHexdumpText.classList.add('hidden');
      el.objWordsTable.classList.remove('hidden');
      el.btnToggleObjView.textContent = 'Hexdump View';
    } else {
      state.objectViewMode = 'hexdump';
      el.objHexdumpText.classList.remove('hidden');
      el.objWordsTable.classList.add('hidden');
      el.btnToggleObjView.textContent = 'Decoded Words View';
    }
  });

  // 7. Emulator Output Controls
  el.btnCopyEmuOutput.addEventListener('click', () => {
    copyToClipboard(el.emuTerminalOutput.textContent, 'Copied emulator output');
  });

  // 8. ISA Reference Modal
  el.btnIsa.addEventListener('click', () => {
    el.isaModal.classList.remove('hidden');
  });

  el.btnCloseIsa.addEventListener('click', () => {
    el.isaModal.classList.add('hidden');
  });

  el.isaModal.addEventListener('click', (e) => {
    if (e.target === el.isaModal) {
      el.isaModal.classList.add('hidden');
    }
  });

  el.isaSearch.addEventListener('input', (e) => {
    const q = e.target.value.toLowerCase();
    if (window.IITPSX_ISA) {
      const filtered = window.IITPSX_ISA.filter(item =>
        item.mnemonic.toLowerCase().includes(q) ||
        (item.opcode !== null && item.opcode.toString().includes(q)) ||
        item.desc.toLowerCase().includes(q)
      );
      renderIsaTable(filtered);
    }
  });

  // Keyboard Shortcuts: Ctrl+Enter / Cmd+Enter to assemble, Ctrl+Shift+Enter to emulate
  window.addEventListener('keydown', (e) => {
    if ((e.ctrlKey || e.metaKey) && e.key === 'Enter') {
      e.preventDefault();
      if (e.shiftKey) {
        runEmulation();
      } else {
        assembleCurrentCode();
      }
    }
    if (e.key === 'Escape') {
      el.isaModal.classList.add('hidden');
      el.downloadDropdown.classList.add('hidden');
    }
  });

  // Initial Load Sequence
  loadExamples();
  loadIsaTable();

  // Load bubblesort.asm as default demonstration
  loadExampleFile('bubblesort.asm');
});
