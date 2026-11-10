; test_sum.asm - calculates sum of 1 to 10
; Author: ARNAV RASTOGI
; User ID: ARNAV RASTOGI
; Declaration of Authorship:
; I hereby declare that this test code is my own original work, developed for
; the CS2102 IITPSx Processor project.

start:
        ldc 0x1000
        a2sp
        adj -3          ; SP+0: i, SP+1: sum, SP+2: N

        ldc 10          ; N = 10
        stl 2

        ldc 0           ; sum = 0
        stl 1

        ldc 1           ; i = 1
        stl 0

loop:
        ldl 2           ; check if i <= N
        ldl 0
        sub
        brlz done

        ; sum += i
        ldl 1
        ldl 0
        add
        stl 1

        ; i++
        ldl 0
        adc 1
        stl 0
        br loop

done:
        ldl 1
        ldc result
        stnl 0
        ldl 1
        adj 3
        HALT

result: data 0
