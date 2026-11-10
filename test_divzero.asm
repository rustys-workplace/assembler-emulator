; test_divzero.asm - division by zero test
; Author: ARNAV RASTOGI
; User ID: ARNAV RASTOGI
; Declaration of Authorship:
; I hereby declare that this test code is my own original work, developed for
; the CS2102 IITPSx Processor project.

start:
        ldc 100
        ldc 0
        div             ; should trigger division by zero error
        HALT
