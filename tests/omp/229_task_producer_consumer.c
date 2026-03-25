/* 229: producer-consumer with task depend -- pipeline of dependent tasks */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 16;
    int *buf = (int *)malloc(N * sizeof(int));
    int *out = (int *)malloc(N * sizeof(int));
    int i;

    for (i = 0; i < N; i++) {
        buf[i] = 0;
        out[i] = 0;
    }

    #pragma omp parallel num_threads(4)
    {
        #pragma omp single
        {
            for (int j = 0; j < N; j++) {
                /* producer: generate data */
                #pragma omp task depend(out: buf[j]) firstprivate(j)
                {
                    buf[j] = j * 3 + 1;
                }

                /* consumer: transform data -- must wait for producer */
                #pragma omp task depend(in: buf[j]) depend(out: out[j]) firstprivate(j)
                {
                    out[j] = buf[j] * 2;
                }
            }
            #pragma omp taskwait
        }
    }

    long sum = 0;
    for (i = 0; i < N; i++)
        sum += out[i];

    /* out[j] = 2*(3j+1) = 6j+2, sum = 6*N*(N-1)/2 + 2*N = 3*N*(N-1) + 2*N */
    long expected = 3L * N * (N - 1) + 2L * N;
    printf("sum = %ld (expected %ld)\n", sum, expected);
    free(buf);
    free(out);
    return (sum != expected);
}
