; ==============================================================================
; IITPSx Test Program: test7.asm
;
; Author: ARNAV RASTOGI
; User ID: ARNAV RASTOGI
; Declaration of Authorship:
; I hereby declare that this test code is my own original work, developed for
; the CS2102 IITPSx Processor project.
;
; Purpose:
;   Comprehensive error-checking test suite verifying assembler diagnostics:
;   - Invalid label beginning with a digit ('1badlabel')
;   - Missing operand on instruction requiring one ('ldc')
;   - Unexpected operand on 0-operand instruction ('add 10')
;   - Unknown instruction mnemonic ('foo 5')
;   - Undefined/missing label reference ('ldc missing_label')
;   - Invalid comment syntax ('// Wrong Comment format')
;   - Duplicate label definition ('dup:')
;   - Undefined label reference in data directive ('data hello')
;   - Standalone SET directive without preceding label ('SET 10')
;   - Invalid non-numeric SET directive value ('value: SET abc')
; ==============================================================================

1badlabel:
ldc 5

start:
ldc

add 10

foo 5

ldc missing_label

// Wrong Comment format

dup:
ldc 1

dup:
ldc 2

data hello

SET 10

value: SET abc

halt
