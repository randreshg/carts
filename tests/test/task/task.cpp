
#include <cstdlib>
#include <stdio.h>

#include <omp.h>
#include <stdlib.h>
#include <time.h>
/*
carts cgeist task.cpp -std=c++17 -fopenmp -O0 -S > task.mlir
carts run task.mlir --convert-openmp-to-arts -o task-arts.mlir
carts run task-arts.mlir --edt-transforms -o task-edt.mlir
carts run task-edt.mlir --create-dbs -o task-db.mlir
carts run task.mlir --edt-lowering -o task-llvm.mlir


carts mlir-translate --mlir-to-llvmir task-arts.mlir &> task-arts.ll

carts compile task-arts.ll -O3 -g0 -march=native -o task

python3 run_comparison.py \
  --problem_sizes "10 5" \
  --iterations_per_size 5 \
  --target_examples matrixmul \
  --example_base_dirs "tasking" \
  --output_file matrixmul_results.json \
  --csv_output_file matrixmul_benchmark.csv \
  --graph_output_file matrixmul_graph.png \
  --md_output_file matrixmul_summary.md
*/


int main() {
  srand(time(NULL));
  printf("Main EDT: Starting...\n");
  
  int shared_number = rand() % 100 + 1;
  int random_number = rand() % 10 + 1;
  printf("EDT 0: The initial number is %d/%d\n", shared_number, random_number);

#pragma omp parallel
  {
#pragma omp single
    {
      printf("EDT 1: The number is %d/%d\n", shared_number, random_number);
#pragma omp task firstprivate(random_number)
      {
        shared_number++;
        random_number++;
        printf("EDT 2: The number is %d/%d\n", shared_number, random_number);
      }
    }
  }

  shared_number++;
  printf("EDT 3: The final number is %d - %d.\n", shared_number, random_number);
  return 0;
}
