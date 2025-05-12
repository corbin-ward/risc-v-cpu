#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Global variables
uint32_t pc = 0;                // Program counter
uint32_t next_pc = 0;           // Next program counter (PC + 4)
uint32_t branch_target = 0;     // Branch target address (for BEQ)
uint32_t jump_target = 0;       // Jump target address (for JAL/JALR)
int alu_zero = 0;               // ALU zero flag
int total_clock_cycles = 0;     // Total clock cycles

// Control signals
int RegWrite = 0;               // Control signal for register write
int MemtoReg = 0;               // Control signal for memory to register
int MemRead = 0;                // Control signal for memory read
int MemWrite = 0;               // Control signal for memory write
int ALUSrc = 0;                 // Control signal for ALU source (0: rs2, 1: imm)
int Branch = 0;                 // Control signal for branch (BEQ)
int ALUOp0 = 0;                 // Control signal for ALU operation (bit 0)
int ALUOp1 = 0;                 // Control signal for ALU operation (bit 1)
int Jump = 0;                   // Control signal for unconditional jump (JAL/JALR)

// Memory and registers
int rf[32] = {0};               // Register file (32 registers)
int d_mem[32] = {0};            // Data memory (32 words)

// Instruction memory - will be filled from input file
uint32_t instr_mem[100];        // Instruction memory
int instr_count = 0;            // Number of instructions (signed int is okay here)

// Function to extract bits from instruction
uint32_t extract_bits(uint32_t instruction, int start, int length) {
    return (instruction >> start) & ((1UL << length) - 1);
}

// Function to sign extend a value
int32_t sign_extend(uint32_t value, int bit_width) {
    // If the most significant bit (sign bit) is 1
    if ((value >> (bit_width - 1)) & 1) {
        // Extend the sign bit to 32 bits
        return (int32_t)(value | (~0U << bit_width));
    }
    return (int32_t)value;
}


// Function to set ALU control based on ALUOp and funct fields
uint32_t ALUControl(int ALUOp0, int ALUOp1, uint32_t funct3, uint32_t funct7) {
    if (ALUOp0 == 0 && ALUOp1 == 0) { // Load/Store/JALR
        return 0b0010; // ADD (for address calculation)
    } else if (ALUOp0 == 1 && ALUOp1 == 0) { // Branch (BEQ)
        return 0b0110; // SUB (for comparison)
    } else if (ALUOp0 == 0 && ALUOp1 == 1) { // R-type or I-type ALU
        if (funct3 == 0) { // add, addi, sub
            if (funct7 == 0x20 && !ALUSrc) { // Check ALUSrc to distinguish sub from addi
                 return 0b0110; // sub
            } else {
                 return 0b0010; // add, addi
            }
        } else if (funct3 == 7) { // and, andi
            return 0b0000; // and
        } else if (funct3 == 6) { // or, ori
            return 0b0001; // or
        }
    }
    // Default or unrecognized, default to ADD
    // printf("Warning: Unhandled ALUOp/funct combination. Defaulting to ADD.\n");
    return 0b0010;
}

// Control Unit function
void ControlUnit(uint32_t opcode) {
    // Reset all control signals
    RegWrite = 0;
    MemtoReg = 0;
    MemRead = 0;
    MemWrite = 0;
    ALUSrc = 0;
    Branch = 0;
    ALUOp0 = 0;
    ALUOp1 = 0;
    Jump = 0; // Reset Jump signal

    // Set control signals based on opcode
    switch (opcode) {
        case 0x33: // R-type (add, sub, and, or)
            RegWrite = 1;
            ALUOp1 = 1; // ALU performs R-type operation
            break;
        case 0x13: // I-type arithmetic (addi, andi, ori)
            RegWrite = 1;
            ALUSrc = 1; // ALU source is immediate
            ALUOp1 = 1; // ALU performs I-type operation
            break;
        case 0x03: // I-type load (lw)
            RegWrite = 1;
            ALUSrc = 1;  // ALU source is immediate (for address calc)
            MemRead = 1;
            MemtoReg = 1; // Data comes from memory
            ALUOp0 = 0; ALUOp1 = 0; // ALU does ADD for address calculation
            break;
        case 0x23: // S-type (sw)
            ALUSrc = 1; // ALU source is immediate (for address calc)
            MemWrite = 1;
            ALUOp0 = 0; ALUOp1 = 0; // ALU does ADD for address calculation
            break;
        case 0x63: // SB-type (beq)
            Branch = 1;
            ALUOp0 = 1; // ALU does SUB for comparison
            break;
        case 0x6F: // J-type (JAL) - UJ-type technically
             RegWrite = 1; // Writes PC+4 to rd
             Jump = 1;     // Unconditional jump
             // No ALU operation needed for target address calculation (PC + imm)
             // No memory access
             break;
        case 0x67: // I-type (JALR)
             RegWrite = 1; // Writes PC+4 to rd
             Jump = 1;     // Unconditional jump
             ALUSrc = 1;   // ALU source is immediate (offset)
             ALUOp0 = 0; ALUOp1 = 0; // ALU does ADD for target address (rs1 + imm)
             break;
        default:
            printf("Unknown opcode: 0x%x\n", opcode);
            // Potentially halt or handle error
            break;
    }
}

// Fetch function
uint32_t Fetch() {
    // Cast instr_count to uint32_t for comparison to avoid sign-compare warning
    if ((pc / 4) >= (uint32_t)instr_count) {
         printf("Attempting to fetch beyond program boundary. PC=0x%x\n", pc);
         // Handle end of program, maybe return a NOP or specific error code
         return 0; // Return NOP (addi x0, x0, 0)
    }
    // Get instruction from instruction memory
    uint32_t instruction = instr_mem[pc / 4];

    // Update next PC (potential value for non-branch/jump or link register)
    next_pc = pc + 4;

    return instruction;
}

// Decode function
void Decode(uint32_t instruction, int *rs1_val, int *rs2_val, uint32_t *rd, uint32_t *rs1, uint32_t *rs2, uint32_t *funct3, uint32_t *funct7, int *imm) {
    // Extract fields from instruction
    uint32_t opcode = instruction & 0x7F;
    *rd = (instruction >> 7) & 0x1F;
    *funct3 = (instruction >> 12) & 0x7;
    *rs1 = (instruction >> 15) & 0x1F;
    *rs2 = (instruction >> 20) & 0x1F;
    *funct7 = (instruction >> 25) & 0x7F;

    // Call Control Unit to set control signals based on opcode
    ControlUnit(opcode);

    // Read values from register file (handle x0)
    *rs1_val = (*rs1 == 0) ? 0 : rf[*rs1];
    *rs2_val = (*rs2 == 0) ? 0 : rf[*rs2];

    // Immediate generation and sign extension based on instruction type/opcode
    *imm = 0; // Default immediate
    if (opcode == 0x13 || opcode == 0x03 || opcode == 0x67) { // I-type (addi, lw, jalr, etc.)
        uint32_t imm_i = extract_bits(instruction, 20, 12);
        *imm = sign_extend(imm_i, 12);
    } else if (opcode == 0x23) { // S-type (sw)
        uint32_t imm_4_0 = extract_bits(instruction, 7, 5);
        uint32_t imm_11_5 = extract_bits(instruction, 25, 7);
        uint32_t imm_s = (imm_11_5 << 5) | imm_4_0;
        *imm = sign_extend(imm_s, 12);
    } else if (opcode == 0x63) { // SB-type (beq)
        uint32_t imm_11   = extract_bits(instruction, 7, 1);
        uint32_t imm_4_1  = extract_bits(instruction, 8, 4);
        uint32_t imm_10_5 = extract_bits(instruction, 25, 6);
        uint32_t imm_12   = extract_bits(instruction, 31, 1); // imm[12] is bit 31
        uint32_t imm_b = (imm_12 << 12) | (imm_11 << 11) | (imm_10_5 << 5) | (imm_4_1 << 1);
        *imm = sign_extend(imm_b, 13); // Branch immediate is 13 bits
    } else if (opcode == 0x6F) { // UJ-type (JAL)
        uint32_t imm_19_12 = extract_bits(instruction, 12, 8);
        uint32_t imm_11    = extract_bits(instruction, 20, 1);
        uint32_t imm_10_1  = extract_bits(instruction, 21, 10);
        uint32_t imm_20    = extract_bits(instruction, 31, 1); // imm[20] is bit 31
        // Reconstruct the immediate: imm[20|10:1|11|19:12]0
        uint32_t imm_j = (imm_20 << 20) | (imm_19_12 << 12) | (imm_11 << 11) | (imm_10_1 << 1);
        *imm = sign_extend(imm_j, 21); // JAL immediate is 21 bits
    }
}


// Execute function - Removed unused rs1 and rs2 index parameters
int Execute(int rs1_val, int rs2_val, int imm, uint32_t funct3, uint32_t funct7) {
    int alu_result = 0;
    // Operand1 is always rs1_val (or PC for some calculations not done in ALU)
    // Operand2 depends on ALUSrc
    int operand2 = ALUSrc ? imm : rs2_val;

    // Determine ALU operation based on control signals and funct fields
    uint32_t alu_ctrl = ALUControl(ALUOp0, ALUOp1, funct3, funct7);

    // Perform ALU operation
    switch (alu_ctrl) {
        case 0b0000: // AND
            alu_result = rs1_val & operand2;
            break;
        case 0b0001: // OR
            alu_result = rs1_val | operand2;
            break;
        case 0b0010: // ADD
            alu_result = rs1_val + operand2;
            break;
        case 0b0110: // SUB
            alu_result = rs1_val - operand2;
            break;
        default:
            printf("Unknown ALU control: 0x%x\n", alu_ctrl);
            alu_result = 0; // Default result
            break;
    }

    // Set zero flag based on the SUB operation for BEQ
    if (Branch) { // Only set zero flag based on comparison for BEQ
        alu_zero = ((rs1_val - rs2_val) == 0); // BEQ compares rs1 and rs2 directly
    } else {
        alu_zero = 0; // Ensure alu_zero is not set by other instructions
    }


    // Calculate branch/jump targets
    if (Branch) {
        branch_target = pc + imm; // BEQ target = PC + sign_extended_offset
    }
    if (Jump) {
         // Cast instr_count to uint32_t for comparison
        uint32_t current_instr_index = pc / 4;
        if (current_instr_index >= (uint32_t)instr_count) {
             printf("Error: Trying to decode instruction for Jump target calculation beyond program boundary.\n");
             // Handle appropriately, maybe set jump_target to a safe default or error state
             jump_target = pc + 4; // Default to next instruction to prevent crash
        } else {
             uint32_t instruction = instr_mem[current_instr_index];
             uint32_t opcode = instruction & 0x7F; // Get opcode again to differentiate JAL/JALR
             if (opcode == 0x6F) { // JAL
                 jump_target = pc + imm; // JAL target = PC + sign_extended_offset
             } else if (opcode == 0x67) { // JALR
                 // Target = (rs1_val + imm) & ~1 (set LSB to 0)
                 jump_target = (rs1_val + imm) & (~1U);
             }
        }
    }


    return alu_result; // Return ALU result (used for R/I-type, Mem Addr, JALR base)
}


// Memory function
int Mem(int alu_result, int rs2_val) {
    int mem_data = 0;

    // Check for memory access validity (address should be word-aligned and within bounds)
     if ((MemRead || MemWrite)) {
          if(alu_result % 4 != 0) {
               printf("Error: Unaligned memory access at address 0x%x\n", alu_result);
               // Handle error appropriately, maybe exit or return specific error code
               return 0; // Or some error indicator
          }
          int mem_index = alu_result / 4;
          if (mem_index < 0 || mem_index >= 32) {
               printf("Error: Memory access out of bounds. Address: 0x%x, Index: %d\n", alu_result, mem_index);
               // Handle error appropriately
               return 0; // Or some error indicator
          }

          // Proceed with memory operation
          if (MemRead) {
               mem_data = d_mem[mem_index];
              // printf("MEM: Read 0x%x from address 0x%x (index %d)\n", mem_data, alu_result, mem_index);
          }
          if (MemWrite) {
               d_mem[mem_index] = rs2_val;
              // printf("MEM: Wrote 0x%x to address 0x%x (index %d)\n", rs2_val, alu_result, mem_index);
          }
     }

    return mem_data;
}

// Writeback function
void Writeback(uint32_t rd, int alu_result, int mem_data) {
    int write_data = 0;

    // Determine data to write back
    if (Jump) { // For JAL/JALR, write PC+4 (stored in next_pc)
        write_data = next_pc;
    } else if (MemtoReg) { // For LW
        write_data = mem_data;
    } else { // For R-type and I-type ALU
        write_data = alu_result;
    }

    // Write to register file if RegWrite is asserted and rd is not x0
    if (RegWrite && rd != 0) {
        //printf("WB: Writing 0x%x to x%u\n", write_data, rd);
        rf[rd] = write_data;
    }

    // Update PC for the next cycle
    if (Jump) { // JAL or JALR taken
        pc = jump_target;
        //printf("WB: Jumping to 0x%x\n", pc);
    } else if (Branch && alu_zero) { // BEQ taken
        pc = branch_target;
        //printf("WB: Branching to 0x%x\n", pc);
    } else { // Default: PC = PC + 4
        pc = next_pc;
       // printf("WB: Proceeding to 0x%x\n", pc);
    }

    // Increment clock cycle count AFTER the instruction completes
    total_clock_cycles++;
}


// Function to decode and print instruction information
void print_instruction(uint32_t instruction) {
    uint32_t opcode = instruction & 0x7F;
    uint32_t rd = (instruction >> 7) & 0x1F;
    uint32_t funct3 = (instruction >> 12) & 0x7;
    uint32_t rs1 = (instruction >> 15) & 0x1F;
    uint32_t rs2 = (instruction >> 20) & 0x1F;
    uint32_t funct7 = (instruction >> 25) & 0x7F;
    int imm_i, imm_s, imm_b, imm_j; // Signed immediates - removed unused imm_u

    // Calculate immediates for printing
    imm_i = sign_extend(extract_bits(instruction, 20, 12), 12); // I-type, JALR
    uint32_t imm_s_raw = (extract_bits(instruction, 25, 7) << 5) | extract_bits(instruction, 7, 5);
    imm_s = sign_extend(imm_s_raw, 12); // S-type
    uint32_t imm_b_raw = (extract_bits(instruction, 31, 1) << 12) | (extract_bits(instruction, 7, 1) << 11) | (extract_bits(instruction, 25, 6) << 5) | (extract_bits(instruction, 8, 4) << 1);
    imm_b = sign_extend(imm_b_raw, 13); // SB-type
    uint32_t imm_j_raw = (extract_bits(instruction, 31, 1) << 20) | (extract_bits(instruction, 12, 8) << 12) | (extract_bits(instruction, 20, 1) << 11) | (extract_bits(instruction, 21, 10) << 1);
    imm_j = sign_extend(imm_j_raw, 21); // UJ-type (JAL)


    printf("--- Instruction 0x%08x (@PC=0x%x) ---\n", instruction, pc);

    switch (opcode) {
        case 0x33: // R-type
            printf("  Type: R | ");
            if (funct3 == 0 && funct7 == 0) printf("add x%u, x%u, x%u\n", rd, rs1, rs2);
            else if (funct3 == 0 && funct7 == 0x20) printf("sub x%u, x%u, x%u\n", rd, rs1, rs2);
            else if (funct3 == 7 && funct7 == 0) printf("and x%u, x%u, x%u\n", rd, rs1, rs2);
            else if (funct3 == 6 && funct7 == 0) printf("or x%u, x%u, x%u\n", rd, rs1, rs2);
            else printf("Unknown R-type (funct3=0x%x, funct7=0x%x)\n", funct3, funct7);
            break;
        case 0x13: // I-type arithmetic
            printf("  Type: I | ");
            if (funct3 == 0) printf("addi x%u, x%u, %d\n", rd, rs1, imm_i);
            else if (funct3 == 7) printf("andi x%u, x%u, %d\n", rd, rs1, imm_i);
            else if (funct3 == 6) printf("ori x%u, x%u, %d\n", rd, rs1, imm_i);
            else printf("Unknown I-type arithmetic (funct3=0x%x)\n", funct3);
            break;
        case 0x03: // I-type load
            printf("  Type: I | ");
            if (funct3 == 2) printf("lw x%u, %d(x%u)\n", rd, imm_i, rs1);
            else printf("Unknown I-type load (funct3=0x%x)\n", funct3);
            break;
        case 0x23: // S-type
            printf("  Type: S | ");
            if (funct3 == 2) printf("sw x%u, %d(x%u)\n", rs2, imm_s, rs1);
            else printf("Unknown S-type (funct3=0x%x)\n", funct3);
            break;
        case 0x63: // SB-type
            printf("  Type: B | ");
            if (funct3 == 0) printf("beq x%u, x%u, %d (target 0x%x)\n", rs1, rs2, imm_b, pc + imm_b);
            else printf("Unknown SB-type (funct3=0x%x)\n", funct3);
            break;
        case 0x6F: // UJ-type (JAL)
             printf("  Type: J | jal x%u, %d (target 0x%x)\n", rd, imm_j, pc + imm_j);
             break;
        case 0x67: // I-type (JALR)
             printf("  Type: I | jalr x%u, x%u, %d\n", rd, rs1, imm_i);
             break;
        default:
            printf("  Unknown instruction type (opcode 0x%x)\n", opcode);
            break;
    }
    // printf("  Control Signals: RegW=%d, MemR=%d, MemW=%d, MemToReg=%d, ALUSrc=%d, ALUOp=%d%d, Branch=%d, Jump=%d\n",
    //        RegWrite, MemRead, MemWrite, MemtoReg, ALUSrc, ALUOp1, ALUOp0, Branch, Jump);
}

// Print register file and data memory state (only non-zero values)
void print_state(int final_state) {
    if (!final_state) {
         printf("----- State after cycle %d -----\n", total_clock_cycles);
    } else {
         printf("Total clock cycles: %d\n", total_clock_cycles);
    }
     printf("PC: 0x%x\n", pc);

    printf("\nRegister File (non-zero):\n");
    int rf_changed = 0;
    for (int i = 0; i < 32; i++) {
        if (rf[i] != 0) {
            // Map register number to ABI name for readability
            const char* abi_name = "";
            switch(i) {
                case 1: abi_name = " (ra)"; break; 
                case 2: abi_name = " (sp)"; break;
                case 3: abi_name = " (gp)"; break; 
                case 4: abi_name = " (tp)"; break;
                case 5: abi_name = " (t0)"; break; 
                case 6: abi_name = " (t1)"; break; 
                case 7: abi_name = " (t2)"; break;
                case 8: abi_name = " (s0/fp)"; break; 
                case 9: abi_name = " (s1)"; break;
                case 10: abi_name = " (a0)"; break; 
                case 11: abi_name = " (a1)"; break;
                case 12: abi_name = " (a2)"; break; 
                case 13: abi_name = " (a3)"; break;
                case 14: abi_name = " (a4)"; break; 
                case 15: abi_name = " (a5)"; break;
                case 16: abi_name = " (a6)"; break; 
                case 17: abi_name = " (a7)"; break;
                case 18: abi_name = " (s2)"; break; 
                case 19: abi_name = " (s3)"; break;
                case 20: abi_name = " (s4)"; break; 
                case 21: abi_name = " (s5)"; break;
                case 22: abi_name = " (s6)"; break; 
                case 23: abi_name = " (s7)"; break;
                case 24: abi_name = " (s8)"; break; 
                case 25: abi_name = " (s9)"; break;
                case 26: abi_name = " (s10)"; break; 
                case 27: abi_name = " (s11)"; break;
                case 28: abi_name = " (t3)"; break; 
                case 29: abi_name = " (t4)"; break;
                case 30: abi_name = " (t5)"; break; 
                case 31: abi_name = " (t6)"; break;
            }
            printf("  x%d%s = 0x%x (%d)\n", i, abi_name, rf[i], rf[i]);
            rf_changed = 1;
        }
    }
     if (!rf_changed) printf("  All zero.\n");

    printf("\nData Memory (non-zero):\n");
    int mem_changed = 0;
    for (int i = 0; i < 32; i++) {
        if (d_mem[i] != 0) {
            printf("  0x%x = 0x%x (%d)\n", i * 4, d_mem[i], d_mem[i]);
            mem_changed = 1;
        }
    }
    if (!mem_changed) printf("  All zero.\n");
    printf("-----------------------------\n\n");
}


// Function to read instructions from file (binary strings)
void read_program(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    char line[100]; // Assuming max line length
    instr_count = 0;
    printf("Loading program from %s...\n", filename);

    while (fgets(line, sizeof(line), file) && instr_count < 100) {
        // Remove trailing newline or carriage return
        line[strcspn(line, "\r\n")] = 0;

        // Check if line is 32 chars long and contains only 0s and 1s
        if (strlen(line) == 32 && strspn(line, "01") == 32) {
            uint32_t instruction = 0;
            for (int i = 0; i < 32; i++) {
                if (line[i] == '1') {
                    instruction |= (1U << (31 - i));
                }
            }
            instr_mem[instr_count++] = instruction;
           // printf("Loaded instruction %d: 0x%08x\n", instr_count - 1, instruction);
        } else if (strlen(line) > 0) { // Ignore empty lines but warn about invalid ones
             printf("Warning: Skipping invalid line in program file: '%s'\n", line);
        }
    }

    fclose(file);
    printf("Loaded %d instructions.\n\n", instr_count);
}


int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <program_file.txt>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char* filename = argv[1];

    // Determine if running part 1 or part 2 sample based on filename (simple check)
    int is_part2 = (strstr(filename, "part2") != NULL);

    // Initialize Memory and Registers
    memset(rf, 0, sizeof(rf)); // Clear register file
    memset(d_mem, 0, sizeof(d_mem)); // Clear data memory

    if (is_part2) {
        printf("Initializing for Part 2 (JAL/JALR support)...\n");
        // Initialize rf for sample_part2.txt
        // s0=x8, a0=x10, a1=x11, a2=x12, a3=x13 (mapping ABI names)
        rf[8] = 0x20;  // s0 = 0x20
        rf[10] = 0x5;  // a0 = 0x5
        rf[11] = 0x2;  // a1 = 0x2
        rf[12] = 0xa;  // a2 = 0xa
        rf[13] = 0xf;  // a3 = 0xf
        // Data memory initialized to all zeros for part 2
    } else {
         printf("Initializing for Part 1...\n");
        // Initialize rf for sample_part1.txt
        rf[1] = 0x20;  // x1 = 0x20
        rf[2] = 0x5;   // x2 = 0x5
        rf[10] = 0x70; // x10 = 0x70
        rf[11] = 0x4;  // x11 = 0x4
        // Initialize data memory for sample_part1.txt
        d_mem[28] = 0x5;  // Address 0x70 (index 28) = 0x5
        d_mem[29] = 0x10; // Address 0x74 (index 29) = 0x10
    }


    // Read program from file
    read_program(filename);

    // Check if program loaded
    if (instr_count == 0) {
         fprintf(stderr, "Error: No valid instructions loaded from %s\n", filename);
         return EXIT_FAILURE;
    }

    // Print initial state
    printf("===== Initial State =====\n");
    print_state(1); // Use flag 1 for initial/final state format

    // Execute program loop
    printf("===== Program Execution =====\n");
    // Cast instr_count to uint32_t for comparison
    while ((pc / 4) < (uint32_t)instr_count) {
        // 1. Fetch
        uint32_t instruction = Fetch();
         // Cast instr_count to uint32_t for comparison
         if (instruction == 0 && (pc/4) >= (uint32_t)instr_count) break; // Stop if fetch returned NOP due to end of program

        // Print instruction details
        print_instruction(instruction);

        // Check for halt condition maybe? (e.g., specific instruction or error)

        // 2. Decode
        int rs1_val, rs2_val, imm;
        uint32_t rd, rs1, rs2, funct3, funct7;
        Decode(instruction, &rs1_val, &rs2_val, &rd, &rs1, &rs2, &funct3, &funct7, &imm);

        // 3. Execute - Call site updated (removed rs1, rs2 indices)
        int alu_result = Execute(rs1_val, rs2_val, imm, funct3, funct7);

        // 4. Memory
        int mem_data = Mem(alu_result, rs2_val);

        // 5. Writeback (updates PC and total_clock_cycles)
        Writeback(rd, alu_result, mem_data);

        // Print state after instruction execution
        print_state(0); // Use flag 0 for intermediate state format

        // Simple loop safeguard
        if (total_clock_cycles > instr_count * 5 && instr_count > 0) {
             printf("Warning: Excessive clock cycles (%d). Potential infinite loop?\n", total_clock_cycles);
             break;
        }
    }

    printf("===== Program terminated. =====\n");
    // Print final state
    print_state(1); // Use flag 1 for initial/final state format


    return EXIT_SUCCESS;
}