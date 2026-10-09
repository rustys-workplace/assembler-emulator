# ==============================================================================
# Makefile for IITPSx Processor Toolchain
#
# Author: ARNAV RASTOGI
# User ID: ARNAV RASTOGI
# Declaration of Authorship:
# I hereby declare that this work is entirely my own original work, written in
# strict ISO C89 compliance (-std=c89 -pedantic) for the CS2102 IITPSx Processor.
# ==============================================================================

CC      = gcc
CFLAGS  = -std=c89 -pedantic -Wall -Wextra -Wpointer-arith -Wwrite-strings -Wstrict-prototypes -O2
RM      = rm -f

PROGS   = asm emu
TESTS   = test1 test3 test4 test5 test6 test_sum test_poly test_triangle bubblesort

.PHONY: all clean test verify help

all: $(PROGS)

asm: asm.c
	$(CC) $(CFLAGS) -o asm asm.c

emu: emu.c
	$(CC) $(CFLAGS) -o emu emu.c

# Execute full automated test and verification suite
test: all
	@echo "======================================================================"
	@echo "                   RUNNING IITPSx TEST SUITE                          "
	@echo "======================================================================"
	@echo ""
	@echo "[1/12] Assembling test1.asm (negative literals, branches, labels)..."
	./asm test1.asm
	@echo ""
	@echo "[2/12] Testing test2.asm (error checking suite)..."
	-./asm test2.asm
	@echo ""
	@echo "[3/12] Assembling and Executing test3.asm (SET instruction demonstration)..."
	./asm test3.asm
	./emu -trace -limit 10 test3.o
	@echo ""
	@echo "[4/12] Assembling and Executing test4.asm (mul & div arithmetic -> A=21)..."
	./asm test4.asm
	./emu -trace test4.o
	@echo ""
	@echo "[5/12] Assembling and Executing test_sum.asm (Sum of 1..10 = 55)..."
	./asm test_sum.asm
	./emu -dump test_sum.o
	@echo ""
	@echo "[6/12] Assembling and Executing test_poly.asm (Polynomial P(4) = 75)..."
	./asm test_poly.asm
	./emu -trace test_poly.o
	@echo ""
	@echo "[7/12] Testing test_divzero.asm (deliberate division by zero detection)..."
	./asm test_divzero.asm
	-./emu test_divzero.o
	@echo ""
	@echo "[8/12] Assembling and Executing test_triangle.asm (Pascal's triangle row 10)..."
	./asm test_triangle.asm
	./emu test_triangle.o
	@echo ""
	@echo "[9/12] Assembling and Executing bubblesort.asm (in-place Bubble Sort)..."
	./asm bubblesort.asm
	./emu -dump bubblesort.o
	@echo ""
	@echo "[10/12] Assembling and Executing test5.asm (SET, data word, loop sum = 55)..."
	./asm test5.asm
	./emu -dump test5.o
	@echo ""
	@echo "[11/12] Assembling test6.asm (multiple empty labels, infinite branch, unused labels)..."
	./asm test6.asm
	@echo ""
	@echo "[12/12] Testing test7.asm (additional comprehensive error checking suite)..."
	-./asm test7.asm
	@echo ""
	@echo "======================================================================"
	@echo "                   ALL TESTS COMPLETED SUCCESSFULLY                   "
	@echo "======================================================================"

clean:
	$(RM) $(PROGS) *.o *.lst *.log *.emu.log

help:
	@echo "Targets:"
	@echo "  all     - Build asm and emu binaries"
	@echo "  test    - Run complete automated test suite"
	@echo "  clean   - Remove generated binaries and test logs"
