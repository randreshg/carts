/* 222: task with depend(inout) on same variable -- serialized access */
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int x = 0;

    #pragma omp parallel num_threads(4)
    {
        #pragma omp single
        {
            /* all tasks depend inout on x -- must execute sequentially */
            #pragma omp task depend(inout: x)
            {
                x = 1;
            }
            #pragma omp task depend(inout: x)
            {
                x = x * 2 + 1; /* 1*2+1=3 */
            }
            #pragma omp task depend(inout: x)
            {
                x = x * 2 + 1; /* 3*2+1=7 */
            }
            #pragma omp task depend(inout: x)
            {
                x = x * 2 + 1; /* 7*2+1=15 */
            }
        }
    }

    int expected = 15;
    printf("x = %d (expected %d)\n", x, expected);
    return (x != expected);
}
