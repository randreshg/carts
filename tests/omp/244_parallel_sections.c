/* 244: parallel sections -- independent code blocks */
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int a = 0, b = 0, c = 0, d = 0;

    #pragma omp parallel sections num_threads(4)
    {
        #pragma omp section
        {
            a = 10;
        }
        #pragma omp section
        {
            b = 20;
        }
        #pragma omp section
        {
            c = 30;
        }
        #pragma omp section
        {
            d = 40;
        }
    }

    int sum = a + b + c + d;
    int expected = 100;
    printf("sum = %d (expected %d)\n", sum, expected);
    return (sum != expected);
}
