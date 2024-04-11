[arts-transform] Run the ARTSTransformer Module Pass

[arts-transform] [identifyEDTs] Identifying EDTs for: main
[arts-transform] Parallel Region Found: 
    call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr nonnull @5, i32 4, ptr nonnull @main.omp_outlined, ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number), !dbg !24

[arts-transform] Handling done region
Data environment: 
Firstprivate: 0
Private: 0
Shared: 2
  - ptr %number
  - ptr %random_number
Lastprivate: 0


[arts-transform] [identifyEDTs] Identifying EDTs for: par.edt.done
 - Function rewrite 'main.omp_outlined' from void (ptr, ptr, ptr, ptr, ptr, ptr) to void (ptr, ptr, ptr, ptr)
 - New CB:   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
Data environment: 
Firstprivate: 0
Private: 0
Shared: 4
  - ptr %random_number
  - ptr %NewRandom
  - ptr %number
  - ptr %shared_number
Lastprivate: 0


[arts-transform] [identifyEDTs] Identifying EDTs for: parallel.edt
[arts-transform] Task Region Found: 
    %0 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 undef, i32 1, i64 48, i64 16, ptr nonnull @.omp_task_entry.), !dbg !9
 - Function rewrite '.omp_task_entry.' from i32 (i32, ptr) to void (i32, ptr, ptr, i32)
 - New CB:   tail call void @task.edt(i32 %3, ptr %shared_number, ptr %number, i32 %5)
Data environment: 
Firstprivate: 0
Private: 2
  - i32 %0
  - i32 %3
Shared: 2
  - ptr %1
  - ptr %2
Lastprivate: 0


[arts-transform] [identifyEDTs] Identifying EDTs for: task.edt
[arts-transform] Task Region Found: 
    %6 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @3, i32 undef, i32 1, i64 48, i64 1, ptr nonnull @.omp_task_entry..4), !dbg !29
 - Function rewrite '.omp_task_entry..4' from i32 (i32, ptr) to void (i32)
 - New CB:   tail call void @task.edt.8(i32 %8)
Data environment: 
Firstprivate: 0
Private: 1
  - i32 %0
Shared: 0
Lastprivate: 0


[arts-transform] [identifyEDTs] Identifying EDTs for: task.edt.8
[arts-transform] 5 EDTs were identified
[arts-ir-builder] Initializing ARTSIRBuilder
[arts-ir-builder] ARTSIRBuilder initialized
[arts-transform] Initializing EDTAnalyzer attributor

[EDTFunctionAnalyzer] initialize: parallel.edt
[EDTFunctionAnalyzer] updateImpl: parallel.edt

[EDTCallAnalyzer] initialize:   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)

[EDTCallAnalyzer] updateImpl:   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)

[EDTArgAnalyzer] initialize:   %random_number = alloca i32, align 4 - 0 in:   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)

[EDTArgAnalyzer] updateImpl:   %random_number = alloca i32, align 4 in:   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
Visit underlying object   %random_number = alloca i32, align 4

[EDTArgAnalyzer] initialize:   %NewRandom = alloca i32, align 4 - 1 in:   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)

[EDTArgAnalyzer] updateImpl:   %NewRandom = alloca i32, align 4 in:   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
Visit underlying object   %NewRandom = alloca i32, align 4
[0-4] : 2
     - 9 -   store i32 %call1, ptr %NewRandom, align 4, !dbg !23, !tbaa !13
       - c:   %call1 = tail call i32 @rand() #6, !dbg !22
     - 5 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - Same CallBase
     -->RM:                           %1 = load i32, ptr %NewRandom, align 4, !dbg !17, !tbaa !13
       - c: <unknown>

[EDTArgAnalyzer] initialize:   %number = alloca i32, align 4 - 2 in:   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)

[EDTArgAnalyzer] updateImpl:   %number = alloca i32, align 4 in:   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
Visit underlying object   %number = alloca i32, align 4

[EDTArgAnalyzer] initialize:   %shared_number = alloca i32, align 4 - 3 in:   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)

[EDTArgAnalyzer] updateImpl:   %shared_number = alloca i32, align 4 in:   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
Visit underlying object   %shared_number = alloca i32, align 4
[EDTFunctionAnalyzer] All CallSites were successfully Initialized

[EDTFunctionAnalyzer] initialize: main
[EDTFunctionAnalyzer] updateImpl: main

[EDTFunctionAnalyzer] initialize: task.edt.8
[EDTFunctionAnalyzer] updateImpl: task.edt.8

[EDTCallAnalyzer] initialize:   tail call void @task.edt.8(i32 %2)
[EDTCallAnalyzer] EDT does not have shared variables
[EDTFunctionAnalyzer] All CallSites were successfully Initialized

[EDTFunctionAnalyzer] initialize: par.edt.done

[EDTFunctionAnalyzer] initialize: task.edt
[EDTFunctionAnalyzer] updateImpl: task.edt

[EDTCallAnalyzer] initialize:   tail call void @task.edt(i32 %0, ptr %shared_number, ptr %number, i32 %1)

[EDTCallAnalyzer] updateImpl:   tail call void @task.edt(i32 %0, ptr %shared_number, ptr %number, i32 %1)

[EDTArgAnalyzer] initialize: ptr %shared_number - 1 in:   tail call void @task.edt(i32 %0, ptr %shared_number, ptr %number, i32 %1)

[EDTArgAnalyzer] updateImpl: ptr %shared_number in:   tail call void @task.edt(i32 %0, ptr %shared_number, ptr %number, i32 %1)

[EDTArgAnalyzer] initialize: ptr %number - 2 in:   tail call void @task.edt(i32 %0, ptr %shared_number, ptr %number, i32 %1)

[EDTArgAnalyzer] updateImpl: ptr %number in:   tail call void @task.edt(i32 %0, ptr %shared_number, ptr %number, i32 %1)
[EDTFunctionAnalyzer] All CallSites were successfully Initialized
[arts-transform] [Attributor] Process started
[EDTFunctionAnalyzer] updateImpl: parallel.edt
[EDTFunctionAnalyzer] All CallSites were successfully Initialized

[EDTCallAnalyzer] updateImpl:   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)

[EDTArgAnalyzer] updateImpl:   %random_number = alloca i32, align 4 in:   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
Visit underlying object   %random_number = alloca i32, align 4

[EDTArgAnalyzer] updateImpl:   %number = alloca i32, align 4 in:   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
Visit underlying object   %number = alloca i32, align 4

[EDTArgAnalyzer] updateImpl:   %shared_number = alloca i32, align 4 in:   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
Visit underlying object   %shared_number = alloca i32, align 4
[EDTFunctionAnalyzer] updateImpl: task.edt
[EDTFunctionAnalyzer] All CallSites were successfully Initialized

[EDTCallAnalyzer] updateImpl:   tail call void @task.edt(i32 %0, ptr %shared_number, ptr %number, i32 %1)

[EDTArgAnalyzer] updateImpl: ptr %shared_number in:   tail call void @task.edt(i32 %0, ptr %shared_number, ptr %number, i32 %1)

[EDTArgAnalyzer] updateImpl: ptr %number in:   tail call void @task.edt(i32 %0, ptr %shared_number, ptr %number, i32 %1)

[EDTArgAnalyzer] updateImpl: ptr %shared_number in:   tail call void @task.edt(i32 %0, ptr %shared_number, ptr %number, i32 %1)
Visit underlying object   %shared_number = alloca i32, align 4

[EDTArgAnalyzer] updateImpl: ptr %number in:   tail call void @task.edt(i32 %0, ptr %shared_number, ptr %number, i32 %1)
Visit underlying object   %number = alloca i32, align 4
[EDTFunctionAnalyzer] updateImpl: parallel.edt
[EDTFunctionAnalyzer] All CallSites were successfully Initialized
[EDTFunctionAnalyzer] updateImpl: par.edt.done

[EDTCallAnalyzer] initialize:   call void @par.edt.done(ptr %number, ptr %random_number), !dbg !25

[EDTCallAnalyzer] updateImpl:   call void @par.edt.done(ptr %number, ptr %random_number), !dbg !25

[EDTArgAnalyzer] initialize:   %number = alloca i32, align 4 - 0 in:   call void @par.edt.done(ptr %number, ptr %random_number), !dbg !25

[EDTArgAnalyzer] updateImpl:   %number = alloca i32, align 4 in:   call void @par.edt.done(ptr %number, ptr %random_number), !dbg !25
Visit underlying object   %number = alloca i32, align 4

[EDTArgAnalyzer] initialize:   %random_number = alloca i32, align 4 - 1 in:   call void @par.edt.done(ptr %number, ptr %random_number), !dbg !25

[EDTArgAnalyzer] updateImpl:   %random_number = alloca i32, align 4 in:   call void @par.edt.done(ptr %number, ptr %random_number), !dbg !25
Visit underlying object   %random_number = alloca i32, align 4
[EDTFunctionAnalyzer] All CallSites were successfully Initialized

[EDTCallAnalyzer] updateImpl:   call void @par.edt.done(ptr %number, ptr %random_number), !dbg !25

[EDTArgAnalyzer] updateImpl:   %number = alloca i32, align 4 in:   call void @par.edt.done(ptr %number, ptr %random_number), !dbg !25
Visit underlying object   %number = alloca i32, align 4

[EDTArgAnalyzer] updateImpl:   %random_number = alloca i32, align 4 in:   call void @par.edt.done(ptr %number, ptr %random_number), !dbg !25
Visit underlying object   %random_number = alloca i32, align 4

[EDTArgAnalyzer] updateImpl:   %number = alloca i32, align 4 in:   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
Visit underlying object   %number = alloca i32, align 4

[EDTArgAnalyzer] updateImpl: ptr %number in:   tail call void @task.edt(i32 %0, ptr %shared_number, ptr %number, i32 %1)
Visit underlying object   %number = alloca i32, align 4
[EDTFunctionAnalyzer] updateImpl: par.edt.done
[EDTFunctionAnalyzer] All CallSites were successfully Initialized

[EDTArgAnalyzer] updateImpl:   %shared_number = alloca i32, align 4 in:   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
Visit underlying object   %shared_number = alloca i32, align 4

[EDTArgAnalyzer] updateImpl: ptr %shared_number in:   tail call void @task.edt(i32 %0, ptr %shared_number, ptr %number, i32 %1)
Visit underlying object   %shared_number = alloca i32, align 4
[EDTFunctionAnalyzer] updateImpl: par.edt.done
[EDTFunctionAnalyzer] All CallSites were successfully Initialized

[EDTArgAnalyzer] updateImpl:   %number = alloca i32, align 4 in:   call void @par.edt.done(ptr %number, ptr %random_number), !dbg !25
Visit underlying object   %number = alloca i32, align 4

[EDTArgAnalyzer] updateImpl:   %number = alloca i32, align 4 in:   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
Visit underlying object   %number = alloca i32, align 4

[EDTArgAnalyzer] updateImpl: ptr %number in:   tail call void @task.edt(i32 %0, ptr %shared_number, ptr %number, i32 %1)
Visit underlying object   %number = alloca i32, align 4

[EDTArgAnalyzer] updateImpl:   %random_number = alloca i32, align 4 in:   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
Visit underlying object   %random_number = alloca i32, align 4
[0-4] : 3
     - 9 -   store i32 %add, ptr %random_number, align 4, !dbg !21, !tbaa !13
       - c:   %add = add nsw i32 %rem, 10, !dbg !20
     - 5 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - Same CallBase
     -->RM:                           %0 = load i32, ptr %random_number, align 4, !dbg !9, !tbaa !13
       - c: <unknown>
     - 5 -   call void @par.edt.done(ptr %number, ptr %random_number), !dbg !25
[EDTDepAnalyzer] Analyzing dependencies...
   - This is a ParallelEDT.
    - Succ:   call void @par.edt.done(ptr %number, ptr %random_number), !dbg !25
   - There is a dependency
   - There is a dependency

[EDTArgAnalyzer] updateImpl:   %random_number = alloca i32, align 4 in:   call void @par.edt.done(ptr %number, ptr %random_number), !dbg !25
Visit underlying object   %random_number = alloca i32, align 4
[0-4] : 3
     - 9 -   store i32 %add, ptr %random_number, align 4, !dbg !21, !tbaa !13
       - c:   %add = add nsw i32 %rem, 10, !dbg !20
     - 5 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - There is no dependency
     -->RM:                           %0 = load i32, ptr %random_number, align 4, !dbg !9, !tbaa !13
       - c: <unknown>
     - 5 -   call void @par.edt.done(ptr %number, ptr %random_number), !dbg !25
[EDTDepAnalyzer] Analyzing dependencies...
   - Same CallBase
     -->RM:                           %1 = load i32, ptr %random_number, align 4, !dbg !17, !tbaa !13
       - c: <unknown>

[EDTCallAnalyzer] updateImpl:   call void @par.edt.done(ptr %number, ptr %random_number), !dbg !25

[EDTArgAnalyzer] updateImpl:   %number = alloca i32, align 4 in:   call void @par.edt.done(ptr %number, ptr %random_number), !dbg !25
Visit underlying object   %number = alloca i32, align 4

[EDTArgAnalyzer] updateImpl:   %number = alloca i32, align 4 in:   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
Visit underlying object   %number = alloca i32, align 4

[EDTArgAnalyzer] updateImpl: ptr %number in:   tail call void @task.edt(i32 %0, ptr %shared_number, ptr %number, i32 %1)
Visit underlying object   %number = alloca i32, align 4

[EDTArgAnalyzer] updateImpl:   %number = alloca i32, align 4 in:   call void @par.edt.done(ptr %number, ptr %random_number), !dbg !25
Visit underlying object   %number = alloca i32, align 4
[0-4] : 8
     - 9 -   store i32 1, ptr %number, align 4, !dbg !12, !tbaa !13
       - c: i32 1
     - 9 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - There is no dependency
     -->RM:                           store i32 %inc, ptr %number, align 4, !dbg !20, !tbaa !13
       - c: <unknown>
     - 5 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - There is no dependency
     -->RM:                           %3 = load i32, ptr %number, align 4, !dbg !20, !tbaa !13
       - c: <unknown>
     - 5 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - There is no dependency
     -->RM:                           %2 = load i32, ptr %number, align 4, !dbg !19, !tbaa !13
       - c: <unknown>
     - 9 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - There is no dependency
     -->RM:                           store i32 %inc.i, ptr %2, align 4, !dbg !25, !tbaa !19, !noalias !9
       - c: <unknown>
     - 5 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - There is no dependency
     -->RM:                           %6 = load i32, ptr %2, align 4, !dbg !25, !tbaa !19, !noalias !9
       - c: <unknown>
     - 5 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - There is no dependency
     -->RM:                           %4 = load i32, ptr %2, align 4, !dbg !16, !tbaa !19, !noalias !9
       - c: <unknown>
     - 5 -   call void @par.edt.done(ptr %number, ptr %random_number), !dbg !25
[EDTDepAnalyzer] Analyzing dependencies...
   - Same CallBase
     -->RM:                           %0 = load i32, ptr %number, align 4, !dbg !12, !tbaa !13
       - c: <unknown>

[EDTArgAnalyzer] updateImpl:   %number = alloca i32, align 4 in:   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
Visit underlying object   %number = alloca i32, align 4
[0-4] : 8
     - 9 -   store i32 1, ptr %number, align 4, !dbg !12, !tbaa !13
       - c: i32 1
     - 9 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - Same CallBase
     -->RM:                           store i32 %inc, ptr %number, align 4, !dbg !20, !tbaa !13
       - c: <unknown>
     - 5 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - Same CallBase
     -->RM:                           %3 = load i32, ptr %number, align 4, !dbg !20, !tbaa !13
       - c: <unknown>
     - 5 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - Same CallBase
     -->RM:                           %2 = load i32, ptr %number, align 4, !dbg !19, !tbaa !13
       - c: <unknown>
     - 9 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - Same CallBase
     -->RM:                           store i32 %inc.i, ptr %2, align 4, !dbg !25, !tbaa !19, !noalias !9
       - c: <unknown>
     - 5 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - Same CallBase
     -->RM:                           %6 = load i32, ptr %2, align 4, !dbg !25, !tbaa !19, !noalias !9
       - c: <unknown>
     - 5 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - Same CallBase
     -->RM:                           %4 = load i32, ptr %2, align 4, !dbg !16, !tbaa !19, !noalias !9
       - c: <unknown>
     - 5 -   call void @par.edt.done(ptr %number, ptr %random_number), !dbg !25
[EDTDepAnalyzer] Analyzing dependencies...
   - This is a ParallelEDT.
    - Succ:   call void @par.edt.done(ptr %number, ptr %random_number), !dbg !25
   - There is a dependency
   - There is a dependency

[EDTArgAnalyzer] updateImpl: ptr %number in:   tail call void @task.edt(i32 %0, ptr %shared_number, ptr %number, i32 %1)
Visit underlying object   %number = alloca i32, align 4
[0-4] : 8
     - 9 -   store i32 1, ptr %number, align 4, !dbg !12, !tbaa !13
       - c: i32 1
     - 9 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - There is no dependency
     -->RM:                           store i32 %inc, ptr %number, align 4, !dbg !20, !tbaa !13
       - c: <unknown>
     - 5 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - There is no dependency
     -->RM:                           %3 = load i32, ptr %number, align 4, !dbg !20, !tbaa !13
       - c: <unknown>
     - 5 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - There is no dependency
     -->RM:                           %2 = load i32, ptr %number, align 4, !dbg !19, !tbaa !13
       - c: <unknown>
     - 9 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - There is no dependency
     -->RM:                           store i32 %inc.i, ptr %2, align 4, !dbg !25, !tbaa !19, !noalias !9
       - c: <unknown>
     - 5 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - There is no dependency
     -->RM:                           %6 = load i32, ptr %2, align 4, !dbg !25, !tbaa !19, !noalias !9
       - c: <unknown>
     - 5 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - There is no dependency
     -->RM:                           %4 = load i32, ptr %2, align 4, !dbg !16, !tbaa !19, !noalias !9
       - c: <unknown>
     - 5 -   call void @par.edt.done(ptr %number, ptr %random_number), !dbg !25
[EDTDepAnalyzer] Analyzing dependencies...
   - There is no dependency
     -->RM:                           %0 = load i32, ptr %number, align 4, !dbg !12, !tbaa !13
       - c: <unknown>

[EDTCallAnalyzer] updateImpl:   call void @par.edt.done(ptr %number, ptr %random_number), !dbg !25

[EDTCallAnalyzer] updateImpl:   tail call void @task.edt(i32 %0, ptr %shared_number, ptr %number, i32 %1)

[EDTArgAnalyzer] updateImpl:   %shared_number = alloca i32, align 4 in:   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
Visit underlying object   %shared_number = alloca i32, align 4
[0-4] : 4
     - 9 -   store i32 10000, ptr %shared_number, align 4, !dbg !17, !tbaa !13
       - c: i32 10000
     - 9 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - Same CallBase
     -->RM:                           store i32 %dec.i, ptr %1, align 4, !dbg !26, !tbaa !19, !noalias !9
       - c: <unknown>
     - 5 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - Same CallBase
     -->RM:                           %7 = load i32, ptr %1, align 4, !dbg !26, !tbaa !19, !noalias !9
       - c: <unknown>
     - 5 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - Same CallBase
     -->RM:                           %5 = load i32, ptr %1, align 4, !dbg !23, !tbaa !19, !noalias !9
       - c: <unknown>

[EDTArgAnalyzer] updateImpl: ptr %shared_number in:   tail call void @task.edt(i32 %0, ptr %shared_number, ptr %number, i32 %1)
Visit underlying object   %shared_number = alloca i32, align 4
[0-4] : 4
     - 9 -   store i32 10000, ptr %shared_number, align 4, !dbg !17, !tbaa !13
       - c: i32 10000
     - 9 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - There is no dependency
     -->RM:                           store i32 %dec.i, ptr %1, align 4, !dbg !26, !tbaa !19, !noalias !9
       - c: <unknown>
     - 5 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - There is no dependency
     -->RM:                           %7 = load i32, ptr %1, align 4, !dbg !26, !tbaa !19, !noalias !9
       - c: <unknown>
     - 5 -   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)
[EDTDepAnalyzer] Analyzing dependencies...
   - There is no dependency
     -->RM:                           %5 = load i32, ptr %1, align 4, !dbg !23, !tbaa !19, !noalias !9
       - c: <unknown>

[EDTCallAnalyzer] updateImpl:   call void @parallel.edt(ptr nonnull %random_number, ptr nonnull %NewRandom, ptr nonnull %number, ptr nonnull %shared_number)

[EDTCallAnalyzer] updateImpl:   tail call void @task.edt(i32 %0, ptr %shared_number, ptr %number, i32 %1)
[EDTFunctionAnalyzer] Manifest
[EDTCallAnalyzer] Manifest
[EDTArgAnalyzer] Manifest
[EDTArgAnalyzer] Manifest
[EDTFunctionAnalyzer] Manifest
[EDTFunctionAnalyzer] Manifest
[EDTCallAnalyzer] Manifest
[EDTFunctionAnalyzer] Manifest
[EDTFunctionAnalyzer] Manifest
[EDTCallAnalyzer] Manifest
[EDTCallAnalyzer] Manifest
[arts-transform] [Attributor] Done, result: changed.

------------------------
EDTs INFORMATION
[arts-transform] Dumping Edts
Number of Edts: 5

----- Edt -----
ID: 2
Function: parallel.edt
Type: PARALLEL
Data environment: 
Firstprivate: 0
Private: 0
Shared: 4
  - ptr %random_number
  - ptr %NewRandom
  - ptr %number
  - ptr %shared_number
Lastprivate: 0
Predecessors: 0
Successors: 1
  - 1
    Values: 2
      -   %random_number = alloca i32, align 4
      -   %number = alloca i32, align 4

----- Edt -----
ID: 0
Function: main
Type: MAIN
Data environment: 
Firstprivate: 0
Private: 0
Shared: 0
Lastprivate: 0
Predecessors: 0
Successors: 0

----- Edt -----
ID: 4
Function: task.edt.8
Type: TASK
Data environment: 
Firstprivate: 0
Private: 1
  - i32 %0
Shared: 0
Lastprivate: 0
Predecessors: 0
Successors: 0

----- Edt -----
ID: 1
Function: par.edt.done
Type: OTHER
Data environment: 
Firstprivate: 0
Private: 0
Shared: 2
  - ptr %number
  - ptr %random_number
Lastprivate: 0
Predecessors: 0
Successors: 0

----- Edt -----
ID: 3
Function: task.edt
Type: TASK
Data environment: 
Firstprivate: 0
Private: 2
  - i32 %0
  - i32 %3
Shared: 2
  - ptr %1
  - ptr %2
Lastprivate: 0
Predecessors: 0
Successors: 0

[arts-transform] Module after ARTSTransformer Module Pass:
; ModuleID = 'example.cpp'
source_filename = "example.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%"class.std::ios_base::Init" = type { i8 }
%struct.ident_t = type { i32, i32, i32, i32, ptr }

@_ZStL8__ioinit = internal global %"class.std::ios_base::Init" zeroinitializer, align 1
@__dso_handle = external hidden global i8
@.str = private unnamed_addr constant [44 x i8] c"I think the number is %d/%d. with %d -- %d\0A\00", align 1
@0 = private unnamed_addr constant [25 x i8] c";example.cpp;main;41;7;;\00", align 1
@.str.2 = private unnamed_addr constant [27 x i8] c"I think the number is %d.\0A\00", align 1
@1 = private unnamed_addr constant [25 x i8] c";example.cpp;main;47;7;;\00", align 1
@2 = private unnamed_addr constant [25 x i8] c";example.cpp;main;34;4;;\00", align 1
@3 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 24, ptr @2 }, align 8
@.str.5 = private unnamed_addr constant [31 x i8] c"The final number is %d - % d.\0A\00", align 1
@llvm.global_ctors = appending global [1 x { i32, ptr, ptr }] [{ i32, ptr, ptr } { i32 65535, ptr @_GLOBAL__sub_I_example.cpp, ptr null }]

declare void @_ZNSt8ios_base4InitC1Ev(ptr noundef nonnull align 1 dereferenceable(1)) unnamed_addr #0

; Function Attrs: nounwind
declare void @_ZNSt8ios_base4InitD1Ev(ptr noundef nonnull align 1 dereferenceable(1)) unnamed_addr #1

; Function Attrs: nofree nounwind
declare i32 @__cxa_atexit(ptr, ptr, ptr) local_unnamed_addr #2

; Function Attrs: mustprogress norecurse nounwind uwtable
define dso_local noundef i32 @main() local_unnamed_addr #3 !dbg !9 {
entry:
  %number = alloca i32, align 4
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %NewRandom = alloca i32, align 4
  store i32 1, ptr %number, align 4, !dbg !12, !tbaa !13
  store i32 10000, ptr %shared_number, align 4, !dbg !17, !tbaa !13
  %call = tail call i32 @rand() #8, !dbg !18
  %rem = srem i32 %call, 10, !dbg !19
  %add = add nsw i32 %rem, 10, !dbg !20
  store i32 %add, ptr %random_number, align 4, !dbg !21, !tbaa !13
  %call1 = tail call i32 @rand() #8, !dbg !22
  store i32 %call1, ptr %NewRandom, align 4, !dbg !23, !tbaa !13
  br label %par.region.0, !dbg !24

par.region.0:                                     ; preds = %entry
  call void @parallel.edt(ptr noalias nocapture nonnull readonly align 4 dereferenceable(4) %random_number, ptr noalias nocapture nonnull readonly align 4 dereferenceable(4) %NewRandom, ptr noalias nocapture noundef nonnull align 4 dereferenceable(4) %number, ptr noalias nocapture noundef nonnull align 4 dereferenceable(4) %shared_number) #12
  br label %par.done.0, !dbg !25

par.done.0:                                       ; preds = %par.region.0
  call void @par.edt.done(ptr noalias nocapture noundef nonnull readonly align 4 dereferenceable(4) %number, ptr noalias nocapture noundef nonnull readonly align 4 dereferenceable(4) %random_number), !dbg !25
  br label %par.region.0.split.ret

par.region.0.split.ret:                           ; preds = %par.done.0
  ret i32 0, !dbg !26
}

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #4

; Function Attrs: nounwind
declare i32 @rand() local_unnamed_addr #1

; Function Attrs: mustprogress nofree norecurse nounwind
define internal void @parallel.edt(ptr noalias nocapture nofree noundef nonnull readonly align 4 dereferenceable(4) %random_number, ptr noalias nocapture nofree noundef nonnull readonly align 4 dereferenceable(4) %NewRandom, ptr noalias nocapture nofree noundef nonnull align 4 dereferenceable(4) %number, ptr noalias nocapture nofree noundef nonnull align 4 dereferenceable(4) %shared_number) #5 {
task.region.0:
  %0 = load i32, ptr %random_number, align 4, !dbg !27, !tbaa !13
  %1 = load i32, ptr %NewRandom, align 4, !dbg !29, !tbaa !13
  tail call void @task.edt(i32 noundef %0, ptr nocapture nofree noundef nonnull align 4 dereferenceable(4) %shared_number, ptr nocapture nofree noundef nonnull align 4 dereferenceable(4) %number, i32 noundef %1) #7
  br label %task.region.1, !dbg !30

task.region.1:                                    ; preds = %task.region.0
  %2 = load i32, ptr %number, align 4, !dbg !31, !tbaa !13
  tail call void @task.edt.8(i32 noundef %2) #7
  br label %task.region.1.split, !dbg !32

task.region.1.split:                              ; preds = %task.region.1
  %3 = load i32, ptr %number, align 4, !dbg !32, !tbaa !13
  %inc = add nsw i32 %3, 1, !dbg !32
  store i32 %inc, ptr %number, align 4, !dbg !32, !tbaa !13
  ret void, !dbg !33

synchronization:
  -----------
}

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #6

declare i32 @__gxx_personality_v0(...)

; Function Attrs: mustprogress nofree nounwind
define internal void @task.edt(i32 noundef %0, ptr nocapture nofree noundef nonnull align 4 dereferenceable(4) %1, ptr nocapture nofree noundef nonnull align 4 dereferenceable(4) %2, i32 noundef %3) #7 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !34) #13, !dbg !37
  %4 = load i32, ptr %2, align 4, !dbg !39, !tbaa !13, !noalias !34
  %5 = load i32, ptr %1, align 4, !dbg !42, !tbaa !13, !noalias !34
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4, i32 noundef %5, i32 noundef %0, i32 noundef %3) #2, !dbg !43, !noalias !34
  %6 = load i32, ptr %2, align 4, !dbg !44, !tbaa !13, !noalias !34
  %inc.i = add nsw i32 %6, 1, !dbg !44
  store i32 %inc.i, ptr %2, align 4, !dbg !44, !tbaa !13, !noalias !34
  %7 = load i32, ptr %1, align 4, !dbg !45, !tbaa !13, !noalias !34
  %dec.i = add nsw i32 %7, -1, !dbg !45
  store i32 %dec.i, ptr %1, align 4, !dbg !45, !tbaa !13, !noalias !34
  ret void, !dbg !37
}

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #8

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(ptr, i32, ptr) local_unnamed_addr #8

; Function Attrs: mustprogress nofree nounwind
define internal void @task.edt.8(i32 noundef %0) #7 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !46) #13, !dbg !49
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %0) #2, !dbg !51, !noalias !46
  ret void, !dbg !49
}

; Function Attrs: nounwind
declare !callback !54 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #8

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #4

; Function Attrs: uwtable
define internal void @_GLOBAL__sub_I_example.cpp() #9 section ".text.startup" !dbg !56 {
entry:
  tail call void @_ZNSt8ios_base4InitC1Ev(ptr noundef nonnull align 1 dereferenceable(1) @_ZStL8__ioinit), !dbg !57
  %0 = tail call i32 @__cxa_atexit(ptr nonnull @_ZNSt8ios_base4InitD1Ev, ptr nonnull @_ZStL8__ioinit, ptr nonnull @__dso_handle) #8, !dbg !62
  ret void
}

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #10

; Function Attrs: mustprogress nofree norecurse nounwind uwtable
define internal void @par.edt.done(ptr noalias nocapture nofree noundef nonnull readonly align 4 dereferenceable(4) %number, ptr noalias nocapture nofree noundef nonnull readonly align 4 dereferenceable(4) %random_number) #11 !dbg !63 {
newFuncRoot:
  br label %par.region.0.split, !dbg !64

par.region.0.split:                               ; preds = %newFuncRoot
  %0 = load i32, ptr %number, align 4, !dbg !64, !tbaa !13
  %1 = load i32, ptr %random_number, align 4, !dbg !65, !tbaa !13
  %call2 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.5, i32 noundef %0, i32 noundef %1) #2, !dbg !66
  br label %par.region.0.split.ret.exitStub, !dbg !67

par.region.0.split.ret.exitStub:                  ; preds = %par.region.0.split
  ret void
}

attributes #0 = { "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #2 = { nofree nounwind }
attributes #3 = { mustprogress norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #5 = { mustprogress nofree norecurse nounwind }
attributes #6 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #7 = { mustprogress nofree nounwind }
attributes #8 = { nounwind }
attributes #9 = { uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #10 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }
attributes #11 = { mustprogress nofree norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #12 = { mustprogress }
attributes #13 = { nofree willreturn }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3, !4, !5, !6, !7}
!llvm.ident = !{!8}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang version 18.0.0", isOptimized: true, runtimeVersion: 0, emissionKind: NoDebug, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "example.cpp", directory: "/home/rherreraguaitero/ME/ARTS-env/test/example")
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = !{i32 1, !"wchar_size", i32 4}
!4 = !{i32 7, !"openmp", i32 51}
!5 = !{i32 8, !"PIC Level", i32 2}
!6 = !{i32 7, !"PIE Level", i32 2}
!7 = !{i32 7, !"uwtable", i32 2}
!8 = !{!"clang version 18.0.0"}
!9 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 19, type: !10, scopeLine: 19, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!10 = !DISubroutineType(types: !11)
!11 = !{}
!12 = !DILocation(line: 21, column: 7, scope: !9)
!13 = !{!14, !14, i64 0}
!14 = !{!"int", !15, i64 0}
!15 = !{!"omnipotent char", !16, i64 0}
!16 = !{!"Simple C++ TBAA"}
!17 = !DILocation(line: 22, column: 7, scope: !9)
!18 = !DILocation(line: 23, column: 23, scope: !9)
!19 = !DILocation(line: 23, column: 30, scope: !9)
!20 = !DILocation(line: 23, column: 35, scope: !9)
!21 = !DILocation(line: 23, column: 7, scope: !9)
!22 = !DILocation(line: 24, column: 19, scope: !9)
!23 = !DILocation(line: 24, column: 7, scope: !9)
!24 = !DILocation(line: 34, column: 4, scope: !9)
!25 = !DILocation(line: 67, column: 45, scope: !9)
!26 = !DILocation(line: 68, column: 3, scope: !9)
!27 = !DILocation(line: 41, column: 37, scope: !28)
!28 = distinct !DISubprogram(name: "main.omp_outlined", scope: !1, file: !1, line: 34, type: !10, scopeLine: 34, flags: DIFlagArtificial | DIFlagPrototyped, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!29 = !DILocation(line: 41, column: 52, scope: !28)
!30 = !DILocation(line: 47, column: 7, scope: !28)
!31 = !DILocation(line: 47, column: 37, scope: !28)
!32 = !DILocation(line: 53, column: 13, scope: !28)
!33 = !DILocation(line: 55, column: 5, scope: !28)
!34 = !{!35}
!35 = distinct !{!35, !36, !".omp_outlined.: %.privates."}
!36 = distinct !{!36, !".omp_outlined."}
!37 = !DILocation(line: 41, column: 7, scope: !38)
!38 = distinct !DISubprogram(linkageName: ".omp_task_entry.", scope: !1, file: !1, line: 41, type: !10, scopeLine: 41, flags: DIFlagArtificial, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!39 = !DILocation(line: 43, column: 64, scope: !40, inlinedAt: !41)
!40 = distinct !DISubprogram(name: ".omp_outlined.", scope: !1, file: !1, type: !10, scopeLine: 42, flags: DIFlagArtificial | DIFlagPrototyped, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!41 = distinct !DILocation(line: 41, column: 7, scope: !38)
!42 = !DILocation(line: 43, column: 72, scope: !40, inlinedAt: !41)
!43 = !DILocation(line: 43, column: 9, scope: !40, inlinedAt: !41)
!44 = !DILocation(line: 44, column: 15, scope: !40, inlinedAt: !41)
!45 = !DILocation(line: 45, column: 22, scope: !40, inlinedAt: !41)
!46 = !{!47}
!47 = distinct !{!47, !48, !".omp_outlined..1: %.privates."}
!48 = distinct !{!48, !".omp_outlined..1"}
!49 = !DILocation(line: 47, column: 7, scope: !50)
!50 = distinct !DISubprogram(linkageName: ".omp_task_entry..4", scope: !1, file: !1, line: 47, type: !10, scopeLine: 47, flags: DIFlagArtificial, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!51 = !DILocation(line: 49, column: 9, scope: !52, inlinedAt: !53)
!52 = distinct !DISubprogram(name: ".omp_outlined..1", scope: !1, file: !1, type: !10, scopeLine: 48, flags: DIFlagArtificial | DIFlagPrototyped, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!53 = distinct !DILocation(line: 47, column: 7, scope: !50)
!54 = !{!55}
!55 = !{i64 2, i64 -1, i64 -1, i1 true}
!56 = distinct !DISubprogram(linkageName: "_GLOBAL__sub_I_example.cpp", scope: !1, file: !1, type: !10, flags: DIFlagArtificial, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!57 = !DILocation(line: 74, column: 25, scope: !58, inlinedAt: !61)
!58 = !DILexicalBlockFile(scope: !60, file: !59, discriminator: 0)
!59 = !DIFile(filename: "/usr/lib64/gcc/x86_64-suse-linux/7/../../../../include/c++/7/iostream", directory: "")
!60 = distinct !DISubprogram(name: "__cxx_global_var_init", scope: !1, file: !1, type: !10, flags: DIFlagArtificial, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!61 = distinct !DILocation(line: 0, scope: !56)
!62 = !DILocation(line: 0, scope: !60, inlinedAt: !61)
!63 = distinct !DISubprogram(name: "main.par.region.0.split", linkageName: "main.par.region.0.split", scope: null, file: !1, type: !10, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!64 = !DILocation(line: 67, column: 45, scope: !63)
!65 = !DILocation(line: 67, column: 53, scope: !63)
!66 = !DILocation(line: 67, column: 3, scope: !63)
!67 = !DILocation(line: 68, column: 3, scope: !63)
