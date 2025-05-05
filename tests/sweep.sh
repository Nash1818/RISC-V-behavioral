#!/bin/bash
# tests/sweep.sh
set -e

COMMON="-march=rv64im -mabi=lp64 -mcmodel=medany -O2"
LD="riscv64-unknown-elf-ld"
CC="riscv64-unknown-elf-gcc"
EMUL=../rve

SIZES=(4 8 16 30 50 60 80 100)
FIXED_REGS=(0 2 4 6 8)

for N in "${SIZES[@]}"; do
  echo "Matrix size: ${N}x${N}"
  for r in "${FIXED_REGS[@]}"; do
    FIXED_FLAGS=""
    for ((i=0; i<$r; i++)); do
      FIXED_FLAGS="$FIXED_FLAGS -ffixed-a${i}"
    done
    $CC $COMMON -DN=$N $FIXED_FLAGS -c matrix_mul.c -o matrix_mul.o
    $CC $COMMON -c start.S -o start.o
    $LD -T linker.ld -o mm.elf start.o matrix_mul.o
    echo -n "  fixed $r regs → "
    $EMUL mm.elf | tail -n 1
  done
  echo ""
done
