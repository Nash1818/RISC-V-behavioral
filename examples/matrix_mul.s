# matrix_mul_no_sll.s
###########################################
# A toy 2x2 matrix multiplication:
#   C = A * B
# A is at address 32
# B is at address 48
# C is at address 64
#
# This version AVOIDS 'sll' instructions
# and uses only 'add'/'addi' to compute offsets.
###########################################

    .text
###########################################
# 1) Setup
###########################################
    li x10, 32   # base address of A
    li x11, 48   # base address of B
    li x12, 64   # base address of C

###########################################
# 2) Multiply
###########################################
# for i in [0..1]:
#   for j in [0..1]:
#       C[i][j] = 0
#       for k in [0..1]:
#           C[i][j] += A[i][k] * B[k][j]

    li x5, 0     # i = 0
loop_i:
    li x6, 0     # j = 0
loop_j:
    li x7, 0     # C[i][j] = 0

    li x8, 0     # k = 0
loop_k:
    ################################################################
    # offset for A[i][k] = ((i*2) + k) * 4
    # We'll compute step by step using add:
    ################################################################
    # x1 = i
    add x1, x0, x5       # x1 = i
    # x1 = i + i => i*2
    add x1, x1, x5
    # x1 = i*2 + k
    add x1, x1, x8
    # multiply by 4 => do two doublings
    add x1, x1, x1       # x1 = (i*2 + k)*2
    add x1, x1, x1       # x1 = (i*2 + k)*4
    # add base of A
    add x1, x1, x10      # x1 = address of A[i][k]

    lw x2, 0(x1)         # x2 = A[i][k]

    ################################################################
    # offset for B[k][j] = ((k*2) + j) * 4
    ################################################################
    # x3 = k
    add x3, x0, x8
    # x3 = k + k => 2*k
    add x3, x3, x8
    # x3 = 2k + j
    add x3, x3, x6
    # multiply by 4 => do two doublings
    add x3, x3, x3
    add x3, x3, x3
    # add base of B
    add x3, x3, x11

    lw x4, 0(x3)         # x4 = B[k][j]

    mul x9, x2, x4       # x9 = A[i][k] * B[k][j]
    add x7, x7, x9       # accumulate partial sum in x7

    addi x8, x8, 1       # k++
    li x9, 2
    bne x8, x9, loop_k

    ################################################################
    # Store C[i][j] at offset = ((i*2) + j) * 4 from x12
    ################################################################
    # x1 = i
    add x1, x0, x5
    # x1 = i + i => i*2
    add x1, x1, x5
    # x1 = i*2 + j
    add x1, x1, x6
    # multiply by 4 => two doublings
    add x1, x1, x1
    add x1, x1, x1
    # add base of C
    add x1, x1, x12

    sw x7, 0(x1)

    # j++
    addi x6, x6, 1
    li x9, 2
    bne x6, x9, loop_j

    # i++
    addi x5, x5, 1
    li x9, 2
    bne x5, x9, loop_i

    ecall     # stop

###########################################
# 3) Data Section
###########################################
    .data
    # A = [[1, 2],
    #      [3, 4]]
Adata:
    .word 1
    .word 2
    .word 3
    .word 4

    # B = [[5, 6],
    #      [7, 8]]
Bdata:
    .word 5
    .word 6
    .word 7
    .word 8

    # Space for C
Cdata:
    .word 0
    .word 0
    .word 0
    .word 0
