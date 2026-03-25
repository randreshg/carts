/* 36_task_final.c - task final(depth>3) { body } */
extern int printf(const char *, ...);
extern int atoi(const char *);

int fib(int n, int depth) {
    int x, y;
    if (n < 2)
        return n;

    #pragma omp task shared(x) final(depth > 3)
    x = fib(n - 1, depth + 1);

    #pragma omp task shared(y) final(depth > 3)
    y = fib(n - 2, depth + 1);

    #pragma omp taskwait
    return x + y;
}

int main(int argc, char **argv) {
    int result;

    #pragma omp parallel
    {
        #pragma omp single
        result = fib(10, 0);
    }

    printf("fib(10) = %d (expected 55)\n", result);
    return (result == 55) ? 0 : 1;
}
