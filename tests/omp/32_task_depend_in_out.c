/* 32_task_depend_in_out.c - task depend(out:A) writes, task depend(in:A) reads */
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int A = 0;
    int result = 0;

    #pragma omp parallel
    {
        #pragma omp single
        {
            #pragma omp task depend(out: A)
            {
                A = 99;
            }

            #pragma omp task depend(in: A)
            {
                result = A;
            }

            #pragma omp taskwait
        }
    }

    printf("result = %d\n", result);
    return (result == 99) ? 0 : 1;
}
