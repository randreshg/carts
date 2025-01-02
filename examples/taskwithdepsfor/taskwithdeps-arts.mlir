-----------------------------------------
ConvertOpenMPToARTSPass STARTED
-----------------------------------------
Loaded dialects: 10
Available dialects: 12
Loaded dialect: omp
Loaded dialect: memref
Loaded dialect: llvm
Loaded dialect: func
Loaded dialect: dlti
Loaded dialect: cf
Loaded dialect: builtin
Loaded dialect: arts
Loaded dialect: arith
Loaded dialect: affine
Available dialect: affine
Available dialect: arith
Available dialect: arts
Available dialect: async
Available dialect: builtin
Available dialect: dlti
Available dialect: func
Available dialect: llvm
Available dialect: math
Available dialect: memref
Available dialect: omp
Available dialect: scf
[convert-openmp-to-arts] Handle function: compute
[convert-openmp-to-arts] OMP hierarchy.
Parallel
  Master
    Task
    Task
[convert-openmp-to-arts] Transforming function: compute
-----------------------------------------
ConvertOpenMPToARTSPass FINISHED
-----------------------------------------
taskwithdeps.mlir:44:7: error: 'omp.terminator' op must be the last operation in the parent block
      omp.terminator
      ^
taskwithdeps.mlir:44:7: note: see current operation: "omp.terminator"() : () -> ()
