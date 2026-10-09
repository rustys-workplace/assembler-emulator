; ==============================================================================
; IITPSx Test Program: test1.asm
;
; Author: ARNAV RASTOGI
; User ID: ARNAV RASTOGI
; Declaration of Authorship:
; I hereby declare that this test code is my own original work, developed for
; the CS2102 IITPSx Processor project.
;
; Purpose:
;   Valid assembly test covering:
;   - Negative, zero, and positive immediate literals
;   - Unused label declaration (triggers warning)
;   - Forward and backward branch displacement calculations
;   - Backward jump infinite loop detection (triggers warning)
;   - Loading code labels and data labels as constant pointer addresses
; ==============================================================================

label:  ; Unused label (demonstrates warning generation)
        ldc 0           ; Load constant 0 into accumulator A
        ldc -5          ; Load signed negative constant -5 into A
        ldc +5          ; Load positive constant +5 into A
loop:   br loop         ; Unconditional backward branch to self (offset = -1)
        br next         ; Forward branch to next instruction (offset = 0)
next:
        ldc loop        ; Load code address of 'loop' as an immediate pointer
        ldc var1        ; Forward reference: load address of 'var1'
var1:   data 0          ; Memory word initialized to 0
        HALT            ; Terminate execution cleanly
