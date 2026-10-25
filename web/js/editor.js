/**
 * IITPSx Assembly Code Editor Controller
 * Handles line numbering, gutter synchronization, error/warning indicators,
 * tab indentation, keyboard shortcuts, and jump-to-line navigation.
 */

class AssemblyEditor {
  constructor(textareaId, gutterId, infoStatusId) {
    this.textarea = document.getElementById(textareaId);
    this.gutter = document.getElementById(gutterId);
    this.infoStatus = document.getElementById(infoStatusId);
    this.errorLines = new Set();
    this.warningLines = new Set();
    this.init();
  }

  init() {
    if (!this.textarea || !this.gutter) return;

    // Synchronize gutter line numbers on input and scroll
    this.textarea.addEventListener('input', () => {
      this.updateGutter();
      this.updateCursorStatus();
    });

    this.textarea.addEventListener('scroll', () => {
      this.gutter.scrollTop = this.textarea.scrollTop;
    });

    // Handle Tab key (indent 4 spaces)
    this.textarea.addEventListener('keydown', (e) => {
      if (e.key === 'Tab') {
        e.preventDefault();
        const start = this.textarea.selectionStart;
        const end = this.textarea.selectionEnd;
        const value = this.textarea.value;
        const indent = '    '; // 4 spaces indent

        this.textarea.value = value.substring(0, start) + indent + value.substring(end);
        this.textarea.selectionStart = this.textarea.selectionEnd = start + indent.length;
        this.updateGutter();
        this.updateCursorStatus();
      }
    });

    // Cursor position tracking
    this.textarea.addEventListener('keyup', () => this.updateCursorStatus());
    this.textarea.addEventListener('click', () => this.updateCursorStatus());

    this.updateGutter();
    this.updateCursorStatus();
  }

  getValue() {
    return this.textarea ? this.textarea.value : '';
  }

  setValue(val) {
    if (!this.textarea) return;
    this.textarea.value = val;
    this.clearMarkers();
    this.updateGutter();
    this.updateCursorStatus();
    this.textarea.scrollTop = 0;
  }

  updateGutter() {
    if (!this.textarea || !this.gutter) return;
    const lines = this.textarea.value.split('\n');
    const lineCount = Math.max(lines.length, 1);

    let html = '';
    for (let i = 1; i <= lineCount; i++) {
      let classes = 'gutter-line';
      if (this.errorLines.has(i)) {
        classes += ' has-error';
      } else if (this.warningLines.has(i)) {
        classes += ' has-warning';
      }
      html += `<div class="${classes}" data-line="${i}">${i}</div>`;
    }
    this.gutter.innerHTML = html;
    this.gutter.scrollTop = this.textarea.scrollTop;
  }

  updateCursorStatus() {
    if (!this.textarea || !this.infoStatus) return;
    const text = this.textarea.value;
    const selStart = this.textarea.selectionStart;
    const textBefore = text.substring(0, selStart);
    const lines = textBefore.split('\n');
    const row = lines.length;
    const col = lines[lines.length - 1].length + 1;
    const totalLines = text.split('\n').length;
    const charCount = text.length;

    this.infoStatus.textContent = `Ln ${row}, Col ${col}  |  ${totalLines} lines  |  ${charCount} chars`;
  }

  setDiagnostics(errors = [], warnings = []) {
    this.errorLines.clear();
    this.warningLines.clear();

    errors.forEach(err => {
      if (err.line && typeof err.line === 'number') {
        this.errorLines.add(err.line);
      }
    });

    warnings.forEach(warn => {
      if (warn.line && typeof warn.line === 'number') {
        this.warningLines.add(warn.line);
      }
    });

    this.updateGutter();
  }

  clearMarkers() {
    this.errorLines.clear();
    this.warningLines.clear();
    this.updateGutter();
  }

  jumpToLine(lineNum) {
    if (!this.textarea || !lineNum) return;
    const lines = this.textarea.value.split('\n');
    if (lineNum > lines.length) return;

    let charOffset = 0;
    for (let i = 0; i < lineNum - 1; i++) {
      charOffset += lines[i].length + 1; // +1 for newline
    }

    this.textarea.focus();
    this.textarea.selectionStart = charOffset;
    this.textarea.selectionEnd = charOffset + (lines[lineNum - 1] ? lines[lineNum - 1].length : 0);

    // Scroll line into view
    const lineHeight = 22;
    const targetScroll = Math.max(0, (lineNum - 5) * lineHeight);
    this.textarea.scrollTop = targetScroll;
    this.gutter.scrollTop = targetScroll;
    this.updateCursorStatus();
  }
}

// Global instance export
window.AssemblyEditor = AssemblyEditor;
