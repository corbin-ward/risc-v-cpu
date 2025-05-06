# RISC-V Single-Cycle CPU Implementation

This project implements a single-cycle RISC-V CPU capable of executing a subset of the RISC-V instruction set architecture (ISA). The implementation follows the datapath and control logic described in the CSE140 lectures and has been enhanced with additional instructions and features.

## Supported Instructions

The CPU supports the following 12 RISC-V instructions:

1.  `lw` - Load word
2.  `sw` - Store word
3.  `add` - Add
4.  `addi` - Add immediate
5.  `sub` - Subtract
6.  `and` - Bitwise AND
7.  `andi` - Bitwise AND immediate
8.  `or` - Bitwise OR
9.  `ori` - Bitwise OR immediate
10. `beq` - Branch if equal
11. `jal` - Jump and Link
12. `jalr` - Jump and Link Register

## Project Structure

- `riscv_cpu.c` - Main implementation file containing all functions
- `Makefile` - For compiling the project
- `sample_program.txt` - Sample RISC-V binary program for testing (additional samples like `sample_part1.txt` and `sample_part2.txt` may be used, influencing initial state)

## Components

The CPU is implemented with the following components:

1.  **Fetch** - Fetches instructions from the program memory based on PC. Includes checks for fetching beyond program boundaries.
2.  **Decode** - Decodes instructions, reads operands from the register file, and generates control signals. Includes detailed immediate generation for various instruction types (I, S, SB, UJ) using a `sign_extend` utility function.
3.  **Execute** - Performs ALU operations and calculates branch/jump addresses. ALU operation is determined by a dedicated `ALUControl` function based on instruction fields.
4.  **Memory** - Accesses data memory for load/store operations. Includes checks for unaligned memory access and out-of-bounds access.
5.  **Writeback** - Writes results back to the register file, including handling links for `jal` and `jalr`.

## Global Variables

- `pc` - Program Counter
- `next_pc` - Next PC value (PC + 4)
- `branch_target` - Branch target address (for BEQ)
- `jump_target` - Jump target address (for JAL/JALR)
- `rf` - Register file (32 registers)
- `d_mem` - Data memory (32 words)
- `alu_zero` - ALU zero flag
- `total_clock_cycles` - Total clock cycles executed

## Control Signals

- `branch` - Branch control signal (for BEQ)
- `mem_read` - Memory read control signal
- `mem_to_reg` - Memory to register control signal
- `mem_write` - Memory write control signal
- `alu_src` - ALU source control signal
- `reg_write` - Register write control signal
- `ALUOp0` - ALU operation control signal (bit 0)
- `ALUOp1` - ALU operation control signal (bit 1)
- `Jump` - Control signal for unconditional jump (JAL/JALR)

## How to Use

1.  Compile the project:
    ```
    make
    ```
   
2.  Run with a RISC-V binary program (e.g., `sample_program.txt`, `sample_part1.txt`, or `sample_part2.txt`):
    ```
    ./riscv_cpu sample_program.txt
    ```
   
3.  The program will execute each instruction and display the state of the CPU after each instruction. The output includes ABI names for registers for better readability.

## Input File Format

The input file should contain RISC-V instructions in binary format, one instruction per line. Each line should contain exactly 32 characters ('0' or '1') representing the 32-bit instruction. The `read_program` function handles loading these instructions.

## Initial State

The initial state of registers and data memory can vary based on the input program file (e.g., specific initializations for `part1` vs. `part2` sample programs are handled in `main`).

Example Initial State (for `sample_part1.txt` as per `riscv_cpu.c`):
- Register File:
  - x1 (ra) = 0x20
  - x2 (sp) = 0x5
  - x10 (a0) = 0x70
  - x11 (a1) = 0x4
- Data Memory:
  - Address 0x70 (index 28) = 0x5
  - Address 0x74 (index 29) = 0x10

## Implementation Details

### Instruction Fetch
The Fetch function reads one instruction from the program file per cycle. It updates the PC based on control signals and includes a check to prevent fetching beyond program boundaries, returning a NOP if an attempt is made.

### Instruction Decode
The Decode function extracts fields (opcode, rd, rs1, rs2, funct3, funct7) from the instruction and reads values from the register file. It generates control signals through the `ControlUnit` function. It also handles the generation and sign-extension of immediate values for I-type, S-type, SB-type (beq), and UJ-type (jal) instructions using a dedicated `sign_extend` function.

### Execute
The Execute function performs ALU operations. The specific ALU operation (ADD, SUB, AND, OR) is determined by the `ALUControl` function, which takes `ALUOp` signals, `funct3`, and `funct7` as input. It calculates branch target addresses for `beq` and jump target addresses for `jal` and `jalr`. The `alu_zero` flag is set based on the comparison result for `beq` instructions.

### Memory Access
The Mem function handles memory operations for `lw` and `sw` instructions. It reads from or writes to the data memory based on control signals. It includes error checking for unaligned memory access (address not word-aligned) and out-of-bounds memory access (address outside the 32-word memory space).

### Writeback
The Writeback function writes results back to the register file (if `RegWrite` is asserted and `rd` is not x0). The data written can be from the ALU result, data memory (for `lw`), or PC+4 (for `jal`/`jalr` link address). It updates the PC to `next_pc`, `branch_target` (if branch taken), or `jump_target` (if jump taken). It also increments the `total_clock_cycles` counter.

## Limitations

- The CPU still supports a subset of the full RISC-V ISA.
- The data memory size is limited to 32 words.
- No pipelining or other advanced performance optimizations are implemented; it remains a single-cycle design.