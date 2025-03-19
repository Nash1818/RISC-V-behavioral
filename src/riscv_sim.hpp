#pragma once

#include <string>
#include <vector>
#include <unordered_map>

// A small struct to hold one decoded instruction.
struct Instruction {
    std::string label;   // e.g. "loop_k"
    std::string opcode;  // e.g. "addi", "bne", ...
    std::string rs1;     // register (for branches or R-type)
    std::string rs2;     // register (for R-type or branch compare)
    std::string rd;      // register (for R-type or I-type)
    std::string labelTarget; // if we parsed a label for branch/jump
    int32_t     imm;     // immediate value if needed
};

class RiscVSim {
public:
    RiscVSim();

    // Loads a .s file, parses both .text (instructions) and .data (.word),
    // and writes data to our memory[] array.
    bool loadAssembly(const std::string &filename);

    // Runs instructions from PC=0 onward, until we run out or hit an ecall
    void run();

    // For debugging/inspection
    void printRegisters() const;
    void printMemory(uint32_t start, uint32_t length) const;

private:
    static const int NUM_REGS = 32;
    static const int MEM_SIZE = 1024; // 1 KB for toy example

    // The register file (x0..x31)
    int32_t registers[NUM_REGS];

    // A simple block of memory for data
    uint8_t memory[MEM_SIZE];

    // We store instructions in a vector. Each element is a parsed instruction.
    std::vector<Instruction> instructions;

    // Labels that point to instruction indices (for branching).
    // e.g. labelToIndex["loop_k"] = 5 means instructions[5] is "loop_k".
    std::unordered_map<std::string, uint32_t> labelToIndex;

    // Labels for data, e.g. dataLabelToAddress["Adata"] = 32
    std::unordered_map<std::string, uint32_t> dataLabelToAddress;

    // Program counter in terms of instruction indices into `instructions`.
    uint32_t pc;

    // ---- Private Helpers ----
    // Parse a line of *text* into an Instruction struct.
    Instruction parseInstructionLine(const std::string &line);

    // Resolve register name (like "x5") to an integer index (5).
    int parseRegisterNumber(const std::string &regName) const;

    // Get/set register values (makes x0 read-only).
    int32_t getRegister(const std::string &regName) const;
    void    setRegister(const std::string &regName, int32_t value);

    // Memory access in 4-byte chunks.
    int32_t loadWord(uint32_t address) const;
    void    storeWord(uint32_t address, int32_t value);

    // Handling label definitions in text mode
    void handleLabel(const std::string &label, uint32_t instrIndex);

    // Execute a single instruction; returns false if we should stop (ecall).
    bool executeInstruction(const Instruction &instr);
};
