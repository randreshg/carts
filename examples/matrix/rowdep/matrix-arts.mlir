
-----------------------------------------
ConvertOpenMPToArtsPass STARTED
-----------------------------------------
[convert-openmp-to-arts] Converting omp.task to arts.edt
[convert-openmp-to-arts] Converting omp.task to arts.edt
[convert-openmp-to-arts] Converting omp.task to arts.edt
[convert-openmp-to-arts] Converting omp.parallel to arts.parallel
-----------------------------------------
ConvertOpenMPToArtsPass FINISHED
-----------------------------------------

-----------------------------------------
EdtPass STARTED
-----------------------------------------
[edt] Converting parallel EDT into single EDT
-----------------------------------------
EdtPass FINISHED
-----------------------------------------

-----------------------------------------
CreateDatablocksPass STARTED
-----------------------------------------
[create-datablocks] Candidate datablocks in function: compute
[create-datablocks] EDT region
  - Candidate Datablock
    Memref: %alloca_0 = memref.alloca() : memref<100x100xf64>
    Access Type: out
    Pinned Indices:
      - <block argument> of type 'index' at index: 0
    Uses:
    - memref.store %22, %alloca_0[%arg0, %arg1] : memref<100x100xf64>
[create-datablocks] EDT region
  - Candidate Datablock
    Memref: %alloca = memref.alloca() : memref<100x100xf64>
    Access Type: out
    Pinned Indices:
      - %c0 = arith.constant 0 : index
    Uses:
    - memref.store %17, %alloca[%c0, %arg0] : memref<100x100xf64>
  - Candidate Datablock
    Memref: %alloca_0 = memref.alloca() : memref<100x100xf64>
    Access Type: in
    Pinned Indices:
      - %c0 = arith.constant 0 : index
    Uses:
    - %17 = memref.load %alloca_0[%c0, %arg0] : memref<100x100xf64>
[create-datablocks] EDT region
  - Candidate Datablock
    Memref: %alloca = memref.alloca() : memref<100x100xf64>
    Access Type: out
    Pinned Indices:
      - <block argument> of type 'index' at index: 0
    Uses:
    - memref.store %31, %alloca[%arg0, %arg1] : memref<100x100xf64>
  - Candidate Datablock
    Memref: %alloca_0 = memref.alloca() : memref<100x100xf64>
    Access Type: in
    Pinned Indices:
      - <block argument> of type 'index' at index: 0
    Uses:
    - %29 = memref.load %alloca_0[%arg0, %arg1] : memref<100x100xf64>
  - Candidate Datablock
    Memref: %alloca_0 = memref.alloca() : memref<100x100xf64>
    Access Type: in
    Pinned Indices:
      - %22 = arith.index_cast %21 : i32 to index
    Uses:
    - %30 = memref.load %alloca_0[%22, %arg1] : memref<100x100xf64>
[create-datablocks] EDT region
  - Candidate Datablock
    Memref: %alloca = memref.alloca() : memref<100x100xf64>
    Access Type: inout
    Pinned Indices:   none
    Uses:
    - memref.store %31, %alloca[%arg0, %arg1] : memref<100x100xf64>
    - %26 = memref.load %alloca[%arg0, %c0] : memref<100x100xf64>
  - Candidate Datablock
    Memref: %alloca_0 = memref.alloca() : memref<100x100xf64>
    Access Type: inout
    Pinned Indices:   none
    Uses:
    - %30 = memref.load %alloca_0[%22, %arg1] : memref<100x100xf64>
    - %29 = memref.load %alloca_0[%arg0, %arg1] : memref<100x100xf64>
    - %23 = memref.load %alloca_0[%22, %c0] : memref<100x100xf64>
    - %18 = memref.load %alloca_0[%arg0, %c0] : memref<100x100xf64>
    - memref.store %22, %alloca_0[%arg0, %arg1] : memref<100x100xf64>
    - %18 = memref.load %alloca_0[%arg0, %c0] : memref<100x100xf64>
[create-datablocks] Candidate datablocks in function: rand
-----------------------------------------
Rewiring uses of:
  %22 = arts.datablock "out" ptr[%alloca_0 : memref<100x100xf64>], indices[%arg0], sizes[%c100_1], type[f64], typeSize[%21] -> memref<100xf64>
-----------------------------------------
Rewiring uses of:
  %18 = arts.datablock "out" ptr[%alloca : memref<100x100xf64>], indices[%c0], sizes[%c100_1], type[f64], typeSize[%17] -> memref<100xf64>
-----------------------------------------
Rewiring uses of:
  %20 = arts.datablock "in" ptr[%alloca_0 : memref<100x100xf64>], indices[%c0], sizes[%c100_2], type[f64], typeSize[%19] -> memref<100xf64>
-----------------------------------------
Rewiring uses of:
  %32 = arts.datablock "out" ptr[%alloca : memref<100x100xf64>], indices[%arg0], sizes[%c100_3], type[f64], typeSize[%31] -> memref<100xf64>
-----------------------------------------
Rewiring uses of:
  %34 = arts.datablock "in" ptr[%alloca_0 : memref<100x100xf64>], indices[%arg0], sizes[%c100_4], type[f64], typeSize[%33] -> memref<100xf64>
-----------------------------------------
Rewiring uses of:
  %36 = arts.datablock "in" ptr[%alloca_0 : memref<100x100xf64>], indices[%24], sizes[%c100_5], type[f64], typeSize[%35] -> memref<100xf64>
-----------------------------------------
Rewiring uses of:
  %3 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100_1, %c100_2], type[f64], typeSize[%2] -> memref<100x100xf64>
-----------------------------------------
Rewiring uses of:
  %5 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100_3, %c100_4], type[f64], typeSize[%4] -> memref<100x100xf64>
-----------------------------------------
CreateDatablocksPass FINISHED
-----------------------------------------

-----------------------------------------
DatablockPass STARTED
-----------------------------------------
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Datablocks may alias
    - It is a dependency because of reachability
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Datablocks may alias
    - It is a dependency because of reachability
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Datablocks may alias
    - It is a dependency because of reachability
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different nodes
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different nodes
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different nodes
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Datablocks may alias
    - It is a dependency because of reachability
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Datablocks may alias
    - It is a dependency because of reachability
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Datablocks may alias
    - It is a dependency because of reachability
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different nodes
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different nodes
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different nodes
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
-----------------------------------------
[datablock-analysis] Printing graph for function: compute
Nodes:
  #2 out
    %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    isLoopDependent=true useCount=2 hasPtrDb=true userEdtPos=0
  #3 out
    %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    isLoopDependent=false useCount=2 hasPtrDb=true userEdtPos=0
  #4 in
    %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    isLoopDependent=false useCount=2 hasPtrDb=true userEdtPos=1
  #5 out
    %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    isLoopDependent=true useCount=2 hasPtrDb=true userEdtPos=0
  #6 in
    %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    isLoopDependent=true useCount=2 hasPtrDb=true userEdtPos=1
  #7 in
    %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    isLoopDependent=true useCount=2 hasPtrDb=true userEdtPos=2
Edges:
  #2 -> #4 (indirect, loop dependent)
  #2 -> #6 (indirect, loop dependent)
  #2 -> #7 (indirect, loop dependent)
Total nodes: 8
-----------------------------------------
DatablockPass FINISHED
-----------------------------------------
-----------------------------------------
CreateEventsPass STARTED
-----------------------------------------
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Datablocks may alias
    - It is a dependency because of reachability
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Datablocks may alias
    - It is a dependency because of reachability
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Datablocks may alias
    - It is a dependency because of reachability
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different nodes
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different nodes
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different nodes
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Datablocks may alias
    - It is a dependency because of reachability
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Datablocks may alias
    - It is a dependency because of reachability
[datablock-analysis] Checking dependency between
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Datablocks may alias
    - It is a dependency because of reachability
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different nodes
[datablock-analysis] Checking dependency between
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different nodes
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Different nodes
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %14 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %11 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %12 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %16 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%15], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
  - %17 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
    - Same EDT user
Events:
  Event
    Producer: 2, Consumer: 4
    Producer: 2, Consumer: 6
    Producer: 2, Consumer: 7
[create-events] Processing grouped event
-----------------------------------------
CreateEventsPass FINISHED
-----------------------------------------
-----------------------------------------
ConvertArtsToFuncsPass START
-----------------------------------------
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str2("B[%d][%d] = %f   \00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("A[%d][%d] = %f   \00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @compute() attributes {llvm.linkage = #llvm.linkage<external>} {
    %c8 = arith.constant 8 : index
    %c0 = arith.constant 0 : index
    %c-1_i32 = arith.constant -1 : i32
    %c100 = arith.constant 100 : index
    %c1 = arith.constant 1 : index
    %c100_i32 = arith.constant 100 : i32
    %alloca = memref.alloca() : memref<100x100xf64>
    %alloca_0 = memref.alloca() : memref<100x100xf64>
    %0 = call @rand() : () -> i32
    %1 = arith.remsi %0, %c100_i32 : i32
    %2 = arts.datablock "inout" ptr[%alloca : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    %3 = arts.datablock "inout" ptr[%alloca_0 : memref<100x100xf64>], indices[], sizes[%c100, %c100], type[f64], typeSize[%c8] -> memref<100x100xf64>
    arts.edt dependencies(%2, %3) : (memref<100x100xf64>, memref<100x100xf64>) attributes {sync} {
      %10 = arts.alloc_event[%c100, %c100] -> : memref<100x100xi64>
      %11 = arith.sitofp %1 : i32 to f64
      scf.for %arg0 = %c0 to %c100 step %c1 {
        %14 = arith.index_cast %arg0 : index to i32
        %15 = arts.datablock "out" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8], event[%10] {hasPtrDb} -> memref<100xf64>
        arts.edt dependencies(%15) : (memref<100xf64>) attributes {task} {
          %16 = arith.sitofp %14 : i32 to f64
          %17 = arith.addf %16, %11 : f64
          scf.for %arg1 = %c0 to %c100 step %c1 {
            memref.store %17, %15[%arg1] : memref<100xf64>
          }
          arts.yield
        }
      }
      %12 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
      %13 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%c0], sizes[%c100], type[f64], typeSize[%c8], event[%10] {hasPtrDb} -> memref<100xf64>
      arts.edt dependencies(%12, %13) : (memref<100xf64>, memref<100xf64>) attributes {task} {
        scf.for %arg0 = %c0 to %c100 step %c1 {
          %14 = memref.load %13[%arg0] : memref<100xf64>
          memref.store %14, %12[%arg0] : memref<100xf64>
        }
        arts.yield
      }
      scf.for %arg0 = %c1 to %c100 step %c1 {
        %14 = arith.index_cast %arg0 : index to i32
        %15 = arith.addi %14, %c-1_i32 : i32
        %16 = arith.index_cast %15 : i32 to index
        %17 = arts.datablock "out" ptr[%2 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8] {hasPtrDb} -> memref<100xf64>
        %18 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%arg0], sizes[%c100], type[f64], typeSize[%c8], event[%10] {hasPtrDb} -> memref<100xf64>
        %19 = arts.datablock "in" ptr[%3 : memref<100x100xf64>], indices[%16], sizes[%c100], type[f64], typeSize[%c8], event[%10] {hasPtrDb} -> memref<100xf64>
        arts.edt dependencies(%17, %18, %19) : (memref<100xf64>, memref<100xf64>, memref<100xf64>) attributes {task} {
          scf.for %arg1 = %c0 to %c100 step %c1 {
            %20 = memref.load %18[%arg1] : memref<100xf64>
            %21 = memref.load %19[%arg1] : memref<100xf64>
            %22 = arith.addf %20, %21 : f64
            memref.store %22, %17[%arg1] : memref<100xf64>
          }
          arts.yield
        }
      }
      arts.yield
    }
    %4 = llvm.mlir.addressof @str0 : !llvm.ptr
    %5 = llvm.getelementptr %4[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<18 x i8>
    %6 = llvm.mlir.addressof @str1 : !llvm.ptr
    %7 = llvm.getelementptr %6[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<2 x i8>
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %10 = arith.index_cast %arg0 : index to i32
      scf.for %arg1 = %c0 to %c100 step %c1 {
        %12 = arith.index_cast %arg1 : index to i32
        %13 = memref.load %3[%arg0, %arg1] : memref<100x100xf64>
        %14 = llvm.call @printf(%5, %10, %12, %13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, f64) -> i32
      }
      %11 = llvm.call @printf(%7) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    %8 = llvm.mlir.addressof @str2 : !llvm.ptr
    %9 = llvm.getelementptr %8[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<18 x i8>
    scf.for %arg0 = %c0 to %c100 step %c1 {
      %10 = arith.index_cast %arg0 : index to i32
      scf.for %arg1 = %c0 to %c100 step %c1 {
        %12 = arith.index_cast %arg1 : index to i32
        %13 = memref.load %2[%arg0, %arg1] : memref<100x100xf64>
        %14 = llvm.call @printf(%9, %10, %12, %13) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, f64) -> i32
      }
      %11 = llvm.call @printf(%7) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    }
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}
[convert-arts-to-funcs] Iterate over all the functions
[convert-arts-to-funcs] Lowering arts.datablock
[convert-arts-to-funcs] Lowering arts.datablock
[convert-arts-to-funcs] Lowering arts.edt sync
-----------------------------------------
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: carts-opt matrix.mlir --lower-affine --convert-openmp-to-arts --edt --create-datablocks --cse --canonicalize --datablock --create-events --cse --canonicalize --convert-arts-to-funcs --cse --canonicalize -debug-only=convert-openmp-to-arts,edt,create-datablocks,datablock,datablock-analysis,create-events,convert-arts-to-funcs,arts-codegen
 #0 0x000055aeff0a9257 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x1195257)
 #1 0x000055aeff0a6e2e llvm::sys::RunSignalHandlers() (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x1192e2e)
 #2 0x000055aeff0a990a SignalHandler(int) Signals.cpp:0:0
 #3 0x00007f06d3745520 (/lib/x86_64-linux-gnu/libc.so.6+0x42520)
 #4 0x000055aeff01ddca mlir::detail::OperandStorage::OperandStorage(mlir::Operation*, mlir::OpOperand*, mlir::ValueRange) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x1109dca)
 #5 0x000055aeff013cb0 mlir::Operation::create(mlir::Location, mlir::OperationName, mlir::TypeRange, mlir::ValueRange, mlir::DictionaryAttr, mlir::OpaqueProperties, mlir::BlockRange, unsigned int) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x10ffcb0)
 #6 0x000055aeff013527 mlir::Operation::create(mlir::Location, mlir::OperationName, mlir::TypeRange, mlir::ValueRange, mlir::NamedAttrList&&, mlir::OpaqueProperties, mlir::BlockRange, mlir::RegionRange) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x10ff527)
 #7 0x000055aeff0133e4 mlir::Operation::create(mlir::OperationState const&) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x10ff3e4)
 #8 0x000055aefef98960 mlir::OpBuilder::create(mlir::OperationState const&) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x1084960)
 #9 0x000055aefe722366 mlir::func::CallOp mlir::OpBuilder::create<mlir::func::CallOp, mlir::func::FuncOp&, llvm::ArrayRef<mlir::Value>&>(mlir::Location, mlir::func::FuncOp&, llvm::ArrayRef<mlir::Value>&) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x80e366)
#10 0x000055aefe71af85 mlir::arts::EdtCodegen::build(mlir::Location) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x806f85)
#11 0x000055aefe716f00 mlir::WalkResult llvm::function_ref<mlir::WalkResult (mlir::Operation*)>::callback_fn<(anonymous namespace)::ConvertArtsToFuncsPass::iterateOps(mlir::Operation*)::$_4>(long, mlir::Operation*) ConvertArtsToFuncs.cpp:0:0
#12 0x000055aefe0ccc38 mlir::WalkResult mlir::detail::walk<mlir::ForwardIterator>(mlir::Operation*, llvm::function_ref<mlir::WalkResult (mlir::Operation*)>, mlir::WalkOrder) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x1b8c38)
#13 0x000055aefe0ccbe7 mlir::WalkResult mlir::detail::walk<mlir::ForwardIterator>(mlir::Operation*, llvm::function_ref<mlir::WalkResult (mlir::Operation*)>, mlir::WalkOrder) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x1b8be7)
#14 0x000055aefe7159aa (anonymous namespace)::ConvertArtsToFuncsPass::runOnOperation() ConvertArtsToFuncs.cpp:0:0
#15 0x000055aefeedd624 mlir::detail::OpToOpPassAdaptor::run(mlir::Pass*, mlir::Operation*, mlir::AnalysisManager, bool, unsigned int) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0xfc9624)
#16 0x000055aefeeddc51 mlir::detail::OpToOpPassAdaptor::runPipeline(mlir::OpPassManager&, mlir::Operation*, mlir::AnalysisManager, bool, unsigned int, mlir::PassInstrumentor*, mlir::PassInstrumentation::PipelineParentInfo const*) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0xfc9c51)
#17 0x000055aefeee0102 mlir::PassManager::run(mlir::Operation*) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0xfcc102)
#18 0x000055aefe73f514 performActions(llvm::raw_ostream&, std::shared_ptr<llvm::SourceMgr> const&, mlir::MLIRContext*, mlir::MlirOptMainConfig const&) MlirOptMain.cpp:0:0
#19 0x000055aefe73e784 mlir::LogicalResult llvm::function_ref<mlir::LogicalResult (std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&)>::callback_fn<mlir::MlirOptMain(llvm::raw_ostream&, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, mlir::DialectRegistry&, mlir::MlirOptMainConfig const&)::$_2>(long, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&) MlirOptMain.cpp:0:0
#20 0x000055aeff03e958 mlir::splitAndProcessBuffer(std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::function_ref<mlir::LogicalResult (std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&)>, llvm::raw_ostream&, bool, bool) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x112a958)
#21 0x000055aefe738d8a mlir::MlirOptMain(llvm::raw_ostream&, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, mlir::DialectRegistry&, mlir::MlirOptMainConfig const&) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x824d8a)
#22 0x000055aefe739254 mlir::MlirOptMain(int, char**, llvm::StringRef, mlir::DialectRegistry&) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x825254)
#23 0x000055aefe053056 main (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x13f056)
#24 0x00007f06d372cd90 (/lib/x86_64-linux-gnu/libc.so.6+0x29d90)
#25 0x00007f06d372ce40 __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x29e40)
#26 0x000055aefe052755 _start (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x13e755)
