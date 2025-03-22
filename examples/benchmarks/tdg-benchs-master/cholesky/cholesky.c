#include "cholesky.h"
#include "omp.h"
#include "timer.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DIM 16
int NB;
#define BS (DIM / NB)

/// cgeist cholesky.c -fopenmp -O3 -S -I/usr/lib/llvm-14/lib/clang/14.0.0/include &> cholesky.mlir

static double matrix[DIM * DIM] = {0};
static double original_matrix[DIM * DIM] = {0};
double ***dummy;

void omp_potrf(double *A, int ts, int ld, double (*AhDep)[NB][NB]) {
  static int INFO;
  static const char L = 'L';
  dpotrf_(&L, &ts, A, &ld, &INFO);
}

void omp_trsm(double *A, double *B, int ts, int ld, double (*AhDep)[NB][NB]) {
  static char LO = 'L', TR = 'T', NU = 'N', RI = 'R';
  static double DONE = 1.0;
  dtrsm_(&RI, &LO, &TR, &NU, &ts, &ts, &DONE, A, &ld, B, &ld);
}

void omp_syrk(double *A, double *B, int ts, int ld, double (*AhDep)[NB][NB]) {
  static char LO = 'L', NT = 'N';
  static double DONE = 1.0, DMONE = -1.0;
  dsyrk_(&LO, &NT, &ts, &ts, &DMONE, A, &ld, &DONE, B, &ld);
}

void omp_gemm(double *A, double *B, double *C, int ts, int ld,
              double (*AhDep)[NB][NB]) {
  static const char TR = 'T', NT = 'N';
  static double DONE = 1.0, DMONE = -1.0;
  dgemm_(&NT, &TR, &ts, &ts, &ts, &DMONE, A, &ld, B, &ld, &DONE, C, &ld);
}

void cholesky_blocked(const int ts, double *Ah[NB][NB], int num_iter) {
  double makespan = 0;
#pragma omp parallel
#pragma omp single
  {
    double(*AhDep)[NB][NB] = Ah;
    for (int iter = 0; iter < num_iter; iter++) {
      START_TIMER;
      for (int k = 0; k < NB; k++) {
        /// Output 0, 0
        #pragma omp task depend(inout : AhDep[k][k])
          omp_potrf(Ah[k][k], ts, ts, AhDep);

        for (int i = k + 1; i < NB; i++) {
          /// Input 0, 0
          /// Output 0, 1
          #pragma omp task depend(in : AhDep[k][k]) depend(inout : AhDep[k][i])
            omp_trsm(Ah[k][k], Ah[k][i], ts, ts, AhDep);
        }

        for (int l = k + 1; l < NB; l++) {
          for (int j = k + 1; j < l; j++) {
            /// Input 0, 1 - 0,1, 
            #pragma omp task depend(in : AhDep[k][l]) depend(in : AhDep[k][j]) depend(inout: AhDep[j][l])
            omp_gemm(Ah[k][l], Ah[k][j], Ah[j][l], ts, ts, AhDep);
          }
          #pragma omp task depend(in : AhDep[k][l]) depend(inout : AhDep[l][l])
            omp_syrk(Ah[k][l], Ah[l][l], ts, ts, AhDep);
        }
      }

#ifndef TDG
#pragma omp taskwait
#endif

      END_TIMER;
      makespan += TIMER;

      convert_to_blocks(ts, NB, DIM, (double(*)[DIM])original_matrix, Ah);
    }
    printf("%g ms passed\n", makespan);
  }
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: <block number> <num_iterations> \n");
    exit(1);
  }

  NB = atoi(argv[1]);
  if (NB > DIM) {
    fprintf(stderr, "NB = %d, DIM = %d, NB must be smaller than DIM\n", NB, DIM);
    exit(-1);
  }

  int num_iter = atoi(argv[2]);
  if (num_iter < 0) {
    fprintf(stderr, "num_iter must be positive\n");
    exit(1);
  }

  const double eps = BLAS_dfpinfo(blas_eps);
  const int n = DIM;
  const int ts = BS;

  printf("ts = %d\n", ts);

  int check = 0;

  initialize_matrix(n, ts, matrix);

  double *Ah[NB][NB];
  for (int i = 0; i < NB; i++)
    for (int j = 0; j < NB; j++)
      Ah[i][j] = (double *)calloc(BS * BS, sizeof(double));

  memcpy(original_matrix, matrix, n * n * sizeof(double));

  convert_to_blocks(ts, NB, n, (double(*)[n])matrix, Ah);

  cholesky_blocked(ts, Ah, num_iter);

  convert_to_linear(ts, NB, n, Ah, (double(*)[n])matrix);

  // if (check) {
  //   const char uplo = 'L';
  //   if (check_factorization(n, original_matrix, matrix, n, uplo, eps))
  //     check++;
  // }

  return 0;
}
