# RISC-V Single-Cycle CPU Implementation

This project implements a single-cycle RISC-V CPU capable of executing a subset of the RISC-V instruction set architecture (ISA). The implementation follows the datapath and control logic described in the CSE140 lectures.

## Supported Instructions

The CPU supports the following 10 RISC-V instructions:

1. `lw` - Load word
2. `sw` - Store word
3. `add` - Add
4. `addi` - Add immediate
5. `sub` - Subtract
6. `and` - Bitwise AND
7. `andi` - Bitwise AND immediate
8. `or` - Bitwise OR
9. `ori` - Bitwise OR immediate
10. `beq` - Branch if equal

## Project Structure

- `riscv_cpu.c` - Main implementation file containing all functions
- `Makefile` - For compiling the project
- `sample_program.txt` - Sample RISC-V binary program for testing

## Components

The CPU is implemented with the following components:

1. **Fetch** - Fetches instructions from the program memory based on PC
2. **Decode** - Decodes instructions and reads operands from register file
3. **Execute** - Performs ALU operations and calculates branch addresses
4. **Memory** - Accesses data memory for load/store operations
5. **Writeback** - Writes results back to the register file

## Global Variables

- `pc` - Program Counter
- `next_pc` - Next PC value (PC + 4)
- `branch_target` - Branch target address
- `rf` - Register file (32 registers)
- `d_mem` - Data memory (32 words)
- `alu_zero` - ALU zero flag
- `total_clock_cycles` - Total clock cycles executed

## Control Signals

- `branch` - Branch control signal
- `mem_read` - Memory read control signal
- `mem_to_reg` - Memory to register control signal
- `mem_write` - Memory write control signal
- `alu_src` - ALU source control signal
- `reg_write` - Register write control signal
- `alu_op` - ALU operation control signal

## How to Use

1. Compile the project:
   ```
   make
   ```

2. Run with a RISC-V binary program:
   ```
   ./riscv_cpu sample_program.txt
   ```

3. The program will execute each instruction and display the state of the CPU after each instruction.

## Input File Format

The input file should contain RISC-V instructions in binary format, one instruction per line. Each line should contain exactly 32 characters ('0' or '1') representing the 32-bit instruction.

## Initial State

- Register File:
  - x1 = 0x20
  - x2 = 0x5
  - x10 = 0x70
  - x11 = 0x4

- Data Memory:
  - Address 0x70 = 0x5
  - Address 0x74 = 0x10

## Implementation Details

### Instruction Fetch
The Fetch function reads one instruction from the program file per cycle. It updates the PC based on control signals.

### Instruction Decode
The Decode function extracts fields from the instruction and reads values from the register file. It also generates control signals through the ControlUnit function.

### Execute
The Execute function performs ALU operations and calculates the branch target address. It also sets the zero flag when the result is zero.

### Memory Access
The Mem function handles memory operations. It reads from or writes to the data memory based on control signals.

### Writeback
The Writeback function writes results back to the register file. It also increments the clock cycle counter.

## Limitations

- The CPU only supports a subset of the RISC-V ISA
- The memory size is limited to 32 words
- No pipelining or other performance optimizations are implemented