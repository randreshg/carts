
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
    Memref: %alloca_0 = memref.alloca() : memref<i32>
    Access Type: out
    Pinned Indices:   none
    Uses:
    - memref.store %c10_i32, %alloca_0[] : memref<i32>
[create-datablocks] EDT region
  - Candidate Datablock
    Memref: %alloca = memref.alloca() : memref<i32>
    Access Type: out
    Pinned Indices:   none
    Uses:
    - memref.store %16, %alloca[] : memref<i32>
  - Candidate Datablock
    Memref: %alloca_0 = memref.alloca() : memref<i32>
    Access Type: in
    Pinned Indices:   none
    Uses:
    - %12 = memref.load %alloca_0[] : memref<i32>
[create-datablocks] EDT region
  - Candidate Datablock
    Memref: %alloca = memref.alloca() : memref<i32>
    Access Type: in
    Pinned Indices:   none
    Uses:
    - %12 = memref.load %alloca[] : memref<i32>
[create-datablocks] EDT region
  - Candidate Datablock
    Memref: %alloca = memref.alloca() : memref<i32>
    Access Type: inout
    Pinned Indices:   none
    Uses:
    - %12 = memref.load %alloca[] : memref<i32>
    - %9 = memref.load %alloca[] : memref<i32>
    - memref.store %16, %alloca[] : memref<i32>
    - %6 = memref.load %alloca[] : memref<i32>
  - Candidate Datablock
    Memref: %alloca_0 = memref.alloca() : memref<i32>
    Access Type: inout
    Pinned Indices:   none
    Uses:
    - %12 = memref.load %alloca_0[] : memref<i32>
    - %3 = memref.load %alloca_0[] : memref<i32>
    - memref.store %c10_i32, %alloca_0[] : memref<i32>
    - %0 = memref.load %alloca_0[] : memref<i32>
-----------------------------------------
Rewiring uses of:
  %4 = arts.datablock "out" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%3] -> memref<i32>
-----------------------------------------
Rewiring uses of:
  %11 = arts.datablock "out" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%10] -> memref<i32>
-----------------------------------------
Rewiring uses of:
  %13 = arts.datablock "in" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%12] -> memref<i32>
-----------------------------------------
Rewiring uses of:
  %16 = arts.datablock "in" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%15] -> memref<i32>
-----------------------------------------
Rewiring uses of:
  %1 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%0] -> memref<i32>
-----------------------------------------
Rewiring uses of:
  %3 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%2] -> memref<i32>
-----------------------------------------
CreateDatablocksPass FINISHED
-----------------------------------------

-----------------------------------------
DatablockPass STARTED
-----------------------------------------
[datablock-analysis] Checking dependency between
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Datablocks may alias
    - It is a dependency because of dominance
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different nodes
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Datablocks may alias
    - It is a dependency because of dominance
[datablock-analysis] Checking dependency between
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Datablocks may alias
    - It is a dependency because of dominance
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different nodes
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Datablocks may alias
    - It is a dependency because of dominance
[datablock-analysis] Checking dependency between
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
-----------------------------------------
[datablock-analysis] Printing graph for function: compute
Nodes:
  #0 inout
    %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    isLoopDependent=false useCount=3 hasPtrDb=false userEdtPos=0
  #1 inout
    %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    isLoopDependent=false useCount=3 hasPtrDb=false userEdtPos=1
  #2 out
    %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    isLoopDependent=false useCount=2 hasPtrDb=true userEdtPos=0
  #3 out
    %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    isLoopDependent=false useCount=2 hasPtrDb=true userEdtPos=0
  #4 in
    %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    isLoopDependent=false useCount=2 hasPtrDb=true userEdtPos=1
  #5 in
    %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    isLoopDependent=false useCount=2 hasPtrDb=true userEdtPos=0
Edges:
  #2 -> #4 (direct)
  #3 -> #5 (direct)
Total nodes: 6
-----------------------------------------
DatablockPass FINISHED
-----------------------------------------
-----------------------------------------
CreateEventsPass STARTED
-----------------------------------------
[datablock-analysis] Checking dependency between
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Datablocks may alias
    - It is a dependency because of dominance
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different nodes
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Datablocks may alias
    - It is a dependency because of dominance
[datablock-analysis] Checking dependency between
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Datablocks may alias
    - It is a dependency because of dominance
[datablock-analysis] Checking dependency between
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Different nodes
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Datablocks may alias
    - It is a dependency because of dominance
[datablock-analysis] Checking dependency between
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Same EDT user
[datablock-analysis] Checking dependency between
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    - Different EDT parents
[datablock-analysis] Checking dependency between
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %2 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %3 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
[datablock-analysis] Checking dependency between
  - %5 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
  - %4 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {hasPtrDb, single} -> memref<i32>
    - Not a writer or reader
Events:
  Event
    Producer: 2, Consumer: 4
  Event
    Producer: 3, Consumer: 5
[create-events] Processing non-grouped event
[create-events] Processing non-grouped event
-----------------------------------------
CreateEventsPass FINISHED
-----------------------------------------
-----------------------------------------
ConvertArtsToLLVMPass START
-----------------------------------------
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str2("Task 3: Final value of b=%d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("Task 2: Reading a=%d and updating b\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("Task 1: Initializing a\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @compute() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c4 = arith.constant 4 : index
    %c5_i32 = arith.constant 5 : i32
    %c10_i32 = arith.constant 10 : i32
    %c0_i32 = arith.constant 0 : i32
    %alloca = memref.alloca() : memref<i32>
    %alloca_0 = memref.alloca() : memref<i32>
    memref.store %c0_i32, %alloca_0[] : memref<i32>
    memref.store %c0_i32, %alloca[] : memref<i32>
    %0 = arts.datablock "inout" ptr[%alloca : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    %1 = arts.datablock "inout" ptr[%alloca_0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4] {single} -> memref<i32>
    arts.edt dependencies(%0, %1) : (memref<i32>, memref<i32>) attributes {sync} {
      %2 = arts.event[] -> {single} : memref<i64>
      %3 = arts.datablock "out" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4], outEvent[%2] {hasPtrDb, single} -> memref<i32>
      arts.edt dependencies(%3) : (memref<i32>) attributes {task} {
        %8 = llvm.mlir.addressof @str0 : !llvm.ptr
        %9 = llvm.getelementptr %8[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<24 x i8>
        %10 = llvm.call @printf(%9) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
        memref.store %c10_i32, %3[] : memref<i32>
        arts.yield
      }
      %4 = arts.event[] -> {single} : memref<i64>
      %5 = arts.datablock "out" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4], outEvent[%4] {hasPtrDb, single} -> memref<i32>
      %6 = arts.datablock "in" ptr[%1 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4], inEvent[%2] {hasPtrDb, single} -> memref<i32>
      arts.edt dependencies(%5, %6) : (memref<i32>, memref<i32>) attributes {task} {
        %8 = memref.load %6[] : memref<i32>
        %9 = llvm.mlir.addressof @str1 : !llvm.ptr
        %10 = llvm.getelementptr %9[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<37 x i8>
        %11 = llvm.call @printf(%10, %8) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
        %12 = arith.addi %8, %c5_i32 : i32
        memref.store %12, %5[] : memref<i32>
        arts.yield
      }
      %7 = arts.datablock "in" ptr[%0 : memref<i32>], indices[], sizes[], type[i32], typeSize[%c4], inEvent[%4] {hasPtrDb, single} -> memref<i32>
      arts.edt dependencies(%7) : (memref<i32>) attributes {task} {
        %8 = memref.load %7[] : memref<i32>
        %9 = llvm.mlir.addressof @str2 : !llvm.ptr
        %10 = llvm.getelementptr %9[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<29 x i8>
        %11 = llvm.call @printf(%10, %8) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
        arts.yield
      }
      arts.yield
    }
    return %c0_i32 : i32
  }
}
[convert-arts-to-llvm] Iterate over all the functions
[convert-arts-to-llvm] Lowering arts.datablock
[convert-arts-to-llvm] Lowering arts.datablock
[convert-arts-to-llvm] Lowering arts.edt sync
-----------------------------------------
[convert-arts-to-llvm] Sync done EDT created
[convert-arts-to-llvm] Parallel epoch created
Processing dependencies to record
Processing dependencies to signal
[convert-arts-to-llvm] Parallel EDT created
[convert-arts-to-llvm] Parallel op lowered

[convert-arts-to-llvm] Lowering arts.event
[convert-arts-to-llvm] Lowering arts.datablock
[convert-arts-to-llvm] Lowering arts.edt
Processing dependencies to record
Processing dependencies to signal
[convert-arts-to-llvm] Lowering arts.event
[convert-arts-to-llvm] Lowering arts.datablock
[convert-arts-to-llvm] Lowering arts.datablock
[convert-arts-to-llvm] Lowering arts.edt
Processing dependencies to record
Processing dependencies to signal
[convert-arts-to-llvm] Lowering arts.datablock
[convert-arts-to-llvm] Lowering arts.edt
Processing dependencies to record
Processing dependencies to signal
-----------------------------------------
ConvertArtsToLLVMPass FINISHED 
-----------------------------------------
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @artsAddDependence(i64, i64, i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEventSatisfySlot(i64, i64, i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentEpochGuid() -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEventCreate(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsSignalEdt(i64, i32, i64) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreateWithEpoch(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsInitializeAndStartEpoch(i64, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreate(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsDbCreateWithGuidAndData(i64, memref<?xi8>, i64) -> memref<?xi8> attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsReserveGuidRoute(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentNode() -> i32 attributes {llvm.linkage = #llvm.linkage<external>, llvm.readnone}
  llvm.mlir.global internal constant @str2("Task 3: Final value of b=%d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("Task 2: Reading a=%d and updating b\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("Task 1: Initializing a\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @compute() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c4 = arith.constant 4 : index
    %c5_i32 = arith.constant 5 : i32
    %c10_i32 = arith.constant 10 : i32
    %c0_i32 = arith.constant 0 : i32
    %0 = call @artsGetCurrentNode() : () -> i32
    %c9_i32 = arith.constant 9 : i32
    %1 = call @artsReserveGuidRoute(%c9_i32, %0) : (i32, i32) -> i64
    %2 = call @artsGetCurrentNode() : () -> i32
    %c9_i32_0 = arith.constant 9 : i32
    %3 = call @artsReserveGuidRoute(%c9_i32_0, %2) : (i32, i32) -> i64
    %4 = call @artsGetCurrentNode() : () -> i32
    %c0_i32_1 = arith.constant 0 : i32
    %alloca = memref.alloca() : memref<i32>
    memref.store %c0_i32_1, %alloca[] : memref<i32>
    %alloca_2 = memref.alloca() : memref<i32>
    %c0_i32_3 = arith.constant 0 : i32
    memref.store %c0_i32_3, %alloca_2[] : memref<i32>
    %5 = memref.load %alloca_2[] : memref<i32>
    %6 = arith.index_cast %5 : i32 to index
    %alloca_4 = memref.alloca(%6) : memref<?xi64>
    %c1_i32 = arith.constant 1 : i32
    %7 = "polygeist.get_func"() <{name = @__arts_edt_1}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %8 = "polygeist.pointer2memref"(%7) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %9 = call @artsEdtCreate(%8, %4, %5, %alloca_4, %c1_i32) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
    %c0_i32_5 = arith.constant 0 : i32
    %10 = call @artsInitializeAndStartEpoch(%9, %c0_i32_5) : (i64, i32) -> i64
    %11 = call @artsGetCurrentNode() : () -> i32
    %c0_i32_6 = arith.constant 0 : i32
    %alloca_7 = memref.alloca() : memref<i32>
    memref.store %c0_i32_6, %alloca_7[] : memref<i32>
    %alloca_8 = memref.alloca() : memref<i32>
    memref.store %c0_i32_6, %alloca_8[] : memref<i32>
    %12 = memref.load %alloca_8[] : memref<i32>
    %c1_i32_9 = arith.constant 1 : i32
    %13 = memref.load %alloca_8[] : memref<i32>
    %14 = arith.addi %13, %c1_i32_9 : i32
    memref.store %14, %alloca_8[] : memref<i32>
    %15 = memref.load %alloca_8[] : memref<i32>
    %c1_i32_10 = arith.constant 1 : i32
    %16 = memref.load %alloca_8[] : memref<i32>
    %17 = arith.addi %16, %c1_i32_10 : i32
    memref.store %17, %alloca_8[] : memref<i32>
    %18 = memref.load %alloca_8[] : memref<i32>
    %alloca_11 = memref.alloca() : memref<i32>
    %c0_i32_12 = arith.constant 0 : i32
    memref.store %c0_i32_12, %alloca_11[] : memref<i32>
    %19 = memref.load %alloca_11[] : memref<i32>
    %20 = arith.index_cast %19 : i32 to index
    %alloca_13 = memref.alloca(%20) : memref<?xi64>
    %21 = "polygeist.get_func"() <{name = @__arts_edt_2}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %22 = "polygeist.pointer2memref"(%21) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %23 = call @artsEdtCreateWithEpoch(%22, %11, %19, %alloca_13, %18, %10) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    call @artsSignalEdt(%23, %12, %1) : (i64, i32, i64) -> ()
    call @artsSignalEdt(%23, %15, %3) : (i64, i32, i64) -> ()
    return %c0_i32 : i32
  }
  func.func private @__arts_edt_1(i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)
  func.func private @__arts_edt_2(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c4 = arith.constant 4 : index
    %c10_i32 = arith.constant 10 : i32
    %c5_i32 = arith.constant 5 : i32
    %alloca = memref.alloca() : memref<index>
    %c0 = arith.constant 0 : index
    memref.store %c0, %alloca[] : memref<index>
    %c1 = arith.constant 1 : index
    %0 = memref.load %alloca[] : memref<index>
    %1 = memref.load %arg3[%0] : memref<?x!llvm.struct<(i64, i32, ptr)>>
    %2 = llvm.extractvalue %1[0] : !llvm.struct<(i64, i32, ptr)> 
    %3 = llvm.extractvalue %1[2] : !llvm.struct<(i64, i32, ptr)> 
    %4 = arith.addi %0, %c1 : index
    memref.store %4, %alloca[] : memref<index>
    %5 = memref.load %alloca[] : memref<index>
    %6 = memref.load %arg3[%5] : memref<?x!llvm.struct<(i64, i32, ptr)>>
    %7 = llvm.extractvalue %6[0] : !llvm.struct<(i64, i32, ptr)> 
    %8 = llvm.extractvalue %6[2] : !llvm.struct<(i64, i32, ptr)> 
    %9 = arith.addi %5, %c1 : index
    memref.store %9, %alloca[] : memref<index>
    %10 = call @artsGetCurrentNode() : () -> i32
    %c1_i32 = arith.constant 1 : i32
    %11 = call @artsEventCreate(%10, %c1_i32) : (i32, i32) -> i64
    %12 = call @artsGetCurrentEpochGuid() : () -> i64
    %13 = call @artsGetCurrentNode() : () -> i32
    %c0_i32 = arith.constant 0 : i32
    %alloca_0 = memref.alloca() : memref<i32>
    memref.store %c0_i32, %alloca_0[] : memref<i32>
    %alloca_1 = memref.alloca() : memref<i32>
    memref.store %c0_i32, %alloca_1[] : memref<i32>
    %14 = memref.load %alloca_1[] : memref<i32>
    %c1_i32_2 = arith.constant 1 : i32
    %15 = memref.load %alloca_1[] : memref<i32>
    %16 = arith.addi %15, %c1_i32_2 : i32
    memref.store %16, %alloca_1[] : memref<i32>
    %17 = memref.load %alloca_0[] : memref<i32>
    %18 = arith.addi %17, %c1_i32_2 : i32
    memref.store %18, %alloca_0[] : memref<i32>
    %19 = memref.load %alloca_1[] : memref<i32>
    %alloca_3 = memref.alloca() : memref<i32>
    %c0_i32_4 = arith.constant 0 : i32
    memref.store %c0_i32_4, %alloca_3[] : memref<i32>
    %20 = memref.load %alloca_3[] : memref<i32>
    %21 = memref.load %alloca_0[] : memref<i32>
    %22 = arith.addi %20, %21 : i32
    memref.store %22, %alloca_3[] : memref<i32>
    %23 = memref.load %alloca_3[] : memref<i32>
    %24 = arith.index_cast %23 : i32 to index
    %alloca_5 = memref.alloca(%24) : memref<?xi64>
    %alloca_6 = memref.alloca() : memref<i32>
    %c0_i32_7 = arith.constant 0 : i32
    memref.store %c0_i32_7, %alloca_6[] : memref<i32>
    %25 = memref.load %alloca_6[] : memref<i32>
    %26 = arith.index_cast %25 : i32 to index
    memref.store %11, %alloca_5[%26] : memref<?xi64>
    %c1_i32_8 = arith.constant 1 : i32
    %27 = arith.addi %25, %c1_i32_8 : i32
    memref.store %27, %alloca_6[] : memref<i32>
    %28 = "polygeist.get_func"() <{name = @__arts_edt_3}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %29 = "polygeist.pointer2memref"(%28) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %30 = call @artsEdtCreateWithEpoch(%29, %13, %23, %alloca_5, %19, %12) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    %31 = call @artsGetCurrentNode() : () -> i32
    %c1_i32_9 = arith.constant 1 : i32
    %32 = call @artsEventCreate(%31, %c1_i32_9) : (i32, i32) -> i64
    %33 = call @artsGetCurrentEpochGuid() : () -> i64
    %34 = call @artsGetCurrentNode() : () -> i32
    %c0_i32_10 = arith.constant 0 : i32
    %alloca_11 = memref.alloca() : memref<i32>
    memref.store %c0_i32_10, %alloca_11[] : memref<i32>
    %alloca_12 = memref.alloca() : memref<i32>
    memref.store %c0_i32_10, %alloca_12[] : memref<i32>
    %35 = memref.load %alloca_12[] : memref<i32>
    %c1_i32_13 = arith.constant 1 : i32
    %36 = memref.load %alloca_12[] : memref<i32>
    %37 = arith.addi %36, %c1_i32_13 : i32
    memref.store %37, %alloca_12[] : memref<i32>
    %38 = memref.load %alloca_11[] : memref<i32>
    %39 = arith.addi %38, %c1_i32_13 : i32
    memref.store %39, %alloca_11[] : memref<i32>
    %40 = memref.load %alloca_12[] : memref<i32>
    %c1_i32_14 = arith.constant 1 : i32
    %41 = memref.load %alloca_12[] : memref<i32>
    %42 = arith.addi %41, %c1_i32_14 : i32
    memref.store %42, %alloca_12[] : memref<i32>
    %43 = memref.load %alloca_12[] : memref<i32>
    %alloca_15 = memref.alloca() : memref<i32>
    %c0_i32_16 = arith.constant 0 : i32
    memref.store %c0_i32_16, %alloca_15[] : memref<i32>
    %44 = memref.load %alloca_15[] : memref<i32>
    %45 = memref.load %alloca_11[] : memref<i32>
    %46 = arith.addi %44, %45 : i32
    memref.store %46, %alloca_15[] : memref<i32>
    %47 = memref.load %alloca_15[] : memref<i32>
    %48 = arith.index_cast %47 : i32 to index
    %alloca_17 = memref.alloca(%48) : memref<?xi64>
    %alloca_18 = memref.alloca() : memref<i32>
    %c0_i32_19 = arith.constant 0 : i32
    memref.store %c0_i32_19, %alloca_18[] : memref<i32>
    %49 = memref.load %alloca_18[] : memref<i32>
    %50 = arith.index_cast %49 : i32 to index
    memref.store %32, %alloca_17[%50] : memref<?xi64>
    %c1_i32_20 = arith.constant 1 : i32
    %51 = arith.addi %49, %c1_i32_20 : i32
    memref.store %51, %alloca_18[] : memref<i32>
    %52 = "polygeist.get_func"() <{name = @__arts_edt_4}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %53 = "polygeist.pointer2memref"(%52) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %54 = call @artsEdtCreateWithEpoch(%53, %34, %47, %alloca_17, %43, %33) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    call @artsAddDependence(%11, %54, %40) : (i64, i64, i32) -> ()
    %55 = call @artsGetCurrentEpochGuid() : () -> i64
    %56 = call @artsGetCurrentNode() : () -> i32
    %c0_i32_21 = arith.constant 0 : i32
    %alloca_22 = memref.alloca() : memref<i32>
    memref.store %c0_i32_21, %alloca_22[] : memref<i32>
    %alloca_23 = memref.alloca() : memref<i32>
    memref.store %c0_i32_21, %alloca_23[] : memref<i32>
    %57 = memref.load %alloca_23[] : memref<i32>
    %c1_i32_24 = arith.constant 1 : i32
    %58 = memref.load %alloca_23[] : memref<i32>
    %59 = arith.addi %58, %c1_i32_24 : i32
    memref.store %59, %alloca_23[] : memref<i32>
    %60 = memref.load %alloca_23[] : memref<i32>
    %alloca_25 = memref.alloca() : memref<i32>
    %c0_i32_26 = arith.constant 0 : i32
    memref.store %c0_i32_26, %alloca_25[] : memref<i32>
    %61 = memref.load %alloca_25[] : memref<i32>
    %62 = arith.index_cast %61 : i32 to index
    %alloca_27 = memref.alloca(%62) : memref<?xi64>
    %63 = "polygeist.get_func"() <{name = @__arts_edt_5}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %64 = "polygeist.pointer2memref"(%63) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %65 = call @artsEdtCreateWithEpoch(%64, %56, %61, %alloca_27, %60, %55) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    call @artsAddDependence(%32, %65, %57) : (i64, i64, i32) -> ()
    return
  }
  func.func private @__arts_edt_3(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c10_i32 = arith.constant 10 : i32
    %alloca = memref.alloca() : memref<index>
    %c0 = arith.constant 0 : index
    memref.store %c0, %alloca[] : memref<index>
    %c1 = arith.constant 1 : index
    %0 = memref.load %alloca[] : memref<index>
    %1 = memref.load %arg3[%0] : memref<?x!llvm.struct<(i64, i32, ptr)>>
    %2 = llvm.extractvalue %1[0] : !llvm.struct<(i64, i32, ptr)> 
    %3 = llvm.extractvalue %1[2] : !llvm.struct<(i64, i32, ptr)> 
    %4 = "polygeist.pointer2memref"(%3) : (!llvm.ptr) -> memref<?xi8>
    %5 = arith.addi %0, %c1 : index
    memref.store %5, %alloca[] : memref<index>
    %6 = llvm.mlir.addressof @str0 : !llvm.ptr
    %7 = llvm.getelementptr %6[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<24 x i8>
    %8 = llvm.call @printf(%7) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %9 = "polygeist.memref2pointer"(%4) : (memref<?xi8>) -> !llvm.ptr
    %10 = llvm.getelementptr %9[] : (!llvm.ptr) -> !llvm.ptr, i32
    llvm.store %c10_i32, %10 : i32, !llvm.ptr
    %alloca_0 = memref.alloca() : memref<i32>
    %c0_i32 = arith.constant 0 : i32
    memref.store %c0_i32, %alloca_0[] : memref<i32>
    %11 = memref.load %alloca_0[] : memref<i32>
    %12 = arith.index_cast %11 : i32 to index
    %13 = memref.load %arg1[%12] : memref<?xi64>
    %c0_i32_1 = arith.constant 0 : i32
    call @artsEventSatisfySlot(%13, %2, %c0_i32_1) : (i64, i64, i32) -> ()
    %c1_i32 = arith.constant 1 : i32
    %14 = arith.addi %11, %c1_i32 : i32
    memref.store %14, %alloca_0[] : memref<i32>
    return
  }
  func.func private @__arts_edt_4(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c5_i32 = arith.constant 5 : i32
    %alloca = memref.alloca() : memref<index>
    %c0 = arith.constant 0 : index
    memref.store %c0, %alloca[] : memref<index>
    %c1 = arith.constant 1 : index
    %0 = memref.load %alloca[] : memref<index>
    %1 = memref.load %arg3[%0] : memref<?x!llvm.struct<(i64, i32, ptr)>>
    %2 = llvm.extractvalue %1[0] : !llvm.struct<(i64, i32, ptr)> 
    %3 = llvm.extractvalue %1[2] : !llvm.struct<(i64, i32, ptr)> 
    %4 = "polygeist.pointer2memref"(%3) : (!llvm.ptr) -> memref<?xi8>
    %5 = arith.addi %0, %c1 : index
    memref.store %5, %alloca[] : memref<index>
    %6 = memref.load %alloca[] : memref<index>
    %7 = memref.load %arg3[%6] : memref<?x!llvm.struct<(i64, i32, ptr)>>
    %8 = llvm.extractvalue %7[0] : !llvm.struct<(i64, i32, ptr)> 
    %9 = llvm.extractvalue %7[2] : !llvm.struct<(i64, i32, ptr)> 
    %10 = "polygeist.pointer2memref"(%9) : (!llvm.ptr) -> memref<?xi8>
    %11 = arith.addi %6, %c1 : index
    memref.store %11, %alloca[] : memref<index>
    %12 = "polygeist.memref2pointer"(%10) : (memref<?xi8>) -> !llvm.ptr
    %13 = llvm.getelementptr %12[] : (!llvm.ptr) -> !llvm.ptr, i32
    %14 = llvm.load %13 : !llvm.ptr -> i32
    %15 = llvm.mlir.addressof @str1 : !llvm.ptr
    %16 = llvm.getelementptr %15[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<37 x i8>
    %17 = llvm.call @printf(%16, %14) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
    %18 = arith.addi %14, %c5_i32 : i32
    %19 = "polygeist.memref2pointer"(%4) : (memref<?xi8>) -> !llvm.ptr
    %20 = llvm.getelementptr %19[] : (!llvm.ptr) -> !llvm.ptr, i32
    llvm.store %18, %20 : i32, !llvm.ptr
    %alloca_0 = memref.alloca() : memref<i32>
    %c0_i32 = arith.constant 0 : i32
    memref.store %c0_i32, %alloca_0[] : memref<i32>
    %21 = memref.load %alloca_0[] : memref<i32>
    %22 = arith.index_cast %21 : i32 to index
    %23 = memref.load %arg1[%22] : memref<?xi64>
    %c0_i32_1 = arith.constant 0 : i32
    call @artsEventSatisfySlot(%23, %2, %c0_i32_1) : (i64, i64, i32) -> ()
    %c1_i32 = arith.constant 1 : i32
    %24 = arith.addi %21, %c1_i32 : i32
    memref.store %24, %alloca_0[] : memref<i32>
    return
  }
  func.func private @__arts_edt_5(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %alloca = memref.alloca() : memref<index>
    %c0 = arith.constant 0 : index
    memref.store %c0, %alloca[] : memref<index>
    %c1 = arith.constant 1 : index
    %0 = memref.load %alloca[] : memref<index>
    %1 = memref.load %arg3[%0] : memref<?x!llvm.struct<(i64, i32, ptr)>>
    %2 = llvm.extractvalue %1[0] : !llvm.struct<(i64, i32, ptr)> 
    %3 = llvm.extractvalue %1[2] : !llvm.struct<(i64, i32, ptr)> 
    %4 = "polygeist.pointer2memref"(%3) : (!llvm.ptr) -> memref<?xi8>
    %5 = arith.addi %0, %c1 : index
    memref.store %5, %alloca[] : memref<index>
    %6 = "polygeist.memref2pointer"(%4) : (memref<?xi8>) -> !llvm.ptr
    %7 = llvm.getelementptr %6[] : (!llvm.ptr) -> !llvm.ptr, i32
    %8 = llvm.load %7 : !llvm.ptr -> i32
    %9 = llvm.mlir.addressof @str2 : !llvm.ptr
    %10 = llvm.getelementptr %9[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<29 x i8>
    %11 = llvm.call @printf(%10, %8) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
    return
  }
}
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func private @artsAddDependence(i64, i64, i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEventSatisfySlot(i64, i64, i32) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentEpochGuid() -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEventCreate(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsSignalEdt(i64, i32, i64) attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreateWithEpoch(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsInitializeAndStartEpoch(i64, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsEdtCreate(memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsDbCreateWithGuidAndData(i64, memref<?xi8>, i64) -> memref<?xi8> attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsReserveGuidRoute(i32, i32) -> i64 attributes {llvm.linkage = #llvm.linkage<external>}
  func.func private @artsGetCurrentNode() -> i32 attributes {llvm.linkage = #llvm.linkage<external>, llvm.readnone}
  llvm.mlir.global internal constant @str2("Task 3: Final value of b=%d\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str1("Task 2: Reading a=%d and updating b\0A\00") {addr_space = 0 : i32}
  llvm.mlir.global internal constant @str0("Task 1: Initializing a\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @compute() -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
    %c2_i32 = arith.constant 2 : i32
    %c1_i32 = arith.constant 1 : i32
    %c9_i32 = arith.constant 9 : i32
    %c0_i32 = arith.constant 0 : i32
    %0 = call @artsGetCurrentNode() : () -> i32
    %1 = call @artsReserveGuidRoute(%c9_i32, %0) : (i32, i32) -> i64
    %2 = call @artsGetCurrentNode() : () -> i32
    %3 = call @artsReserveGuidRoute(%c9_i32, %2) : (i32, i32) -> i64
    %4 = call @artsGetCurrentNode() : () -> i32
    %alloca = memref.alloca() : memref<0xi64>
    %cast = memref.cast %alloca : memref<0xi64> to memref<?xi64>
    %5 = "polygeist.get_func"() <{name = @__arts_edt_1}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %6 = "polygeist.pointer2memref"(%5) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %7 = call @artsEdtCreate(%6, %4, %c0_i32, %cast, %c1_i32) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32) -> i64
    %8 = call @artsInitializeAndStartEpoch(%7, %c0_i32) : (i64, i32) -> i64
    %9 = call @artsGetCurrentNode() : () -> i32
    %alloca_0 = memref.alloca() : memref<0xi64>
    %cast_1 = memref.cast %alloca_0 : memref<0xi64> to memref<?xi64>
    %10 = "polygeist.get_func"() <{name = @__arts_edt_2}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %11 = "polygeist.pointer2memref"(%10) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %12 = call @artsEdtCreateWithEpoch(%11, %9, %c0_i32, %cast_1, %c2_i32, %8) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    call @artsSignalEdt(%12, %c0_i32, %1) : (i64, i32, i64) -> ()
    call @artsSignalEdt(%12, %c1_i32, %3) : (i64, i32, i64) -> ()
    return %c0_i32 : i32
  }
  func.func private @__arts_edt_1(i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)
  func.func private @__arts_edt_2(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0 = arith.constant 0 : index
    %c1_i32 = arith.constant 1 : i32
    %c2_i32 = arith.constant 2 : i32
    %c0_i32 = arith.constant 0 : i32
    %0 = call @artsGetCurrentNode() : () -> i32
    %1 = call @artsEventCreate(%0, %c1_i32) : (i32, i32) -> i64
    %2 = call @artsGetCurrentEpochGuid() : () -> i64
    %3 = call @artsGetCurrentNode() : () -> i32
    %alloca = memref.alloca() : memref<1xi64>
    %cast = memref.cast %alloca : memref<1xi64> to memref<?xi64>
    memref.store %1, %alloca[%c0] : memref<1xi64>
    %4 = "polygeist.get_func"() <{name = @__arts_edt_3}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %5 = "polygeist.pointer2memref"(%4) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %6 = call @artsEdtCreateWithEpoch(%5, %3, %c1_i32, %cast, %c1_i32, %2) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    %7 = call @artsGetCurrentNode() : () -> i32
    %8 = call @artsEventCreate(%7, %c1_i32) : (i32, i32) -> i64
    %9 = call @artsGetCurrentEpochGuid() : () -> i64
    %10 = call @artsGetCurrentNode() : () -> i32
    %alloca_0 = memref.alloca() : memref<1xi64>
    %cast_1 = memref.cast %alloca_0 : memref<1xi64> to memref<?xi64>
    memref.store %8, %alloca_0[%c0] : memref<1xi64>
    %11 = "polygeist.get_func"() <{name = @__arts_edt_4}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %12 = "polygeist.pointer2memref"(%11) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %13 = call @artsEdtCreateWithEpoch(%12, %10, %c1_i32, %cast_1, %c2_i32, %9) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    call @artsAddDependence(%1, %13, %c1_i32) : (i64, i64, i32) -> ()
    %14 = call @artsGetCurrentEpochGuid() : () -> i64
    %15 = call @artsGetCurrentNode() : () -> i32
    %alloca_2 = memref.alloca() : memref<0xi64>
    %cast_3 = memref.cast %alloca_2 : memref<0xi64> to memref<?xi64>
    %16 = "polygeist.get_func"() <{name = @__arts_edt_5}> : () -> !llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %17 = "polygeist.pointer2memref"(%16) : (!llvm.ptr<!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>) -> memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>
    %18 = call @artsEdtCreateWithEpoch(%17, %15, %c0_i32, %cast_3, %c1_i32, %14) : (memref<?x!llvm.func<void (i32, memref<?xi64>, i32, memref<?x!llvm.struct<(i64, i32, ptr)>>)>>, i32, i32, memref<?xi64>, i32, i64) -> i64
    call @artsAddDependence(%8, %18, %c0_i32) : (i64, i64, i32) -> ()
    return
  }
  func.func private @__arts_edt_3(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0 = arith.constant 0 : index
    %c0_i32 = arith.constant 0 : i32
    %c10_i32 = arith.constant 10 : i32
    %0 = memref.load %arg3[%c0] : memref<?x!llvm.struct<(i64, i32, ptr)>>
    %1 = llvm.extractvalue %0[0] : !llvm.struct<(i64, i32, ptr)> 
    %2 = llvm.extractvalue %0[2] : !llvm.struct<(i64, i32, ptr)> 
    %3 = llvm.mlir.addressof @str0 : !llvm.ptr
    %4 = llvm.getelementptr %3[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<24 x i8>
    %5 = llvm.call @printf(%4) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr) -> i32
    %6 = llvm.getelementptr %2[] : (!llvm.ptr) -> !llvm.ptr, i32
    llvm.store %c10_i32, %6 : i32, !llvm.ptr
    %7 = memref.load %arg1[%c0] : memref<?xi64>
    call @artsEventSatisfySlot(%7, %1, %c0_i32) : (i64, i64, i32) -> ()
    return
  }
  func.func private @__arts_edt_4(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c0_i32 = arith.constant 0 : i32
    %c5_i32 = arith.constant 5 : i32
    %0 = memref.load %arg3[%c0] : memref<?x!llvm.struct<(i64, i32, ptr)>>
    %1 = llvm.extractvalue %0[0] : !llvm.struct<(i64, i32, ptr)> 
    %2 = llvm.extractvalue %0[2] : !llvm.struct<(i64, i32, ptr)> 
    %3 = memref.load %arg3[%c1] : memref<?x!llvm.struct<(i64, i32, ptr)>>
    %4 = llvm.extractvalue %3[2] : !llvm.struct<(i64, i32, ptr)> 
    %5 = llvm.getelementptr %4[] : (!llvm.ptr) -> !llvm.ptr, i32
    %6 = llvm.load %5 : !llvm.ptr -> i32
    %7 = llvm.mlir.addressof @str1 : !llvm.ptr
    %8 = llvm.getelementptr %7[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<37 x i8>
    %9 = llvm.call @printf(%8, %6) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
    %10 = arith.addi %6, %c5_i32 : i32
    %11 = llvm.getelementptr %2[] : (!llvm.ptr) -> !llvm.ptr, i32
    llvm.store %10, %11 : i32, !llvm.ptr
    %12 = memref.load %arg1[%c0] : memref<?xi64>
    call @artsEventSatisfySlot(%12, %1, %c0_i32) : (i64, i64, i32) -> ()
    return
  }
  func.func private @__arts_edt_5(%arg0: i32, %arg1: memref<?xi64>, %arg2: i32, %arg3: memref<?x!llvm.struct<(i64, i32, ptr)>>) {
    %c0 = arith.constant 0 : index
    %0 = memref.load %arg3[%c0] : memref<?x!llvm.struct<(i64, i32, ptr)>>
    %1 = llvm.extractvalue %0[2] : !llvm.struct<(i64, i32, ptr)> 
    %2 = llvm.getelementptr %1[] : (!llvm.ptr) -> !llvm.ptr, i32
    %3 = llvm.load %2 : !llvm.ptr -> i32
    %4 = llvm.mlir.addressof @str2 : !llvm.ptr
    %5 = llvm.getelementptr %4[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<29 x i8>
    %6 = llvm.call @printf(%5, %3) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32) -> i32
    return
  }
}

