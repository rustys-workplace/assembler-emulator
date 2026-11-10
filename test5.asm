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
