#include "riscv_sim.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <cstdlib>
#include <cstdint>

RiscVSim::RiscVSim() : pc(0) {
    // Initialize registers and memory to 0
    for(int i = 0; i < NUM_REGS; i++) {
        registers[i] = 0;
    }
    for(int i = 0; i < MEM_SIZE; i++) {
        memory[i] = 0;
    }
}

bool RiscVSim::loadAssembly(const std::string &filename) {
    std::ifstream fin(filename);
    if(!fin.is_open()) {
        std::cerr << "Cannot open " << filename << std::endl;
        return false;
    }

    instructions.clear();
    labelToIndex.clear();
    dataLabelToAddress.clear();
    pc = 0;

    bool parsingText = false;   // We'll flip to true when we see ".text"
    uint32_t instrIndex = 0;    // How many instructions we've stored

    // In matrix_mul.s, we assume A starts at 32, B at 48, etc.
    // So let's store data from 32 onwards. You can pick another if you like.
    uint32_t dataAddress = 32;

    std::string line;
    while(std::getline(fin, line)) {
        // Trim leading whitespace
        auto startPos = line.find_first_not_of(" \t");
        if(startPos == std::string::npos) {
            continue; // empty line
        }
        line = line.substr(startPos);

        // Strip comments (anything after '#')
        auto commentPos = line.find('#');
        if(commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }
        if(line.empty()) {
            continue;
        }

        // Check for .text / .data
        if(line == ".text") {
            parsingText = true;
            continue;
        }
        if(line == ".data") {
            parsingText = false;
            continue;
        }

        // If in TEXT mode => parse instructions
        if(parsingText) {
            // If line ends with ':', that's a label (e.g. "loop_k:")
            if(line.back() == ':') {
                std::string lbl = line.substr(0, line.size() - 1);
                handleLabel(lbl, instrIndex);
                continue;
            }

            // Otherwise parse as an instruction
            Instruction instr = parseInstructionLine(line);

            // If there's a label in the same line, handle it
            if(!instr.label.empty()) {
                handleLabel(instr.label, instrIndex);
            }

            // If there's a valid opcode, store the instruction
            if(!instr.opcode.empty()) {
                instructions.push_back(instr);
                instrIndex++;
            }
        }
        else {
            // In DATA mode => parse data directives
            // e.g. ".word 1", "Adata:", etc.

            // Label in data mode
            if(line.back() == ':') {
                std::string dataLabel = line.substr(0, line.size() - 1);
                dataLabelToAddress[dataLabel] = dataAddress;
                continue;
            }

            // .word directive?
            if(line.rfind(".word", 0) == 0) {
                // e.g. ".word 1,2,3"
                std::string rest = line.substr(5); // remove ".word"
                // Trim leading spaces
                while(!rest.empty() && isspace(rest.front())) {
                    rest.erase(rest.begin());
                }
                // Replace commas with spaces
                for(char &c : rest) {
                    if(c == ',') c = ' ';
                }
                // Parse integers
                std::stringstream ss(rest);
                int value;
                while(ss >> value) {
                    storeWord(dataAddress, value);
                    dataAddress += 4;
                }
            }
            else if(line.rfind(".align", 0) == 0) {
                // e.g. ".align 2"
                std::string rest = line.substr(6);
                while(!rest.empty() && isspace(rest.front())) {
                    rest.erase(rest.begin());
                }
                int alignValue = std::stoi(rest);
                uint32_t alignBoundary = 1u << alignValue; // 2^alignValue
                dataAddress = (dataAddress + alignBoundary - 1) & ~(alignBoundary - 1);
            }
            else {
                // Unrecognized directive
                std::cerr << "[Warning] Unrecognized data directive: " << line << "\n";
            }
        }
    }

    fin.close();
    return true;
}

void RiscVSim::run() {
    // Execute instructions until PC is out of range or we hit an ecall
    while(pc < instructions.size()) {
        if(!executeInstruction(instructions[pc])) {
            break;
        }
    }
}

void RiscVSim::printRegisters() const {
    std::cout << "Register file:\n";
    for(int i = 0; i < NUM_REGS; i++) {
        std::cout << "x" << i << " = " << registers[i] << "\n";
    }
}

void RiscVSim::printMemory(uint32_t start, uint32_t length) const {
    if(start + length > MEM_SIZE) {
        length = MEM_SIZE - start;
    }
    std::cout << "\nMemory dump from " << start << " to " << (start + length - 1) << ":\n";
    // We'll print every 4 bytes as an int
    for(uint32_t i = 0; i < length; i += 4) {
        uint32_t addr = start + i;
        int32_t val = loadWord(addr);
        std::cout << "[" << addr << "] = " << val << "\n";
    }
}

//----------------------- Private Helpers -----------------------

Instruction RiscVSim::parseInstructionLine(const std::string &line) {
    Instruction instr;
    instr.label        = "";
    instr.opcode       = "";
    instr.rs1          = "";
    instr.rs2          = "";
    instr.rd           = "";
    instr.labelTarget  = "";
    instr.imm          = 0;

    // Remove commas => replace them with space
    std::string tmp = line;
    for(char &c : tmp) {
        if(c == ',') c = ' ';
    }

    std::stringstream ss(tmp);
    std::vector<std::string> tokens;
    std::string tok;
    while(ss >> tok) {
        tokens.push_back(tok);
    }
    if(tokens.empty()) {
        return instr; // no instruction
    }

    // If first token ends with ':', it's a label
    if(tokens[0].back() == ':') {
        instr.label = tokens[0].substr(0, tokens[0].size() - 1);
        tokens.erase(tokens.begin());
        if(tokens.empty()) {
            // no opcode after label
            return instr;
        }
    }

    // Now tokens[0] is opcode
    instr.opcode = tokens[0];

    // =========== Handle each opcode pattern ===========

    // 1) addi rd, rs1, imm
    // 2) add/sub/mul rd, rs1, rs2
    // 3) lw/sw  rd, offset(rs1)
    // 4) beq/bne rs1, rs2, label|imm
    // 5) li rd, imm  => expands to addi rd, x0, imm
    // 6) ecall

    if(instr.opcode == "addi") {
        // tokens => [ "addi", "rd", "rs1", "imm" ]
        // e.g. addi x5, x6, 100
        if(tokens.size() >= 4) {
            instr.rd  = tokens[1];
            instr.rs1 = tokens[2];
            instr.imm = std::stoi(tokens[3]);
        }
    }
    else if(instr.opcode == "add" || instr.opcode == "sub" || instr.opcode == "mul") {
        // tokens => [ "add", "rd", "rs1", "rs2" ]
        if(tokens.size() >= 4) {
            instr.rd  = tokens[1];
            instr.rs1 = tokens[2];
            instr.rs2 = tokens[3];
        }
    }

    // Note: Adding sll support...
    else if (instr.opcode == "sll") {
        // We expect something like:
        //   sll x1, x2, x3   (register shift)
        // or
        //   sll x1, x2, 2    (immediate shift, toy usage)
        // In real RISC-V, you'd normally write slli x1, x2, 2 for an immediate shift.
    
        if (tokens.size() >= 4) {
            instr.rd  = tokens[1];  // e.g. "x1"
            instr.rs1 = tokens[2];  // e.g. "x5"
    
            // Check if tokens[3] is a number
            bool isNumber = true;
            for (char c : tokens[3]) {
                if (!std::isdigit(c) && c != '-' && c != '+') {
                    isNumber = false;
                    break;
                }
            }
    
            if (isNumber) {
                // Then we treat it as an "slli" (shift-left immediate).
                instr.opcode = "slli";       // rename the opcode
                instr.imm    = std::stoi(tokens[3]);
            } else {
                // Otherwise, it's a register-based shift
                instr.rs2 = tokens[3];      // e.g. "x8"
            }
        }
    }    

    else if(instr.opcode == "lw" || instr.opcode == "sw") {
        // tokens => [ "lw", "rd", "offset(rs1)" ]
        // example: lw x5, 8(x6)
        if(tokens.size() >= 3) {
            instr.rd = tokens[1];
            // parse offset(rs1)
            std::string memStr = tokens[2];
            auto openParen = memStr.find('(');
            auto closeParen = memStr.find(')');
            if(openParen != std::string::npos && closeParen != std::string::npos) {
                std::string offset = memStr.substr(0, openParen);
                std::string base   = memStr.substr(openParen + 1, closeParen - openParen - 1);
                instr.rs1 = base; // e.g. x6
                if(!offset.empty()) {
                    instr.imm = std::stoi(offset);
                }
            }
        }
    }
    else if(instr.opcode == "li") {
        // tokens => [ "li", "rd", "imm" ]
        if(tokens.size() >= 3) {
            instr.rd  = tokens[1];
            instr.rs1 = "x0";
            instr.imm = std::stoi(tokens[2]);
            // expand to addi
            instr.opcode = "addi";
        }
    }
    else if(instr.opcode == "beq" || instr.opcode == "bne") {
        // Real RISC-V: beq rs1, rs2, offset
        // Typically you see: beq x8, x9, loop_k
        // => tokens => [ "beq", "x8", "x9", "loop_k" ]
        if(tokens.size() >= 4) {
            instr.rs1 = tokens[1];    // x8
            instr.rs2 = tokens[2];    // x9

            // The 3rd operand might be a label or an immediate
            std::string possibleLabel = tokens[3];
            // Check if numeric
            bool isNumber = true;
            for(char c : possibleLabel) {
                if(!std::isdigit(c) && c != '-' && c != '+') {
                    isNumber = false;
                    break;
                }
            }
            if(isNumber) {
                instr.imm = std::stoi(possibleLabel);
            } else {
                instr.labelTarget = possibleLabel;
            }
        }
    }
    else if(instr.opcode == "ecall") {
        // No operands
    }
    else {
        // unrecognized opcode => ignore or warn
        // for now, do nothing
    }

    return instr;
}

int RiscVSim::parseRegisterNumber(const std::string &regName) const {
    // expect something like x0, x1, ...
    if(regName.size() < 2) return 0;
    if(regName[0] != 'x') return 0;
    int num = std::stoi(regName.substr(1));
    if(num < 0 || num >= NUM_REGS) {
        return 0;
    }
    return num;
}

int32_t RiscVSim::getRegister(const std::string &regName) const {
    int r = parseRegisterNumber(regName);
    if(r == 0) {
        return 0; // x0 is always 0
    }
    return registers[r];
}

void RiscVSim::setRegister(const std::string &regName, int32_t value) {
    int r = parseRegisterNumber(regName);
    if(r == 0) {
        // x0 is read-only
        return;
    }
    registers[r] = value;
}

int32_t RiscVSim::loadWord(uint32_t address) const {
    if(address + 3 >= MEM_SIZE) {
        std::cerr << "Load out of range: " << address << std::endl;
        return 0;
    }
    // Read 4 bytes (little-endian)
    int32_t val = 0;
    val |= (memory[address + 0] <<  0);
    val |= (memory[address + 1] <<  8);
    val |= (memory[address + 2] << 16);
    val |= (memory[address + 3] << 24);
    return val;
}

void RiscVSim::storeWord(uint32_t address, int32_t value) {
    if(address + 3 >= MEM_SIZE) {
        std::cerr << "Store out of range: " << address << std::endl;
        return;
    }
    memory[address + 0] = (value >>  0) & 0xFF;
    memory[address + 1] = (value >>  8) & 0xFF;
    memory[address + 2] = (value >> 16) & 0xFF;
    memory[address + 3] = (value >> 24) & 0xFF;
}

void RiscVSim::handleLabel(const std::string &label, uint32_t instrIndex) {
    // label -> which instruction index
    labelToIndex[label] = instrIndex;
}

bool RiscVSim::executeInstruction(const Instruction &instr) {
    // We'll increment pc by 1 unless we do a branch/jump
    uint32_t nextPc = pc + 1;

    // ------------------------------------------------
    // Arithmetic / logic
    // ------------------------------------------------
    if(instr.opcode == "addi") {
        int32_t val = getRegister(instr.rs1) + instr.imm;
        setRegister(instr.rd, val);
    }
    else if(instr.opcode == "add") {
        int32_t val = getRegister(instr.rs1) + getRegister(instr.rs2);
        setRegister(instr.rd, val);
    }
    else if(instr.opcode == "sub") {
        int32_t val = getRegister(instr.rs1) - getRegister(instr.rs2);
        setRegister(instr.rd, val);
    }
    else if(instr.opcode == "mul") {
        int64_t val = (int64_t)getRegister(instr.rs1) * (int64_t)getRegister(instr.rs2);
        setRegister(instr.rd, (int32_t)(val & 0xFFFFFFFF));
    }

    // sll support
    else if (instr.opcode == "sll") {
        // sll rd, rs1, rs2 => rd = (uint32_t)rs1 << (rs2 & 0x1F)
        int32_t val1 = getRegister(instr.rs1);
        int32_t val2 = getRegister(instr.rs2);    // shift amount
        int shiftAmount = (val2 & 0x1F);          // only low 5 bits for 32-bit
        uint32_t result = ((uint32_t)val1) << shiftAmount;
        setRegister(instr.rd, (int32_t)result);
    }
    else if (instr.opcode == "slli") {
        // slli rd, rs1, imm => rd = (uint32_t)rs1 << (imm & 0x1F)
        int32_t val1 = getRegister(instr.rs1);
        int shiftAmount = (instr.imm & 0x1F);
        uint32_t result = ((uint32_t)val1) << shiftAmount;
        setRegister(instr.rd, (int32_t)result);
    }
    

    // ------------------------------------------------
    // Memory
    // ------------------------------------------------
    else if(instr.opcode == "lw") {
        uint32_t addr = (uint32_t)(getRegister(instr.rs1) + instr.imm);
        int32_t data = loadWord(addr);
        setRegister(instr.rd, data);
    }
    else if(instr.opcode == "sw") {
        uint32_t addr = (uint32_t)(getRegister(instr.rs1) + instr.imm);
        storeWord(addr, getRegister(instr.rd));
    }

    // ------------------------------------------------
    // Branches
    // ------------------------------------------------
    else if(instr.opcode == "beq") {
        // if rs1 == rs2 => branch
        int32_t val1 = getRegister(instr.rs1);
        int32_t val2 = getRegister(instr.rs2);

        if(val1 == val2) {
            if(!instr.labelTarget.empty()) {
                // jump to label
                auto it = labelToIndex.find(instr.labelTarget);
                if(it != labelToIndex.end()) {
                    nextPc = it->second;
                }
            } else {
                // numeric offset
                nextPc = pc + 1 + instr.imm;
            }
        }
    }
    else if(instr.opcode == "bne") {
        // if rs1 != rs2 => branch
        int32_t val1 = getRegister(instr.rs1);
        int32_t val2 = getRegister(instr.rs2);

        if(val1 != val2) {
            if(!instr.labelTarget.empty()) {
                auto it = labelToIndex.find(instr.labelTarget);
                if(it != labelToIndex.end()) {
                    nextPc = it->second;
                }
            } else {
                nextPc = pc + 1 + instr.imm;
            }
        }
    }

    // ------------------------------------------------
    // System
    // ------------------------------------------------
    else if(instr.opcode == "ecall") {
        // Stop execution
        return false;
    }

    // ------------------------------------------------
    // Unknown or unhandled opcode => do nothing
    // ------------------------------------------------

    pc = nextPc;
    return (pc < instructions.size());
}
