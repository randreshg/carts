#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MATCH_SCORE 2
#define MISMATCH_PENALTY -1
#define GAP_PENALTY -2

/*
cgeist smith-waterman.c -fopenmp -O0 -S -I//usr/lib/llvm-14/lib/clang/14.0.0/include > smith-waterman.mlir

carts-opt smith-waterman.mlir --lower-affine --convert-openmp-to-arts --edt --hoist-invariant --create-datablocks --canonicalize --datablock --create-events --create-epochs --canonicalize  -debug-only=hoist-invariant,create-datablocks,datablock &> smith-waterman_arts.mlir

carts-opt smith-waterman.mlir --lower-affine --convert-openmp-to-arts --edt --hoist-invariant --create-datablocks --canonicalize --datablock --create-events --create-epochs --canonicalize --convert-arts-to-llvm  -debug-only=hoist-invariant,create-datablocks,convert-arts-to-llvm &> smith-waterman_arts.mlir

carts-opt smith-waterman.mlir --lower-affine --convert-openmp-to-arts --edt --create-datablocks --cse --canonicalize --datablock --create-events --create-epochs --cse --canonicalize --convert-arts-to-llvm --cse --canonicalize -debug-only=convert-arts-to-llvm &> smith-waterman_arts.mlir

carts-opt smith-waterman.mlir --lower-affine --convert-openmp-to-arts --edt --create-datablocks --cse --canonicalize --datablock --create-events --create-epochs --cse --canonicalize --convert-arts-to-llvm --cse --canonicalize --raise-scf-to-affine --canonicalize --affine-cfg --affine-expand-index-ops --affine-scalrep --affine-cfg --loop-invariant-code-motion --cse --canonicalize --lower-affine --cse --canonicalize --convert-polygeist-to-llvm --cse --canonicalize -debug-only=create-datablocks,convert-arts-to-llvm,convert-polygeist-to-llvm &> smith-waterman.ll
*/
 

int main() {
  char seq1[] = "AGTACGCA";
  char seq2[] = "TATGCGC";
  int len1 = strlen(seq1);
  int len2 = strlen(seq2);

  int score_parallel[len1 + 1][len2 + 1];
  int score_sequential[len1 + 1][len2 + 1];

  // Initialize matrices to 0
  for (int i = 0; i <= len1; i++) {
    for (int j = 0; j <= len2; j++) {
      score_parallel[i][j] = 0;
      score_sequential[i][j] = 0;
    }
  }

  // Parallel Smith-Waterman inline
  #pragma omp parallel
  #pragma omp single
  {
    /// Print sequences
    printf("seq1: %s\n", seq1);
    printf("seq2: %s\n", seq2);
    for (int i = 1; i <= len1; i++) {
      for (int j = 1; j <= len2; j++) {
        // #pragma omp task depend(in : score_parallel[i - 1][j], \
        //                              score_parallel[i][j - 1], \
        //                              score_parallel[i - 1][j - 1]) \
        //                  depend(out : score_parallel[i][j])
        #pragma omp task
        {
          int match = score_parallel[i - 1][j - 1] +
                      ((seq1[i - 1] == seq2[j - 1]) ? MATCH_SCORE : MISMATCH_PENALTY);
          int del = score_parallel[i - 1][j] + GAP_PENALTY;
          int ins = score_parallel[i][j - 1] + GAP_PENALTY;
          score_parallel[i][j] = (match > del) ? ((match > ins) ? match : ins)
                                               : ((del > ins) ? del : ins);
          if (score_parallel[i][j] < 0)
            score_parallel[i][j] = 0;
        }
      }
    }
  }

  // Sequential Smith-Waterman inline
  for (int i = 1; i <= len1; i++) {
    for (int j = 1; j <= len2; j++) {
      int match = score_sequential[i - 1][j - 1] +
                  ((seq1[i - 1] == seq2[j - 1]) ? MATCH_SCORE : MISMATCH_PENALTY);
      int del = score_sequential[i - 1][j] + GAP_PENALTY;
      int ins = score_sequential[i][j - 1] + GAP_PENALTY;
      score_sequential[i][j] = (match > del) ? ((match > ins) ? match : ins)
                                             : ((del > ins) ? del : ins);
      if (score_sequential[i][j] < 0)
        score_sequential[i][j] = 0;
    }
  }

  // Verify the results inline
  int ok = 1;
  for (int i = 0; i <= len1; i++) {
    for (int j = 0; j <= len2; j++) {
      if (score_parallel[i][j] != score_sequential[i][j]) {
        printf("Mismatch at (%d, %d): Parallel=%d, Sequential=%d\n", i, j,
               score_parallel[i][j], score_sequential[i][j]);
        ok = 0;
      }
    }
  }
  if (ok)
    printf("Results match!\n");
  else
    printf("Results DO NOT match!\n");

  return 0;
}
