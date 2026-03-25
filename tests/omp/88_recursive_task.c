/* 88_recursive_task.c - recursive fibonacci with tasks */
extern int printf(const char *, ...);
extern int atoi(const char *);

int fib(int n) {
    if (n < 2)
        return n;
    int x, y;
    #pragma omp task shared(x) firstprivate(n)
    x = fib(n - 1);
    #pragma omp task shared(y) firstprivate(n)
    y = fib(n - 2);
    #pragma omp taskwait
    return x + y;
}

int main(int argc, char **argv) {
    int n = 10;
    int result;

    #pragma omp parallel
    {
        #pragma omp single
        result = fib(n);
    }

    /* fib(10) = 55 */
    printf("fib(%d) = %d (expected 55)\n", n, result);
    return (result != 55);
}
