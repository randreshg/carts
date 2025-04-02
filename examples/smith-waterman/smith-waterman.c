#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MATCH_SCORE 2
#define MISMATCH_PENALTY -1
#define GAP_PENALTY -2

// Parallel task-based Smith-Waterman
void smith_waterman_omp(char *seq1, char *seq2, int len1, int len2,
                        int **score) {
#pragma omp parallel
#pragma omp single
  {
    for (int i = 1; i <= len1; i++) {
      for (int j = 1; j <= len2; j++) {
#pragma omp task depend(in : score[i - 1][j], score[i][j - 1],                 \
                            score[i - 1][j - 1]) depend(out : score[i][j])
        {
          int match =
              score[i - 1][j - 1] +
              ((seq1[i - 1] == seq2[j - 1]) ? MATCH_SCORE : MISMATCH_PENALTY);
          int del = score[i - 1][j] + GAP_PENALTY;
          int ins = score[i][j - 1] + GAP_PENALTY;
          score[i][j] = (match > del) ? ((match > ins) ? match : ins)
                                      : ((del > ins) ? del : ins);
          if (score[i][j] < 0)
            score[i][j] = 0;
        }
      }
    }
  }
}

// Sequential Smith-Waterman for verification
void smith_waterman_sequential(char *seq1, char *seq2, int len1, int len2,
                               int **score) {
  for (int i = 1; i <= len1; i++) {
    for (int j = 1; j <= len2; j++) {
      int match =
          score[i - 1][j - 1] +
          ((seq1[i - 1] == seq2[j - 1]) ? MATCH_SCORE : MISMATCH_PENALTY);
      int del = score[i - 1][j] + GAP_PENALTY;
      int ins = score[i][j - 1] + GAP_PENALTY;
      score[i][j] = (match > del) ? ((match > ins) ? match : ins)
                                  : ((del > ins) ? del : ins);
      if (score[i][j] < 0)
        score[i][j] = 0;
    }
  }
}

// Verify if parallel and sequential results match
int verify_results(int **parallel, int **sequential, int len1, int len2) {
  for (int i = 0; i <= len1; i++) {
    for (int j = 0; j <= len2; j++) {
      if (parallel[i][j] != sequential[i][j]) {
        printf("Mismatch at (%d, %d): Parallel=%d, Sequential=%d\n", i, j,
               parallel[i][j], sequential[i][j]);
        return 0;
      }
    }
  }
  return 1;
}

int main() {
  char seq1[] = "AGTACGCA";
  char seq2[] = "TATGCGC";
  int len1 = strlen(seq1);
  int len2 = strlen(seq2);

  // Allocate parallel and sequential score matrices
  int **score_parallel = (int **)calloc(len1 + 1, sizeof(int *));
  int **score_sequential = (int **)calloc(len1 + 1, sizeof(int *));
  for (int i = 0; i <= len1; i++) {
    score_parallel[i] = (int *)calloc(len2 + 1, sizeof(int));
    score_sequential[i] = (int *)calloc(len2 + 1, sizeof(int));
  }

  // Run parallel and sequential versions
  smith_waterman_omp(seq1, seq2, len1, len2, score_parallel);
  smith_waterman_sequential(seq1, seq2, len1, len2, score_sequential);

  // Verify results
  if (verify_results(score_parallel, score_sequential, len1, len2)) {
    printf("Results match!\n");
  } else {
    printf("Results DO NOT match!\n");
  }

  // Free memory
  for (int i = 0; i <= len1; i++) {
    free(score_parallel[i]);
    free(score_sequential[i]);
  }
  free(score_parallel);
  free(score_sequential);

  return 0;
}