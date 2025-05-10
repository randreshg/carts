#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* 
python3 run_comparison.py \
  --problem_sizes 100 150 200 \
  --iterations_per_size 5 5 5 \
  --target_examples addition \
  --timeout_seconds 10 \
  --example_base_dirs "parallel" \
  --output_file multi_size_results.json \
  --csv_output_file multi_size_benchmark.csv \
  --graph_output_file multi_size_graph.png \
  --md_output_file multi_size_summary.md
*/

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: %s <num_threads>\n", argv[0]);
    return 1;
  }
  const int N = atoi(argv[1]);
  if (N <= 0) {
    printf("Error: N must be a positive integer.\n");
    return 1;
  }

  int a[N], b[N], c[N];

  // Initialize arrays
  for (int i = 0; i < N; i++) {
    a[i] = i;
    b[i] = i * 2;
  }

  printf("Performing parallel array addition...\n");

  // Start timer
  double t_start = omp_get_wtime();

  // Parallel loop for array addition
  #pragma omp parallel for
  for (int i = 0; i < N; i++) {
    // optional per-iteration debug
    // printf("Thread %d is working on iteration %d\n",
    //        omp_get_thread_num(), i);
    c[i] = a[i] + b[i];
  }

  // Stop timer
  double t_end = omp_get_wtime();
  printf("Parallel addition finished in %f seconds.\n", t_end - t_start);

  // Verify results
  bool success = true;
  for (int i = 0; i < N; i++) {
    if (c[i] != a[i] + b[i]) {
      success = false;
      break;
    }
  }
  // Output correctness in a script-parseable format
  if (success) {
    printf("Result: CORRECT\\n");
  } else {
    printf("Result: INCORRECT\\n");
  }
  fflush(stdout); // Ensure output is flushed

  // Print C
  printf("C array: ");
  for (int i = 0; i < N; i++) {
    printf("%d ", c[i]);
  }
  printf("\n");

  return 0;
}

