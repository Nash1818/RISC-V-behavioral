#include "riscv_sim.hpp"
#include <iostream>

int main(int argc, char** argv) {
    if(argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <assembly-file>\n";
        return 1;
    }

    RiscVSim sim;
    if(!sim.loadAssembly(argv[1])) {
        return 1;
    }

    sim.run();

    // Print final registers
    sim.printRegisters();

    // Print memory. We'll dump from address 0 up through 96 bytes
    // so we can see A (32..44), B (48..60), and C (64..76).
    sim.printMemory(0, 96);

    return 0;
}
