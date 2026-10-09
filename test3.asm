; ==============================================================================
; IITPSx Test Program: test3.asm
;
; Author: ARNAV RASTOGI
; User ID: ARNAV RASTOGI
; Declaration of Authorship:
; I hereby declare that this test code is my own original work, developed for
; the CS2102 IITPSx Processor project.
;
; Purpose:
;   Demonstrates implementation of the SET pseudo-instruction directive.
;   - val is assigned the constant value 75 via SET.
;   - val2 is assigned the constant value 66 via SET (forward reference).
;   - Accumulator A computes 75 + 66 = 141 (0x8D).
;   - SET does not emit machine code words or advance the PC.
; ==============================================================================

val:    SET 75          ; Assign constant 75 to label 'val' without advancing PC
        ldc val         ; Load constant 'val' (75) into accumulator A
        adc val2        ; Add constant 'val2' (66) to A -> A = 75 + 66 = 141 (0x8D)
        HALT            ; Stop emulator execution cleanly
val2:   SET 66          ; Forward-defined constant: assign 66 to label 'val2'
