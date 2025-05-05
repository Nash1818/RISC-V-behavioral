#ifndef N
#define N 4
#endif


#include <stdint.h>

volatile int64_t A[N][N] = {{1,2,3,4},{5,6,7,8},{9,8,7,6},{5,4,3,2}};
volatile int64_t B[N][N] = {{4,3,2,1},{8,7,6,5},{12,11,10,9},{16,15,14,13}};
volatile int64_t C[N][N];

static inline void uart_putc(char c)
{
    *((volatile uint8_t*)0x10000000) = c;
}

static void uart_print_i64(int64_t x)
{
    char buf[32];
    int pos = 0;
    if (x == 0) { uart_putc('0'); return; }
    if (x < 0)  { uart_putc('-'); x = -x; }
    /* build string in reverse */
    while (x) { buf[pos++] = '0' + (x % 10); x /= 10; }
    /* output forward */
    while (pos--) uart_putc(buf[pos]);
}

int main(void)
{
    /* C = A × B */
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j) {
            int64_t s = 0;
            for (int k = 0; k < N; ++k)
                s += A[i][k] * B[k][j];
            C[i][j] = s;
        }

    /* print the whole matrix */
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            uart_print_i64(C[i][j]);
            uart_putc(j == N-1 ? '\n' : ' ');
        }
    }
    return 0;
}
