; ==============================================================================
; IITPSx Test Program: test6.asm
;
; Author: ARNAV RASTOGI
; User ID: ARNAV RASTOGI
; Declaration of Authorship:
; I hereby declare that this test code is my own original work, developed for
; the CS2102 IITPSx Processor project.
;
; Purpose:
;   Valid assembly test covering:
;   - Multiple isolated/empty labels on consecutive lines
;   - Instruction assembly and branch calculation
;   - Backward jump infinite loop detection (triggers warning)
;   - Unused label detection (triggers warning)
;   - Code following infinite branch
; ==============================================================================

unused1:
unused2:

ldc 5
ldc 10
add

loop:
br loop

afterLoop:
ldc 1
ldc 2
sub

neverUsedLabel:
ldc 3
halt
