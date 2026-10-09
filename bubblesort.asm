; ==============================================================================
; IITPSx In-Place Bubble Sort Program (bubblesort.asm)
;
; Author: ARNAV RASTOGI
; User ID: ARNAV RASTOGI
; Declaration of Authorship:
; I hereby declare that this assembly source code is entirely my own original
; work, developed for the CS2102 IITPSx Processor project.
;
; Description:
;   Implements a complete in-place Bubble Sort algorithm in IITPSx assembly language.
;   Sorts an array of 32-bit signed integers in ascending order.
;
; Memory Layout & Variables:
;   - count: Memory word storing the total number of elements (10).
;   - array: 10 consecutive memory words initialized to:
;            [25, -4, 13, 0, 99, -50, 42, 8, 1, 7]
;
; Stack Frame Allocation (at address 0x1000 = 4096):
;   SP + 0: Outer loop index / remaining passes counter 'i' (runs from n-1 down to 1)
;   SP + 1: Inner loop index 'j' (runs from 0 up to i-1)
;   SP + 2: Total array element count 'n' (10)
;   SP + 3: Value of element at array[j] ('elem_j')
;   SP + 4: Value of adjacent element at array[j + 1] ('elem_j1')
;   SP + 5: Absolute memory address pointer to array[j] ('ptr_j')
;
; Expected Output After Execution:
;   Array sorted ascending: [-50, -4, 0, 1, 7, 8, 13, 25, 42, 99]
; ==============================================================================

start:
        ; ----------------------------------------------------------------------
        ; Stack Initialization & Frame Setup
        ; ----------------------------------------------------------------------
        ldc 0x1000      ; Load accumulator A with high memory address 0x1000 (4096)
        a2sp            ; Transfer value in A to Stack Pointer SP (SP = 0x1000)
        adj -6          ; Allocate 6 local words on stack frame: slots SP+0 through SP+5

        ; ----------------------------------------------------------------------
        ; Load Array Length 'n' and Initialize Outer Counter 'i'
        ; ----------------------------------------------------------------------
        ldc count       ; A = memory address of label 'count'
        ldnl 0          ; A = memory[A + 0] = memory[count] = 10
        stl 2           ; Store array length into stack slot SP+2 (n = 10); A restored

        ldl 2           ; Load n (10) from stack slot SP+2 into accumulator A
        adc -1          ; Decrement: A = n - 1 = 9 (outer loop bound)
        stl 0           ; Store initial i = 9 into stack slot SP+0; A restored

outer_loop:
        ; ----------------------------------------------------------------------
        ; Outer Loop Termination Condition: While i > 0
        ; ----------------------------------------------------------------------
        ldl 0           ; Load current outer index 'i' into accumulator A
        brz sort_done   ; If i == 0, all passes complete -> jump to sort_done
        ldl 0           ; Reload 'i' into accumulator A
        brlz sort_done  ; If i < 0, safety check -> jump to sort_done

        ; ----------------------------------------------------------------------
        ; Inner Loop Initialization: j = 0
        ; ----------------------------------------------------------------------
        ldc 0           ; Load constant 0 into accumulator A
        stl 1           ; Store into stack slot SP+1 (inner counter j = 0); A restored

inner_loop:
        ; ----------------------------------------------------------------------
        ; Inner Loop Termination Condition: While j < i (i.e., (i - j) > 0)
        ; ----------------------------------------------------------------------
        ldl 0           ; Load 'i' into accumulator A
        ldl 1           ; Push 'i' to B, load 'j' from SP+1 into A (B = i, A = j)
        sub             ; Evaluate difference: A = B - A = (i - j)
        brz inner_done  ; If (i - j) == 0, inner pass is complete -> jump to inner_done
        brlz inner_done ; If (i - j) < 0, inner pass is complete -> jump to inner_done

        ; ----------------------------------------------------------------------
        ; Compute Pointer to array[j]: ptr_j = &array + j
        ; ----------------------------------------------------------------------
        ldc array       ; A = base address of array
        ldl 1           ; Push base address to B, load 'j' into A (B = &array, A = j)
        add             ; Compute address: A = B + A = &array + j
        stl 5           ; Store pointer into stack slot SP+5 (ptr_j); A restored

        ; ----------------------------------------------------------------------
        ; Load Adjacent Elements: elem_j = array[j], elem_j1 = array[j+1]
        ; ----------------------------------------------------------------------
        ldl 5           ; Load pointer 'ptr_j' into accumulator A
        ldnl 0          ; Load non-local: A = memory[ptr_j + 0] = array[j]
        stl 3           ; Store elem_j into stack slot SP+3; A restored

        ldl 5           ; Load pointer 'ptr_j' into accumulator A
        ldnl 1          ; Load non-local: A = memory[ptr_j + 1] = array[j + 1]
        stl 4           ; Store elem_j1 into stack slot SP+4; A restored

        ; ----------------------------------------------------------------------
        ; Compare Adjacent Elements: If (elem_j > elem_j1) Then Swap
        ; Mathematically equivalent to: (elem_j1 - elem_j) < 0
        ; ----------------------------------------------------------------------
        ldl 4           ; Load elem_j1 into accumulator A
        ldl 3           ; Push elem_j1 to B, load elem_j into A (B = elem_j1, A = elem_j)
        sub             ; Subtract: A = B - A = (elem_j1 - elem_j)
        brlz do_swap    ; If A < 0, then elem_j > elem_j1 -> branch to swap logic
        br no_swap      ; Otherwise elements are already in order -> skip swap

do_swap:
        ; ----------------------------------------------------------------------
        ; In-Place Swap:
        ;   memory[ptr_j + 0] = elem_j1
        ;   memory[ptr_j + 1] = elem_j
        ; ----------------------------------------------------------------------
        ldl 4           ; Load elem_j1 into accumulator A
        ldl 5           ; Push elem_j1 to B, load ptr_j into A (B = elem_j1, A = ptr_j)
        stnl 0          ; Store non-local: memory[ptr_j + 0] = B = elem_j1

        ldl 3           ; Load elem_j into accumulator A
        ldl 5           ; Push elem_j to B, load ptr_j into A (B = elem_j, A = ptr_j)
        stnl 1          ; Store non-local: memory[ptr_j + 1] = B = elem_j

no_swap:
        ; ----------------------------------------------------------------------
        ; Advance Inner Loop Index: j = j + 1
        ; ----------------------------------------------------------------------
        ldl 1           ; Load current 'j' into accumulator A
        adc 1           ; Increment: A = j + 1
        stl 1           ; Store updated 'j' back into stack slot SP+1; A restored
        br inner_loop   ; Repeat inner comparison loop

inner_done:
        ; ----------------------------------------------------------------------
        ; Advance Outer Loop Bound: i = i - 1
        ; ----------------------------------------------------------------------
        ldl 0           ; Load current outer bound 'i' into accumulator A
        adc -1          ; Decrement: A = i - 1
        stl 0           ; Store updated 'i' back into stack slot SP+0; A restored
        br outer_loop   ; Repeat outer pass loop

sort_done:
        ; ----------------------------------------------------------------------
        ; Clean Up Stack Frame and Halt Processor
        ; ----------------------------------------------------------------------
        adj 6           ; Deallocate 6 local stack words (restore SP = 0x1000)
        HALT            ; Stop processor execution cleanly

; ------------------------------------------------------------------------------
; Data Section
; ------------------------------------------------------------------------------
count:  data 10         ; Array length word initialized to 10
array:  data 25         ; Element 0:  25
        data -4         ; Element 1:  -4
        data 13         ; Element 2:  13
        data 0          ; Element 3:   0
        data 99         ; Element 4:  99
        data -50        ; Element 5: -50
        data 42         ; Element 6:  42
        data 8          ; Element 7:   8
        data 1          ; Element 8:   1
        data 7          ; Element 9:   7
