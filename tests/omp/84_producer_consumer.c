/* 84_producer_consumer.c - task depend(out) produces, task depend(in) consumes */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *buf = (int *)malloc(N * sizeof(int));
    long result = 0;
    int i;

    #pragma omp parallel
    {
        #pragma omp single
        {
            #pragma omp task depend(out: buf[0])
            {
                int j;
                for (j = 0; j < N; j++)
                    buf[j] = j * 2;
            }

            #pragma omp task depend(in: buf[0])
            {
                long local_sum = 0;
                int j;
                for (j = 0; j < N; j++)
                    local_sum += buf[j];
                result = local_sum;
            }

            #pragma omp taskwait
        }
    }

    /* sum = 2*(0+1+...+1023) = 2*1023*1024/2 = 1047552 */
    long expected = 1047552;
    printf("result = %ld (expected %ld)\n", result, expected);
    free(buf);
    return (result != expected);
}
