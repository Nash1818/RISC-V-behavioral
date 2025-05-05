# Installing the RISCV - 64 Toolchain (as of May 2025 installation needs to be done after modifying 2 files...):
- git clone https://github.com/riscv-collab/riscv-gnu-toolchain.git (Note: do not use --recursive while cloning)
- After cloning this in the root directory go to .gitmodules then search for shallow=True and remove all instances.
- Then go to Makefile.in in the root directory and remove --depth=1 flag
- Then proceed with usual installation steps as directed on the repository Readme.

# Adding an Instruction in your toolchain ISA:
- Follow the steps in this link, it mentions all the steps required:
    - link: https://pcotret.gitlab.io/riscv-custom/sw_toolchain.html#


# RV64IM Minimal Emulator

Bare-metal RISC-V 64-bit interpreter in vanilla C++ 
Supports the **RV64I** base instruction set and the **M** (integer mul/div) extension, plus a lightweight cycle model useful for register-pressure studies.

---

# for cycles support explaination
| The Behavioural model | Real silicon / hardware counter |
|------------------------|----------------------------------|
| Adds a **constant latency** per opcode (1 for ALU, 2 for LD/ST) ± an optional spill penalty you chose. | Counts every rising-edge of the processor clock while the program runs (mcycle CSR, `perf stat`, etc.). |
| **Ignores** pipeline depth, forwarding delays, branch mis-predict flushes, caches, TLBs, DRAM wait-states. | Includes *all* those effects automatically. |
| Runs exactly the same no matter whether the host PC is 1 GHz or 5 GHz. | Depends on the real core’s micro-architecture and clock frequency. |


## 1 Directory layout

| path | purpose |
|------|---------|
| **emulator.cpp** | host-side interpreter and cycle counter |
| **tests/start.S** | start-up stub – sets `sp`, jumps to `main` |
| **tests/linker.ld** | link script – code at `0x8000 0000`, 256 KiB stack |
| **tests/matrix_mul.c** | N × N `int64` matrix-multiply benchmark |
| **tests/bench.sh** | helper script to reserve registers and print cycles |
| **build/rve** | host executable after compilation |

---

## 2 Building the emulator (How to Run tests)

```bash
g++ -std=c++20 -O2 ./emulator.cpp -o build/rve

COMMON="-march=rv64im -mabi=lp64 -mcmodel=medany -O2"

riscv64-unknown-elf-gcc $COMMON -c tests/matrix_mul.c -o matrix_mul.o
riscv64-unknown-elf-gcc $COMMON -c tests/start.S      -o start.o
riscv64-unknown-elf-ld -T tests/linker.ld -o tests/mm.elf start.o matrix_mul.o

./build/rve tests/mm.elf

# 3 Ways to change cycle benchmarking:
 ```for bench.sh in bash
for n in 0 2 4 6 8; do ./bench.sh $n; done