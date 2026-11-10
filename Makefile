# Makefile for IITPSx Processor Toolchain
# Author: ARNAV RASTOGI
# User ID: ARNAV RASTOGI
# Declaration of Authorship:
# I hereby declare that this work is my own original work,
# written for the CS2102 IITPSx Processor project.

CC      = gcc
CFLAGS  = -std=c89 -pedantic -Wall -Wextra -Wpointer-arith -Wwrite-strings -Wstrict-prototypes -O2
RM      = rm -f

PROGS   = asm emu

.PHONY: all clean test help serve web

all: $(PROGS)

asm: asm.c
	$(CC) $(CFLAGS) -o asm asm.c

emu: emu.c
	$(CC) $(CFLAGS) -o emu emu.c

test: all
	@echo "=== Running tests ==="
	@echo ""
	@echo "--- test1: basic test ---"
	./asm test1.asm
	@echo ""
	@echo "--- test2: error check test ---"
	-./asm test2.asm
	@echo ""
	@echo "--- test3: SET instruction ---"
	./asm test3.asm
	./emu -trace -limit 10 test3.o
	@echo ""
	@echo "--- test4: mul and div ---"
	./asm test4.asm
	./emu -trace test4.o
	@echo ""
	@echo "--- test_sum: loop sum 1..10 ---"
	./asm test_sum.asm
	./emu -dump test_sum.o
	@echo ""
	@echo "--- test_poly: polynomial P(4) ---"
	./asm test_poly.asm
	./emu -trace test_poly.o
	@echo ""
	@echo "--- test_divzero: divide by zero ---"
	./asm test_divzero.asm
	-./emu test_divzero.o
	@echo ""
	@echo "--- test_triangle: pascal triangle ---"
	./asm test_triangle.asm
	./emu test_triangle.o
	@echo ""
	@echo "--- bubblesort: bubble sort array ---"
	./asm bubblesort.asm
	./emu -dump bubblesort.o
	@echo ""
	@echo "--- test5: loop sum ---"
	./asm test5.asm
	./emu -dump test5.o
	@echo ""
	@echo "--- test6: labels and branch ---"
	./asm test6.asm
	@echo ""
	@echo "--- test7: error test ---"
	-./asm test7.asm
	@echo ""
	@echo "=== All tests finished ==="

serve:
	python3 -m http.server 8000

web: serve

clean:
	$(RM) $(PROGS) *.o *.lst *.log *.emu.log

help:
	@echo "all   : build asm and emu"
	@echo "test  : run test cases"
	@echo "clean : remove object and log files"
