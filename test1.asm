label:  ; unused label
        ldc 0
        ldc -5
        ldc +5
loop:   br loop
        br next
next:
        ldc loop
        ldc var1
var1:   data 0
        HALT
