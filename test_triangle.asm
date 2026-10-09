; ==============================================================================
; IITPSx Test Program: test_triangle.asm
;
; Author: ARNAV RASTOGI
; User ID: ARNAV RASTOGI
; Declaration of Authorship:
; I hereby declare that this test code is my own original work, developed for
; the CS2102 IITPSx Processor project.
;
; Purpose:
;   Calculates Pascal's triangle row 10 combinatorial values.
;   Exercises procedure calls ('call' / 'return'), nested stack frames,
;   stack adjustments ('adj'), and non-local memory access ('ldnl' / 'stnl').
; ==============================================================================

        ldc 0x1000      ; Set initial stack pointer to 0x1000
        a2sp            ; SP = 0x1000
        adj -1          ; Allocate slot for result pointer
        ldc result      ; A = address of result
        stl 0           ; Save result pointer on stack
        ldc count       ; A = address of count
        ldnl 0          ; A = memory[count] = 10
        call main       ; Call main procedure
        adj 1           ; Clean up frame
        HALT            ; Clean exit
;
main:   adj -3
        stl 1
        stl 2
        ldc 0           ; Zero accumulator
        stl 0
loop:   adj -1
        ldl 3
        stl 0
        ldl 1
        call triangle
        adj 1
        ldl 3
        stnl 0
        ldl 3
        adc 1
        stl 3
        ldl 0
        adc 1
        stl 0
        ldl 0           ; Reload it
        ldl 2
        sub
        brlz loop
        ldl 1           ; Get return address
        adj 3
        return
;
triangle:adj -3
        stl 1
        stl 2
        ldc 1
        shl
        ldl 3
        sub
        brlz skip
        ldl 3
        ldl 2
        sub
        stl 2
skip:   ldl 2
        brz one
        ldl 3
        adc -1
        stl 0
        adj -1
        ldl 1
        stl 0
        ldl 3
        adc -1
        call triangle
        ldl 1
        stl 0
        stl 1
        ldl 3
        call triangle
        adj 1
        ldl 0
        add
        ldl 1
        adj 3
        return
one:    ldc 1
        ldl 1
        adj 3
        return
;
count:  data 10
result: data 0
