; ==============================================================================
; IITPSx Test Program: test_poly.asm
;
; Author: ARNAV RASTOGI
; User ID: ARNAV RASTOGI
; Declaration of Authorship:
; I hereby declare that this test code is my own original work, developed for
; the CS2102 IITPSx Processor project.
;
; Purpose:
;   Algorithmic test case evaluating the second-degree polynomial:
;     P(x) = c2 * x^2 + c1 * x + c0
;   Extensively exercises the mul and add arithmetic instructions.
;
; Parameters:
;   c2 = 3
;   c1 = 5
;   c0 = 7
;   x  = 4
;
; Step-by-Step Mathematical Evaluation:
;   1. x^2           = 4 * 4  = 16
;   2. c2 * x^2      = 3 * 16 = 48
;   3. c1 * x        = 5 * 4  = 20
;   4. c2*x^2 + c1*x = 48 + 20 = 68
;   5. P(4)          = 68 + 7 = 75 (0x4B)
; ==============================================================================

start:
        ; Set stack pointer to address 0x1000 (4096)
        ldc 0x1000      ; A = 0x1000
        a2sp            ; SP = 0x1000
        adj -1          ; Reserve 1 stack slot at SP+0 for intermediate product

        ; ----------------------------------------------------------------------
        ; Term 1: Compute c2 * x^2 = 3 * (4 * 4) = 48
        ; ----------------------------------------------------------------------
        ldc 4           ; Load x = 4 into A
        ldc 4           ; B = 4, A = 4
        mul             ; A = 4 * 4 = 16 (x^2)
        ldc 3           ; B = 16, A = 3 (c2)
        mul             ; A = 16 * 3 = 48 (c2 * x^2)
        stl 0           ; Save intermediate result 48 into SP+0; A restored

        ; ----------------------------------------------------------------------
        ; Term 2: Compute c1 * x = 5 * 4 = 20
        ; ----------------------------------------------------------------------
        ldc 5           ; Load c1 = 5 into A
        ldc 4           ; B = 5, A = 4 (x)
        mul             ; A = 5 * 4 = 20 (c1 * x)

        ; ----------------------------------------------------------------------
        ; Accumulate: (c1 * x) + (c2 * x^2) = 20 + 48 = 68
        ; ----------------------------------------------------------------------
        ldl 0           ; B = 20, A = memory[SP + 0] = 48
        add             ; A = B + A = 20 + 48 = 68

        ; ----------------------------------------------------------------------
        ; Add constant term: c0 = 7
        ; ----------------------------------------------------------------------
        adc 7           ; A = 68 + 7 = 75 (0x4B)

        ; Store result into memory word 'result'
        ldc result      ; B = 75, A = address of 'result'
        stnl 0          ; memory[result + 0] = B = 75

        ; Clean up stack frame and halt with final answer in A
        adj 1           ; Restore stack pointer
        ldc result      ; Load address of result
        ldnl 0          ; Load computed result: A = 75
        HALT            ; Stop execution cleanly

; Data section: memory word to store final polynomial value
result: data 0          ; Initialized to 0, receives 75 upon completion
