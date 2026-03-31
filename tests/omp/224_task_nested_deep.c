/* 224: deeply nested tasks -- task spawning task spawning task */
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int result = 0;

#pragma omp parallel num_threads(4)
  {
#pragma omp single
    {
#pragma omp task shared(result)
      {
        int a = 1;
#pragma omp task shared(result) firstprivate(a)
        {
          int b = a + 2; /* 3 */
#pragma omp task shared(result) firstprivate(b)
          {
            int c = b + 4; /* 7 */
            result = c;
          }
#pragma omp taskwait
        }
#pragma omp taskwait
      }
#pragma omp taskwait
    }
  }

  int expected = 7;
  printf("result = %d (expected %d)\n", result, expected);
  return (result != expected);
}
