; test_poly.asm - evaluates polynomial P(x) = 3*x^2 + 5*x + 7 for x=4
; Author: ARNAV RASTOGI
; User ID: ARNAV RASTOGI
; Declaration of Authorship:
; I hereby declare that this test code is my own original work, developed for
; the CS2102 IITPSx Processor project.

start:
        ldc 0x1000
        a2sp
        adj -1          ; slot for intermediate product

        ; compute 3 * x^2 (where x=4)
        ldc 4
        ldc 4
        mul
        ldc 3
        mul
        stl 0           ; save 3*x^2 = 48

        ; compute 5 * x
        ldc 5
        ldc 4
        mul             ; 5 * 4 = 20

        ; add 3*x^2
        ldl 0
        add             ; 20 + 48 = 68

        ; add constant 7
        adc 7           ; 68 + 7 = 75

        ; store result
        ldc result
        stnl 0

        adj 1
        ldc result
        ldnl 0
        HALT

result: data 0
