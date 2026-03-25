/* 33_task_depend_inout.c - task depend(inout:A) read-modify-write */
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int A = 10;

    #pragma omp parallel
    {
        #pragma omp single
        {
            #pragma omp task depend(out: A)
            {
                A = 5;
            }

            #pragma omp task depend(inout: A)
            {
                A = A * 3;
            }

            #pragma omp task depend(inout: A)
            {
                A = A + 7;
            }

            #pragma omp taskwait
        }
    }

    printf("A = %d (expected 22)\n", A);
    return (A == 22) ? 0 : 1;
}
