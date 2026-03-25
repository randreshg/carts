/* 10: if(N > 100) conditional parallelism */

extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));

    #pragma omp parallel for if(N > 100)
    for (int i = 0; i < N; i++) {
        A[i] = i * 4;
    }

    /* sum = sum(i*4, i=0..1023) = 4*523776 = 2095104 */
    long sum = 0;
    for (int i = 0; i < N; i++)
        sum += A[i];

    printf("sum = %ld (expected 2095104)\n", sum);
    free(A);
    return (sum != 2095104);
}
