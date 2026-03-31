/* 223: task depend chain -- in -> out -> inout ordering */
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
  int a = 0, b = 0, c = 0;

#pragma omp parallel num_threads(4)
  {
#pragma omp single
    {
/* step 1: produce a */
#pragma omp task depend(out : a)
      {
        a = 10;
      }

/* step 2: read a, produce b */
#pragma omp task depend(in : a) depend(out : b)
      {
        b = a * 2; /* 20 */
      }

/* step 3: read a and b, produce c */
#pragma omp task depend(in : a, b) depend(out : c)
      {
        c = a + b; /* 30 */
      }

/* step 4: read-write c using a */
#pragma omp task depend(inout : c) depend(in : a)
      {
        c = c + a; /* 40 */
      }
    }
  }

  int expected = 40;
  printf("a=%d b=%d c=%d (expected c=%d)\n", a, b, c, expected);
  return (c != expected) || (a != 10) || (b != 20);
}
