; bubblesort.asm - Bubble Sort implementation in IITPSx assembly
; Author: ARNAV RASTOGI
; User ID: ARNAV RASTOGI
; Declaration of Authorship:
; I hereby declare that this assembly source code is my own original
; work, developed for the CS2102 IITPSx Processor project.

start:
        ; set up stack at 0x1000
        ldc 0x1000
        a2sp
        adj -6          ; allocate 6 local variables

        ; load array size n = 10
        ldc count
        ldnl 0
        stl 2           ; SP+2 is n

        ; i = n - 1
        ldl 2
        adc -1
        stl 0           ; SP+0 is i

outer_loop:
        ; while (i > 0)
        ldl 0
        brz sort_done
        ldl 0
        brlz sort_done

        ; j = 0
        ldc 0
        stl 1           ; SP+1 is j

inner_loop:
        ; check if j < i by doing (i - j)
        ldl 0
        ldl 1
        sub
        brz inner_done
        brlz inner_done

        ; get pointer &array[j]
        ldc array
        ldl 1
        add
        stl 5           ; SP+5 is ptr_j

        ; load array[j] and array[j+1]
        ldl 5
        ldnl 0
        stl 3           ; SP+3 is elem_j

        ldl 5
        ldnl 1
        stl 4           ; SP+4 is elem_j1

        ; compare array[j] and array[j+1]
        ; if (array[j] > array[j+1]) swap
        ldl 4
        ldl 3
        sub
        brlz do_swap
        br no_swap

do_swap:
        ; swap the elements in memory
        ldl 4
        ldl 5
        stnl 0

        ldl 3
        ldl 5
        stnl 1

no_swap:
        ; j++
        ldl 1
        adc 1
        stl 1
        br inner_loop

inner_done:
        ; i--
        ldl 0
        adc -1
        stl 0
        br outer_loop

sort_done:
        adj 6           ; restore stack pointer
        HALT

; data
count:  data 10
array:  data 25
        data -4
        data 13
        data 0
        data 99
        data -50
        data 42
        data 8
        data 1
        data 7
