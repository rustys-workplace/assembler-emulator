; ==============================================================================
; IITPSx Test Program: test4.asm
;
; Author: ARNAV RASTOGI
; User ID: ARNAV RASTOGI
; Declaration of Authorship:
; I hereby declare that this test code is my own original work, developed for
; the CS2102 IITPSx Processor project.
;
; Purpose:
;   Demonstrates the two IITPSx arithmetic extensions:
;   - mul (Opcode 19): Stack multiplication (A = B * A)
;   - div (Opcode 20): Stack division (A = B / A)
;
; Computation:
;   1. ldc 6  -> A = 6
;   2. ldc 7  -> B = 6, A = 7
;   3. mul    -> A = 6 * 7 = 42
;   4. ldc 2  -> B = 42, A = 2
;   5. div    -> A = 42 / 2 = 21 (0x15)
;   6. HALT   -> Terminates with A = 21.
; ==============================================================================

ldc 6           ; Load 6 into A
ldc 7           ; B = 6, A = 7
mul             ; Evaluate: A = B * A = 6 * 7 = 42
ldc 2           ; B = 42, A = 2
div             ; Evaluate: A = B / A = 42 / 2 = 21
HALT            ; Terminate cleanly leaving 21 (0x15) in accumulator A
