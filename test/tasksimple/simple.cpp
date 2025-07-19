
#include <cstdlib>
#include <stdio.h>

#include <omp.h>
#include <stdlib.h>
#include <time.h>
/*
carts cgeist simple.cpp -std=c++17 -fopenmp -O0 -S > simple.mlir

carts opt simple.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize --loop-invariant-code-motion --canonicalize --arts-inliner --convert-openmp-to-arts --symbol-dce -debug-only=convert-openmp-to-arts &> simple-arts.mlir

carts opt simple.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize --loop-invariant-code-motion --canonicalize --arts-inliner --convert-openmp-to-arts --edt --edt-invariant-code-motion --symbol-dce &> simple-arts.mlir

carts opt simple.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize --loop-invariant-code-motion --canonicalize --arts-inliner --convert-openmp-to-arts --edt --edt-invariant-code-motion --canonicalize --create-dbs --cse -debug-only=create-dbs &> simple-arts.mlir

carts opt simple.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize --loop-invariant-code-motion --canonicalize --arts-inliner --convert-openmp-to-arts --edt --edt-invariant-code-motion --canonicalize --create-dbs  --canonicalize --db --canonicalize --cse -debug-only=db,db-analysis,db-alias-analysis,db-dataflow-analysis,db-info &> simple-arts.mlir

carts opt simple.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize --loop-invariant-code-motion --canonicalize --arts-inliner --convert-openmp-to-arts --edt --edt-invariant-code-motion --canonicalize --create-dbs --db --canonicalize --cse --polygeist-mem2reg --edt-pointer-rematerialization --create-epochs --canonicalize --preprocess-dbs --cse -debug-only=convert-arts-to-llvm,arts-codegen,preprocess-dbs &> simple-arts.mlir

carts opt simple.mlir --lower-affine --cse --polygeist-mem2reg --canonicalize --loop-invariant-code-motion --canonicalize --arts-inliner --convert-openmp-to-arts --edt --edt-invariant-code-motion --canonicalize --create-dbs --db --canonicalize --cse --polygeist-mem2reg --edt-pointer-rematerialization --create-epochs --canonicalize --preprocess-dbs --convert-arts-to-llvm --cse -debug-only=convert-arts-to-llvm,arts-codegen,edt-codegen,db-codegen &> simple-arts.mlir


carts mlir-translate --mlir-to-llvmir simple-arts.mlir &> simple-arts.ll

carts compile simple-arts.ll -O3 -g0 -march=native -o simple

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
