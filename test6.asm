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
