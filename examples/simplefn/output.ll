clang++ -fopenmp -O3 -g0 -emit-llvm -c simplefn.cpp -o simplefn.bc -lstdc++
clang++: warning: -Z-reserved-lib-stdc++: 'linker' input unused [-Wunused-command-line-argument]
llvm-dis simplefn.bc
opt -load-pass-plugin=/home/randres/projects/carts/.install/carts/lib/OMPTransform.so \
	-debug-only=omp-transform,arts,carts,arts-ir-builder,arts-utils\
	-passes="omp-transform" simplefn.bc -o simplefn_arts_ir.bc

-------------------------------------------------
[omp-transform] Running OmpTransformPass on Module: 
simplefn.bc

-------------------------------------------------

-------------------------------------------------
[omp-transform] Running OmpTransform on Module: 
[omp-transform] Processing main function
[omp-transform] Main EDT Function: define internal void @carts.edt.main() !carts !27 {
entry:
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %call = tail call i64 @time(ptr noundef null) #3
  %conv = trunc i64 %call to i32
  tail call void @srand(i32 noundef %conv) #3
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %shared_number) #3
  %call1 = tail call i32 @rand() #3
  %rem = srem i32 %call1, 100
  %add = add nsw i32 %rem, 1
  store i32 %add, ptr %shared_number, align 4, !tbaa !16
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %random_number) #3
  %call2 = tail call i32 @rand() #3
  %rem3 = srem i32 %call2, 10
  %add4 = add nsw i32 %rem3, 1
  store i32 %add4, ptr %random_number, align 4, !tbaa !16
  %call5 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %add, i32 noundef %add4)
  call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr nonnull @1, i32 2, ptr nonnull @main.omp_outlined, ptr nonnull %shared_number, ptr nonnull %random_number)
  %0 = load i32, ptr %shared_number, align 4, !tbaa !16
  %inc = add nsw i32 %0, 1
  store i32 %inc, ptr %shared_number, align 4, !tbaa !16
  %1 = load i32, ptr %random_number, align 4, !tbaa !16
  %call6 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.3, i32 noundef %inc, i32 noundef %1)
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %random_number) #3
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %shared_number) #3
  ret void
}


- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt.main
[omp-transform] Other Function Found:
  %call = tail call i64 @time(ptr noundef null) #3
[omp-transform] Other Function Found:
  tail call void @srand(i32 noundef %conv) #3
[omp-transform] Other Function Found:
  %call1 = tail call i32 @rand() #3
[omp-transform] Other Function Found:
  %call2 = tail call i32 @rand() #3
[omp-transform] Other Function Found:
  %call5 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %add, i32 noundef %add4)
- - - - - - - - - - - - - - - -
[omp-transform] Parallel Region Found:
  call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr nonnull @1, i32 2, ptr nonnull @main.omp_outlined, ptr nonnull %shared_number, ptr nonnull %random_number)
[arts-ir-builder] Building EDT:
Created new function: declare internal void @carts.edt.parallel(ptr, ptr)
Rewiring new function arguments:
  - Rewiring: ptr %random_number -> ptr %1
  - Rewiring: ptr %shared_number -> ptr %0
[arts-ir-builder] New callsite:   call void @carts.edt.parallel(ptr %shared_number, ptr %random_number)
[arts-utils]   - Replacing uses of:   call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr nonnull @1, i32 2, ptr nonnull @main.omp_outlined, ptr nonnull %shared_number, ptr nonnull %random_number)
[arts-utils]   - Worklist size: 0
[arts-utils]    - Removing instruction:   call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr nonnull @1, i32 2, ptr nonnull @main.omp_outlined, ptr nonnull %shared_number, ptr nonnull %random_number)
[arts-utils]   - Replacing uses of: ptr %.global_tid.
[arts-utils]   - Worklist size: 1
[arts-utils]    - Replacing:   %2 = load i32, ptr undef, align 4, !tbaa !6
[arts-utils]   - Replacing uses of: ptr %.bound_tid.
[arts-utils]   - Worklist size: 0
[arts-utils]   - Replacing uses of: ptr %shared_number
[arts-utils]   - Worklist size: 0
[arts-utils]   - Replacing uses of: ptr %random_number
[arts-utils]   - Worklist size: 0
[arts-utils]   - Replacing uses of: ; Function Attrs: alwaysinline norecurse nounwind uwtable
declare dso_local void @main.omp_outlined(ptr noalias nocapture noundef readonly, ptr noalias nocapture readnone, ptr noundef nonnull align 4 dereferenceable(4), ptr nocapture noundef nonnull readonly align 4 dereferenceable(4)) #7

[arts-utils]   - Worklist size: 0
[arts-utils]    - Removing function: ; Function Attrs: alwaysinline norecurse nounwind uwtable
declare dso_local void @main.omp_outlined(ptr noalias nocapture noundef readonly, ptr noalias nocapture readnone, ptr noundef nonnull align 4 dereferenceable(4), ptr nocapture noundef nonnull readonly align 4 dereferenceable(4)) #7

[arts-ir-builder] New function:
; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.parallel(ptr %0, ptr %1) #6 {
entry:
  %2 = load i32, ptr undef, align 4, !tbaa !16
  %3 = tail call i32 @__kmpc_single(ptr nonnull @1, i32 undef)
  %.not = icmp eq i32 %3, 0
  br i1 %.not, label %omp_if.end, label %omp_if.then

omp_if.then:                                      ; preds = %entry
  %4 = load i32, ptr %0, align 4, !tbaa !16
  %5 = load i32, ptr %1, align 4, !tbaa !16
  %call = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %4, i32 noundef %5)
  tail call void @_Z12taskFunctionRiS_(ptr noundef nonnull align 4 dereferenceable(4) %0, ptr noundef nonnull align 4 dereferenceable(4) %1)
  tail call void @__kmpc_end_single(ptr nonnull @1, i32 undef)
  br label %omp_if.end

omp_if.end:                                       ; preds = %omp_if.then, %entry
  tail call void @__kmpc_barrier(ptr nonnull @2, i32 undef)
  ret void
}


- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt.parallel
- - - - - - - - - - - - - - - -
[omp-transform] Single Region Found:
  %3 = tail call i32 @__kmpc_single(ptr nonnull @1, i32 undef)
[arts-utils]   - Replacing uses of:   tail call void @__kmpc_barrier(ptr nonnull @2, i32 undef)
[arts-utils]   - Worklist size: 0
[arts-utils]    - Removing instruction:   tail call void @__kmpc_barrier(ptr nonnull @2, i32 undef)
[omp-transform] Other Function Found:
  %call = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %2, i32 noundef %3)
[omp-transform] Other Function Found:
  tail call void @_Z12taskFunctionRiS_(ptr noundef nonnull align 4 dereferenceable(4) %0, ptr noundef nonnull align 4 dereferenceable(4) %1)

- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: _Z12taskFunctionRiS_
[omp-transform] Call to Global Thread Num Found:
  %0 = tail call i32 @__kmpc_global_thread_num(ptr nonnull @1)
- - - - - - - - - - - - - - - -
[omp-transform] Task Region Found:
  %1 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 %0, i32 1, i64 48, i64 8, ptr nonnull @.omp_task_entry.)
[arts-ir-builder] Building EDT:
Created new function: declare internal void @carts.edt.task(ptr, i32)
Rewiring new function arguments:
  - Rewiring:   %6 = load i32, ptr %3, align 4, !tbaa !21, !noalias !14 -> i32 %1
  - Rewiring:   %4 = load ptr, ptr %2, align 8, !tbaa !17, !alias.scope !14, !noalias !19 -> ptr %0
[arts-ir-builder] New callsite:   call void @carts.edt.task(ptr %shared_number, i32 %4)
[arts-utils]   - Replacing uses of:   %1 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 %0, i32 1, i64 48, i64 8, ptr nonnull @.omp_task_entry.)
[arts-utils]   - Worklist size: 3
[arts-utils]    - Replacing:   %3 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr undef, i64 0, i32 1
[arts-utils]    - Removing instruction:   %3 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr undef, i64 0, i32 1
[arts-utils]    - Replacing:   store i32 %3, ptr undef, align 8, !tbaa !17
[arts-utils]    - Removing instruction:   store i32 %3, ptr undef, align 8, !tbaa !17
[arts-utils]    - Replacing:   %4 = tail call i32 @__kmpc_omp_task(ptr nonnull @1, i32 %0, ptr nonnull undef)
[arts-utils]    - Removing instruction:   %4 = tail call i32 @__kmpc_omp_task(ptr nonnull @1, i32 %0, ptr nonnull undef)
[arts-utils]    - Replacing:   %2 = load ptr, ptr undef, align 8, !tbaa !6
[arts-utils]    - Removing instruction:   %2 = load ptr, ptr undef, align 8, !tbaa !6
[arts-utils]    - Replacing:   store ptr %shared_number, ptr undef, align 8, !tbaa.struct !6
[arts-utils]    - Removing instruction:   store ptr %shared_number, ptr undef, align 8, !tbaa.struct !6
[arts-utils]    - Removing instruction:   %1 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 %0, i32 1, i64 48, i64 8, ptr nonnull @.omp_task_entry.)
[arts-utils]   - Replacing uses of: i32 %0
[arts-utils]   - Worklist size: 0
[arts-utils]   - Replacing uses of: ptr %1
[arts-utils]   - Worklist size: 2
[arts-utils]    - Replacing:   %3 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr undef, i64 0, i32 1
[arts-utils]    - Replacing:   %2 = load ptr, ptr undef, align 8, !tbaa !6
[arts-utils]   - Replacing uses of: ; Function Attrs: nofree norecurse nounwind uwtable
declare dso_local noundef i32 @.omp_task_entry.(i32, ptr noalias nocapture noundef) #3

[arts-utils]   - Worklist size: 0
[arts-utils]    - Removing function: ; Function Attrs: nofree norecurse nounwind uwtable
declare dso_local noundef i32 @.omp_task_entry.(i32, ptr noalias nocapture noundef) #3

[arts-ir-builder] New function:
; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.task(ptr %0, i32 %1) #2 {
entry:
  %2 = load ptr, ptr undef, align 8, !tbaa !10
  %3 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr undef, i64 0, i32 1
  tail call void @llvm.experimental.noalias.scope.decl(metadata !15)
  %4 = load ptr, ptr undef, align 8, !tbaa !18, !alias.scope !15, !noalias !20
  %5 = load i32, ptr %0, align 8, !tbaa !6, !noalias !15
  %inc.i = add nsw i32 %5, 1
  store i32 %inc.i, ptr %0, align 8, !tbaa !6, !noalias !15
  %6 = load i32, ptr undef, align 4, !tbaa !6, !noalias !15
  %inc1.i = add nsw i32 %1, 1
  store i32 %inc1.i, ptr undef, align 4, !tbaa !6, !noalias !15
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %inc.i, i32 noundef %inc1.i), !noalias !15
  ret void
}


- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt.task
[omp-transform] Other Function Found:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !17)
[omp-transform] Other Function Found:
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %inc.i, i32 noundef %inc1.i), !noalias !16
[arts-utils]    - Removing:   %0 = tail call i32 @__kmpc_global_thread_num(ptr nonnull @1)
[arts-utils]   - Replacing uses of:   %0 = tail call i32 @__kmpc_global_thread_num(ptr nonnull @1)
[arts-utils]   - Worklist size: 0
[arts-utils]    - Removing instruction:   %0 = tail call i32 @__kmpc_global_thread_num(ptr nonnull @1)
[arts-utils] Removing dead instructions from: carts.edt.task
[arts-utils] - Removing:   %2 = load ptr, ptr undef, align 8, !tbaa !8
[arts-utils] - Removing:   %3 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr undef, i64 0, i32 1
[arts-utils] - Removing:   %4 = load ptr, ptr undef, align 8, !tbaa !19, !alias.scope !16, !noalias !21
[arts-utils] - Removing:   %6 = load i32, ptr undef, align 4, !tbaa !23, !noalias !16
[arts-utils] - Removing:   store i32 %inc1.i, ptr undef, align 4, !tbaa !23, !noalias !16
[arts-utils] Removing 5 dead instructions
[arts-utils]    - Removing:   %2 = load ptr, ptr undef, align 8, !tbaa !8
[arts-utils]   - Replacing uses of:   %2 = load ptr, ptr undef, align 8, !tbaa !8
[arts-utils]   - Worklist size: 0
[arts-utils]    - Removing instruction:   %2 = load ptr, ptr undef, align 8, !tbaa !8
[arts-utils]    - Removing:   %2 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr undef, i64 0, i32 1
[arts-utils]   - Replacing uses of:   %2 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr undef, i64 0, i32 1
[arts-utils]   - Worklist size: 0
[arts-utils]    - Removing instruction:   %2 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr undef, i64 0, i32 1
[arts-utils]    - Removing:   %2 = load ptr, ptr undef, align 8, !tbaa !11, !alias.scope !8, !noalias !16
[arts-utils]   - Replacing uses of:   %2 = load ptr, ptr undef, align 8, !tbaa !11, !alias.scope !8, !noalias !16
[arts-utils]   - Worklist size: 0
[arts-utils]    - Removing instruction:   %2 = load ptr, ptr undef, align 8, !tbaa !11, !alias.scope !8, !noalias !16
[arts-utils]    - Removing:   %3 = load i32, ptr undef, align 4, !tbaa !11, !noalias !8
[arts-utils]   - Replacing uses of:   %3 = load i32, ptr undef, align 4, !tbaa !11, !noalias !8
[arts-utils]   - Worklist size: 0
[arts-utils]    - Removing instruction:   %3 = load i32, ptr undef, align 4, !tbaa !11, !noalias !8
[arts-utils]    - Removing:   store i32 %inc1.i, ptr undef, align 4, !tbaa !11, !noalias !8
[arts-utils]   - Replacing uses of:   store i32 %inc1.i, ptr undef, align 4, !tbaa !11, !noalias !8
[arts-utils]   - Worklist size: 0
[arts-utils]    - Removing instruction:   store i32 %inc1.i, ptr undef, align 4, !tbaa !11, !noalias !8

[omp-transform] Processing function: carts.edt.task - Finished
- - - - - - - - - - - - - - - - - - - - - - - -
[arts-utils] Removing dead instructions from: _Z12taskFunctionRiS_
[arts-utils] Removing 0 dead instructions

[omp-transform] Processing function: _Z12taskFunctionRiS_ - Finished
- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Single End Region Found:
  tail call void @__kmpc_end_single(ptr nonnull @1, i32 undef)
[omp-transform] Other Function Found:
  %call = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %2, i32 noundef %3)
[omp-transform] Other Function Found:
  tail call void @carts.edt(ptr noundef nonnull align 4 dereferenceable(4) %0, ptr noundef nonnull align 4 dereferenceable(4) %1)
[omp-transform] Single End Region Found:
  tail call void @__kmpc_end_single(ptr nonnull @1, i32 undef)
[arts-utils]    - Removing:   tail call void @__kmpc_end_single(ptr nonnull @1, i32 undef)
[arts-utils]   - Replacing uses of:   tail call void @__kmpc_end_single(ptr nonnull @1, i32 undef)
[arts-utils]   - Worklist size: 0
[arts-utils]    - Removing instruction:   tail call void @__kmpc_end_single(ptr nonnull @1, i32 undef)
[arts-utils] Removing dead instructions from: carts.edt.parallel
[arts-utils] Removing 0 dead instructions

[omp-transform] Processing function: carts.edt.parallel - Finished
- - - - - - - - - - - - - - - - - - - - - - - -

- - - - - - - - - - - - - - - - - - - - - - - -
[omp-transform] Processing function: carts.edt.sync.done
[omp-transform] Other Function Found:
  %call6 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.3, i32 noundef %inc, i32 noundef %1)
[arts-utils] Removing dead instructions from: carts.edt.sync.done
[arts-utils] Removing 0 dead instructions

[omp-transform] Processing function: carts.edt.sync.done - Finished
- - - - - - - - - - - - - - - - - - - - - - - -
[arts-utils] Removing dead instructions from: carts.edt.main
[arts-utils] Removing 0 dead instructions

[omp-transform] Processing function: carts.edt.main - Finished
- - - - - - - - - - - - - - - - - - - - - - - -

-------------------------------------------------
[omp-transform] Module after Identifying EDTs
; ModuleID = 'simplefn.bc'
source_filename = "simplefn.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }

@.str = private unnamed_addr constant [28 x i8] c"EDT 1: The number is %d/%d\0A\00", align 1
@0 = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, ptr @0 }, align 8
@.str.1 = private unnamed_addr constant [36 x i8] c"EDT 1: The initial number is %d/%d\0A\00", align 1
@.str.2 = private unnamed_addr constant [28 x i8] c"EDT 0: The number is %d/%d\0A\00", align 1
@2 = private unnamed_addr constant %struct.ident_t { i32 0, i32 322, i32 0, i32 22, ptr @0 }, align 8
@.str.3 = private unnamed_addr constant [37 x i8] c"EDT 2: The final number is %d - %d.\0A\00", align 1

; Function Attrs: mustprogress noinline nounwind uwtable
define dso_local void @carts.edt(ptr noundef nonnull align 4 dereferenceable(4) %shared_number, ptr nocapture noundef nonnull readonly align 4 dereferenceable(4) %random_number) local_unnamed_addr #0 !carts !6 {
entry:
  %0 = load i32, ptr %random_number, align 4, !tbaa !8
  call void @carts.edt.task(ptr %shared_number, i32 %0) #8
  ret void
}

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #1

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.task(ptr %0, i32 %1) #2 !carts !12 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !14)
  %2 = load i32, ptr %0, align 8, !tbaa !8, !noalias !14
  %inc.i = add nsw i32 %2, 1
  store i32 %inc.i, ptr %0, align 8, !tbaa !8, !noalias !14
  %inc1.i = add nsw i32 %1, 1
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %inc.i, i32 noundef %inc1.i), !noalias !14
  ret void
}

; Function Attrs: nounwind
declare i32 @__kmpc_global_thread_num(ptr) local_unnamed_addr #3

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #3

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(ptr, i32, ptr) local_unnamed_addr #3

; Function Attrs: mustprogress norecurse nounwind uwtable
define dso_local noundef i32 @main() local_unnamed_addr #4 {
entry:
  call void @carts.edt.main()
  ret i32 0
}

; Function Attrs: nounwind
declare void @srand(i32 noundef) local_unnamed_addr #5

; Function Attrs: nounwind
declare i64 @time(ptr noundef) local_unnamed_addr #5

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #2

; Function Attrs: nounwind
declare i32 @rand() local_unnamed_addr #5

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
define internal void @carts.edt.parallel(ptr %0, ptr %1) #2 !carts !17 {
entry:
  %2 = load i32, ptr %0, align 4, !tbaa !8
  %3 = load i32, ptr %1, align 4, !tbaa !8
  %call = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %2, i32 noundef %3)
  tail call void @carts.edt(ptr noundef nonnull align 4 dereferenceable(4) %0, ptr noundef nonnull align 4 dereferenceable(4) %1)
  br label %exit

exit:                                             ; preds = %entry
  ret void
}

; Function Attrs: convergent nounwind
declare i32 @__kmpc_single(ptr, i32) local_unnamed_addr #6

; Function Attrs: convergent nounwind
declare void @__kmpc_end_single(ptr, i32) local_unnamed_addr #6

; Function Attrs: convergent nounwind
declare void @__kmpc_barrier(ptr, i32) local_unnamed_addr #6

; Function Attrs: nounwind
declare !callback !18 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #3

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #2

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #7

define internal void @carts.edt.main() !carts !20 {
entry:
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %call = tail call i64 @time(ptr noundef null) #3
  %conv = trunc i64 %call to i32
  tail call void @srand(i32 noundef %conv) #3
  %call1 = tail call i32 @rand() #3
  %rem = srem i32 %call1, 100
  %add = add nsw i32 %rem, 1
  store i32 %add, ptr %shared_number, align 4, !tbaa !8
  %call2 = tail call i32 @rand() #3
  %rem3 = srem i32 %call2, 10
  %add4 = add nsw i32 %rem3, 1
  store i32 %add4, ptr %random_number, align 4, !tbaa !8
  %call5 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %add, i32 noundef %add4)
  call void @carts.edt.parallel(ptr %shared_number, ptr %random_number) #8
  br label %codeRepl

codeRepl:                                         ; preds = %entry
  call void @carts.edt.sync.done(ptr %shared_number, ptr %random_number)
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  ret void
}

define internal void @carts.edt.sync.done(ptr %shared_number, ptr %random_number) !carts !6 {
newFuncRoot:
  br label %entry.split

entry.split:                                      ; preds = %newFuncRoot
  %0 = load i32, ptr %shared_number, align 4, !tbaa !8
  %inc = add nsw i32 %0, 1
  store i32 %inc, ptr %shared_number, align 4, !tbaa !8
  %1 = load i32, ptr %random_number, align 4, !tbaa !8
  %call6 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.3, i32 noundef %inc, i32 noundef %1)
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
  ret void
}

attributes #0 = { mustprogress noinline nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #2 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #3 = { nounwind }
attributes #4 = { mustprogress norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #6 = { convergent nounwind }
attributes #7 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }
attributes #8 = { memory(argmem: readwrite) }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"openmp", i32 51}
!2 = !{i32 8, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
!4 = !{i32 7, !"uwtable", i32 2}
!5 = !{!"clang version 18.1.8"}
!6 = !{!"task", !7}
!7 = !{!"dep", !"dep"}
!8 = !{!9, !9, i64 0}
!9 = !{!"int", !10, i64 0}
!10 = !{!"omnipotent char", !11, i64 0}
!11 = !{!"Simple C++ TBAA"}
!12 = !{!"task", !13}
!13 = !{!"dep", !"param"}
!14 = !{!15}
!15 = distinct !{!15, !16, !".omp_outlined.: %__context"}
!16 = distinct !{!16, !".omp_outlined."}
!17 = !{!"sync", !7}
!18 = !{!19}
!19 = !{i64 2, i64 -1, i64 -1, i1 true}
!20 = !{!"main", !21}
!21 = !{}


-------------------------------------------------
[omp-transform] [Attributor] Done with 6 functions, result: changed.

-------------------------------------------------
[omp-transform] OmpTransformPass has finished

; ModuleID = 'simplefn.bc'
source_filename = "simplefn.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }

@.str = private unnamed_addr constant [28 x i8] c"EDT 1: The number is %d/%d\0A\00", align 1
@0 = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, ptr @0 }, align 8
@.str.1 = private unnamed_addr constant [36 x i8] c"EDT 1: The initial number is %d/%d\0A\00", align 1
@.str.2 = private unnamed_addr constant [28 x i8] c"EDT 0: The number is %d/%d\0A\00", align 1
@2 = private unnamed_addr constant %struct.ident_t { i32 0, i32 322, i32 0, i32 22, ptr @0 }, align 8
@.str.3 = private unnamed_addr constant [37 x i8] c"EDT 2: The final number is %d - %d.\0A\00", align 1

; Function Attrs: mustprogress noinline nounwind memory(argmem: readwrite) uwtable
define dso_local void @carts.edt(ptr nocapture noundef nonnull align 4 dereferenceable(4) %shared_number, ptr nocapture noundef nonnull readonly align 4 dereferenceable(4) %random_number) local_unnamed_addr #0 !carts !6 {
entry:
  %0 = load i32, ptr %random_number, align 4, !tbaa !8
  call void @carts.edt.task(ptr nocapture %shared_number, i32 %0) #11
  ret void
}

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #1

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(readwrite)
define internal void @carts.edt.task(ptr nocapture %0, i32 %1) #2 !carts !12 {
entry:
  tail call void @llvm.experimental.noalias.scope.decl(metadata !14)
  %2 = load i32, ptr %0, align 8, !tbaa !8, !noalias !14
  %inc.i = add nsw i32 %2, 1
  store i32 %inc.i, ptr %0, align 8, !tbaa !8, !noalias !14
  %inc1.i = add nsw i32 %1, 1
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %inc.i, i32 noundef %inc1.i) #3, !noalias !14
  ret void
}

; Function Attrs: nounwind
declare i32 @__kmpc_global_thread_num(ptr) local_unnamed_addr #3

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #3

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(ptr, i32, ptr) local_unnamed_addr #3

; Function Attrs: mustprogress norecurse nounwind uwtable
define dso_local noundef i32 @main() local_unnamed_addr #4 {
entry:
  call void @carts.edt.main()
  ret i32 0
}

; Function Attrs: nounwind
declare void @srand(i32 noundef) local_unnamed_addr #5

; Function Attrs: nounwind
declare i64 @time(ptr noundef) local_unnamed_addr #5

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #6

; Function Attrs: nounwind
declare i32 @rand() local_unnamed_addr #5

; Function Attrs: nocallback nofree norecurse nosync nounwind willreturn memory(readwrite)
define internal void @carts.edt.parallel(ptr nocapture %0, ptr nocapture readonly %1) #7 !carts !17 {
entry:
  %2 = load i32, ptr %0, align 4, !tbaa !8
  %3 = load i32, ptr %1, align 4, !tbaa !8
  %call = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %2, i32 noundef %3) #3
  tail call void @carts.edt(ptr nocapture noundef nonnull align 4 dereferenceable(4) %0, ptr nocapture noundef nonnull readonly align 4 dereferenceable(4) %1)
  br label %exit

exit:                                             ; preds = %entry
  ret void
}

; Function Attrs: convergent nounwind
declare i32 @__kmpc_single(ptr, i32) local_unnamed_addr #8

; Function Attrs: convergent nounwind
declare void @__kmpc_end_single(ptr, i32) local_unnamed_addr #8

; Function Attrs: convergent nounwind
declare void @__kmpc_barrier(ptr, i32) local_unnamed_addr #8

; Function Attrs: nounwind
declare !callback !18 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #3

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #6

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #9

; Function Attrs: norecurse nounwind
define internal void @carts.edt.main() #10 !carts !20 {
entry:
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %call = tail call i64 @time(ptr noundef null) #3
  %conv = trunc i64 %call to i32
  tail call void @srand(i32 noundef %conv) #3
  %call1 = tail call i32 @rand() #3
  %rem = srem i32 %call1, 100
  %add = add nsw i32 %rem, 1
  store i32 %add, ptr %shared_number, align 4, !tbaa !8
  %call2 = tail call i32 @rand() #3
  %rem3 = srem i32 %call2, 10
  %add4 = add nsw i32 %rem3, 1
  store i32 %add4, ptr %random_number, align 4, !tbaa !8
  %call5 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %add, i32 noundef %add4) #3
  call void @carts.edt.parallel(ptr nocapture %shared_number, ptr nocapture %random_number) #12
  br label %codeRepl

codeRepl:                                         ; preds = %entry
  call void @carts.edt.sync.done(ptr nocapture %shared_number, ptr nocapture %random_number) #3
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  ret void
}

; Function Attrs: norecurse nounwind
define internal void @carts.edt.sync.done(ptr nocapture %shared_number, ptr nocapture readonly %random_number) #10 !carts !6 {
newFuncRoot:
  br label %entry.split

entry.split:                                      ; preds = %newFuncRoot
  %0 = load i32, ptr %shared_number, align 4, !tbaa !8
  %inc = add nsw i32 %0, 1
  store i32 %inc, ptr %shared_number, align 4, !tbaa !8
  %1 = load i32, ptr %random_number, align 4, !tbaa !8
  %call6 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.3, i32 noundef %inc, i32 noundef %1) #3
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
  ret void
}

attributes #0 = { mustprogress noinline nounwind memory(argmem: readwrite) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #2 = { nocallback nofree nosync nounwind willreturn memory(readwrite) }
attributes #3 = { nounwind }
attributes #4 = { mustprogress norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #6 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #7 = { nocallback nofree norecurse nosync nounwind willreturn memory(readwrite) }
attributes #8 = { convergent nounwind }
attributes #9 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }
attributes #10 = { norecurse nounwind }
attributes #11 = { nounwind memory(argmem: readwrite) }
attributes #12 = { nounwind memory(readwrite) }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"openmp", i32 51}
!2 = !{i32 8, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
!4 = !{i32 7, !"uwtable", i32 2}
!5 = !{!"clang version 18.1.8"}
!6 = !{!"task", !7}
!7 = !{!"dep", !"dep"}
!8 = !{!9, !9, i64 0}
!9 = !{!"int", !10, i64 0}
!10 = !{!"omnipotent char", !11, i64 0}
!11 = !{!"Simple C++ TBAA"}
!12 = !{!"task", !13}
!13 = !{!"dep", !"param"}
!14 = !{!15}
!15 = distinct !{!15, !16, !".omp_outlined.: %__context"}
!16 = distinct !{!16, !".omp_outlined."}
!17 = !{!"sync", !7}
!18 = !{!19}
!19 = !{i64 2, i64 -1, i64 -1, i1 true}
!20 = !{!"main", !21}
!21 = !{}


-------------------------------------------------
# -debug-only=omp-transform,arts,carts,arts-ir-builder,arts-utils\
llvm-dis simplefn_arts_ir.bc
opt -load-pass-plugin=/home/randres/projects/carts/.install/carts/lib/ARTSAnalysis.so \
		-debug-only=arts-analysis,arts,carts,arts-ir-builder,arts-graph,carts-metadata,arts-codegen\
		-passes="arts-analysis" simplefn_arts_ir.bc -o simplefn_arts_analysis.bc

-------------------------------------------------
[arts-analysis] Running ARTS Analysis pass on Module: 
simplefn_arts_ir.bc
-------------------------------------------------
[arts-analysis] Creating and Initializing EDTs: 
- - - - - - - - - - - - - - - - - - - - - - - - 
[arts-analysis] Initializing ARTS Cache
[arts-codegen] Initializing ARTSCodegen
[arts-codegen] ARTSCodegen initialized
 - Analyzing Functions
-- Function: carts.edt
[arts] Function: carts.edt has CARTS Metadata
[arts] Creating EDT #0 for function: carts.edt
   - DepV: ptr %shared_number
   - DepV: ptr %random_number
[arts] Creating Task EDT for function: carts.edt

-- Function: carts.edt.task
[arts] Function: carts.edt.task has CARTS Metadata
[arts] Creating EDT #1 for function: carts.edt.task
   - DepV: ptr %0
   - ParamV: i32 %1
[arts] Creating Task EDT for function: carts.edt.task

-- Function: main
[arts] Function: main doesn't have CARTS Metadata

-- Function: carts.edt.parallel
[arts] Function: carts.edt.parallel has CARTS Metadata
[arts] Creating EDT #2 for function: carts.edt.parallel
   - DepV: ptr %0
   - DepV: ptr %1
[arts] Creating Task EDT for function: carts.edt.parallel
[arts] Creating Sync EDT for function: carts.edt.parallel

-- Function: carts.edt.main
[arts] Function: carts.edt.main has CARTS Metadata
[arts] Creating EDT #3 for function: carts.edt.main
[arts] Creating Task EDT for function: carts.edt.main
[arts] Creating Main EDT for function: carts.edt.main

-- Function: carts.edt.sync.done
[arts] Function: carts.edt.sync.done has CARTS Metadata
[arts] Creating EDT #4 for function: carts.edt.sync.done
   - DepV: ptr %shared_number
   - DepV: ptr %random_number
[arts] Creating Task EDT for function: carts.edt.sync.done

- - - - - - - - - - - - - - - - - - - - - - - - 
[arts-analysis] Computing Graph
[Attributor] Initializing AAEDTInfo: 

[AAEDTInfoFunction::initialize] EDT #0 for function "carts.edt"
   - Call to EDTFunction:
      tail call void @carts.edt(ptr nocapture noundef nonnull align 4 dereferenceable(4) %0, ptr nocapture noundef nonnull readonly align 4 dereferenceable(4) %1)

[AAEDTInfoFunction::initialize] EDT #1 for function "carts.edt.task"
   - Call to EDTFunction:
      call void @carts.edt.task(ptr nocapture %shared_number, i32 %0) #11

[AAEDTInfoFunction::initialize] EDT #2 for function "carts.edt.parallel"
   - Call to EDTFunction:
      call void @carts.edt.parallel(ptr nocapture %shared_number, ptr nocapture %random_number) #11
   - DoneEDT: EDT #4

[AAEDTInfoFunction::initialize] EDT #3 for function "carts.edt.main"
   - Call to EDTFunction:
      call void @carts.edt.main()
[arts] Function: main doesn't have CARTS Metadata
   - The ContextEDT is the MainEDT

[AAEDTInfoFunction::initialize] EDT #4 for function "carts.edt.sync.done"
   - Call to EDTFunction:
      call void @carts.edt.sync.done(ptr nocapture %shared_number, ptr nocapture %random_number) #3

[AAEDTInfoFunction::updateImpl] EDT #0
   - EDT #1 is a child of EDT #0
        - Inserting CreationEdge from "EDT #0" to "EDT #1"

[AAEDTInfoFunction::updateImpl] EDT #1
   - All ReachedEDTs are fixed for EDT #1
   - Getting ParentSyncEDT
     - ParentSyncEDT: EDT #2

[AAEDTInfoCallsite::initialize] EDT #1

[AAEDTInfoFunction::updateImpl] EDT #2
   - EDT #0 is a child of EDT #2
        - Inserting CreationEdge from "EDT #2" to "EDT #0"

[AAEDTInfoFunction::updateImpl] EDT #3
   - EDT #2 is a child of EDT #3
        - Inserting CreationEdge from "EDT #3" to "EDT #2"

[AAEDTInfoFunction::updateImpl] EDT #0
   - EDT #1 is a child of EDT #0
   - All ReachedEDTs are fixed for EDT #0
   - Getting ParentSyncEDT
     - ParentSyncEDT: EDT #2

[AAEDTInfoCallsite::initialize] EDT #0

[AAEDTInfoFunction::updateImpl] EDT #2
   - EDT #0 is a child of EDT #2
   - All ReachedEDTs are fixed for EDT #2

[AAEDTInfoCallsite::initialize] EDT #2

[AAEDTInfoFunction::updateImpl] EDT #3
   - EDT #2 is a child of EDT #3

[AAEDTInfoCallsite::updateImpl] EDT #1

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #1
   - Creating DepSlot #0 for value: ptr %shared_number
        - Inserting DataBlockEdge from "EDT #0" to "EDT #1" in Slot #0

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #1

[AAEDTInfoCallsite::updateImpl] EDT #0

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #0
   - Creating DepSlot #0 for value: ptr %0
        - Inserting DataBlockEdge from "EDT #2" to "EDT #0" in Slot #0

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #0
   - Creating DepSlot #1 for value: ptr %1
        - Inserting DataBlockEdge from "EDT #2" to "EDT #0" in Slot #1

[AAEDTInfoCallsite::updateImpl] EDT #2

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #2
   - Creating DepSlot #0 for value:   %shared_number = alloca i32, align 4
        - Inserting DataBlockEdge from "EDT #3" to "EDT #2" in Slot #0

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #2
   - Creating DepSlot #1 for value:   %random_number = alloca i32, align 4
        - Inserting DataBlockEdge from "EDT #3" to "EDT #2" in Slot #1

[AAEDTInfoCallsiteArg::updateImpl] ptr %shared_number from EDT #1

[AADataBlockInfoCtxAndVal::initialize] ptr %shared_number from EDT #1

[AAEDTInfoCallsite::updateImpl] EDT #1

[AAEDTInfoCallsiteArg::updateImpl] ptr %0 from EDT #0

[AADataBlockInfoCtxAndVal::initialize] ptr %0 from EDT #0

[AAEDTInfoCallsiteArg::updateImpl] ptr %1 from EDT #0

[AADataBlockInfoCtxAndVal::initialize] ptr %1 from EDT #0

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2

[AADataBlockInfoCtxAndVal::initialize]   %shared_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AADataBlockInfoCtxAndVal::initialize]   %random_number = alloca i32, align 4 from EDT #2

[AADataBlockInfoCtxAndVal::updateImpl] ptr %shared_number from EDT #1

[AAEDTInfoFunction::updateImpl] EDT #3
   - EDT #2 is a child of EDT #3

[AAEDTInfoCallsite::updateImpl] EDT #0

[AAEDTInfoCallsite::updateImpl] EDT #2

[AAEDTInfoCallsiteArg::updateImpl] ptr %shared_number from EDT #1

[AADataBlockInfoCtxAndVal::updateImpl] ptr %0 from EDT #0
   - Analyzing DependentChildEDTs on ptr %0 (ptr %shared_number)
        - Number of DependentChildEDTs: 1

[AADataBlockInfoCtxAndVal::updateImpl] ptr %0 from EDT #0

[AADataBlockInfoCtxAndVal::updateImpl] ptr %1 from EDT #0
   - Analyzing DependentChildEDTs on ptr %1 (ptr %random_number)
        - Number of DependentChildEDTs: 0

[AADataBlockInfoCtxAndVal::updateImpl] ptr %1 from EDT #0

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentChildEDTs on   %shared_number = alloca i32, align 4 (ptr %0)
        - Number of DependentChildEDTs: 1

[AADataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
   - Analyzing DependentSiblingEDTs on   %random_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!
   - Analyzing DependentChildEDTs on   %random_number = alloca i32, align 4 (ptr %1)
        - Number of DependentChildEDTs: 1

[AAEDTInfoFunction::updateImpl] EDT #4
   - All ReachedEDTs are fixed for EDT #4

[AAEDTInfoCallsite::initialize] EDT #4

[AAEDTInfoCallsiteArg::updateImpl] ptr %0 from EDT #0

[AAEDTInfoCallsiteArg::updateImpl] ptr %1 from EDT #0

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoFunction::updateImpl] EDT #3
   - EDT #2 is a child of EDT #3

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AADataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
   - Analyzing DependentSiblingEDTs on   %random_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AAEDTInfoCallsite::updateImpl] EDT #4

[AAEDTInfoCallsiteArg::initialize] CallArg #0 from EDT #4
   - Creating DepSlot #0 for value:   %shared_number = alloca i32, align 4

[AAEDTInfoCallsiteArg::initialize] CallArg #1 from EDT #4
   - Creating DepSlot #1 for value:   %random_number = alloca i32, align 4

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #4

[AADataBlockInfoCtxAndVal::initialize]   %shared_number = alloca i32, align 4 from EDT #4

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #4

[AADataBlockInfoCtxAndVal::initialize]   %random_number = alloca i32, align 4 from EDT #4

[AAEDTInfoFunction::updateImpl] EDT #3
   - EDT #2 is a child of EDT #3
   - EDT #4 is a child of EDT #3
        - Inserting CreationEdge from "EDT #3" to "EDT #4"
   - All ReachedEDTs are fixed for EDT #3

[AAEDTInfoCallsite::initialize] EDT #3

[AAEDTInfoCallsite::updateImpl] EDT #4

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #4

[AADataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #4

[AAEDTInfoCallsite::updateImpl] EDT #3
   - All DataBlocks were fixed for EDT #3

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
   - AAPointerInfo is not at fixpoint!

[AADataBlockInfoCtxAndVal::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
   - Analyzing DependentSiblingEDTs on   %random_number = alloca i32, align 4
        - Number of DependentSiblingEDTs: 1

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #4

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #4

[AAEDTInfoCallsiteArg::updateImpl]   %random_number = alloca i32, align 4 from EDT #2
        - Inserting DataBlockEdge from "EDT #0" to "EDT #4" in Slot #1

[AADataBlockInfoCtxAndVal::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
   - Analyzing DependentSiblingEDTs on   %shared_number = alloca i32, align 4
        - Number of DependentSiblingEDTs: 1

[AAEDTInfoCallsiteArg::updateImpl]   %shared_number = alloca i32, align 4 from EDT #2
        - Inserting DataBlockEdge from "EDT #1" to "EDT #4" in Slot #0
-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #0 -> 
     -ParentEDT: EDT #2
     -ParentSyncEDT: EDT #2
     #Child EDTs: 1{1}
     #Reached DescendantEDTs: 0{}

-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #1 -> 
     -ParentEDT: EDT #0
     -ParentSyncEDT: EDT #2
     #Child EDTs: 0{}
     #Reached DescendantEDTs: 0{}

-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #2 -> 
     -ParentEDT: EDT #3
     -ParentSyncEDT: <null>
     #Child EDTs: 1{0}
     #Reached DescendantEDTs: 1{1}

-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #3 -> 
     -ParentSyncEDT: <null>
     #Child EDTs: 2{2, 4}
     #Reached DescendantEDTs: 2{1, 0}

-------------------------------
[AAEDTInfoFunction::manifest] 
State for EDT #4 -> 
     -ParentEDT: EDT #3
     -ParentSyncEDT: <null>
     #Child EDTs: 0{}
     #Reached DescendantEDTs: 0{}

        - Inserting Guid of child "EDT #4" in the edge from "EDT #3" to "EDT #2"
        - Inserting Guid of "EDT #4" in the  from "EDT #2" to "EDT #0"
        - Inserting Guid of "EDT #4" in the  from "EDT #0" to "EDT #1"
-------------------------------
[AADataBlockInfoCtxAndVal::manifest] ptr %shared_number from EDT #1
DataBlock ->
     - Context: EDT #1 / Slot 0
     - SignalEDT: EDT #4
     - ParentCtx: EDT #0 / Slot 0
     - ParentSync: EDT #2 / Slot 0
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 0{}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest] ptr %0 from EDT #0
DataBlock ->
     - Context: EDT #0 / Slot 0
     - ParentCtx: EDT #2 / Slot 0
     - ChildDBss: 1
      - EDT #1 / Slot 0
     - #DependentChildEDTs: 1{1}
     - #DependentSiblingEDTs: 0{}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest] ptr %1 from EDT #0
DataBlock ->
     - Context: EDT #0 / Slot 1
     - SignalEDT: EDT #4
     - ParentCtx: EDT #2 / Slot 1
     - ParentSync: EDT #2 / Slot 1
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 0{}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest]   %shared_number = alloca i32, align 4 from EDT #2
DataBlock ->
     - Context: EDT #2 / Slot 0
     - ChildDBss: 1
      - EDT #0 / Slot 0
     - DependentSiblingEDT: EDT #4 / Slot #0
     - #DependentChildEDTs: 1{0}
     - #DependentSiblingEDTs: 1{4}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest]   %random_number = alloca i32, align 4 from EDT #2
DataBlock ->
     - Context: EDT #2 / Slot 1
     - ChildDBss: 1
      - EDT #0 / Slot 1
     - DependentSiblingEDT: EDT #4 / Slot #1
     - #DependentChildEDTs: 1{0}
     - #DependentSiblingEDTs: 1{4}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest]   %shared_number = alloca i32, align 4 from EDT #4
DataBlock ->
     - Context: EDT #4 / Slot 0
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 0{}

-------------------------------
[AADataBlockInfoCtxAndVal::manifest]   %random_number = alloca i32, align 4 from EDT #4
DataBlock ->
     - Context: EDT #4 / Slot 1
     - #DependentChildEDTs: 0{}
     - #DependentSiblingEDTs: 0{}

[Attributor] Done with 6 functions, result: unchanged.
- - - - - - - - - - - - - - - - - - - - - - - -
[arts-graph] Printing the ARTS Graph
- - - - - - - - - - - - - - - - - - 
- EDT #3 - "edt_3.main"
  - Type: main
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 0
  - Incoming Creation Edges:
    - The EDT has no incoming Creation edges
    - The EDT has no incoming DataBlock slots
  - Outgoing Edges:
    - [Creation] "EDT #4"
    - [Creation] "EDT #2"
    - [DataBlock] "EDT #2" in Slot #1
      -   %random_number = alloca i32, align 4
    - [DataBlock] "EDT #2" in Slot #0
      -   %shared_number = alloca i32, align 4
- - - - - - - - - - - - - - - - - - 
- EDT #2 - "edt_2.sync"
  - Type: sync
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 2
      - ptr %0
      - ptr %1
  - Incoming Creation Edges:
    - [Creation] "EDT #3"
  - Incoming DataBlock Slots:
    - EDTSlot #0
      - [DataBlock]   %shared_number = alloca i32, align 4 from "EDT #3"/ No parent
    - EDTSlot #1
      - [DataBlock]   %random_number = alloca i32, align 4 from "EDT #3"/ No parent
  - Outgoing Edges:
    - [Creation] "EDT #0"
    - [DataBlock] "EDT #0" in Slot #1
      - ptr %1 /   %random_number = alloca i32, align 4
    - [DataBlock] "EDT #0" in Slot #0
      - ptr %0 /   %shared_number = alloca i32, align 4
- - - - - - - - - - - - - - - - - - 
- EDT #1 - "edt_1.task"
  - Type: task
  - Parent Sync: "EDT #2"
  - Data Environment:
    - Number of ParamV = 1
      - i32 %1
    - Number of DepV = 1
      - ptr %0
  - Incoming Creation Edges:
    - [Creation] "EDT #0"
  - Incoming DataBlock Slots:
    - EDTSlot #0
      - [DataBlock] ptr %shared_number from "EDT #0" /   %shared_number = alloca i32, align 4 [Parent EDT #2]
  - Outgoing Edges:
    - The EDT has no outgoing creation edges
    - [DataBlock] "EDT #4" in Slot #0
      - ptr %shared_number /   %shared_number = alloca i32, align 4
- - - - - - - - - - - - - - - - - - 
- EDT #4 - "edt_4.task"
  - Type: task
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 2
      - ptr %shared_number
      - ptr %random_number
  - Incoming Creation Edges:
    - [Creation] "EDT #3"
  - Incoming DataBlock Slots:
    - EDTSlot #0
      - [DataBlock] ptr %shared_number from "EDT #1" /   %shared_number = alloca i32, align 4 [Parent EDT #2]
    - EDTSlot #1
      - [DataBlock] ptr %1 from "EDT #0" /   %random_number = alloca i32, align 4 [Parent EDT #2]
  - Outgoing Edges:
    - The EDT has no outgoing creation edges
- - - - - - - - - - - - - - - - - - 
- EDT #0 - "edt_0.task"
  - Type: task
  - Parent Sync: "EDT #2"
  - Data Environment:
    - Number of ParamV = 0
    - Number of DepV = 2
      - ptr %shared_number
      - ptr %random_number
  - Incoming Creation Edges:
    - [Creation] "EDT #2"
  - Incoming DataBlock Slots:
    - EDTSlot #0
      - [DataBlock] ptr %0 from "EDT #2" /   %shared_number = alloca i32, align 4 [Parent EDT #2]
    - EDTSlot #1
      - [DataBlock] ptr %1 from "EDT #2" /   %random_number = alloca i32, align 4 [Parent EDT #2]
  - Outgoing Edges:
    - [Creation] "EDT #1"
    - [DataBlock] "EDT #4" in Slot #1
      - ptr %1 /   %random_number = alloca i32, align 4
    - [DataBlock] "EDT #1" in Slot #0
      - ptr %shared_number /   %shared_number = alloca i32, align 4

- - - - - - - - - - - - - - - - - - - - - - - -

[arts-codegen] Creating codegen for EDT #0
[arts-codegen] Creating function for EDT #0
[arts-codegen] Reserving GUID for EDT #0
[arts-codegen] Creating codegen for EDT #2
[arts-codegen] Creating function for EDT #2
[arts-codegen] Creating codegen for EDT #1
[arts-codegen] Creating function for EDT #1
[arts-codegen] Reserving GUID for EDT #1
[arts-codegen] Creating codegen for EDT #4
[arts-codegen] Creating function for EDT #4
[arts-codegen] Reserving GUID for EDT #4
[arts-codegen] Creating codegen for EDT #3
[arts-codegen] Creating function for EDT #3
[arts-codegen] Reserving GUID for EDT #2
[arts-codegen] Reserving GUID for EDT #3
     EDT #3 doesn't have a parent EDT, using parent function

All EDT Guids have been reserved

Generating Code for EDT #3
[arts-codegen] Inserting Entry for EDT #3
 - Inserting ParamV
     EDT #3 doesn't have a parent EDT
 - Inserting DepV
     EDT #3 doesn't have incoming slot nodes
 - Inserting DataBlocks
[arts-codegen] Creating DBCodegen   %random_number = alloca i32, align 4
[arts-codegen] Creating DBCodegen   %shared_number = alloca i32, align 4
[arts-codegen] Inserting Call for EDT #3
 - EDT Call:   %5 = call i64 @artsEdtCreateWithGuid(ptr @edt_3.main, i64 %1, i32 %2, ptr %edt_3_paramv, i32 %4)

Generating Code for EDT #2
[arts-codegen] Inserting Entry for EDT #2
 - Inserting ParamV
   - ParamVGuid[0]: EDT4
 - Inserting DepV
   - DepV[0]: ptr %0 ->   %edt_2.depv_0.ptr.load = load ptr, ptr %edt_2.depv_0.ptr, align 8
   - DepV[1]: ptr %1 ->   %edt_2.depv_1.ptr.load = load ptr, ptr %edt_2.depv_1.ptr, align 8
 - Inserting DataBlocks
[arts-codegen] Inserting Call for EDT #2
 - ParamVGuid[0]: EDT4
 - EDT Call:   %9 = call i64 @artsEdtCreateWithGuid(ptr @edt_2.sync, i64 %3, i32 %6, ptr %edt_2_paramv, i32 %8)

Generating Code for EDT #0
[arts-codegen] Inserting Entry for EDT #0
 - Inserting ParamV
   - ParamVGuid[0]: EDT4
 - Inserting DepV
   - DepV[0]: ptr %shared_number ->   %edt_0.depv_0.ptr.load = load ptr, ptr %edt_0.depv_0.ptr, align 8
   - DepV[1]: ptr %random_number ->   %edt_0.depv_1.ptr.load = load ptr, ptr %edt_0.depv_1.ptr, align 8
 - Inserting DataBlocks
[arts-codegen] Inserting Call for EDT #0
 - ParamVGuid[0]: EDT4
 - EDT Call:   %8 = call i64 @artsEdtCreateWithGuid(ptr @edt_0.task, i64 %1, i32 %5, ptr %edt_0_paramv, i32 %7)

Generating Code for EDT #1
[arts-codegen] Inserting Entry for EDT #1
 - Inserting ParamV
   - ParamV[0]: i32 %1 ->   %1 = trunc i64 %0 to i32
   - ParamVGuid[1]: EDT4
 - Inserting DepV
   - DepV[0]: ptr %0 ->   %edt_1.depv_0.ptr.load = load ptr, ptr %edt_1.depv_0.ptr, align 8
 - Inserting DataBlocks
[arts-codegen] Inserting Call for EDT #1
 - ParamV[0]:   %3 = load i32, ptr %edt_0.depv_1.ptr.load, align 4, !tbaa !6
 - ParamVGuid[1]: EDT4
 - EDT Call:   %8 = call i64 @artsEdtCreateWithGuid(ptr @edt_1.task, i64 %1, i32 %4, ptr %edt_1_paramv, i32 %7)

Generating Code for EDT #4
[arts-codegen] Inserting Entry for EDT #4
 - Inserting ParamV
 - Inserting DepV
   - DepV[0]: ptr %shared_number ->   %edt_4.depv_0.ptr.load = load ptr, ptr %edt_4.depv_0.ptr, align 8
   - DepV[1]: ptr %random_number ->   %edt_4.depv_1.ptr.load = load ptr, ptr %edt_4.depv_1.ptr, align 8
 - Inserting DataBlocks
[arts-codegen] Inserting Call for EDT #4
 - EDT Call:   %13 = call i64 @artsEdtCreateWithGuid(ptr @edt_4.task, i64 %1, i32 %10, ptr %edt_4_paramv, i32 %12)

All EDT Entries and Calls have been inserted

Inserting EDT Signals
[arts-codegen] Inserting Signals from EDT #0

Signal DB ptr %1 from EDT #0 to EDT #4
 - ContextEDT: 0
 - Using DepSlot of FromEDT - Slot: 1 - DBPtr:   %edt_0.depv_1.ptr.load = load ptr, ptr %edt_0.depv_1.ptr, align 8
 - DBGuid:   %edt_0.depv_1.guid.load = load i64, ptr %edt_0.depv_1.guid, align 8

Signal DB ptr %shared_number from EDT #0 to EDT #1
 - ContextEDT: 1
 - We have a parent
 - DBPtr:   %edt_0.depv_0.ptr.load = load ptr, ptr %edt_0.depv_0.ptr, align 8
 - DBGuid:   %edt_0.depv_0.guid.load = load i64, ptr %edt_0.depv_0.guid, align 8
[arts-codegen] Inserting Signals from EDT #1

Signal DB ptr %shared_number from EDT #1 to EDT #4
 - ContextEDT: 1
 - Using DepSlot of FromEDT - Slot: 0 - DBPtr:   %edt_1.depv_0.ptr.load = load ptr, ptr %edt_1.depv_0.ptr, align 8
 - DBGuid:   %edt_1.depv_0.guid.load = load i64, ptr %edt_1.depv_0.guid, align 8
[arts-codegen] Inserting Signals from EDT #4
[arts-codegen] Inserting Signals from EDT #2

Signal DB ptr %1 from EDT #2 to EDT #0
 - ContextEDT: 0
 - We have a parent
 - DBPtr:   %edt_2.depv_1.ptr.load = load ptr, ptr %edt_2.depv_1.ptr, align 8
 - DBGuid:   %edt_2.depv_1.guid.load = load i64, ptr %edt_2.depv_1.guid, align 8

Signal DB ptr %0 from EDT #2 to EDT #0
 - ContextEDT: 0
 - We have a parent
 - DBPtr:   %edt_2.depv_0.ptr.load = load ptr, ptr %edt_2.depv_0.ptr, align 8
 - DBGuid:   %edt_2.depv_0.guid.load = load i64, ptr %edt_2.depv_0.guid, align 8
[arts-codegen] Inserting Signals from EDT #3

Signal DB   %random_number = alloca i32, align 4 from EDT #3 to EDT #2
 - ContextEDT: 2
 - We created the EDT
 - DBPtr:   %db.random_number.ptr = inttoptr i64 %db.random_number.addr.ld to ptr
 - DBGuid:   %4 = call i64 @artsDbCreatePtr(ptr %db.random_number.addr, i64 %db.random_number.size.ld, i32 7)

Signal DB   %shared_number = alloca i32, align 4 from EDT #3 to EDT #2
 - ContextEDT: 2
 - We created the EDT
 - DBPtr:   %db.shared_number.ptr = inttoptr i64 %db.shared_number.addr.ld to ptr
 - DBGuid:   %5 = call i64 @artsDbCreatePtr(ptr %db.shared_number.addr, i64 %db.shared_number.size.ld, i32 7)
[arts-codegen] Inserting Init Functions
[arts-codegen] Inserting ARTS Shutdown Function

-------------------------------------------------
[arts-analysis] Process has finished

; ModuleID = 'simplefn_arts_ir.bc'
source_filename = "simplefn.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }
%struct.artsEdtDep_t = type { i64, i32, ptr }

@.str = private unnamed_addr constant [28 x i8] c"EDT 1: The number is %d/%d\0A\00", align 1
@0 = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, ptr @0 }, align 8
@.str.1 = private unnamed_addr constant [36 x i8] c"EDT 1: The initial number is %d/%d\0A\00", align 1
@.str.2 = private unnamed_addr constant [28 x i8] c"EDT 0: The number is %d/%d\0A\00", align 1
@2 = private unnamed_addr constant %struct.ident_t { i32 0, i32 322, i32 0, i32 22, ptr @0 }, align 8
@.str.3 = private unnamed_addr constant [37 x i8] c"EDT 2: The final number is %d - %d.\0A\00", align 1

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #0

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nounwind
declare i32 @__kmpc_global_thread_num(ptr) local_unnamed_addr #1

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #1

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(ptr, i32, ptr) local_unnamed_addr #1

; Function Attrs: nounwind
declare void @srand(i32 noundef) local_unnamed_addr #2

; Function Attrs: nounwind
declare i64 @time(ptr noundef) local_unnamed_addr #2

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #3

; Function Attrs: nounwind
declare i32 @rand() local_unnamed_addr #2

; Function Attrs: convergent nounwind
declare i32 @__kmpc_single(ptr, i32) local_unnamed_addr #4

; Function Attrs: convergent nounwind
declare void @__kmpc_end_single(ptr, i32) local_unnamed_addr #4

; Function Attrs: convergent nounwind
declare void @__kmpc_barrier(ptr, i32) local_unnamed_addr #4

; Function Attrs: nounwind
declare !callback !6 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #1

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #3

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #5

define internal void @edt_0.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %0 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt_1.task_guid.addr = alloca i64, align 8
  store i64 %0, ptr %edt_1.task_guid.addr, align 8
  %1 = load i64, ptr %edt_1.task_guid.addr, align 8
  %edt_0.paramv_0.guid.edt_4 = getelementptr inbounds i64, ptr %paramv, i64 0
  %2 = load i64, ptr %edt_0.paramv_0.guid.edt_4, align 8
  %edt_0.depv_0.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  %edt_0.depv_0.guid.load = load i64, ptr %edt_0.depv_0.guid, align 8
  %edt_0.depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 2
  %edt_0.depv_0.ptr.load = load ptr, ptr %edt_0.depv_0.ptr, align 8
  %edt_0.depv_1.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 0
  %edt_0.depv_1.guid.load = load i64, ptr %edt_0.depv_1.guid, align 8
  %edt_0.depv_1.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 2
  %edt_0.depv_1.ptr.load = load ptr, ptr %edt_0.depv_1.ptr, align 8
  br label %edt.body

edt.body:                                         ; preds = %entry
  %3 = load i32, ptr %edt_0.depv_1.ptr.load, align 4, !tbaa !8
  %edt_1_paramc = alloca i32, align 4
  store i32 2, ptr %edt_1_paramc, align 4
  %4 = load i32, ptr %edt_1_paramc, align 4
  %edt_1_paramv = alloca i64, i32 %4, align 8
  %edt_1.paramv_0 = getelementptr inbounds i64, ptr %edt_1_paramv, i64 0
  %5 = sext i32 %3 to i64
  store i64 %5, ptr %edt_1.paramv_0, align 8
  %edt_1.paramv_1.guid.edt_4 = getelementptr inbounds i64, ptr %edt_1_paramv, i64 1
  store i64 %2, ptr %edt_1.paramv_1.guid.edt_4, align 8
  %6 = alloca i32, align 4
  store i32 1, ptr %6, align 4
  %7 = load i32, ptr %6, align 4
  %8 = call i64 @artsEdtCreateWithGuid(ptr @edt_1.task, i64 %1, i32 %4, ptr %edt_1_paramv, i32 %7)
  br label %exit

exit:                                             ; preds = %edt.body
  %toedt.4.slot.1 = alloca i32, align 4
  store i32 1, ptr %toedt.4.slot.1, align 4
  %9 = load i32, ptr %toedt.4.slot.1, align 4
  call void @artsSignalEdt(i64 %2, i32 %9, i64 %edt_0.depv_1.guid.load)
  %toedt.1.slot.0 = alloca i32, align 4
  store i32 0, ptr %toedt.1.slot.0, align 4
  %10 = load i32, ptr %toedt.1.slot.0, align 4
  call void @artsSignalEdt(i64 %1, i32 %10, i64 %edt_0.depv_0.guid.load)
  ret void
}

define internal void @edt_2.sync(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %0 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt_0.task_guid.addr = alloca i64, align 8
  store i64 %0, ptr %edt_0.task_guid.addr, align 8
  %1 = load i64, ptr %edt_0.task_guid.addr, align 8
  %edt_2.paramv_0.guid.edt_4 = getelementptr inbounds i64, ptr %paramv, i64 0
  %2 = load i64, ptr %edt_2.paramv_0.guid.edt_4, align 8
  %edt_2.depv_0.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  %edt_2.depv_0.guid.load = load i64, ptr %edt_2.depv_0.guid, align 8
  %edt_2.depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 2
  %edt_2.depv_0.ptr.load = load ptr, ptr %edt_2.depv_0.ptr, align 8
  %edt_2.depv_1.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 0
  %edt_2.depv_1.guid.load = load i64, ptr %edt_2.depv_1.guid, align 8
  %edt_2.depv_1.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 2
  %edt_2.depv_1.ptr.load = load ptr, ptr %edt_2.depv_1.ptr, align 8
  br label %edt.body

edt.body:                                         ; preds = %entry
  %3 = load i32, ptr %edt_2.depv_0.ptr.load, align 4, !tbaa !8
  %4 = load i32, ptr %edt_2.depv_1.ptr.load, align 4, !tbaa !8
  %call = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.2, i32 noundef %3, i32 noundef %4) #1
  %edt_0_paramc = alloca i32, align 4
  store i32 1, ptr %edt_0_paramc, align 4
  %5 = load i32, ptr %edt_0_paramc, align 4
  %edt_0_paramv = alloca i64, i32 %5, align 8
  %edt_0.paramv_0.guid.edt_4 = getelementptr inbounds i64, ptr %edt_0_paramv, i64 0
  store i64 %2, ptr %edt_0.paramv_0.guid.edt_4, align 8
  %6 = alloca i32, align 4
  store i32 2, ptr %6, align 4
  %7 = load i32, ptr %6, align 4
  %8 = call i64 @artsEdtCreateWithGuid(ptr @edt_0.task, i64 %1, i32 %5, ptr %edt_0_paramv, i32 %7)
  br label %exit1

exit1:                                            ; preds = %edt.body
  br label %exit

exit:                                             ; preds = %exit1
  %toedt.0.slot.1 = alloca i32, align 4
  store i32 1, ptr %toedt.0.slot.1, align 4
  %9 = load i32, ptr %toedt.0.slot.1, align 4
  call void @artsSignalEdt(i64 %1, i32 %9, i64 %edt_2.depv_1.guid.load)
  %toedt.0.slot.0 = alloca i32, align 4
  store i32 0, ptr %toedt.0.slot.0, align 4
  %10 = load i32, ptr %toedt.0.slot.0, align 4
  call void @artsSignalEdt(i64 %1, i32 %10, i64 %edt_2.depv_0.guid.load)
  ret void
}

declare i64 @artsReserveGuidRoute(i32, i32)

define internal void @edt_1.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %edt_1.paramv_0 = getelementptr inbounds i64, ptr %paramv, i64 0
  %0 = load i64, ptr %edt_1.paramv_0, align 8
  %1 = trunc i64 %0 to i32
  %edt_1.paramv_1.guid.edt_4 = getelementptr inbounds i64, ptr %paramv, i64 1
  %2 = load i64, ptr %edt_1.paramv_1.guid.edt_4, align 8
  %edt_1.depv_0.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  %edt_1.depv_0.guid.load = load i64, ptr %edt_1.depv_0.guid, align 8
  %edt_1.depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 2
  %edt_1.depv_0.ptr.load = load ptr, ptr %edt_1.depv_0.ptr, align 8
  br label %edt.body

edt.body:                                         ; preds = %entry
  tail call void @llvm.experimental.noalias.scope.decl(metadata !12)
  %3 = load i32, ptr %edt_1.depv_0.ptr.load, align 8, !tbaa !8, !noalias !12
  %inc.i = add nsw i32 %3, 1
  store i32 %inc.i, ptr %edt_1.depv_0.ptr.load, align 8, !tbaa !8, !noalias !12
  %inc1.i = add nsw i32 %1, 1
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %inc.i, i32 noundef %inc1.i) #1, !noalias !12
  br label %exit

exit:                                             ; preds = %edt.body
  %toedt.4.slot.0 = alloca i32, align 4
  store i32 0, ptr %toedt.4.slot.0, align 4
  %4 = load i32, ptr %toedt.4.slot.0, align 4
  call void @artsSignalEdt(i64 %2, i32 %4, i64 %edt_1.depv_0.guid.load)
  ret void
}

define internal void @edt_4.task(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %edt_4.depv_0.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 0
  %edt_4.depv_0.guid.load = load i64, ptr %edt_4.depv_0.guid, align 8
  %edt_4.depv_0.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 0, i32 2
  %edt_4.depv_0.ptr.load = load ptr, ptr %edt_4.depv_0.ptr, align 8
  %edt_4.depv_1.guid = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 0
  %edt_4.depv_1.guid.load = load i64, ptr %edt_4.depv_1.guid, align 8
  %edt_4.depv_1.ptr = getelementptr inbounds %struct.artsEdtDep_t, ptr %depv, i32 1, i32 2
  %edt_4.depv_1.ptr.load = load ptr, ptr %edt_4.depv_1.ptr, align 8
  br label %edt.body

edt.body:                                         ; preds = %entry
  br label %entry.split

entry.split:                                      ; preds = %edt.body
  %0 = load i32, ptr %edt_4.depv_0.ptr.load, align 4, !tbaa !8
  %inc = add nsw i32 %0, 1
  store i32 %inc, ptr %edt_4.depv_0.ptr.load, align 4, !tbaa !8
  %1 = load i32, ptr %edt_4.depv_1.ptr.load, align 4, !tbaa !8
  %call6 = call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.3, i32 noundef %inc, i32 noundef %1) #1
  br label %entry.split.ret.exitStub

entry.split.ret.exitStub:                         ; preds = %entry.split
  br label %exit

exit:                                             ; preds = %entry.split.ret.exitStub
  call void @artsShutdown()
  ret void
}

define internal void @edt_3.main(i32 %paramc, ptr %paramv, i32 %depc, ptr %depv) {
entry:
  %0 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt_4.task_guid.addr = alloca i64, align 8
  store i64 %0, ptr %edt_4.task_guid.addr, align 8
  %1 = load i64, ptr %edt_4.task_guid.addr, align 8
  %2 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt_2.sync_guid.addr = alloca i64, align 8
  store i64 %2, ptr %edt_2.sync_guid.addr, align 8
  %3 = load i64, ptr %edt_2.sync_guid.addr, align 8
  %db.random_number.addr = alloca i64, align 8
  %db.random_number.size = alloca i64, align 8
  store i64 4, ptr %db.random_number.size, align 8
  %db.random_number.size.ld = load i64, ptr %db.random_number.size, align 8
  %4 = call i64 @artsDbCreatePtr(ptr %db.random_number.addr, i64 %db.random_number.size.ld, i32 7)
  %db.random_number.addr.ld = load i64, ptr %db.random_number.addr, align 8
  %db.random_number.ptr = inttoptr i64 %db.random_number.addr.ld to ptr
  %db.shared_number.addr = alloca i64, align 8
  %db.shared_number.size = alloca i64, align 8
  store i64 4, ptr %db.shared_number.size, align 8
  %db.shared_number.size.ld = load i64, ptr %db.shared_number.size, align 8
  %5 = call i64 @artsDbCreatePtr(ptr %db.shared_number.addr, i64 %db.shared_number.size.ld, i32 7)
  %db.shared_number.addr.ld = load i64, ptr %db.shared_number.addr, align 8
  %db.shared_number.ptr = inttoptr i64 %db.shared_number.addr.ld to ptr
  br label %edt.body

edt.body:                                         ; preds = %entry
  %shared_number = alloca i32, align 4
  %random_number = alloca i32, align 4
  %call = tail call i64 @time(ptr noundef null) #1
  %conv = trunc i64 %call to i32
  tail call void @srand(i32 noundef %conv) #1
  %call1 = tail call i32 @rand() #1
  %rem = srem i32 %call1, 100
  %add = add nsw i32 %rem, 1
  store i32 %add, ptr %db.shared_number.ptr, align 4, !tbaa !8
  %call2 = tail call i32 @rand() #1
  %rem3 = srem i32 %call2, 10
  %add4 = add nsw i32 %rem3, 1
  store i32 %add4, ptr %db.random_number.ptr, align 4, !tbaa !8
  %call5 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.1, i32 noundef %add, i32 noundef %add4) #1
  %edt_2_paramc = alloca i32, align 4
  store i32 1, ptr %edt_2_paramc, align 4
  %6 = load i32, ptr %edt_2_paramc, align 4
  %edt_2_paramv = alloca i64, i32 %6, align 8
  %edt_2.paramv_0.guid.edt_4 = getelementptr inbounds i64, ptr %edt_2_paramv, i64 0
  store i64 %1, ptr %edt_2.paramv_0.guid.edt_4, align 8
  %7 = alloca i32, align 4
  store i32 2, ptr %7, align 4
  %8 = load i32, ptr %7, align 4
  %9 = call i64 @artsEdtCreateWithGuid(ptr @edt_2.sync, i64 %3, i32 %6, ptr %edt_2_paramv, i32 %8)
  br label %codeRepl

codeRepl:                                         ; preds = %edt.body
  %edt_4_paramc = alloca i32, align 4
  store i32 0, ptr %edt_4_paramc, align 4
  %10 = load i32, ptr %edt_4_paramc, align 4
  %edt_4_paramv = alloca i64, i32 %10, align 8
  %11 = alloca i32, align 4
  store i32 2, ptr %11, align 4
  %12 = load i32, ptr %11, align 4
  %13 = call i64 @artsEdtCreateWithGuid(ptr @edt_4.task, i64 %1, i32 %10, ptr %edt_4_paramv, i32 %12)
  br label %entry.split.ret

entry.split.ret:                                  ; preds = %codeRepl
  br label %exit

exit:                                             ; preds = %entry.split.ret
  %toedt.2.slot.1 = alloca i32, align 4
  store i32 1, ptr %toedt.2.slot.1, align 4
  %14 = load i32, ptr %toedt.2.slot.1, align 4
  call void @artsSignalEdt(i64 %3, i32 %14, i64 %4)
  %toedt.2.slot.0 = alloca i32, align 4
  store i32 0, ptr %toedt.2.slot.0, align 4
  %15 = load i32, ptr %toedt.2.slot.0, align 4
  call void @artsSignalEdt(i64 %3, i32 %15, i64 %5)
  ret void
}

declare i64 @artsDbCreatePtr(ptr, i64, i32)

declare i64 @artsEdtCreateWithGuid(ptr, i64, i32, ptr, i32)

declare void @artsSignalEdt(i64, i32, i64)

define dso_local void @initPerNode(i32 %nodeId, i32 %argc, ptr %argv) {
entry:
  ret void
}

define dso_local void @initPerWorker(i32 %nodeId, i32 %workerId, i32 %argc, ptr %argv) {
entry:
  %0 = icmp eq i32 %nodeId, 0
  %1 = icmp eq i32 %workerId, 0
  %2 = and i1 %0, %1
  br i1 %2, label %then, label %exit

then:                                             ; preds = %entry
  %3 = tail call i64 @artsReserveGuidRoute(i32 1, i32 0)
  %edt_3.main_guid.addr = alloca i64, align 8
  store i64 %3, ptr %edt_3.main_guid.addr, align 8
  %4 = load i64, ptr %edt_3.main_guid.addr, align 8
  br label %body

body:                                             ; preds = %then
  %edt_3_paramc = alloca i32, align 4
  store i32 0, ptr %edt_3_paramc, align 4
  %5 = load i32, ptr %edt_3_paramc, align 4
  %edt_3_paramv = alloca i64, i32 %5, align 8
  %6 = alloca i32, align 4
  store i32 0, ptr %6, align 4
  %7 = load i32, ptr %6, align 4
  %8 = call i64 @artsEdtCreateWithGuid(ptr @edt_3.main, i64 %4, i32 %5, ptr %edt_3_paramv, i32 %7)
  br label %exit

exit:                                             ; preds = %body, %entry
  ret void
}

define dso_local i32 @main(i32 %argc, ptr %argv) {
entry:
  call void @artsRT(i32 %argc, ptr %argv)
  ret i32 0
}

declare void @artsRT(i32, ptr)

declare void @artsShutdown()

attributes #0 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nounwind }
attributes #2 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #4 = { convergent nounwind }
attributes #5 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"openmp", i32 51}
!2 = !{i32 8, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
!4 = !{i32 7, !"uwtable", i32 2}
!5 = !{!"clang version 18.1.8"}
!6 = !{!7}
!7 = !{i64 2, i64 -1, i64 -1, i1 true}
!8 = !{!9, !9, i64 0}
!9 = !{!"int", !10, i64 0}
!10 = !{!"omnipotent char", !11, i64 0}
!11 = !{!"Simple C++ TBAA"}
!12 = !{!13}
!13 = distinct !{!13, !14, !".omp_outlined.: %__context"}
!14 = distinct !{!14, !".omp_outlined."}


-------------------------------------------------
[arts-graph] Destroying the CARTS Graph
llvm-dis simplefn_arts_analysis.bc
opt -O3 simplefn_arts_analysis.bc -o simplefn_opt.bc
llvm-dis simplefn_opt.bc
clang++ simplefn_opt.bc -O3 -g3 -march=native -o simplefn_opt -I/home/randres/projects/carts/carts/include -L/home/randres/projects/carts/.install/arts/lib -larts -lrdmacm
