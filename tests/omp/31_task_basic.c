/* 31_task_basic.c - single { task { A[0]=42 }; taskwait } */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int A[1];
    A[0] = 0;

    #pragma omp parallel
    {
        #pragma omp single
        {
            #pragma omp task
            {
                A[0] = 42;
            }
            #pragma omp taskwait
        }
    }

    printf("A[0] = %d\n", A[0]);
    return (A[0] == 42) ? 0 : 1;
}
