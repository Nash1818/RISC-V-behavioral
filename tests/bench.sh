#!/usr/bin/env bash
set -e
# how many caller-saved regs we forbid (0…10)
N="$1"                    
COMMON="-march=rv64im -mabi=lp64 -mcmodel=medany -O2"
# build blacklist a0,a1,a2,…  (x10..)
FIXED=""
for ((k=0;k<N;k++)); do FIXED+=" -ffixed-a$k"; done

riscv64-unknown-elf-gcc $COMMON $FIXED -c matrix_mul.c -o matrix_mul.o
riscv64-unknown-elf-gcc $COMMON            -c start.S      -o start.o
riscv64-unknown-elf-ld  -T linker.ld -o mm.elf start.o matrix_mul.o

echo -n "fixed $N regs -> "
# <../rve {filename.elf}
../rve mm.elf | tail -1        
