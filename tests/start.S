    .section .text
    .globl _start

_start:
    # 1. set up a stack inside DRAM (symbol comes from linker.ld)
    la     sp, __stack_top

    # 2. call C entry; jal writes a valid return address into ra
    call   main                  # == jal ra, main

    # 3. clean exit so the emulator stops gracefully
    ecall                        # imm = 0  (CPU::op_system handles this)

    # 4. should never get here, but just in case
    j      .                     # tight loop
