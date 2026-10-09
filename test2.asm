; ==============================================================================
; IITPSx Test Program: test2.asm
;
; Author: ARNAV RASTOGI
; User ID: ARNAV RASTOGI
; Declaration of Authorship:
; I hereby declare that this test code is my own original work, developed for
; the CS2102 IITPSx Processor project.
;
; Purpose:
;   Comprehensive error-checking test suite designed to exercise the assembler's
;   lexical, syntactic, and semantic diagnostic capabilities.
;
; Expected Diagnostics:
;   - Duplicate label definition
;   - Undefined/missing label reference
;   - Malformed numeric literal (not a valid decimal, octal, or hex number)
;   - Missing operand on an instruction requiring one
;   - Unexpected operand on a 0-operand instruction
;   - Extra characters/tokens at end of line
;   - Bogus/invalid label identifier name
;   - Bogus/unknown instruction mnemonic
; ==============================================================================

label:                  ; First definition of 'label'
label:                  ; Error: duplicate label definition
br nonesuch             ; Error: undefined label 'nonesuch'
ldc 08ge                ; Error: malformed number / undefined label '08ge'
ldc                     ; Error: missing operand for ldc
add 5                   ; Error: unexpected operand for 0-operand instruction 'add'
ldc 5, 6                ; Error: extra tokens on end of line
0def:                   ; Error: invalid label name (begins with digit '0')
fibble                  ; Error: unknown mnemonic 'fibble'
0def                    ; Error: unknown mnemonic '0def'
