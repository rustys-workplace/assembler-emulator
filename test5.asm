; ==============================================================================
; IITPSx Test Program: test5.asm
;
; Author: ARNAV RASTOGI
; User ID: ARNAV RASTOGI
; Declaration of Authorship:
; I hereby declare that this test code is my own original work, developed for
; the CS2102 IITPSx Processor project.
;
; Purpose:
;   Valid assembly test covering:
;   - SET pseudo-instruction defining constant value
;   - data directive reserving memory word
;   - Local stack variable operations (ldl, stl)
;   - Backward branch loop calculation
;   - Storing final result into data memory word (stnl)
; ==============================================================================

count:  SET 10
sum:    data 0

start:
ldc count
stl 0

ldc 0
stl 1

loop:
ldl 0
brz done

ldl 1
ldl 0
add
stl 1

ldl 0
adc -1
stl 0

br loop

done:
ldl 1
ldc sum
stnl 0

halt
