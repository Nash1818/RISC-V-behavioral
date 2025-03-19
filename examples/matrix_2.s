# matrix_mul.s
###########################################
# A toy 2x2 matrix multiplication:
#   C = A * B
# A is at address 32
# B is at address 48
# C is at address 64
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
    # offsetA = (i*2 + k)*4
    sll x1, x5, 1    # x1 = i*2
    add x1, x1, x8   # x1 += k
    sll x1, x1, 2    # x1 *= 4
    add x1, x1, x10  # x1 = address of A[i][k]
    lw x2, 0(x1)     # x2 = A[i][k]

    # offsetB = (k*2 + j)*4
    sll x3, x8, 1    # x3 = k*2
    add x3, x3, x6   # x3 += j
    sll x3, x3, 2    # x3 *= 4
    add x3, x3, x11  # x3 = address of B[k][j]
    lw x4, 0(x3)     # x4 = B[k][j]

    mul x9, x2, x4
    add x7, x7, x9   # accumulate partial sum

    addi x8, x8, 1   # k++
    li x9, 2
    bne x8, x9, loop_k

    # store C[i][j] at offsetC = (i*2 + j)*4 from x12
    sll x1, x5, 1
    add x1, x1, x6
    sll x1, x1, 2
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

    # Reserve space for C
Cdata:
    .word 0
    .word 0
    .word 0
    .word 0
