; ==============================================================================
; IITPSx Test Program: test_divzero.asm
;
; Author: ARNAV RASTOGI
; User ID: ARNAV RASTOGI
; Declaration of Authorship:
; I hereby declare that this test code is my own original work, developed for
; the CS2102 IITPSx Processor project.
;
; Purpose:
;   Deliberate division-by-zero test case.
;   Tests the architectural emulator's zero-division detection and fault handling.
;   When 'div' executes with accumulator A == 0, the emulator must:
;     1. Halt execution immediately.
;     2. Print a runtime error message to stderr.
;     3. Output the processor register state.
;     4. Exit with a non-zero exit code.
; ==============================================================================

start:
        ldc 100         ; Load dividend 100 into accumulator A
        ldc 0           ; Push 100 to B, load divisor 0 into A (B = 100, A = 0)
        div             ; Evaluates B / A -> Triggers Runtime Error: Division by zero
        HALT            ; Unreachable halt
