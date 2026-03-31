/* 70_struct_access.c - struct with x,y,z fields, parallel update */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

struct Vec3 {
  double x;
  double y;
  double z;
};

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  struct Vec3 *points = (struct Vec3 *)malloc(N * sizeof(struct Vec3));
  int i;

  for (i = 0; i < N; i++) {
    points[i].x = (double)i;
    points[i].y = (double)(i * 2);
    points[i].z = (double)(i * 3);
  }

#pragma omp parallel for
  for (i = 0; i < N; i++) {
    points[i].x *= 2.0;
    points[i].y *= 2.0;
    points[i].z *= 2.0;
  }

  int pass = 1;
  for (i = 0; i < N; i++) {
    if (points[i].x != (double)(i * 2) || points[i].y != (double)(i * 4) ||
        points[i].z != (double)(i * 6)) {
      pass = 0;
      break;
    }
  }

  printf("%s\n", pass ? "PASS" : "FAIL");
  free(points);

  return pass ? 0 : 1;
}
