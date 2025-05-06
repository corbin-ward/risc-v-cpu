CC = gcc
CFLAGS = -Wall -Wextra -g

all: riscv_cpu

riscv_cpu: riscv_cpu.c
	$(CC) $(CFLAGS) -o riscv_cpu riscv_cpu.c

clean:
	rm -f riscv_cpu