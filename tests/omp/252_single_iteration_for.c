/* 252: single iteration parallel for -- N=1 */
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int A[1];
    A[0] = 0;

    #pragma omp parallel for num_threads(4)
    for (int i = 0; i < 1; i++)
        A[i] = 99;

    printf("A[0] = %d (expected 99)\n", A[0]);
    return (A[0] != 99);
}
