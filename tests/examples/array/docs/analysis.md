# Array example analysis

Walk through these steps and fix any problem that you find in the way

1. **Navigate to the array example directory:**

   ```bash
   cd ~/Documents/carts/tests/examples/array
   ```

2. **Build carts if any changes were made:**

   ```bash
   carts build
   ```

   If there is no array.mlir run:

   ```bash
      carts cgeist array.c -O0 --print-debug-info -S --raise-scf-to-affine &> array_seq.mlir
      carts run array_seq.mlir --collect-metadata &> array_arts_metadata.mlir
      carts cgeist array.c -O0 --print-debug-info -S -fopenmp --raise-scf-to-affine &> array.mlir
   ```

3. **Run the pipeline and stop after any stage**
    Remember that you can also debug any pass. For example to stop at create_db pass

   Check for example the create db pass
   ```bash
      carts run array.mlir --create-dbs &> array_create_dbs.mlir
   ```
   Analyze the inline comments, I provided a summarized version of the module after all finished. 
   ```mlir
   module attributes {...} {
      ...
      func.func @main(%arg0: i32, %arg1: memref<?xmemref<?xi8>>) -> i32 attributes {llvm.linkage = #llvm.linkage<external>} {
         ...
         scf.if %1 {
            %5 = llvm.mlir.addressof @str0 : !llvm.ptr
            %6 = llvm.getelementptr %5[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<13 x i8>
            %7 = memref.load %arg1[%c0] : memref<?xmemref<?xi8>>
            %8 = polygeist.memref2pointer %7 : memref<?xi8> to !llvm.ptr
            %9 = llvm.call @printf(%6, %8) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, !llvm.ptr) -> i32
         }
         %4 = arith.select %2, %c0_i32, %3 : i32
         scf.if %2 {
            %5 = memref.load %arg1[%c1] : memref<?xmemref<?xi8>>
            ...
            /// This is fine grained db_alloc - this is expected because the SSA form of the array matches the control_dbs. For more details check the CreateDbs.cpp pass.
            %guid, %ptr = arts.db_alloc[<inout>, <heap>, <write>] route(%c0_i32 : i32) sizes[%10] elementType(i32) elementSizes[%c1] {...} : (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
            %guid_0, %ptr_1 = arts.db_alloc[<inout>, <heap>, <write>] route(%c0_i32 : i32) sizes[%10] elementType(i32) elementSizes[%c1] {...} : (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
            /// This is the epoch that synchronizes the two EDTs.
            %18 = arts.epoch {
            ...
            scf.for %arg2 = %c0 to %25 step %c1 {
               %31 = arith.index_cast %arg2 : index to i32
               /// We acquire the ptr db, with indices %arg2, offsets %c0, sizes %c1, and twin_diff set to false
               /// This is correct, because we were able to prove that the access is disjoint across the two EDTs by using the control_dbs. For more details check the CreateDbs.cpp pass.
               %guid_2, %ptr_3 = arts.db_acquire[<inout>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<memref<?xi32>>>) indices[%arg2] offsets[%c0] sizes[%c1] {arts.twin_diff = false} -> (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
               /// This is the EDT that writes to the ptr db.
               arts.edt <task> <intranode> route(%c0_i32) (%ptr_3) : memref<?xmemref<memref<?xi32>>> {
               ^bb0(%arg3: memref<?xmemref<memref<?xi32>>>):
                  /// We ref the db with indices %c0, which is the first element of the db (outer dimension)
                  %34 = arts.db_ref %arg3[%c0] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
                  /// We store the value in the first element of the db (inner dimension)
                  memref.store %31, %34[%c0] : memref<?xi32>
                  /// We load the value from the first element of the db.
                  %35 = memref.load %34[%c0] : memref<?xi32>
                  %36 = llvm.call @printf(%33, %31, %31, %35) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
                  arts.db_release(%arg3) : memref<?xmemref<memref<?xi32>>>
               }
               %32 = arith.cmpi eq, %31, %c0_i32 : i32
               scf.if %32 {
                  /// These acquires are also fine grained acquires and we have twin_diff = false
                  %guid_4, %ptr_5 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<memref<?xi32>>>) indices[%arg2] offsets[%%c0] sizes[%c1] {arts.twin_diff = false} -> (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
                  %guid_6, %ptr_7 = arts.db_acquire[<inout>] (%guid_0 : memref<?xi64>, %ptr_1 : memref<?xmemref<memref<?xi32>>>) indices[%arg2] offsets[%c0] sizes[%c1] {arts.twin_diff = false} -> (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
                  /// Carefully check the dbref + load/store pattern in here.
                  /// The dbref works on the outer dimension of the db, and the load/store works on the inner dimension.
                  /// The pattern within the EDT is correct because the dbAcquire has as index %arg2...
                  arts.edt <task> <intranode> route(%c0_i32) (%ptr_5, %ptr_7) : memref<?xmemref<memref<?xi32>>>, memref<?xmemref<memref<?xi32>>> {
                  ^bb0(%arg3: memref<?xmemref<memref<?xi32>>>, %arg4: memref<?xmemref<memref<?xi32>>>):
                  %33 = llvm.getelementptr %27[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<34 x i8>
                  %34 = llvm.getelementptr %28[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<35 x i8>
                  %35 = arts.db_ref %arg3[%c0] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
                  %36 = memref.load %35[%c0] : memref<?xi32>
                  %37 = llvm.call @printf(%33, %31, %31, %36) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
                  %38 = memref.load %35[%c0] : memref<?xi32>
                  %39 = arith.addi %38, %c5_i32 : i32
                  %40 = arts.db_ref %arg4[%c0] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
                  memref.store %39, %40[%c0] : memref<?xi32>
                  %41 = memref.load %40[%c0] : memref<?xi32>
                  %42 = llvm.call @printf(%34, %31, %31, %41) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
                  arts.db_release(%arg3) : memref<?xmemref<memref<?xi32>>>
                  arts.db_release(%arg4) : memref<?xmemref<memref<?xi32>>>
                  }
               } else {
                  %33 = arith.addi %31, %c-1_i32 : i32
                  %34 = arith.index_cast %33 : i32 to index
                  /// These acquires are also fine grained acquires and we have twin_diff = false
                  %guid_4, %ptr_5 = arts.db_acquire[<in>] (%guid : memref<?xi64>, %ptr : memref<?xmemref<memref<?xi32>>>) indices[%arg2] offsets[%arg2] sizes[%c1] {arts.twin_diff = false} -> (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
                  %guid_6, %ptr_7 = arts.db_acquire[<in>] (%guid_0 : memref<?xi64>, %ptr_1 : memref<?xmemref<memref<?xi32>>>) indices[%34] offsets[%34] sizes[%c1] {arts.twin_diff = false} -> (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
                  %guid_8, %ptr_9 = arts.db_acquire[<inout>] (%guid_0 : memref<?xi64>, %ptr_1 : memref<?xmemref<memref<?xi32>>>) indices[%arg2] offsets[%arg2] sizes[%c1] {arts.twin_diff = false} -> (memref<?xi64>, memref<?xmemref<memref<?xi32>>>)
                  arts.edt <task> <intranode> route(%c0_i32) (%ptr_5, %ptr_7, %ptr_9) : memref<?xmemref<memref<?xi32>>>, memref<?xmemref<memref<?xi32>>>, memref<?xmemref<memref<?xi32>>> {
                  ^bb0(%arg3: memref<?xmemref<memref<?xi32>>>, %arg4: memref<?xmemref<memref<?xi32>>>, %arg5: memref<?xmemref<memref<?xi32>>>):
                  %35 = llvm.getelementptr %29[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<46 x i8>
                  %36 = llvm.getelementptr %30[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<35 x i8>
                  %37 = arts.db_ref %arg3[%c0] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
                  %38 = memref.load %37[%c0] : memref<?xi32>
                  %39 = arts.db_ref %arg4[%c0] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
                  %40 = memref.load %39[%c0] : memref<?xi32>
                  %41 = llvm.call @printf(%35, %31, %31, %38, %33, %40) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32, i32, i32) -> i32
                  %42 = memref.load %37[%c0] : memref<?xi32>
                  %43 = memref.load %39[%c0] : memref<?xi32>
                  %44 = arith.addi %42, %43 : i32
                  %45 = arith.addi %44, %c5_i32 : i32
                  %46 = arts.db_ref %arg5[%c0] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
                  memref.store %45, %46[%c0] : memref<?xi32>
                  %47 = memref.load %46[%c0] : memref<?xi32>
                  %48 = llvm.call @printf(%36, %31, %31, %47) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32) -> i32
                  arts.db_release(%arg3) : memref<?xmemref<memref<?xi32>>>
                  arts.db_release(%arg4) : memref<?xmemref<memref<?xi32>>>
                  arts.db_release(%arg5) : memref<?xmemref<memref<?xi32>>>
                  }
               }
            } {}
            } : i64
            ...
            /// The dballoc was also rewritten, check that we have the correct dbref + load/store pattern in here.
            /// The dbref works on the outer dimension of the db, and the load/store works on the inner dimension.
            scf.for %arg2 = %c0 to %22 step %c1 {
            %25 = arith.index_cast %arg2 : index to i32
            %26 = arts.db_ref %ptr[%arg2] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
            %27 = memref.load %26[%c0] : memref<?xi32>
            %28 = arts.db_ref %ptr_1[%arg2] : memref<?xmemref<memref<?xi32>>> -> memref<?xi32>
            %29 = memref.load %28[%c0] : memref<?xi32>
            %30 = llvm.call @printf(%24, %25, %27, %25, %29) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, i32, i32, i32, i32) -> i32
            } {...}
            arts.db_free(%guid_0) : memref<?xi64>
            arts.db_free(%ptr_1) : memref<?xmemref<memref<?xi32>>>
            arts.db_free(%guid) : memref<?xi64>
            arts.db_free(%ptr) : memref<?xmemref<memref<?xi32>>>
         }
         return %4 : i32
      }
      ...
      }

   ```

   Lets analyze the code after running the pipeline and verify if it matches the expected output.

4. **Finally lets carts execute and check**
```bash
   carts execute array.c
   ./array_arts 8
```

---

## Conditional Stencil Pattern (No Bounds Checking Needed)

The array.c example demonstrates a **conditional stencil** pattern that does NOT require bounds checking.

### 1. The Pattern in C Code

```c
if (i == 0) {
  #pragma omp task depend(in : A[i]) depend(inout : B[i])
  { B[i] = A[i] + 5; }
} else {
  #pragma omp task depend(in : A[i]) depend(in : B[i - 1]) depend(inout : B[i])
  { B[i] = A[i] + B[i - 1] + 5; }
}
```

**Key observation:** The `B[i-1]` dependency is **inside the else block** - it's only registered when `i > 0`.

### 2. How CARTS Handles This

Looking at the MLIR above (lines 72-124), notice:

```mlir
%32 = arith.cmpi eq, %31, %c0_i32 : i32      /// Check: is i == 0?
scf.if %32 {
   /// When i == 0: Only A[i] and B[i] dependencies - NO B[i-1]
   %guid_4, %ptr_5 = arts.db_acquire[<in>] ... indices[%arg2] ...
   %guid_6, %ptr_7 = arts.db_acquire[<inout>] ... indices[%arg2] ...
   arts.edt <task> ... (%ptr_5, %ptr_7) ...
} else {
   /// When i > 0: A[i], B[i-1], and B[i] dependencies
   %33 = arith.addi %31, %c-1_i32 : i32       /// Compute i-1
   %34 = arith.index_cast %33 : i32 to index
   %guid_4, %ptr_5 = arts.db_acquire[<in>] ... indices[%arg2] ...
   %guid_6, %ptr_7 = arts.db_acquire[<in>] ... indices[%34] ...   /// B[i-1] ONLY HERE
   %guid_8, %ptr_9 = arts.db_acquire[<inout>] ... indices[%arg2] ...
   arts.edt <task> ... (%ptr_5, %ptr_7, %ptr_9) ...
}
```

The `B[i-1]` access (indices `%34`) is computed and acquired ONLY in the else branch where `i > 0` is guaranteed.

### 3. Comparison with jacobi-task-dep (Unconditional Stencil)

| Aspect | array.c | jacobi-task-dep |
|--------|---------|-----------------|
| **Pattern** | `B[i-1]` in else block | `u[i-1]` unconditional |
| **Guard** | `if (i == 0)` in C code | None in dependency |
| **Bounds Check** | NOT needed | NEEDED |
| **Why** | Control flow guards access | All iterations declare stencil |

In jacobi-task-dep:
```c
#pragma omp task depend(in : u[i-1], u[i], u[i+1]) depend(out : unew[i])
// Dependencies declared for ALL i values, including boundaries!
```

When `i=0`, this declares `u[-1]` which is out of bounds. CARTS must:
1. Generate `bounds_valid` condition: `0 <= (i-1) < size`
2. Use `artsSignalEdtNull` when bounds are invalid

### 4. Why No `bounds_valid` for array.c

CARTS correctly identifies that array.c does NOT need bounds checking because:

1. **Control flow guards the access**: The `scf.if` condition ensures `B[i-1]` is only accessed when `i > 0`
2. **The dependency is conditional**: Unlike jacobi where `u[i-1]` is declared for ALL iterations
3. **No runtime check required**: The C programmer already handled the boundary case explicitly

### 5. Summary: Conditional vs Unconditional Stencil

| Pattern Type | Example | Bounds Check | CARTS Handling |
|--------------|---------|--------------|----------------|
| **Conditional** | `if (i > 0) depend(B[i-1])` | NOT needed | Control flow preserved |
| **Unconditional** | `depend(u[i-1])` for all i | NEEDED | `bounds_valid` + `artsSignalEdtNull` |
