; ==============================================================================
; IITPSx Test Program: test_sum.asm
;
; Author: ARNAV RASTOGI
; User ID: ARNAV RASTOGI
; Declaration of Authorship:
; I hereby declare that this test code is my own original work, developed for
; the CS2102 IITPSx Processor project.
;
; Purpose:
;   Algorithmic test case calculating the sum of the first N natural numbers
;   using an accumulator loop and local variables on the stack.
;
; Algorithm:
;   sum = 0
;   for (i = 1; i <= 10; i++) {
;       sum += i;
;   }
;   result = sum;  // 1 + 2 + ... + 10 = 55 (0x37)
; ==============================================================================

start:
        ; Initialize Stack Pointer to address 0x1000 (4096)
        ldc 0x1000      ; A = 0x1000
        a2sp            ; SP = A = 0x1000

        ; Allocate 3 local words on stack frame:
        ;   SP+0: loop counter i (1..10)
        ;   SP+1: running total sum
        ;   SP+2: upper bound N (10)
        adj -3          ; SP = SP - 3

        ; Set upper bound N = 10
        ldc 10          ; A = 10
        stl 2           ; memory[SP + 2] = 10 (N)

        ; Initialize sum = 0
        ldc 0           ; A = 0
        stl 1           ; memory[SP + 1] = 0 (sum)

        ; Initialize counter i = 1
        ldc 1           ; A = 1
        stl 0           ; memory[SP + 0] = 1 (i)

loop:
        ; Loop condition check: while (i <= N), or (N - i >= 0)
        ldl 2           ; A = N (10)
        ldl 0           ; B = N, A = i
        sub             ; A = B - A = (N - i)
        brlz done       ; If (N - i) < 0, then i > N -> exit loop

        ; Accumulate sum: sum = sum + i
        ldl 1           ; A = sum
        ldl 0           ; B = sum, A = i
        add             ; A = B + A = sum + i
        stl 1           ; memory[SP + 1] = updated sum

        ; Increment counter: i = i + 1
        ldl 0           ; A = i
        adc 1           ; A = i + 1
        stl 0           ; memory[SP + 0] = updated i

        br loop         ; Repeat iteration

done:
        ; Store final computed sum into result memory word
        ldl 1           ; A = final sum (55)
        ldc result      ; B = 55, A = address of 'result'
        stnl 0          ; memory[result + 0] = B = 55

        ; Deallocate stack frame and halt
        ldl 1           ; Leave 55 in accumulator A
        adj 3           ; Restore SP = 0x1000
        HALT            ; Terminate execution cleanly

; Data section: reserved memory word for output
result: data 0          ; Initialized to 0, receives 55 upon completion
