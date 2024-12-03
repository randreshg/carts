clang++ -fopenmp -O3 -g3 -emit-ast -o taskwithdeps.ast taskwithdeps.cpp
clang++ -fopenmp -O3 -g3 -emit-llvm -c taskwithdeps.cpp -o taskwithdeps.bc -lstdc++
clang++: warning: -Z-reserved-lib-stdc++: 'linker' input unused [-Wunused-command-line-argument]
llvm-dis taskwithdeps.bc
opt -load-pass-plugin=/home/randres/projects/carts/.install/carts/lib/OMPTransform.so \
	-debug-only=omp-visitor,omp-transform,arts,carts,arts-ir-builder\
	-passes="omp-transform" -ast-file=taskwithdeps.ast taskwithdeps.bc -o taskwithdeps_arts_ir.bc

-------------------------------------------------
[omp-transform] Running OmpTransformPass on Module: taskwithdeps.bc

-------------------------------------------------
[omp-visitor] Running OMPVisitor
Found OpenMP Directive (OMPParallelDirective) at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:31:3
Found OpenMP Directive (OMPSingleDirective) at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:32:3
  (Unhandled directive type)
Found OpenMP Directive (OMPTaskDirective) at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:40:7
Found OpenMP Directive (OMPTaskDirective) at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:42:7
Found OpenMP Directive (OMPTaskDirective) at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:44:7
- - - - - - - - - - - - - - - - - - - 
Printing OpenMP Directive Hierarchy...
Directive: parallel
  Location: /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:31:3
  Instruction:   ret void, !dbg !279
  Directive: task
    Location: /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:40:7
    Instruction:   %7 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @3, i32 %0, ptr nonnull %2, i32 1, ptr nonnull %.dep.arr.addr.i, i32 0, ptr null), !dbg !294, !noalias !276
    Dependencies: No
  Directive: task
    Location: /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:42:7
    Instruction:   %13 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @5, i32 %0, ptr nonnull %8, i32 1, ptr nonnull %.dep.arr.addr2.i, i32 0, ptr null), !dbg !308, !noalias !276
    Dependencies: No
  Directive: task
    Location: /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:44:7
    Instruction:   %19 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @7, i32 %0, ptr nonnull %14, i32 1, ptr nonnull %.dep.arr.addr5.i, i32 0, ptr null), !dbg !309, !noalias !276
-------------------------------------------------
[omp-transform] OmpTransformPass has finished

; ModuleID = 'taskwithdeps.bc'
source_filename = "taskwithdeps.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }
%struct.kmp_depend_info = type { i64, i64, i8 }

@0 = private unnamed_addr constant [30 x i8] c";taskwithdeps.cpp;main;32;3;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 29, ptr @0 }, align 8
@2 = private unnamed_addr constant [30 x i8] c";taskwithdeps.cpp;main;40;7;;\00", align 1
@3 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 29, ptr @2 }, align 8
@4 = private unnamed_addr constant [30 x i8] c";taskwithdeps.cpp;main;42;7;;\00", align 1
@5 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 29, ptr @4 }, align 8
@6 = private unnamed_addr constant [30 x i8] c";taskwithdeps.cpp;main;44;7;;\00", align 1
@7 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 29, ptr @6 }, align 8
@8 = private unnamed_addr constant %struct.ident_t { i32 0, i32 322, i32 0, i32 29, ptr @0 }, align 8
@9 = private unnamed_addr constant [30 x i8] c";taskwithdeps.cpp;main;31;3;;\00", align 1
@10 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 29, ptr @9 }, align 8

; Function Attrs: mustprogress nofree noinline norecurse nosync nounwind willreturn memory(argmem: readwrite) uwtable
define dso_local void @_Z16long_computationRi(ptr nocapture noundef nonnull align 4 dereferenceable(4) %x) local_unnamed_addr #0 !dbg !261 {
entry:
  tail call void @llvm.dbg.value(metadata ptr %x, metadata !266, metadata !DIExpression()), !dbg !268
  tail call void @llvm.dbg.value(metadata i32 0, metadata !267, metadata !DIExpression()), !dbg !268
  %x.promoted = load i32, ptr %x, align 4, !tbaa !269
  tail call void @llvm.dbg.value(metadata i32 0, metadata !267, metadata !DIExpression()), !dbg !268
  tail call void @llvm.dbg.value(metadata i32 undef, metadata !267, metadata !DIExpression()), !dbg !268
  %0 = add i32 %x.promoted, 1783293664, !dbg !273
  store i32 %0, ptr %x, align 4, !dbg !275, !tbaa !269
  ret void, !dbg !278
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #2

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #2

; Function Attrs: mustprogress nofree noinline norecurse nosync nounwind willreturn memory(argmem: readwrite) uwtable
define dso_local void @_Z17short_computationRi(ptr nocapture noundef nonnull align 4 dereferenceable(4) %x) local_unnamed_addr #0 !dbg !279 {
entry:
  tail call void @llvm.dbg.value(metadata ptr %x, metadata !281, metadata !DIExpression()), !dbg !283
  tail call void @llvm.dbg.value(metadata i32 0, metadata !282, metadata !DIExpression()), !dbg !283
  %x.promoted = load i32, ptr %x, align 4, !tbaa !269
  tail call void @llvm.dbg.value(metadata i32 0, metadata !282, metadata !DIExpression()), !dbg !283
  tail call void @llvm.dbg.value(metadata i32 undef, metadata !282, metadata !DIExpression()), !dbg !283
  %0 = add i32 %x.promoted, 499500, !dbg !284
  store i32 %0, ptr %x, align 4, !dbg !286, !tbaa !269
  ret void, !dbg !289
}

; Function Attrs: mustprogress norecurse nounwind uwtable
define dso_local noundef i32 @main() local_unnamed_addr #3 !dbg !290 {
entry:
  %x = alloca i32, align 4, !DIAssignID !295
  call void @llvm.dbg.assign(metadata i1 undef, metadata !292, metadata !DIExpression(), metadata !295, metadata ptr %x, metadata !DIExpression()), !dbg !296
  %y = alloca i32, align 4, !DIAssignID !297
  call void @llvm.dbg.assign(metadata i1 undef, metadata !293, metadata !DIExpression(), metadata !297, metadata ptr %y, metadata !DIExpression()), !dbg !296
  %res = alloca i32, align 4, !DIAssignID !298
  call void @llvm.dbg.assign(metadata i1 undef, metadata !294, metadata !DIExpression(), metadata !298, metadata ptr %res, metadata !DIExpression()), !dbg !296
  %call = tail call i64 @time(ptr noundef null) #7, !dbg !299
  %conv = trunc i64 %call to i32, !dbg !299
  tail call void @srand(i32 noundef %conv) #7, !dbg !300
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %x) #7, !dbg !301
  %call1 = tail call i32 @rand() #7, !dbg !302
  store i32 %call1, ptr %x, align 4, !dbg !303, !tbaa !269, !DIAssignID !304
  call void @llvm.dbg.assign(metadata i32 %call1, metadata !292, metadata !DIExpression(), metadata !304, metadata ptr %x, metadata !DIExpression()), !dbg !296
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %y) #7, !dbg !301
  %call2 = tail call i32 @rand() #7, !dbg !305
  store i32 %call2, ptr %y, align 4, !dbg !306, !tbaa !269, !DIAssignID !307
  call void @llvm.dbg.assign(metadata i32 %call2, metadata !293, metadata !DIExpression(), metadata !307, metadata ptr %y, metadata !DIExpression()), !dbg !296
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %res) #7, !dbg !301
  store i32 0, ptr %res, align 4, !dbg !308, !tbaa !269, !DIAssignID !309
  call void @llvm.dbg.assign(metadata i32 0, metadata !294, metadata !DIExpression(), metadata !309, metadata ptr %res, metadata !DIExpression()), !dbg !296
  call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr nonnull @10, i32 3, ptr nonnull @main.omp_outlined, ptr nonnull %res, ptr nonnull %x, ptr nonnull %y), !dbg !310
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %res) #7, !dbg !311
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %y) #7, !dbg !311
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %x) #7, !dbg !311
  ret i32 0, !dbg !312
}

; Function Attrs: nounwind
declare !dbg !131 void @srand(i32 noundef) local_unnamed_addr #4

; Function Attrs: nounwind
declare !dbg !313 i64 @time(ptr noundef) local_unnamed_addr #4

; Function Attrs: nounwind
declare !dbg !123 i32 @rand() local_unnamed_addr #4

; Function Attrs: convergent nounwind
declare i32 @__kmpc_single(ptr, i32) local_unnamed_addr #5

; Function Attrs: convergent nounwind
declare void @__kmpc_end_single(ptr, i32) local_unnamed_addr #5

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn uwtable
define internal noundef i32 @.omp_task_entry.(i32 %0, ptr noalias nocapture noundef readonly %1) #6 !dbg !322 {
entry:
  tail call void @llvm.dbg.value(metadata i32 poison, metadata !326, metadata !DIExpression()), !dbg !331
  tail call void @llvm.dbg.value(metadata ptr %1, metadata !327, metadata !DIExpression()), !dbg !331
  %2 = load ptr, ptr %1, align 8, !dbg !332, !tbaa !333
  tail call void @llvm.experimental.noalias.scope.decl(metadata !337), !dbg !332
  tail call void @llvm.dbg.value(metadata i32 poison, metadata !340, metadata !DIExpression()), !dbg !367
  tail call void @llvm.dbg.value(metadata ptr %1, metadata !361, metadata !DIExpression(DW_OP_plus_uconst, 16, DW_OP_stack_value)), !dbg !367
  tail call void @llvm.dbg.value(metadata ptr null, metadata !362, metadata !DIExpression()), !dbg !367
  tail call void @llvm.dbg.value(metadata ptr null, metadata !363, metadata !DIExpression()), !dbg !367
  tail call void @llvm.dbg.value(metadata ptr %1, metadata !364, metadata !DIExpression()), !dbg !367
  tail call void @llvm.dbg.value(metadata ptr %2, metadata !365, metadata !DIExpression()), !dbg !367
  call void @llvm.dbg.declare(metadata ptr %2, metadata !366, metadata !DIExpression(DW_OP_deref)), !dbg !369
  %3 = load ptr, ptr %2, align 8, !dbg !370, !tbaa !371, !alias.scope !337
  store i32 0, ptr %3, align 4, !dbg !373, !tbaa !269, !noalias !337
  ret i32 0, !dbg !332
}

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #7

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task_with_deps(ptr, i32, ptr, i32, ptr, i32, ptr) local_unnamed_addr #7

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn uwtable
define internal noundef i32 @.omp_task_entry..2(i32 %0, ptr noalias nocapture noundef readonly %1) #6 !dbg !374 {
entry:
  tail call void @llvm.dbg.value(metadata i32 poison, metadata !376, metadata !DIExpression()), !dbg !378
  tail call void @llvm.dbg.value(metadata ptr %1, metadata !377, metadata !DIExpression()), !dbg !378
  %2 = load ptr, ptr %1, align 8, !dbg !379, !tbaa !333
  tail call void @llvm.experimental.noalias.scope.decl(metadata !380), !dbg !379
  call void @llvm.dbg.value(metadata i32 poison, metadata !383, metadata !DIExpression()), !dbg !398
  call void @llvm.dbg.value(metadata ptr %1, metadata !392, metadata !DIExpression(DW_OP_plus_uconst, 16, DW_OP_stack_value)), !dbg !398
  call void @llvm.dbg.value(metadata ptr null, metadata !393, metadata !DIExpression()), !dbg !398
  call void @llvm.dbg.value(metadata ptr null, metadata !394, metadata !DIExpression()), !dbg !398
  call void @llvm.dbg.value(metadata ptr %1, metadata !395, metadata !DIExpression()), !dbg !398
  call void @llvm.dbg.value(metadata ptr %2, metadata !396, metadata !DIExpression()), !dbg !398
  call void @llvm.dbg.declare(metadata ptr %2, metadata !397, metadata !DIExpression(DW_OP_deref)), !dbg !400
  %3 = load ptr, ptr %2, align 8, !dbg !401, !tbaa !402, !alias.scope !380
  tail call void @_Z16long_computationRi(ptr noundef nonnull align 4 dereferenceable(4) %3), !dbg !404, !noalias !380
  ret i32 0, !dbg !379
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn uwtable
define internal noundef i32 @.omp_task_entry..4(i32 %0, ptr noalias nocapture noundef readonly %1) #6 !dbg !405 {
entry:
  tail call void @llvm.dbg.value(metadata i32 poison, metadata !407, metadata !DIExpression()), !dbg !409
  tail call void @llvm.dbg.value(metadata ptr %1, metadata !408, metadata !DIExpression()), !dbg !409
  %2 = load ptr, ptr %1, align 8, !dbg !410, !tbaa !333
  tail call void @llvm.experimental.noalias.scope.decl(metadata !411), !dbg !410
  call void @llvm.dbg.value(metadata i32 poison, metadata !414, metadata !DIExpression()), !dbg !429
  call void @llvm.dbg.value(metadata ptr %1, metadata !423, metadata !DIExpression(DW_OP_plus_uconst, 16, DW_OP_stack_value)), !dbg !429
  call void @llvm.dbg.value(metadata ptr null, metadata !424, metadata !DIExpression()), !dbg !429
  call void @llvm.dbg.value(metadata ptr null, metadata !425, metadata !DIExpression()), !dbg !429
  call void @llvm.dbg.value(metadata ptr %1, metadata !426, metadata !DIExpression()), !dbg !429
  call void @llvm.dbg.value(metadata ptr %2, metadata !427, metadata !DIExpression()), !dbg !429
  call void @llvm.dbg.declare(metadata ptr %2, metadata !428, metadata !DIExpression(DW_OP_deref)), !dbg !431
  %3 = load ptr, ptr %2, align 8, !dbg !432, !tbaa !433, !alias.scope !411
  tail call void @_Z17short_computationRi(ptr noundef nonnull align 4 dereferenceable(4) %3), !dbg !435, !noalias !411
  ret i32 0, !dbg !410
}

; Function Attrs: convergent nounwind
declare void @__kmpc_barrier(ptr, i32) local_unnamed_addr #5

; Function Attrs: alwaysinline norecurse nounwind uwtable
define internal void @main.omp_outlined(ptr noalias nocapture noundef readonly %.global_tid., ptr noalias nocapture readnone %.bound_tid., ptr noundef nonnull align 4 dereferenceable(4) %res, ptr noundef nonnull align 4 dereferenceable(4) %x, ptr noundef nonnull align 4 dereferenceable(4) %y) #8 !dbg !436 {
entry:
  %.dep.arr.addr.i = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr2.i = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr5.i = alloca [1 x %struct.kmp_depend_info], align 8
  tail call void @llvm.dbg.value(metadata ptr %.global_tid., metadata !440, metadata !DIExpression()), !dbg !445
  tail call void @llvm.dbg.value(metadata ptr poison, metadata !441, metadata !DIExpression()), !dbg !445
  tail call void @llvm.dbg.value(metadata ptr %res, metadata !442, metadata !DIExpression()), !dbg !445
  tail call void @llvm.dbg.value(metadata ptr %x, metadata !443, metadata !DIExpression()), !dbg !445
  tail call void @llvm.dbg.value(metadata ptr %y, metadata !444, metadata !DIExpression()), !dbg !445
  tail call void @llvm.experimental.noalias.scope.decl(metadata !446), !dbg !449
  call void @llvm.lifetime.start.p0(i64 24, ptr nonnull %.dep.arr.addr.i), !dbg !450
  call void @llvm.lifetime.start.p0(i64 24, ptr nonnull %.dep.arr.addr2.i), !dbg !450
  call void @llvm.lifetime.start.p0(i64 24, ptr nonnull %.dep.arr.addr5.i), !dbg !450
  call void @llvm.dbg.value(metadata ptr %.global_tid., metadata !453, metadata !DIExpression()), !dbg !459
  call void @llvm.dbg.value(metadata ptr poison, metadata !454, metadata !DIExpression()), !dbg !459
  call void @llvm.dbg.value(metadata ptr %res, metadata !455, metadata !DIExpression()), !dbg !459
  call void @llvm.dbg.value(metadata ptr %x, metadata !456, metadata !DIExpression()), !dbg !459
  call void @llvm.dbg.value(metadata ptr %y, metadata !457, metadata !DIExpression()), !dbg !459
  %0 = load i32, ptr %.global_tid., align 4, !dbg !450, !tbaa !269, !alias.scope !446
  %1 = tail call i32 @__kmpc_single(ptr nonnull @1, i32 %0), !dbg !450, !noalias !446
  %.not.i = icmp eq i32 %1, 0, !dbg !450
  br i1 %.not.i, label %main.omp_outlined_debug__.exit, label %omp_if.then.i, !dbg !450

omp_if.then.i:                                    ; preds = %entry
  %2 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @3, i32 %0, i32 1, i64 40, i64 8, ptr nonnull @.omp_task_entry.), !dbg !460, !noalias !446
  %3 = load ptr, ptr %2, align 8, !dbg !460, !tbaa !333, !noalias !446
  store ptr %res, ptr %3, align 8, !dbg !460, !tbaa.struct !463, !noalias !446
  %4 = ptrtoint ptr %res to i64, !dbg !460
  store i64 %4, ptr %.dep.arr.addr.i, align 8, !dbg !460, !tbaa !465, !noalias !446
  %5 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr.i, i64 0, i32 1, !dbg !460
  store i64 4, ptr %5, align 8, !dbg !460, !tbaa !468, !noalias !446
  %6 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr.i, i64 0, i32 2, !dbg !460
  store i8 3, ptr %6, align 8, !dbg !460, !tbaa !469, !noalias !446
  %7 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @3, i32 %0, ptr nonnull %2, i32 1, ptr nonnull %.dep.arr.addr.i, i32 0, ptr null), !dbg !460, !noalias !446
  %8 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @5, i32 %0, i32 1, i64 40, i64 8, ptr nonnull @.omp_task_entry..2), !dbg !470, !noalias !446
  %9 = load ptr, ptr %8, align 8, !dbg !470, !tbaa !333, !noalias !446
  store ptr %x, ptr %9, align 8, !dbg !470, !tbaa.struct !463, !noalias !446
  %10 = ptrtoint ptr %x to i64, !dbg !470
  store i64 %10, ptr %.dep.arr.addr2.i, align 8, !dbg !470, !tbaa !465, !noalias !446
  %11 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr2.i, i64 0, i32 1, !dbg !470
  store i64 4, ptr %11, align 8, !dbg !470, !tbaa !468, !noalias !446
  %12 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr2.i, i64 0, i32 2, !dbg !470
  store i8 3, ptr %12, align 8, !dbg !470, !tbaa !469, !noalias !446
  %13 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @5, i32 %0, ptr nonnull %8, i32 1, ptr nonnull %.dep.arr.addr2.i, i32 0, ptr null), !dbg !470, !noalias !446
  %14 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @7, i32 %0, i32 1, i64 40, i64 8, ptr nonnull @.omp_task_entry..4), !dbg !471, !noalias !446
  %15 = load ptr, ptr %14, align 8, !dbg !471, !tbaa !333, !noalias !446
  store ptr %y, ptr %15, align 8, !dbg !471, !tbaa.struct !463, !noalias !446
  %16 = ptrtoint ptr %y to i64, !dbg !471
  store i64 %16, ptr %.dep.arr.addr5.i, align 8, !dbg !471, !tbaa !465, !noalias !446
  %17 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr5.i, i64 0, i32 1, !dbg !471
  store i64 4, ptr %17, align 8, !dbg !471, !tbaa !468, !noalias !446
  %18 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr5.i, i64 0, i32 2, !dbg !471
  store i8 3, ptr %18, align 8, !dbg !471, !tbaa !469, !noalias !446
  %19 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @7, i32 %0, ptr nonnull %14, i32 1, ptr nonnull %.dep.arr.addr5.i, i32 0, ptr null), !dbg !471, !noalias !446
  call void @__kmpc_end_single(ptr nonnull @1, i32 %0), !dbg !472, !noalias !446
  br label %main.omp_outlined_debug__.exit, !dbg !472

main.omp_outlined_debug__.exit:                   ; preds = %entry, %omp_if.then.i
  call void @__kmpc_barrier(ptr nonnull @8, i32 %0), !dbg !473, !noalias !446
  call void @llvm.lifetime.end.p0(i64 24, ptr nonnull %.dep.arr.addr.i), !dbg !474
  call void @llvm.lifetime.end.p0(i64 24, ptr nonnull %.dep.arr.addr2.i), !dbg !474
  call void @llvm.lifetime.end.p0(i64 24, ptr nonnull %.dep.arr.addr5.i), !dbg !474
  ret void, !dbg !449
}

; Function Attrs: nounwind
declare !callback !475 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #7

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata, metadata) #1

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.value(metadata, metadata, metadata) #1

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #9

attributes #0 = { mustprogress nofree noinline norecurse nosync nounwind willreturn memory(argmem: readwrite) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
attributes #2 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #3 = { mustprogress norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { convergent nounwind }
attributes #6 = { mustprogress nofree norecurse nosync nounwind willreturn uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #7 = { nounwind }
attributes #8 = { alwaysinline norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #9 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!252, !253, !254, !255, !256, !257, !258, !259}
!llvm.ident = !{!260}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang version 18.1.8", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, imports: !2, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "taskwithdeps.cpp", directory: "/home/randres/projects/carts/examples/taskwithdeps", checksumkind: CSK_MD5, checksum: "56ac19435574785abbfd0d1e47b1efdd")
!2 = !{!3, !11, !15, !22, !26, !34, !39, !41, !50, !54, !58, !69, !71, !75, !79, !83, !88, !92, !96, !100, !104, !112, !116, !120, !122, !126, !130, !135, !141, !145, !149, !151, !159, !163, !171, !173, !177, !181, !185, !189, !194, !199, !204, !205, !206, !207, !209, !210, !211, !212, !213, !214, !215, !217, !218, !219, !220, !221, !222, !223, !228, !229, !230, !231, !232, !233, !234, !235, !236, !237, !238, !239, !240, !241, !242, !243, !244, !245, !246, !247, !248, !249, !250, !251}
!3 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !5, file: !10, line: 52)
!4 = !DINamespace(name: "std", scope: null)
!5 = !DISubprogram(name: "abs", scope: !6, file: !6, line: 848, type: !7, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!6 = !DIFile(filename: "/usr/include/stdlib.h", directory: "", checksumkind: CSK_MD5, checksum: "02258fad21adf111bb9df9825e61954a")
!7 = !DISubroutineType(types: !8)
!8 = !{!9, !9}
!9 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!10 = !DIFile(filename: "/usr/lib/gcc/x86_64-linux-gnu/11/../../../../include/c++/11/bits/std_abs.h", directory: "")
!11 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !12, file: !14, line: 127)
!12 = !DIDerivedType(tag: DW_TAG_typedef, name: "div_t", file: !6, line: 63, baseType: !13)
!13 = distinct !DICompositeType(tag: DW_TAG_structure_type, file: !6, line: 59, size: 64, flags: DIFlagFwdDecl, identifier: "_ZTS5div_t")
!14 = !DIFile(filename: "/usr/lib/gcc/x86_64-linux-gnu/11/../../../../include/c++/11/cstdlib", directory: "")
!15 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !16, file: !14, line: 128)
!16 = !DIDerivedType(tag: DW_TAG_typedef, name: "ldiv_t", file: !6, line: 71, baseType: !17)
!17 = distinct !DICompositeType(tag: DW_TAG_structure_type, file: !6, line: 67, size: 128, flags: DIFlagTypePassByValue, elements: !18, identifier: "_ZTS6ldiv_t")
!18 = !{!19, !21}
!19 = !DIDerivedType(tag: DW_TAG_member, name: "quot", scope: !17, file: !6, line: 69, baseType: !20, size: 64)
!20 = !DIBasicType(name: "long", size: 64, encoding: DW_ATE_signed)
!21 = !DIDerivedType(tag: DW_TAG_member, name: "rem", scope: !17, file: !6, line: 70, baseType: !20, size: 64, offset: 64)
!22 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !23, file: !14, line: 130)
!23 = !DISubprogram(name: "abort", scope: !6, file: !6, line: 598, type: !24, flags: DIFlagPrototyped | DIFlagNoReturn, spFlags: DISPFlagOptimized)
!24 = !DISubroutineType(types: !25)
!25 = !{null}
!26 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !27, file: !14, line: 132)
!27 = !DISubprogram(name: "aligned_alloc", scope: !6, file: !6, line: 592, type: !28, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!28 = !DISubroutineType(types: !29)
!29 = !{!30, !31, !31}
!30 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64)
!31 = !DIDerivedType(tag: DW_TAG_typedef, name: "size_t", file: !32, line: 18, baseType: !33)
!32 = !DIFile(filename: ".install/llvm/lib/clang/18/include/__stddef_size_t.h", directory: "/home/randres/projects/carts", checksumkind: CSK_MD5, checksum: "2c44e821a2b1951cde2eb0fb2e656867")
!33 = !DIBasicType(name: "unsigned long", size: 64, encoding: DW_ATE_unsigned)
!34 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !35, file: !14, line: 134)
!35 = !DISubprogram(name: "atexit", scope: !6, file: !6, line: 602, type: !36, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!36 = !DISubroutineType(types: !37)
!37 = !{!9, !38}
!38 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !24, size: 64)
!39 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !40, file: !14, line: 137)
!40 = !DISubprogram(name: "at_quick_exit", scope: !6, file: !6, line: 607, type: !36, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!41 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !42, file: !14, line: 140)
!42 = !DISubprogram(name: "atof", scope: !43, file: !43, line: 25, type: !44, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!43 = !DIFile(filename: "/usr/include/x86_64-linux-gnu/bits/stdlib-float.h", directory: "", checksumkind: CSK_MD5, checksum: "adfe1626ff4efc68ac58c367ff5f206b")
!44 = !DISubroutineType(types: !45)
!45 = !{!46, !47}
!46 = !DIBasicType(name: "double", size: 64, encoding: DW_ATE_float)
!47 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !48, size: 64)
!48 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !49)
!49 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!50 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !51, file: !14, line: 141)
!51 = !DISubprogram(name: "atoi", scope: !6, file: !6, line: 362, type: !52, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!52 = !DISubroutineType(types: !53)
!53 = !{!9, !47}
!54 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !55, file: !14, line: 142)
!55 = !DISubprogram(name: "atol", scope: !6, file: !6, line: 367, type: !56, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!56 = !DISubroutineType(types: !57)
!57 = !{!20, !47}
!58 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !59, file: !14, line: 143)
!59 = !DISubprogram(name: "bsearch", scope: !60, file: !60, line: 20, type: !61, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!60 = !DIFile(filename: "/usr/include/x86_64-linux-gnu/bits/stdlib-bsearch.h", directory: "", checksumkind: CSK_MD5, checksum: "724ededa330cc3e0cbd34c5b4030a6f6")
!61 = !DISubroutineType(types: !62)
!62 = !{!30, !63, !63, !31, !31, !65}
!63 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !64, size: 64)
!64 = !DIDerivedType(tag: DW_TAG_const_type, baseType: null)
!65 = !DIDerivedType(tag: DW_TAG_typedef, name: "__compar_fn_t", file: !6, line: 816, baseType: !66)
!66 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !67, size: 64)
!67 = !DISubroutineType(types: !68)
!68 = !{!9, !63, !63}
!69 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !70, file: !14, line: 144)
!70 = !DISubprogram(name: "calloc", scope: !6, file: !6, line: 543, type: !28, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!71 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !72, file: !14, line: 145)
!72 = !DISubprogram(name: "div", scope: !6, file: !6, line: 860, type: !73, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!73 = !DISubroutineType(types: !74)
!74 = !{!12, !9, !9}
!75 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !76, file: !14, line: 146)
!76 = !DISubprogram(name: "exit", scope: !6, file: !6, line: 624, type: !77, flags: DIFlagPrototyped | DIFlagNoReturn, spFlags: DISPFlagOptimized)
!77 = !DISubroutineType(types: !78)
!78 = !{null, !9}
!79 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !80, file: !14, line: 147)
!80 = !DISubprogram(name: "free", scope: !6, file: !6, line: 555, type: !81, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!81 = !DISubroutineType(types: !82)
!82 = !{null, !30}
!83 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !84, file: !14, line: 148)
!84 = !DISubprogram(name: "getenv", scope: !6, file: !6, line: 641, type: !85, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!85 = !DISubroutineType(types: !86)
!86 = !{!87, !47}
!87 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !49, size: 64)
!88 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !89, file: !14, line: 149)
!89 = !DISubprogram(name: "labs", scope: !6, file: !6, line: 849, type: !90, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!90 = !DISubroutineType(types: !91)
!91 = !{!20, !20}
!92 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !93, file: !14, line: 150)
!93 = !DISubprogram(name: "ldiv", scope: !6, file: !6, line: 862, type: !94, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!94 = !DISubroutineType(types: !95)
!95 = !{!16, !20, !20}
!96 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !97, file: !14, line: 151)
!97 = !DISubprogram(name: "malloc", scope: !6, file: !6, line: 540, type: !98, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!98 = !DISubroutineType(types: !99)
!99 = !{!30, !31}
!100 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !101, file: !14, line: 153)
!101 = !DISubprogram(name: "mblen", scope: !6, file: !6, line: 930, type: !102, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!102 = !DISubroutineType(types: !103)
!103 = !{!9, !47, !31}
!104 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !105, file: !14, line: 154)
!105 = !DISubprogram(name: "mbstowcs", scope: !6, file: !6, line: 941, type: !106, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!106 = !DISubroutineType(types: !107)
!107 = !{!31, !108, !111, !31}
!108 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !109)
!109 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !110, size: 64)
!110 = !DIBasicType(name: "wchar_t", size: 32, encoding: DW_ATE_signed)
!111 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !47)
!112 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !113, file: !14, line: 155)
!113 = !DISubprogram(name: "mbtowc", scope: !6, file: !6, line: 933, type: !114, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!114 = !DISubroutineType(types: !115)
!115 = !{!9, !108, !111, !31}
!116 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !117, file: !14, line: 157)
!117 = !DISubprogram(name: "qsort", scope: !6, file: !6, line: 838, type: !118, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!118 = !DISubroutineType(types: !119)
!119 = !{null, !30, !31, !31, !65}
!120 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !121, file: !14, line: 160)
!121 = !DISubprogram(name: "quick_exit", scope: !6, file: !6, line: 630, type: !77, flags: DIFlagPrototyped | DIFlagNoReturn, spFlags: DISPFlagOptimized)
!122 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !123, file: !14, line: 163)
!123 = !DISubprogram(name: "rand", scope: !6, file: !6, line: 454, type: !124, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!124 = !DISubroutineType(types: !125)
!125 = !{!9}
!126 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !127, file: !14, line: 164)
!127 = !DISubprogram(name: "realloc", scope: !6, file: !6, line: 551, type: !128, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!128 = !DISubroutineType(types: !129)
!129 = !{!30, !30, !31}
!130 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !131, file: !14, line: 165)
!131 = !DISubprogram(name: "srand", scope: !6, file: !6, line: 456, type: !132, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!132 = !DISubroutineType(types: !133)
!133 = !{null, !134}
!134 = !DIBasicType(name: "unsigned int", size: 32, encoding: DW_ATE_unsigned)
!135 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !136, file: !14, line: 166)
!136 = !DISubprogram(name: "strtod", scope: !6, file: !6, line: 118, type: !137, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!137 = !DISubroutineType(types: !138)
!138 = !{!46, !111, !139}
!139 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !140)
!140 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !87, size: 64)
!141 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !142, file: !14, line: 167)
!142 = !DISubprogram(name: "strtol", scope: !6, file: !6, line: 177, type: !143, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!143 = !DISubroutineType(types: !144)
!144 = !{!20, !111, !139, !9}
!145 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !146, file: !14, line: 168)
!146 = !DISubprogram(name: "strtoul", scope: !6, file: !6, line: 181, type: !147, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!147 = !DISubroutineType(types: !148)
!148 = !{!33, !111, !139, !9}
!149 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !150, file: !14, line: 169)
!150 = !DISubprogram(name: "system", scope: !6, file: !6, line: 791, type: !52, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!151 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !152, file: !14, line: 171)
!152 = !DISubprogram(name: "wcstombs", scope: !6, file: !6, line: 945, type: !153, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!153 = !DISubroutineType(types: !154)
!154 = !{!31, !155, !156, !31}
!155 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !87)
!156 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !157)
!157 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !158, size: 64)
!158 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !110)
!159 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !160, file: !14, line: 172)
!160 = !DISubprogram(name: "wctomb", scope: !6, file: !6, line: 937, type: !161, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!161 = !DISubroutineType(types: !162)
!162 = !{!9, !87, !110}
!163 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !164, entity: !165, file: !14, line: 200)
!164 = !DINamespace(name: "__gnu_cxx", scope: null)
!165 = !DIDerivedType(tag: DW_TAG_typedef, name: "lldiv_t", file: !6, line: 81, baseType: !166)
!166 = distinct !DICompositeType(tag: DW_TAG_structure_type, file: !6, line: 77, size: 128, flags: DIFlagTypePassByValue, elements: !167, identifier: "_ZTS7lldiv_t")
!167 = !{!168, !170}
!168 = !DIDerivedType(tag: DW_TAG_member, name: "quot", scope: !166, file: !6, line: 79, baseType: !169, size: 64)
!169 = !DIBasicType(name: "long long", size: 64, encoding: DW_ATE_signed)
!170 = !DIDerivedType(tag: DW_TAG_member, name: "rem", scope: !166, file: !6, line: 80, baseType: !169, size: 64, offset: 64)
!171 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !164, entity: !172, file: !14, line: 206)
!172 = !DISubprogram(name: "_Exit", scope: !6, file: !6, line: 636, type: !77, flags: DIFlagPrototyped | DIFlagNoReturn, spFlags: DISPFlagOptimized)
!173 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !164, entity: !174, file: !14, line: 210)
!174 = !DISubprogram(name: "llabs", scope: !6, file: !6, line: 852, type: !175, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!175 = !DISubroutineType(types: !176)
!176 = !{!169, !169}
!177 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !164, entity: !178, file: !14, line: 216)
!178 = !DISubprogram(name: "lldiv", scope: !6, file: !6, line: 866, type: !179, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!179 = !DISubroutineType(types: !180)
!180 = !{!165, !169, !169}
!181 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !164, entity: !182, file: !14, line: 227)
!182 = !DISubprogram(name: "atoll", scope: !6, file: !6, line: 374, type: !183, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!183 = !DISubroutineType(types: !184)
!184 = !{!169, !47}
!185 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !164, entity: !186, file: !14, line: 228)
!186 = !DISubprogram(name: "strtoll", scope: !6, file: !6, line: 201, type: !187, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!187 = !DISubroutineType(types: !188)
!188 = !{!169, !111, !139, !9}
!189 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !164, entity: !190, file: !14, line: 229)
!190 = !DISubprogram(name: "strtoull", scope: !6, file: !6, line: 206, type: !191, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!191 = !DISubroutineType(types: !192)
!192 = !{!193, !111, !139, !9}
!193 = !DIBasicType(name: "unsigned long long", size: 64, encoding: DW_ATE_unsigned)
!194 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !164, entity: !195, file: !14, line: 231)
!195 = !DISubprogram(name: "strtof", scope: !6, file: !6, line: 124, type: !196, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!196 = !DISubroutineType(types: !197)
!197 = !{!198, !111, !139}
!198 = !DIBasicType(name: "float", size: 32, encoding: DW_ATE_float)
!199 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !164, entity: !200, file: !14, line: 232)
!200 = !DISubprogram(name: "strtold", scope: !6, file: !6, line: 127, type: !201, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!201 = !DISubroutineType(types: !202)
!202 = !{!203, !111, !139}
!203 = !DIBasicType(name: "long double", size: 128, encoding: DW_ATE_float)
!204 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !165, file: !14, line: 240)
!205 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !172, file: !14, line: 242)
!206 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !174, file: !14, line: 244)
!207 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !208, file: !14, line: 245)
!208 = !DISubprogram(name: "div", linkageName: "_ZN9__gnu_cxx3divExx", scope: !164, file: !14, line: 213, type: !179, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!209 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !178, file: !14, line: 246)
!210 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !182, file: !14, line: 248)
!211 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !195, file: !14, line: 249)
!212 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !186, file: !14, line: 250)
!213 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !190, file: !14, line: 251)
!214 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !200, file: !14, line: 252)
!215 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !23, file: !216, line: 38)
!216 = !DIFile(filename: "/usr/lib/gcc/x86_64-linux-gnu/11/../../../../include/c++/11/stdlib.h", directory: "", checksumkind: CSK_MD5, checksum: "0f5b773a303c24013fb112082e6d18a5")
!217 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !35, file: !216, line: 39)
!218 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !76, file: !216, line: 40)
!219 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !40, file: !216, line: 43)
!220 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !121, file: !216, line: 46)
!221 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !12, file: !216, line: 51)
!222 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !16, file: !216, line: 52)
!223 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !224, file: !216, line: 54)
!224 = !DISubprogram(name: "abs", linkageName: "_ZSt3absg", scope: !4, file: !10, line: 103, type: !225, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!225 = !DISubroutineType(types: !226)
!226 = !{!227, !227}
!227 = !DIBasicType(name: "__float128", size: 128, encoding: DW_ATE_float)
!228 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !42, file: !216, line: 55)
!229 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !51, file: !216, line: 56)
!230 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !55, file: !216, line: 57)
!231 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !59, file: !216, line: 58)
!232 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !70, file: !216, line: 59)
!233 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !208, file: !216, line: 60)
!234 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !80, file: !216, line: 61)
!235 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !84, file: !216, line: 62)
!236 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !89, file: !216, line: 63)
!237 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !93, file: !216, line: 64)
!238 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !97, file: !216, line: 65)
!239 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !101, file: !216, line: 67)
!240 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !105, file: !216, line: 68)
!241 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !113, file: !216, line: 69)
!242 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !117, file: !216, line: 71)
!243 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !123, file: !216, line: 72)
!244 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !127, file: !216, line: 73)
!245 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !131, file: !216, line: 74)
!246 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !136, file: !216, line: 75)
!247 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !142, file: !216, line: 76)
!248 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !146, file: !216, line: 77)
!249 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !150, file: !216, line: 78)
!250 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !152, file: !216, line: 80)
!251 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !160, file: !216, line: 81)
!252 = !{i32 7, !"Dwarf Version", i32 5}
!253 = !{i32 2, !"Debug Info Version", i32 3}
!254 = !{i32 1, !"wchar_size", i32 4}
!255 = !{i32 7, !"openmp", i32 51}
!256 = !{i32 8, !"PIC Level", i32 2}
!257 = !{i32 7, !"PIE Level", i32 2}
!258 = !{i32 7, !"uwtable", i32 2}
!259 = !{i32 7, !"debug-info-assignment-tracking", i1 true}
!260 = !{!"clang version 18.1.8"}
!261 = distinct !DISubprogram(name: "long_computation", linkageName: "_Z16long_computationRi", scope: !1, file: !1, line: 12, type: !262, scopeLine: 12, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !265)
!262 = !DISubroutineType(types: !263)
!263 = !{null, !264}
!264 = !DIDerivedType(tag: DW_TAG_reference_type, baseType: !9, size: 64)
!265 = !{!266, !267}
!266 = !DILocalVariable(name: "x", arg: 1, scope: !261, file: !1, line: 12, type: !264)
!267 = !DILocalVariable(name: "i", scope: !261, file: !1, line: 13, type: !9)
!268 = !DILocation(line: 0, scope: !261)
!269 = !{!270, !270, i64 0}
!270 = !{!"int", !271, i64 0}
!271 = !{!"omnipotent char", !272, i64 0}
!272 = !{!"Simple C++ TBAA"}
!273 = !DILocation(line: 14, column: 3, scope: !274)
!274 = distinct !DILexicalBlock(scope: !261, file: !1, line: 14, column: 3)
!275 = !DILocation(line: 15, column: 7, scope: !276)
!276 = distinct !DILexicalBlock(scope: !277, file: !1, line: 14, column: 33)
!277 = distinct !DILexicalBlock(scope: !274, file: !1, line: 14, column: 3)
!278 = !DILocation(line: 17, column: 1, scope: !261)
!279 = distinct !DISubprogram(name: "short_computation", linkageName: "_Z17short_computationRi", scope: !1, file: !1, line: 20, type: !262, scopeLine: 20, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !280)
!280 = !{!281, !282}
!281 = !DILocalVariable(name: "x", arg: 1, scope: !279, file: !1, line: 20, type: !264)
!282 = !DILocalVariable(name: "i", scope: !279, file: !1, line: 21, type: !9)
!283 = !DILocation(line: 0, scope: !279)
!284 = !DILocation(line: 22, column: 3, scope: !285)
!285 = distinct !DILexicalBlock(scope: !279, file: !1, line: 22, column: 3)
!286 = !DILocation(line: 23, column: 7, scope: !287)
!287 = distinct !DILexicalBlock(scope: !288, file: !1, line: 22, column: 30)
!288 = distinct !DILexicalBlock(scope: !285, file: !1, line: 22, column: 3)
!289 = !DILocation(line: 25, column: 1, scope: !279)
!290 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 28, type: !124, scopeLine: 28, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !291)
!291 = !{!292, !293, !294}
!292 = !DILocalVariable(name: "x", scope: !290, file: !1, line: 30, type: !9)
!293 = !DILocalVariable(name: "y", scope: !290, file: !1, line: 30, type: !9)
!294 = !DILocalVariable(name: "res", scope: !290, file: !1, line: 30, type: !9)
!295 = distinct !DIAssignID()
!296 = !DILocation(line: 0, scope: !290)
!297 = distinct !DIAssignID()
!298 = distinct !DIAssignID()
!299 = !DILocation(line: 29, column: 9, scope: !290)
!300 = !DILocation(line: 29, column: 3, scope: !290)
!301 = !DILocation(line: 30, column: 3, scope: !290)
!302 = !DILocation(line: 30, column: 11, scope: !290)
!303 = !DILocation(line: 30, column: 7, scope: !290)
!304 = distinct !DIAssignID()
!305 = !DILocation(line: 30, column: 23, scope: !290)
!306 = !DILocation(line: 30, column: 19, scope: !290)
!307 = distinct !DIAssignID()
!308 = !DILocation(line: 30, column: 31, scope: !290)
!309 = distinct !DIAssignID()
!310 = !DILocation(line: 31, column: 3, scope: !290)
!311 = !DILocation(line: 57, column: 1, scope: !290)
!312 = !DILocation(line: 56, column: 3, scope: !290)
!313 = !DISubprogram(name: "time", scope: !314, file: !314, line: 76, type: !315, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!314 = !DIFile(filename: "/usr/include/time.h", directory: "", checksumkind: CSK_MD5, checksum: "db37158473a25e1d89b19f8bc6892801")
!315 = !DISubroutineType(types: !316)
!316 = !{!317, !321}
!317 = !DIDerivedType(tag: DW_TAG_typedef, name: "time_t", file: !318, line: 10, baseType: !319)
!318 = !DIFile(filename: "/usr/include/x86_64-linux-gnu/bits/types/time_t.h", directory: "", checksumkind: CSK_MD5, checksum: "5c299a4954617c88bb03645c7864e1b1")
!319 = !DIDerivedType(tag: DW_TAG_typedef, name: "__time_t", file: !320, line: 160, baseType: !20)
!320 = !DIFile(filename: "/usr/include/x86_64-linux-gnu/bits/types.h", directory: "", checksumkind: CSK_MD5, checksum: "d108b5f93a74c50510d7d9bc0ab36df9")
!321 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !317, size: 64)
!322 = distinct !DISubprogram(linkageName: ".omp_task_entry.", scope: !1, file: !1, line: 40, type: !323, scopeLine: 40, flags: DIFlagArtificial | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !325)
!323 = !DISubroutineType(types: !324)
!324 = !{}
!325 = !{!326, !327}
!326 = !DILocalVariable(arg: 1, scope: !322, type: !9, flags: DIFlagArtificial)
!327 = !DILocalVariable(arg: 2, scope: !322, type: !328, flags: DIFlagArtificial)
!328 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !329)
!329 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !330, size: 64)
!330 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "kmp_task_t_with_privates", file: !1, size: 320, flags: DIFlagFwdDecl, identifier: "_ZTS24kmp_task_t_with_privates")
!331 = !DILocation(line: 0, scope: !322)
!332 = !DILocation(line: 40, column: 7, scope: !322)
!333 = !{!334, !336, i64 0}
!334 = !{!"_ZTS24kmp_task_t_with_privates", !335, i64 0}
!335 = !{!"_ZTS10kmp_task_t", !336, i64 0, !336, i64 8, !270, i64 16, !271, i64 24, !271, i64 32}
!336 = !{!"any pointer", !271, i64 0}
!337 = !{!338}
!338 = distinct !{!338, !339, !".omp_outlined.: %__context"}
!339 = distinct !{!339, !".omp_outlined."}
!340 = !DILocalVariable(name: ".global_tid.", arg: 1, scope: !341, type: !344, flags: DIFlagArtificial)
!341 = distinct !DISubprogram(name: ".omp_outlined.", scope: !1, file: !1, type: !342, scopeLine: 41, flags: DIFlagArtificial | DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !360)
!342 = !DISubroutineType(types: !343)
!343 = !{null, !344, !345, !348, !350, !355, !356}
!344 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !9)
!345 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !346)
!346 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !347)
!347 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !344, size: 64)
!348 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !349)
!349 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !30)
!350 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !351)
!351 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !352)
!352 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !353, size: 64)
!353 = !DISubroutineType(types: !354)
!354 = !{null, !348, null}
!355 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !30)
!356 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !357)
!357 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !358)
!358 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !359, size: 64)
!359 = !DICompositeType(tag: DW_TAG_structure_type, file: !1, line: 40, size: 64, flags: DIFlagFwdDecl)
!360 = !{!340, !361, !362, !363, !364, !365, !366}
!361 = !DILocalVariable(name: ".part_id.", arg: 2, scope: !341, type: !345, flags: DIFlagArtificial)
!362 = !DILocalVariable(name: ".privates.", arg: 3, scope: !341, type: !348, flags: DIFlagArtificial)
!363 = !DILocalVariable(name: ".copy_fn.", arg: 4, scope: !341, type: !350, flags: DIFlagArtificial)
!364 = !DILocalVariable(name: ".task_t.", arg: 5, scope: !341, type: !355, flags: DIFlagArtificial)
!365 = !DILocalVariable(name: "__context", arg: 6, scope: !341, type: !356, flags: DIFlagArtificial)
!366 = !DILocalVariable(name: "res", scope: !341, file: !1, line: 30, type: !9)
!367 = !DILocation(line: 0, scope: !341, inlinedAt: !368)
!368 = distinct !DILocation(line: 40, column: 7, scope: !322)
!369 = !DILocation(line: 30, column: 31, scope: !341, inlinedAt: !368)
!370 = !DILocation(line: 41, column: 11, scope: !341, inlinedAt: !368)
!371 = !{!372, !336, i64 0}
!372 = !{!"_ZTSZ4mainE3$_0", !336, i64 0}
!373 = !DILocation(line: 41, column: 15, scope: !341, inlinedAt: !368)
!374 = distinct !DISubprogram(linkageName: ".omp_task_entry..2", scope: !1, file: !1, line: 42, type: !323, scopeLine: 42, flags: DIFlagArtificial | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !375)
!375 = !{!376, !377}
!376 = !DILocalVariable(arg: 1, scope: !374, type: !9, flags: DIFlagArtificial)
!377 = !DILocalVariable(arg: 2, scope: !374, type: !328, flags: DIFlagArtificial)
!378 = !DILocation(line: 0, scope: !374)
!379 = !DILocation(line: 42, column: 7, scope: !374)
!380 = !{!381}
!381 = distinct !{!381, !382, !".omp_outlined..1: %__context"}
!382 = distinct !{!382, !".omp_outlined..1"}
!383 = !DILocalVariable(name: ".global_tid.", arg: 1, scope: !384, type: !344, flags: DIFlagArtificial)
!384 = distinct !DISubprogram(name: ".omp_outlined..1", scope: !1, file: !1, type: !385, scopeLine: 43, flags: DIFlagArtificial | DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !391)
!385 = !DISubroutineType(types: !386)
!386 = !{null, !344, !345, !348, !350, !355, !387}
!387 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !388)
!388 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !389)
!389 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !390, size: 64)
!390 = !DICompositeType(tag: DW_TAG_structure_type, file: !1, line: 42, size: 64, flags: DIFlagFwdDecl)
!391 = !{!383, !392, !393, !394, !395, !396, !397}
!392 = !DILocalVariable(name: ".part_id.", arg: 2, scope: !384, type: !345, flags: DIFlagArtificial)
!393 = !DILocalVariable(name: ".privates.", arg: 3, scope: !384, type: !348, flags: DIFlagArtificial)
!394 = !DILocalVariable(name: ".copy_fn.", arg: 4, scope: !384, type: !350, flags: DIFlagArtificial)
!395 = !DILocalVariable(name: ".task_t.", arg: 5, scope: !384, type: !355, flags: DIFlagArtificial)
!396 = !DILocalVariable(name: "__context", arg: 6, scope: !384, type: !387, flags: DIFlagArtificial)
!397 = !DILocalVariable(name: "x", scope: !384, file: !1, line: 30, type: !9)
!398 = !DILocation(line: 0, scope: !384, inlinedAt: !399)
!399 = distinct !DILocation(line: 42, column: 7, scope: !374)
!400 = !DILocation(line: 30, column: 7, scope: !384, inlinedAt: !399)
!401 = !DILocation(line: 43, column: 28, scope: !384, inlinedAt: !399)
!402 = !{!403, !336, i64 0}
!403 = !{!"_ZTSZ4mainE3$_1", !336, i64 0}
!404 = !DILocation(line: 43, column: 11, scope: !384, inlinedAt: !399)
!405 = distinct !DISubprogram(linkageName: ".omp_task_entry..4", scope: !1, file: !1, line: 44, type: !323, scopeLine: 44, flags: DIFlagArtificial | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !406)
!406 = !{!407, !408}
!407 = !DILocalVariable(arg: 1, scope: !405, type: !9, flags: DIFlagArtificial)
!408 = !DILocalVariable(arg: 2, scope: !405, type: !328, flags: DIFlagArtificial)
!409 = !DILocation(line: 0, scope: !405)
!410 = !DILocation(line: 44, column: 7, scope: !405)
!411 = !{!412}
!412 = distinct !{!412, !413, !".omp_outlined..3: %__context"}
!413 = distinct !{!413, !".omp_outlined..3"}
!414 = !DILocalVariable(name: ".global_tid.", arg: 1, scope: !415, type: !344, flags: DIFlagArtificial)
!415 = distinct !DISubprogram(name: ".omp_outlined..3", scope: !1, file: !1, type: !416, scopeLine: 45, flags: DIFlagArtificial | DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !422)
!416 = !DISubroutineType(types: !417)
!417 = !{null, !344, !345, !348, !350, !355, !418}
!418 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !419)
!419 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !420)
!420 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !421, size: 64)
!421 = !DICompositeType(tag: DW_TAG_structure_type, file: !1, line: 44, size: 64, flags: DIFlagFwdDecl)
!422 = !{!414, !423, !424, !425, !426, !427, !428}
!423 = !DILocalVariable(name: ".part_id.", arg: 2, scope: !415, type: !345, flags: DIFlagArtificial)
!424 = !DILocalVariable(name: ".privates.", arg: 3, scope: !415, type: !348, flags: DIFlagArtificial)
!425 = !DILocalVariable(name: ".copy_fn.", arg: 4, scope: !415, type: !350, flags: DIFlagArtificial)
!426 = !DILocalVariable(name: ".task_t.", arg: 5, scope: !415, type: !355, flags: DIFlagArtificial)
!427 = !DILocalVariable(name: "__context", arg: 6, scope: !415, type: !418, flags: DIFlagArtificial)
!428 = !DILocalVariable(name: "y", scope: !415, file: !1, line: 30, type: !9)
!429 = !DILocation(line: 0, scope: !415, inlinedAt: !430)
!430 = distinct !DILocation(line: 44, column: 7, scope: !405)
!431 = !DILocation(line: 30, column: 19, scope: !415, inlinedAt: !430)
!432 = !DILocation(line: 45, column: 29, scope: !415, inlinedAt: !430)
!433 = !{!434, !336, i64 0}
!434 = !{!"_ZTSZ4mainE3$_2", !336, i64 0}
!435 = !DILocation(line: 45, column: 11, scope: !415, inlinedAt: !430)
!436 = distinct !DISubprogram(name: "main.omp_outlined", scope: !1, file: !1, line: 31, type: !437, scopeLine: 31, flags: DIFlagArtificial | DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !439)
!437 = !DISubroutineType(types: !438)
!438 = !{null, !345, !345, !264, !264, !264}
!439 = !{!440, !441, !442, !443, !444}
!440 = !DILocalVariable(name: ".global_tid.", arg: 1, scope: !436, type: !345, flags: DIFlagArtificial)
!441 = !DILocalVariable(name: ".bound_tid.", arg: 2, scope: !436, type: !345, flags: DIFlagArtificial)
!442 = !DILocalVariable(name: "res", arg: 3, scope: !436, type: !264, flags: DIFlagArtificial)
!443 = !DILocalVariable(name: "x", arg: 4, scope: !436, type: !264, flags: DIFlagArtificial)
!444 = !DILocalVariable(name: "y", arg: 5, scope: !436, type: !264, flags: DIFlagArtificial)
!445 = !DILocation(line: 0, scope: !436)
!446 = !{!447}
!447 = distinct !{!447, !448, !"main.omp_outlined_debug__: %.global_tid."}
!448 = distinct !{!448, !"main.omp_outlined_debug__"}
!449 = !DILocation(line: 31, column: 3, scope: !436)
!450 = !DILocation(line: 32, column: 3, scope: !451, inlinedAt: !458)
!451 = distinct !DISubprogram(name: "main.omp_outlined_debug__", scope: !1, file: !1, line: 32, type: !437, scopeLine: 32, flags: DIFlagArtificial | DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !452)
!452 = !{!453, !454, !455, !456, !457}
!453 = !DILocalVariable(name: ".global_tid.", arg: 1, scope: !451, type: !345, flags: DIFlagArtificial)
!454 = !DILocalVariable(name: ".bound_tid.", arg: 2, scope: !451, type: !345, flags: DIFlagArtificial)
!455 = !DILocalVariable(name: "res", arg: 3, scope: !451, file: !1, line: 30, type: !264)
!456 = !DILocalVariable(name: "x", arg: 4, scope: !451, file: !1, line: 30, type: !264)
!457 = !DILocalVariable(name: "y", arg: 5, scope: !451, file: !1, line: 30, type: !264)
!458 = distinct !DILocation(line: 31, column: 3, scope: !436)
!459 = !DILocation(line: 0, scope: !451, inlinedAt: !458)
!460 = !DILocation(line: 40, column: 7, scope: !461, inlinedAt: !458)
!461 = distinct !DILexicalBlock(scope: !462, file: !1, line: 33, column: 3)
!462 = distinct !DILexicalBlock(scope: !451, file: !1, line: 32, column: 3)
!463 = !{i64 0, i64 8, !464}
!464 = !{!336, !336, i64 0}
!465 = !{!466, !467, i64 0}
!466 = !{!"_ZTS15kmp_depend_info", !467, i64 0, !467, i64 8, !271, i64 16}
!467 = !{!"long", !271, i64 0}
!468 = !{!466, !467, i64 8}
!469 = !{!466, !271, i64 16}
!470 = !DILocation(line: 42, column: 7, scope: !461, inlinedAt: !458)
!471 = !DILocation(line: 44, column: 7, scope: !461, inlinedAt: !458)
!472 = !DILocation(line: 55, column: 3, scope: !461, inlinedAt: !458)
!473 = !DILocation(line: 32, column: 21, scope: !462, inlinedAt: !458)
!474 = !DILocation(line: 32, column: 21, scope: !451, inlinedAt: !458)
!475 = !{!476}
!476 = !{i64 2, i64 -1, i64 -1, i1 true}


-------------------------------------------------

    Dependencies: No
  Number of Threads: 1
# -debug-only=omp-transform,arts,carts,arts-ir-builder,arts-utils\
llvm-dis taskwithdeps_arts_ir.bc
opt -load-pass-plugin=/home/randres/projects/carts/.install/carts/lib/ARTSAnalysis.so \
		-debug-only=arts-analysis,arts,carts,arts-ir-builder,arts-graph,carts-metadata,arts-codegen\
		-passes="arts-analysis" taskwithdeps_arts_ir.bc -o taskwithdeps_arts_analysis.bc

-------------------------------------------------
[arts-analysis] Running ARTS Analysis pass on Module: 
taskwithdeps_arts_ir.bc
-------------------------------------------------
[arts-analysis] Creating and Initializing EDTs: 
- - - - - - - - - - - - - - - - - - - - - - - - 
[arts-analysis] Initializing ARTS Cache
[arts-codegen] Initializing ARTSCodegen
[arts-codegen] ARTSCodegen initialized
 - Analyzing Functions
-- Function: _Z16long_computationRi
[arts] Function: _Z16long_computationRi doesn't have CARTS Metadata

-- Function: _Z17short_computationRi
[arts] Function: _Z17short_computationRi doesn't have CARTS Metadata

-- Function: main
[arts] Function: main doesn't have CARTS Metadata

-- Function: .omp_task_entry.
[arts] Function: .omp_task_entry. doesn't have CARTS Metadata

-- Function: .omp_task_entry..2
[arts] Function: .omp_task_entry..2 doesn't have CARTS Metadata

-- Function: .omp_task_entry..4
[arts] Function: .omp_task_entry..4 doesn't have CARTS Metadata

-- Function: main.omp_outlined
[arts] Function: main.omp_outlined doesn't have CARTS Metadata

- - - - - - - - - - - - - - - - - - - - - - - - 
[arts-analysis] Computing Graph
[Attributor] Initializing AAEDTInfo: 
[Attributor] Done with 7 functions, result: unchanged.
- - - - - - - - - - - - - - - - - - - - - - - -
[arts-graph] Printing the ARTS Graph

- - - - - - - - - - - - - - - - - - - - - - - -


All EDT Guids have been reserved
opt: /home/randres/projects/carts/carts/src/analysis/ARTSAnalysisPass.cpp:1365: void (anonymous namespace)::ARTSAnalysisPass::generateCode(Module &, ARTSCache &): Assertion `EntryEDTNode && "EntryEDTNode is null!"' failed.
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: opt -load-pass-plugin=/home/randres/projects/carts/.install/carts/lib/ARTSAnalysis.so -debug-only=arts-analysis,arts,carts,arts-ir-builder,arts-graph,carts-metadata,arts-codegen -passes=arts-analysis taskwithdeps_arts_ir.bc -o taskwithdeps_arts_analysis.bc
 #0 0x00007fc80c6906b7 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/randres/projects/carts/.install/llvm/lib/libLLVMSupport.so.18.1+0x1926b7)
 #1 0x00007fc80c68e21e llvm::sys::RunSignalHandlers() (/home/randres/projects/carts/.install/llvm/lib/libLLVMSupport.so.18.1+0x19021e)
 #2 0x00007fc80c690d8a SignalHandler(int) Signals.cpp:0:0
 #3 0x00007fc80bfdb520 (/lib/x86_64-linux-gnu/libc.so.6+0x42520)
 #4 0x00007fc80c02f9fc pthread_kill (/lib/x86_64-linux-gnu/libc.so.6+0x969fc)
 #5 0x00007fc80bfdb476 gsignal (/lib/x86_64-linux-gnu/libc.so.6+0x42476)
 #6 0x00007fc80bfc17f3 abort (/lib/x86_64-linux-gnu/libc.so.6+0x287f3)
 #7 0x00007fc80bfc171b (/lib/x86_64-linux-gnu/libc.so.6+0x2871b)
 #8 0x00007fc80bfd2e96 (/lib/x86_64-linux-gnu/libc.so.6+0x39e96)
 #9 0x00007fc80ad2743e (/home/randres/projects/carts/.install/carts/lib/ARTSAnalysis.so+0x2343e)
#10 0x00007fc80ca96576 llvm::PassManager<llvm::Module, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/randres/projects/carts/.install/llvm/lib/libLLVMCore.so.18.1+0x2c9576)
#11 0x0000559cc3110acb llvm::runPassPipeline(llvm::StringRef, llvm::Module&, llvm::TargetMachine*, llvm::TargetLibraryInfoImpl*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::StringRef, llvm::ArrayRef<llvm::PassPlugin>, llvm::opt_tool::OutputKind, llvm::opt_tool::VerifierKind, bool, bool, bool, bool, bool, bool, bool) (/home/randres/projects/carts/.install/llvm/bin/opt+0x19acb)
#12 0x0000559cc311e900 main (/home/randres/projects/carts/.install/llvm/bin/opt+0x27900)
#13 0x00007fc80bfc2d90 (/lib/x86_64-linux-gnu/libc.so.6+0x29d90)
#14 0x00007fc80bfc2e40 __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x29e40)
#15 0x0000559cc3109ef5 _start (/home/randres/projects/carts/.install/llvm/bin/opt+0x12ef5)
Aborted
make: *** [Makefile:28: taskwithdeps_arts_analysis.bc] Error 134
