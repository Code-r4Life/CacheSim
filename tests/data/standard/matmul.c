// matmul.c: real workload; b[k][j] is walked column-wise, which is hard on caches
#include <stdio.h>
#define N 64
static double a[N][N], b[N][N], c[N][N];
int main(void) {
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) { a[i][j] = i + j; b[i][j] = i - j; }
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            for (int k = 0; k < N; k++)
                c[i][j] += a[i][k] * b[k][j];
    printf("%f\n", c[N-1][N-1]);   // keeps the compiler from deleting the work
    return 0;
}
