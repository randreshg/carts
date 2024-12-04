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
Found OpenMP Directive (OMPTaskDirective) at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:34:7
Found OpenMP Directive (OMPTaskDirective) at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:40:7
Found OpenMP Directive (OMPTaskDirective) at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:42:7
Found OpenMP Directive (OMPTaskDirective) at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:44:7
Found OpenMP Directive (OMPParallelDirective) at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:47:3
Found OpenMP Directive (OMPSingleDirective) at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:48:3
  (Unhandled directive type)
Found OpenMP Directive (OMPTaskDirective) at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:50:7
Found OpenMP Directive (OMPTaskDirective) at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:55:7
Found OpenMP Directive (OMPTaskDirective) at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:57:7
- - - - - - - - - - - - - - - - - - - 
Printing OpenMP Directive Hierarchy...
Directive: parallel
  Location: /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:31:3
  Instruction:   ret void, !dbg !285
  Directive: task
    Location: /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:34:7
    Instruction:   %15 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @3, i32 %0, ptr nonnull %2, i32 3, ptr nonnull %.dep.arr.addr.i, i32 0, ptr null), !dbg !300, !noalias !282
    Dependen  cies: No
  Directive: task
    Location: /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:40:7
    Instruction:   %20 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @5, i32 %0, ptr nonnull %16, i32 1, ptr nonnull %.dep.arr.addr2.i, i32 0, ptr null), !dbg !316, !noalias !282
    Dependencies: No
  Directive: task
    Location: /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:42:7
    Instruction:   %25 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @7, i32 %0, ptr nonnull %21, i32 1, ptr nonnull %.dep.arr.addr5.i, i32 0, ptr null), !dbg !317, !noalias !282
    Dependencies: No
  Directive: task
    Location: /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:44:7
    Instruction:   %30 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @9, i32 %0, ptr nonnull %26, i32 1, ptr nonnull %.dep.arr.addr8.i, i32 0, ptr null), !dbg !318, !noalias !282
    Dependencies: No
  Number of Threads: 1
Directive: parallel
  Location: /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:47:3
  Instruction:   ret void, !dbg !285
  Directive: task
    Location: /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:50:7
    Instruction:   %7 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @16, i32 %0, ptr nonnull %2, i32 1, ptr nonnull %.dep.arr.addr.i, i32 0, ptr null), !dbg !300, !noalias !282
    Dependencies: No
  Directive: task
    Location: /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:55:7
    Instruction:   %13 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @18, i32 %0, ptr nonnull %8, i32 1, ptr nonnull %.dep.arr.addr2.i, i32 0, ptr null), !dbg !315, !noalias !282
    Dependencies: No
  Directive: task
    Location: /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:57:7
    Instruction:   %19 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @20, i32 %0, ptr nonnull %14, i32 1, ptr nonnull %.dep.arr.addr5.i, i32 0, ptr null), !dbg !316, !noalias !282
-------------------------------------------------
[omp-transform] OmpTransformPass has finished

; ModuleID = 'taskwithdeps.bc'
source_filename = "taskwithdeps.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }
%struct.anon = type { ptr, ptr, ptr }
%struct.kmp_depend_info = type { i64, i64, i8 }
%struct.anon.6 = type { ptr, ptr }
%struct.anon.8 = type { ptr, ptr }

@0 = private unnamed_addr constant [30 x i8] c";taskwithdeps.cpp;main;32;3;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 29, ptr @0 }, align 8
@2 = private unnamed_addr constant [30 x i8] c";taskwithdeps.cpp;main;34;7;;\00", align 1
@3 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 29, ptr @2 }, align 8
@4 = private unnamed_addr constant [30 x i8] c";taskwithdeps.cpp;main;40;7;;\00", align 1
@5 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 29, ptr @4 }, align 8
@6 = private unnamed_addr constant [30 x i8] c";taskwithdeps.cpp;main;42;7;;\00", align 1
@7 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 29, ptr @6 }, align 8
@8 = private unnamed_addr constant [30 x i8] c";taskwithdeps.cpp;main;44;7;;\00", align 1
@9 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 29, ptr @8 }, align 8
@10 = private unnamed_addr constant %struct.ident_t { i32 0, i32 322, i32 0, i32 29, ptr @0 }, align 8
@11 = private unnamed_addr constant [30 x i8] c";taskwithdeps.cpp;main;31;3;;\00", align 1
@12 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 29, ptr @11 }, align 8
@13 = private unnamed_addr constant [30 x i8] c";taskwithdeps.cpp;main;48;3;;\00", align 1
@14 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 29, ptr @13 }, align 8
@15 = private unnamed_addr constant [30 x i8] c";taskwithdeps.cpp;main;50;7;;\00", align 1
@16 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 29, ptr @15 }, align 8
@17 = private unnamed_addr constant [30 x i8] c";taskwithdeps.cpp;main;55;7;;\00", align 1
@18 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 29, ptr @17 }, align 8
@.str = private unnamed_addr constant [3 x i8] c"%d\00", align 1, !dbg !0
@19 = private unnamed_addr constant [30 x i8] c";taskwithdeps.cpp;main;57;7;;\00", align 1
@20 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 29, ptr @19 }, align 8
@21 = private unnamed_addr constant %struct.ident_t { i32 0, i32 322, i32 0, i32 29, ptr @13 }, align 8
@22 = private unnamed_addr constant [30 x i8] c";taskwithdeps.cpp;main;47;3;;\00", align 1
@23 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 29, ptr @22 }, align 8

; Function Attrs: mustprogress nofree noinline norecurse nosync nounwind willreturn memory(argmem: readwrite) uwtable
define dso_local void @_Z16long_computationRi(ptr nocapture noundef nonnull align 4 dereferenceable(4) %x) local_unnamed_addr #0 !dbg !267 {
entry:
  tail call void @llvm.dbg.value(metadata ptr %x, metadata !272, metadata !DIExpression()), !dbg !274
  tail call void @llvm.dbg.value(metadata i32 0, metadata !273, metadata !DIExpression()), !dbg !274
  %x.promoted = load i32, ptr %x, align 4, !tbaa !275
  tail call void @llvm.dbg.value(metadata i32 0, metadata !273, metadata !DIExpression()), !dbg !274
  tail call void @llvm.dbg.value(metadata i32 undef, metadata !273, metadata !DIExpression()), !dbg !274
  %0 = add i32 %x.promoted, 1783293664, !dbg !279
  store i32 %0, ptr %x, align 4, !dbg !281, !tbaa !275
  ret void, !dbg !284
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #2

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #2

; Function Attrs: mustprogress nofree noinline norecurse nosync nounwind willreturn memory(argmem: readwrite) uwtable
define dso_local void @_Z17short_computationRi(ptr nocapture noundef nonnull align 4 dereferenceable(4) %x) local_unnamed_addr #0 !dbg !285 {
entry:
  tail call void @llvm.dbg.value(metadata ptr %x, metadata !287, metadata !DIExpression()), !dbg !289
  tail call void @llvm.dbg.value(metadata i32 0, metadata !288, metadata !DIExpression()), !dbg !289
  %x.promoted = load i32, ptr %x, align 4, !tbaa !275
  tail call void @llvm.dbg.value(metadata i32 0, metadata !288, metadata !DIExpression()), !dbg !289
  tail call void @llvm.dbg.value(metadata i32 undef, metadata !288, metadata !DIExpression()), !dbg !289
  %0 = add i32 %x.promoted, 499500, !dbg !290
  store i32 %0, ptr %x, align 4, !dbg !292, !tbaa !275
  ret void, !dbg !295
}

; Function Attrs: mustprogress norecurse nounwind uwtable
define dso_local noundef i32 @main() local_unnamed_addr #3 !dbg !296 {
entry:
  %x = alloca i32, align 4, !DIAssignID !301
  call void @llvm.dbg.assign(metadata i1 undef, metadata !298, metadata !DIExpression(), metadata !301, metadata ptr %x, metadata !DIExpression()), !dbg !302
  %y = alloca i32, align 4, !DIAssignID !303
  call void @llvm.dbg.assign(metadata i1 undef, metadata !299, metadata !DIExpression(), metadata !303, metadata ptr %y, metadata !DIExpression()), !dbg !302
  %res = alloca i32, align 4, !DIAssignID !304
  call void @llvm.dbg.assign(metadata i1 undef, metadata !300, metadata !DIExpression(), metadata !304, metadata ptr %res, metadata !DIExpression()), !dbg !302
  %call = tail call i64 @time(ptr noundef null) #7, !dbg !305
  %conv = trunc i64 %call to i32, !dbg !305
  tail call void @srand(i32 noundef %conv) #7, !dbg !306
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %x) #7, !dbg !307
  %call1 = tail call i32 @rand() #7, !dbg !308
  store i32 %call1, ptr %x, align 4, !dbg !309, !tbaa !275, !DIAssignID !310
  call void @llvm.dbg.assign(metadata i32 %call1, metadata !298, metadata !DIExpression(), metadata !310, metadata ptr %x, metadata !DIExpression()), !dbg !302
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %y) #7, !dbg !307
  %call2 = tail call i32 @rand() #7, !dbg !311
  store i32 %call2, ptr %y, align 4, !dbg !312, !tbaa !275, !DIAssignID !313
  call void @llvm.dbg.assign(metadata i32 %call2, metadata !299, metadata !DIExpression(), metadata !313, metadata ptr %y, metadata !DIExpression()), !dbg !302
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %res) #7, !dbg !307
  store i32 0, ptr %res, align 4, !dbg !314, !tbaa !275, !DIAssignID !315
  call void @llvm.dbg.assign(metadata i32 0, metadata !300, metadata !DIExpression(), metadata !315, metadata ptr %res, metadata !DIExpression()), !dbg !302
  call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr nonnull @12, i32 3, ptr nonnull @main.omp_outlined, ptr nonnull %res, ptr nonnull %x, ptr nonnull %y), !dbg !316
  call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr nonnull @23, i32 3, ptr nonnull @main.omp_outlined.14, ptr nonnull %x, ptr nonnull %res, ptr nonnull %y), !dbg !317
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %res) #7, !dbg !318
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %y) #7, !dbg !318
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %x) #7, !dbg !318
  ret i32 0, !dbg !319
}

; Function Attrs: nounwind
declare !dbg !137 void @srand(i32 noundef) local_unnamed_addr #4

; Function Attrs: nounwind
declare !dbg !320 i64 @time(ptr noundef) local_unnamed_addr #4

; Function Attrs: nounwind
declare !dbg !129 i32 @rand() local_unnamed_addr #4

; Function Attrs: convergent nounwind
declare i32 @__kmpc_single(ptr, i32) local_unnamed_addr #5

; Function Attrs: convergent nounwind
declare void @__kmpc_end_single(ptr, i32) local_unnamed_addr #5

; Function Attrs: norecurse nounwind uwtable
define internal noundef i32 @.omp_task_entry.(i32 %0, ptr noalias nocapture noundef readonly %1) #6 !dbg !329 {
entry:
  tail call void @llvm.dbg.value(metadata i32 poison, metadata !333, metadata !DIExpression()), !dbg !338
  tail call void @llvm.dbg.value(metadata ptr %1, metadata !334, metadata !DIExpression()), !dbg !338
  %2 = load ptr, ptr %1, align 8, !dbg !339, !tbaa !340
  tail call void @llvm.experimental.noalias.scope.decl(metadata !344), !dbg !339
  call void @llvm.dbg.value(metadata i32 poison, metadata !347, metadata !DIExpression()), !dbg !376
  call void @llvm.dbg.value(metadata ptr %1, metadata !368, metadata !DIExpression(DW_OP_plus_uconst, 16, DW_OP_stack_value)), !dbg !376
  call void @llvm.dbg.value(metadata ptr null, metadata !369, metadata !DIExpression()), !dbg !376
  call void @llvm.dbg.value(metadata ptr null, metadata !370, metadata !DIExpression()), !dbg !376
  call void @llvm.dbg.value(metadata ptr %1, metadata !371, metadata !DIExpression()), !dbg !376
  call void @llvm.dbg.value(metadata ptr %2, metadata !372, metadata !DIExpression()), !dbg !376
  call void @llvm.dbg.declare(metadata ptr %2, metadata !373, metadata !DIExpression(DW_OP_deref)), !dbg !378
  call void @llvm.dbg.declare(metadata ptr %2, metadata !374, metadata !DIExpression(DW_OP_plus_uconst, 16, DW_OP_deref)), !dbg !379
  call void @llvm.dbg.declare(metadata ptr %2, metadata !375, metadata !DIExpression(DW_OP_plus_uconst, 8, DW_OP_deref)), !dbg !380
  %3 = load ptr, ptr %2, align 8, !dbg !381, !tbaa !383, !alias.scope !344
  store i32 0, ptr %3, align 4, !dbg !385, !tbaa !275, !noalias !344
  %call.i = tail call i32 @rand() #7, !dbg !386, !noalias !344
  %4 = getelementptr inbounds %struct.anon, ptr %2, i64 0, i32 1, !dbg !387
  %5 = load ptr, ptr %4, align 8, !dbg !387, !tbaa !388, !alias.scope !344
  store i32 %call.i, ptr %5, align 4, !dbg !389, !tbaa !275, !noalias !344
  %call1.i = tail call i32 @rand() #7, !dbg !390, !noalias !344
  %6 = getelementptr inbounds %struct.anon, ptr %2, i64 0, i32 2, !dbg !391
  %7 = load ptr, ptr %6, align 8, !dbg !391, !tbaa !392, !alias.scope !344
  store i32 %call1.i, ptr %7, align 4, !dbg !393, !tbaa !275, !noalias !344
  ret i32 0, !dbg !339
}

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #7

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task_with_deps(ptr, i32, ptr, i32, ptr, i32, ptr) local_unnamed_addr #7

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn uwtable
define internal noundef i32 @.omp_task_entry..2(i32 %0, ptr noalias nocapture noundef readonly %1) #8 !dbg !394 {
entry:
  tail call void @llvm.dbg.value(metadata i32 poison, metadata !396, metadata !DIExpression()), !dbg !398
  tail call void @llvm.dbg.value(metadata ptr %1, metadata !397, metadata !DIExpression()), !dbg !398
  %2 = load ptr, ptr %1, align 8, !dbg !399, !tbaa !340
  tail call void @llvm.experimental.noalias.scope.decl(metadata !400), !dbg !399
  tail call void @llvm.dbg.value(metadata i32 poison, metadata !403, metadata !DIExpression()), !dbg !418
  tail call void @llvm.dbg.value(metadata ptr %1, metadata !412, metadata !DIExpression(DW_OP_plus_uconst, 16, DW_OP_stack_value)), !dbg !418
  tail call void @llvm.dbg.value(metadata ptr null, metadata !413, metadata !DIExpression()), !dbg !418
  tail call void @llvm.dbg.value(metadata ptr null, metadata !414, metadata !DIExpression()), !dbg !418
  tail call void @llvm.dbg.value(metadata ptr %1, metadata !415, metadata !DIExpression()), !dbg !418
  tail call void @llvm.dbg.value(metadata ptr %2, metadata !416, metadata !DIExpression()), !dbg !418
  call void @llvm.dbg.declare(metadata ptr %2, metadata !417, metadata !DIExpression(DW_OP_deref)), !dbg !420
  %3 = load ptr, ptr %2, align 8, !dbg !421, !tbaa !422, !alias.scope !400
  store i32 0, ptr %3, align 4, !dbg !424, !tbaa !275, !noalias !400
  ret i32 0, !dbg !399
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn uwtable
define internal noundef i32 @.omp_task_entry..4(i32 %0, ptr noalias nocapture noundef readonly %1) #8 !dbg !425 {
entry:
  tail call void @llvm.dbg.value(metadata i32 poison, metadata !427, metadata !DIExpression()), !dbg !429
  tail call void @llvm.dbg.value(metadata ptr %1, metadata !428, metadata !DIExpression()), !dbg !429
  %2 = load ptr, ptr %1, align 8, !dbg !430, !tbaa !340
  tail call void @llvm.experimental.noalias.scope.decl(metadata !431), !dbg !430
  call void @llvm.dbg.value(metadata i32 poison, metadata !434, metadata !DIExpression()), !dbg !449
  call void @llvm.dbg.value(metadata ptr %1, metadata !443, metadata !DIExpression(DW_OP_plus_uconst, 16, DW_OP_stack_value)), !dbg !449
  call void @llvm.dbg.value(metadata ptr null, metadata !444, metadata !DIExpression()), !dbg !449
  call void @llvm.dbg.value(metadata ptr null, metadata !445, metadata !DIExpression()), !dbg !449
  call void @llvm.dbg.value(metadata ptr %1, metadata !446, metadata !DIExpression()), !dbg !449
  call void @llvm.dbg.value(metadata ptr %2, metadata !447, metadata !DIExpression()), !dbg !449
  call void @llvm.dbg.declare(metadata ptr %2, metadata !448, metadata !DIExpression(DW_OP_deref)), !dbg !451
  %3 = load ptr, ptr %2, align 8, !dbg !452, !tbaa !453, !alias.scope !431
  tail call void @_Z16long_computationRi(ptr noundef nonnull align 4 dereferenceable(4) %3), !dbg !455, !noalias !431
  ret i32 0, !dbg !430
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn uwtable
define internal noundef i32 @.omp_task_entry..6(i32 %0, ptr noalias nocapture noundef readonly %1) #8 !dbg !456 {
entry:
  tail call void @llvm.dbg.value(metadata i32 poison, metadata !458, metadata !DIExpression()), !dbg !460
  tail call void @llvm.dbg.value(metadata ptr %1, metadata !459, metadata !DIExpression()), !dbg !460
  %2 = load ptr, ptr %1, align 8, !dbg !461, !tbaa !340
  tail call void @llvm.experimental.noalias.scope.decl(metadata !462), !dbg !461
  call void @llvm.dbg.value(metadata i32 poison, metadata !465, metadata !DIExpression()), !dbg !480
  call void @llvm.dbg.value(metadata ptr %1, metadata !474, metadata !DIExpression(DW_OP_plus_uconst, 16, DW_OP_stack_value)), !dbg !480
  call void @llvm.dbg.value(metadata ptr null, metadata !475, metadata !DIExpression()), !dbg !480
  call void @llvm.dbg.value(metadata ptr null, metadata !476, metadata !DIExpression()), !dbg !480
  call void @llvm.dbg.value(metadata ptr %1, metadata !477, metadata !DIExpression()), !dbg !480
  call void @llvm.dbg.value(metadata ptr %2, metadata !478, metadata !DIExpression()), !dbg !480
  call void @llvm.dbg.declare(metadata ptr %2, metadata !479, metadata !DIExpression(DW_OP_deref)), !dbg !482
  %3 = load ptr, ptr %2, align 8, !dbg !483, !tbaa !484, !alias.scope !462
  tail call void @_Z17short_computationRi(ptr noundef nonnull align 4 dereferenceable(4) %3), !dbg !486, !noalias !462
  ret i32 0, !dbg !461
}

; Function Attrs: convergent nounwind
declare void @__kmpc_barrier(ptr, i32) local_unnamed_addr #5

; Function Attrs: alwaysinline norecurse nounwind uwtable
define internal void @main.omp_outlined(ptr noalias nocapture noundef readonly %.global_tid., ptr noalias nocapture readnone %.bound_tid., ptr noundef nonnull align 4 dereferenceable(4) %res, ptr noundef nonnull align 4 dereferenceable(4) %x, ptr noundef nonnull align 4 dereferenceable(4) %y) #9 !dbg !487 {
entry:
  %.dep.arr.addr.i = alloca [3 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr2.i = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr5.i = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr8.i = alloca [1 x %struct.kmp_depend_info], align 8
  tail call void @llvm.dbg.value(metadata ptr %.global_tid., metadata !491, metadata !DIExpression()), !dbg !496
  tail call void @llvm.dbg.value(metadata ptr poison, metadata !492, metadata !DIExpression()), !dbg !496
  tail call void @llvm.dbg.value(metadata ptr %res, metadata !493, metadata !DIExpression()), !dbg !496
  tail call void @llvm.dbg.value(metadata ptr %x, metadata !494, metadata !DIExpression()), !dbg !496
  tail call void @llvm.dbg.value(metadata ptr %y, metadata !495, metadata !DIExpression()), !dbg !496
  tail call void @llvm.experimental.noalias.scope.decl(metadata !497), !dbg !500
  call void @llvm.lifetime.start.p0(i64 72, ptr nonnull %.dep.arr.addr.i), !dbg !501
  call void @llvm.lifetime.start.p0(i64 24, ptr nonnull %.dep.arr.addr2.i), !dbg !501
  call void @llvm.lifetime.start.p0(i64 24, ptr nonnull %.dep.arr.addr5.i), !dbg !501
  call void @llvm.lifetime.start.p0(i64 24, ptr nonnull %.dep.arr.addr8.i), !dbg !501
  call void @llvm.dbg.value(metadata ptr %.global_tid., metadata !504, metadata !DIExpression()), !dbg !510
  call void @llvm.dbg.value(metadata ptr poison, metadata !505, metadata !DIExpression()), !dbg !510
  call void @llvm.dbg.value(metadata ptr %res, metadata !506, metadata !DIExpression()), !dbg !510
  call void @llvm.dbg.value(metadata ptr %x, metadata !507, metadata !DIExpression()), !dbg !510
  call void @llvm.dbg.value(metadata ptr %y, metadata !508, metadata !DIExpression()), !dbg !510
  %0 = load i32, ptr %.global_tid., align 4, !dbg !501, !tbaa !275, !alias.scope !497
  %1 = tail call i32 @__kmpc_single(ptr nonnull @1, i32 %0), !dbg !501, !noalias !497
  %.not.i = icmp eq i32 %1, 0, !dbg !501
  br i1 %.not.i, label %main.omp_outlined_debug__.exit, label %omp_if.then.i, !dbg !501

omp_if.then.i:                                    ; preds = %entry
  %2 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @3, i32 %0, i32 1, i64 40, i64 24, ptr nonnull @.omp_task_entry.), !dbg !511, !noalias !497
  %3 = load ptr, ptr %2, align 8, !dbg !511, !tbaa !340, !noalias !497
  store ptr %res, ptr %3, align 8, !dbg !511, !tbaa.struct !514, !noalias !497
  %agg.captured.sroa.2.0..sroa_idx.i = getelementptr inbounds i8, ptr %3, i64 8, !dbg !511
  store ptr %x, ptr %agg.captured.sroa.2.0..sroa_idx.i, align 8, !dbg !511, !tbaa.struct !516, !noalias !497
  %agg.captured.sroa.3.0..sroa_idx.i = getelementptr inbounds i8, ptr %3, i64 16, !dbg !511
  store ptr %y, ptr %agg.captured.sroa.3.0..sroa_idx.i, align 8, !dbg !511, !tbaa.struct !517, !noalias !497
  %4 = ptrtoint ptr %res to i64, !dbg !511
  store i64 %4, ptr %.dep.arr.addr.i, align 8, !dbg !511, !tbaa !518, !noalias !497
  %5 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr.i, i64 0, i32 1, !dbg !511
  store i64 4, ptr %5, align 8, !dbg !511, !tbaa !521, !noalias !497
  %6 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr.i, i64 0, i32 2, !dbg !511
  store i8 3, ptr %6, align 8, !dbg !511, !tbaa !522, !noalias !497
  %7 = ptrtoint ptr %x to i64, !dbg !511
  %8 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr.i, i64 1, !dbg !511
  store i64 %7, ptr %8, align 8, !dbg !511, !tbaa !518, !noalias !497
  %9 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr.i, i64 1, i32 1, !dbg !511
  store i64 4, ptr %9, align 8, !dbg !511, !tbaa !521, !noalias !497
  %10 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr.i, i64 1, i32 2, !dbg !511
  store i8 3, ptr %10, align 8, !dbg !511, !tbaa !522, !noalias !497
  %11 = ptrtoint ptr %y to i64, !dbg !511
  %12 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr.i, i64 2, !dbg !511
  store i64 %11, ptr %12, align 8, !dbg !511, !tbaa !518, !noalias !497
  %13 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr.i, i64 2, i32 1, !dbg !511
  store i64 4, ptr %13, align 8, !dbg !511, !tbaa !521, !noalias !497
  %14 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr.i, i64 2, i32 2, !dbg !511
  store i8 3, ptr %14, align 8, !dbg !511, !tbaa !522, !noalias !497
  %15 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @3, i32 %0, ptr nonnull %2, i32 3, ptr nonnull %.dep.arr.addr.i, i32 0, ptr null), !dbg !511, !noalias !497
  %16 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @5, i32 %0, i32 1, i64 40, i64 8, ptr nonnull @.omp_task_entry..2), !dbg !523, !noalias !497
  %17 = load ptr, ptr %16, align 8, !dbg !523, !tbaa !340, !noalias !497
  store ptr %res, ptr %17, align 8, !dbg !523, !tbaa.struct !517, !noalias !497
  store i64 %4, ptr %.dep.arr.addr2.i, align 8, !dbg !523, !tbaa !518, !noalias !497
  %18 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr2.i, i64 0, i32 1, !dbg !523
  store i64 4, ptr %18, align 8, !dbg !523, !tbaa !521, !noalias !497
  %19 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr2.i, i64 0, i32 2, !dbg !523
  store i8 3, ptr %19, align 8, !dbg !523, !tbaa !522, !noalias !497
  %20 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @5, i32 %0, ptr nonnull %16, i32 1, ptr nonnull %.dep.arr.addr2.i, i32 0, ptr null), !dbg !523, !noalias !497
  %21 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @7, i32 %0, i32 1, i64 40, i64 8, ptr nonnull @.omp_task_entry..4), !dbg !524, !noalias !497
  %22 = load ptr, ptr %21, align 8, !dbg !524, !tbaa !340, !noalias !497
  store ptr %x, ptr %22, align 8, !dbg !524, !tbaa.struct !517, !noalias !497
  store i64 %7, ptr %.dep.arr.addr5.i, align 8, !dbg !524, !tbaa !518, !noalias !497
  %23 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr5.i, i64 0, i32 1, !dbg !524
  store i64 4, ptr %23, align 8, !dbg !524, !tbaa !521, !noalias !497
  %24 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr5.i, i64 0, i32 2, !dbg !524
  store i8 3, ptr %24, align 8, !dbg !524, !tbaa !522, !noalias !497
  %25 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @7, i32 %0, ptr nonnull %21, i32 1, ptr nonnull %.dep.arr.addr5.i, i32 0, ptr null), !dbg !524, !noalias !497
  %26 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @9, i32 %0, i32 1, i64 40, i64 8, ptr nonnull @.omp_task_entry..6), !dbg !525, !noalias !497
  %27 = load ptr, ptr %26, align 8, !dbg !525, !tbaa !340, !noalias !497
  store ptr %y, ptr %27, align 8, !dbg !525, !tbaa.struct !517, !noalias !497
  store i64 %11, ptr %.dep.arr.addr8.i, align 8, !dbg !525, !tbaa !518, !noalias !497
  %28 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr8.i, i64 0, i32 1, !dbg !525
  store i64 4, ptr %28, align 8, !dbg !525, !tbaa !521, !noalias !497
  %29 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr8.i, i64 0, i32 2, !dbg !525
  store i8 3, ptr %29, align 8, !dbg !525, !tbaa !522, !noalias !497
  %30 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @9, i32 %0, ptr nonnull %26, i32 1, ptr nonnull %.dep.arr.addr8.i, i32 0, ptr null), !dbg !525, !noalias !497
  call void @__kmpc_end_single(ptr nonnull @1, i32 %0), !dbg !526, !noalias !497
  br label %main.omp_outlined_debug__.exit, !dbg !526

main.omp_outlined_debug__.exit:                   ; preds = %entry, %omp_if.then.i
  call void @__kmpc_barrier(ptr nonnull @10, i32 %0), !dbg !527, !noalias !497
  call void @llvm.lifetime.end.p0(i64 72, ptr nonnull %.dep.arr.addr.i), !dbg !528
  call void @llvm.lifetime.end.p0(i64 24, ptr nonnull %.dep.arr.addr2.i), !dbg !528
  call void @llvm.lifetime.end.p0(i64 24, ptr nonnull %.dep.arr.addr5.i), !dbg !528
  call void @llvm.lifetime.end.p0(i64 24, ptr nonnull %.dep.arr.addr8.i), !dbg !528
  ret void, !dbg !500
}

; Function Attrs: nounwind
declare !callback !529 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #7

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn uwtable
define internal noundef i32 @.omp_task_entry..9(i32 %0, ptr noalias nocapture noundef readonly %1) #8 !dbg !531 {
entry:
  tail call void @llvm.dbg.value(metadata i32 poison, metadata !533, metadata !DIExpression()), !dbg !535
  tail call void @llvm.dbg.value(metadata ptr %1, metadata !534, metadata !DIExpression()), !dbg !535
  %2 = load ptr, ptr %1, align 8, !dbg !536, !tbaa !340
  tail call void @llvm.experimental.noalias.scope.decl(metadata !537), !dbg !536
  tail call void @llvm.dbg.value(metadata i32 poison, metadata !540, metadata !DIExpression()), !dbg !556
  tail call void @llvm.dbg.value(metadata ptr %1, metadata !549, metadata !DIExpression(DW_OP_plus_uconst, 16, DW_OP_stack_value)), !dbg !556
  tail call void @llvm.dbg.value(metadata ptr null, metadata !550, metadata !DIExpression()), !dbg !556
  tail call void @llvm.dbg.value(metadata ptr null, metadata !551, metadata !DIExpression()), !dbg !556
  tail call void @llvm.dbg.value(metadata ptr %1, metadata !552, metadata !DIExpression()), !dbg !556
  tail call void @llvm.dbg.value(metadata ptr %2, metadata !553, metadata !DIExpression()), !dbg !556
  call void @llvm.dbg.declare(metadata ptr %2, metadata !554, metadata !DIExpression(DW_OP_plus_uconst, 8, DW_OP_deref)), !dbg !558
  call void @llvm.dbg.declare(metadata ptr %2, metadata !555, metadata !DIExpression(DW_OP_deref)), !dbg !559
  %3 = getelementptr inbounds %struct.anon.6, ptr %2, i64 0, i32 1, !dbg !560
  %4 = load ptr, ptr %3, align 8, !dbg !560, !tbaa !562, !alias.scope !537
  %5 = load i32, ptr %4, align 4, !dbg !560, !tbaa !275, !noalias !537
  %6 = load ptr, ptr %2, align 8, !dbg !564, !tbaa !565, !alias.scope !537
  %7 = load i32, ptr %6, align 4, !dbg !566, !tbaa !275, !noalias !537
  %add.i = add nsw i32 %7, %5, !dbg !566
  store i32 %add.i, ptr %6, align 4, !dbg !566, !tbaa !275, !noalias !537
  %8 = load i32, ptr %4, align 4, !dbg !567, !tbaa !275, !noalias !537
  %inc.i = add nsw i32 %8, 1, !dbg !567
  store i32 %inc.i, ptr %4, align 4, !dbg !567, !tbaa !275, !noalias !537
  ret i32 0, !dbg !536
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn uwtable
define internal noundef i32 @.omp_task_entry..11(i32 %0, ptr noalias nocapture noundef readonly %1) #8 !dbg !568 {
entry:
  tail call void @llvm.dbg.value(metadata i32 poison, metadata !570, metadata !DIExpression()), !dbg !572
  tail call void @llvm.dbg.value(metadata ptr %1, metadata !571, metadata !DIExpression()), !dbg !572
  %2 = load ptr, ptr %1, align 8, !dbg !573, !tbaa !340
  tail call void @llvm.experimental.noalias.scope.decl(metadata !574), !dbg !573
  tail call void @llvm.dbg.value(metadata i32 poison, metadata !577, metadata !DIExpression()), !dbg !593
  tail call void @llvm.dbg.value(metadata ptr %1, metadata !586, metadata !DIExpression(DW_OP_plus_uconst, 16, DW_OP_stack_value)), !dbg !593
  tail call void @llvm.dbg.value(metadata ptr null, metadata !587, metadata !DIExpression()), !dbg !593
  tail call void @llvm.dbg.value(metadata ptr null, metadata !588, metadata !DIExpression()), !dbg !593
  tail call void @llvm.dbg.value(metadata ptr %1, metadata !589, metadata !DIExpression()), !dbg !593
  tail call void @llvm.dbg.value(metadata ptr %2, metadata !590, metadata !DIExpression()), !dbg !593
  call void @llvm.dbg.declare(metadata ptr %2, metadata !591, metadata !DIExpression(DW_OP_deref)), !dbg !595
  call void @llvm.dbg.declare(metadata ptr %2, metadata !592, metadata !DIExpression(DW_OP_plus_uconst, 8, DW_OP_deref)), !dbg !596
  %3 = getelementptr inbounds %struct.anon.8, ptr %2, i64 0, i32 1, !dbg !597
  %4 = load ptr, ptr %3, align 8, !dbg !597, !tbaa !598, !alias.scope !574
  %5 = load i32, ptr %4, align 4, !dbg !597, !tbaa !275, !noalias !574
  %6 = load ptr, ptr %2, align 8, !dbg !600, !tbaa !601, !alias.scope !574
  %7 = load i32, ptr %6, align 4, !dbg !602, !tbaa !275, !noalias !574
  %add.i = add nsw i32 %7, %5, !dbg !602
  store i32 %add.i, ptr %6, align 4, !dbg !602, !tbaa !275, !noalias !574
  ret i32 0, !dbg !573
}

; Function Attrs: nofree nounwind
declare !dbg !603 noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #10

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nofree norecurse nounwind uwtable
define internal noundef i32 @.omp_task_entry..13(i32 %0, ptr noalias nocapture noundef readonly %1) #11 personality ptr @__gxx_personality_v0 !dbg !607 {
entry:
  tail call void @llvm.dbg.value(metadata i32 poison, metadata !609, metadata !DIExpression()), !dbg !611
  tail call void @llvm.dbg.value(metadata ptr %1, metadata !610, metadata !DIExpression()), !dbg !611
  %2 = load ptr, ptr %1, align 8, !dbg !612, !tbaa !340
  tail call void @llvm.experimental.noalias.scope.decl(metadata !613), !dbg !612
  call void @llvm.dbg.value(metadata i32 poison, metadata !616, metadata !DIExpression()), !dbg !631
  call void @llvm.dbg.value(metadata ptr %1, metadata !625, metadata !DIExpression(DW_OP_plus_uconst, 16, DW_OP_stack_value)), !dbg !631
  call void @llvm.dbg.value(metadata ptr null, metadata !626, metadata !DIExpression()), !dbg !631
  call void @llvm.dbg.value(metadata ptr null, metadata !627, metadata !DIExpression()), !dbg !631
  call void @llvm.dbg.value(metadata ptr %1, metadata !628, metadata !DIExpression()), !dbg !631
  call void @llvm.dbg.value(metadata ptr %2, metadata !629, metadata !DIExpression()), !dbg !631
  call void @llvm.dbg.declare(metadata ptr %2, metadata !630, metadata !DIExpression(DW_OP_deref)), !dbg !633
  %3 = load ptr, ptr %2, align 8, !dbg !634, !tbaa !635, !alias.scope !613
  %4 = load i32, ptr %3, align 4, !dbg !634, !tbaa !275, !noalias !613
  %call.i = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %4), !dbg !637, !noalias !613
  ret i32 0, !dbg !612
}

; Function Attrs: alwaysinline norecurse nounwind uwtable
define internal void @main.omp_outlined.14(ptr noalias nocapture noundef readonly %.global_tid., ptr noalias nocapture readnone %.bound_tid., ptr noundef nonnull align 4 dereferenceable(4) %x, ptr noundef nonnull align 4 dereferenceable(4) %res, ptr noundef nonnull align 4 dereferenceable(4) %y) #9 !dbg !638 {
entry:
  %.dep.arr.addr.i = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr2.i = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr5.i = alloca [1 x %struct.kmp_depend_info], align 8
  tail call void @llvm.dbg.value(metadata ptr %.global_tid., metadata !640, metadata !DIExpression()), !dbg !645
  tail call void @llvm.dbg.value(metadata ptr poison, metadata !641, metadata !DIExpression()), !dbg !645
  tail call void @llvm.dbg.value(metadata ptr %x, metadata !642, metadata !DIExpression()), !dbg !645
  tail call void @llvm.dbg.value(metadata ptr %res, metadata !643, metadata !DIExpression()), !dbg !645
  tail call void @llvm.dbg.value(metadata ptr %y, metadata !644, metadata !DIExpression()), !dbg !645
  tail call void @llvm.experimental.noalias.scope.decl(metadata !646), !dbg !649
  call void @llvm.lifetime.start.p0(i64 24, ptr nonnull %.dep.arr.addr.i), !dbg !650
  call void @llvm.lifetime.start.p0(i64 24, ptr nonnull %.dep.arr.addr2.i), !dbg !650
  call void @llvm.lifetime.start.p0(i64 24, ptr nonnull %.dep.arr.addr5.i), !dbg !650
  call void @llvm.dbg.value(metadata ptr %.global_tid., metadata !653, metadata !DIExpression()), !dbg !659
  call void @llvm.dbg.value(metadata ptr poison, metadata !654, metadata !DIExpression()), !dbg !659
  call void @llvm.dbg.value(metadata ptr %x, metadata !655, metadata !DIExpression()), !dbg !659
  call void @llvm.dbg.value(metadata ptr %res, metadata !656, metadata !DIExpression()), !dbg !659
  call void @llvm.dbg.value(metadata ptr %y, metadata !657, metadata !DIExpression()), !dbg !659
  %0 = load i32, ptr %.global_tid., align 4, !dbg !650, !tbaa !275, !alias.scope !646
  %1 = tail call i32 @__kmpc_single(ptr nonnull @14, i32 %0), !dbg !650, !noalias !646
  %.not.i = icmp eq i32 %1, 0, !dbg !650
  br i1 %.not.i, label %main.omp_outlined_debug__.7.exit, label %omp_if.then.i, !dbg !650

omp_if.then.i:                                    ; preds = %entry
  %2 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @16, i32 %0, i32 1, i64 40, i64 16, ptr nonnull @.omp_task_entry..9), !dbg !660, !noalias !646
  %3 = load ptr, ptr %2, align 8, !dbg !660, !tbaa !340, !noalias !646
  store ptr %res, ptr %3, align 8, !dbg !660, !tbaa.struct !516, !noalias !646
  %agg.captured.sroa.2.0..sroa_idx.i = getelementptr inbounds i8, ptr %3, i64 8, !dbg !660
  store ptr %x, ptr %agg.captured.sroa.2.0..sroa_idx.i, align 8, !dbg !660, !tbaa.struct !517, !noalias !646
  %4 = ptrtoint ptr %x to i64, !dbg !660
  store i64 %4, ptr %.dep.arr.addr.i, align 8, !dbg !660, !tbaa !518, !noalias !646
  %5 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr.i, i64 0, i32 1, !dbg !660
  store i64 4, ptr %5, align 8, !dbg !660, !tbaa !521, !noalias !646
  %6 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr.i, i64 0, i32 2, !dbg !660
  store i8 1, ptr %6, align 8, !dbg !660, !tbaa !522, !noalias !646
  %7 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @16, i32 %0, ptr nonnull %2, i32 1, ptr nonnull %.dep.arr.addr.i, i32 0, ptr null), !dbg !660, !noalias !646
  %8 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @18, i32 %0, i32 1, i64 40, i64 16, ptr nonnull @.omp_task_entry..11), !dbg !663, !noalias !646
  %9 = load ptr, ptr %8, align 8, !dbg !663, !tbaa !340, !noalias !646
  store ptr %res, ptr %9, align 8, !dbg !663, !tbaa.struct !516, !noalias !646
  %agg.captured1.sroa.2.0..sroa_idx.i = getelementptr inbounds i8, ptr %9, i64 8, !dbg !663
  store ptr %y, ptr %agg.captured1.sroa.2.0..sroa_idx.i, align 8, !dbg !663, !tbaa.struct !517, !noalias !646
  %10 = ptrtoint ptr %y to i64, !dbg !663
  store i64 %10, ptr %.dep.arr.addr2.i, align 8, !dbg !663, !tbaa !518, !noalias !646
  %11 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr2.i, i64 0, i32 1, !dbg !663
  store i64 4, ptr %11, align 8, !dbg !663, !tbaa !521, !noalias !646
  %12 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr2.i, i64 0, i32 2, !dbg !663
  store i8 1, ptr %12, align 8, !dbg !663, !tbaa !522, !noalias !646
  %13 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @18, i32 %0, ptr nonnull %8, i32 1, ptr nonnull %.dep.arr.addr2.i, i32 0, ptr null), !dbg !663, !noalias !646
  %14 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @20, i32 %0, i32 1, i64 40, i64 8, ptr nonnull @.omp_task_entry..13), !dbg !664, !noalias !646
  %15 = load ptr, ptr %14, align 8, !dbg !664, !tbaa !340, !noalias !646
  store ptr %res, ptr %15, align 8, !dbg !664, !tbaa.struct !517, !noalias !646
  %16 = ptrtoint ptr %res to i64, !dbg !664
  store i64 %16, ptr %.dep.arr.addr5.i, align 8, !dbg !664, !tbaa !518, !noalias !646
  %17 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr5.i, i64 0, i32 1, !dbg !664
  store i64 4, ptr %17, align 8, !dbg !664, !tbaa !521, !noalias !646
  %18 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr5.i, i64 0, i32 2, !dbg !664
  store i8 1, ptr %18, align 8, !dbg !664, !tbaa !522, !noalias !646
  %19 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @20, i32 %0, ptr nonnull %14, i32 1, ptr nonnull %.dep.arr.addr5.i, i32 0, ptr null), !dbg !664, !noalias !646
  call void @__kmpc_end_single(ptr nonnull @14, i32 %0), !dbg !665, !noalias !646
  br label %main.omp_outlined_debug__.7.exit, !dbg !665

main.omp_outlined_debug__.7.exit:                 ; preds = %entry, %omp_if.then.i
  call void @__kmpc_barrier(ptr nonnull @21, i32 %0), !dbg !666, !noalias !646
  call void @llvm.lifetime.end.p0(i64 24, ptr nonnull %.dep.arr.addr.i), !dbg !667
  call void @llvm.lifetime.end.p0(i64 24, ptr nonnull %.dep.arr.addr2.i), !dbg !667
  call void @llvm.lifetime.end.p0(i64 24, ptr nonnull %.dep.arr.addr5.i), !dbg !667
  ret void, !dbg !649
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata, metadata) #1

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.value(metadata, metadata, metadata) #1

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #12

attributes #0 = { mustprogress nofree noinline norecurse nosync nounwind willreturn memory(argmem: readwrite) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
attributes #2 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #3 = { mustprogress norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { convergent nounwind }
attributes #6 = { norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #7 = { nounwind }
attributes #8 = { mustprogress nofree norecurse nosync nounwind willreturn uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #9 = { alwaysinline norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #10 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #11 = { nofree norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #12 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }

!llvm.dbg.cu = !{!8}
!llvm.module.flags = !{!258, !259, !260, !261, !262, !263, !264, !265}
!llvm.ident = !{!266}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(scope: null, file: !2, line: 58, type: !3, isLocal: true, isDefinition: true)
!2 = !DIFile(filename: "taskwithdeps.cpp", directory: "/home/randres/projects/carts/examples/taskwithdeps", checksumkind: CSK_MD5, checksum: "94f4e015cbc0b22744f447aab3db52b5")
!3 = !DICompositeType(tag: DW_TAG_array_type, baseType: !4, size: 24, elements: !6)
!4 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !5)
!5 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!6 = !{!7}
!7 = !DISubrange(count: 3)
!8 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !2, producer: "clang version 18.1.8", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, globals: !9, imports: !10, splitDebugInlining: false, nameTableKind: None)
!9 = !{!0}
!10 = !{!11, !19, !23, !30, !34, !42, !47, !49, !56, !60, !64, !75, !77, !81, !85, !89, !94, !98, !102, !106, !110, !118, !122, !126, !128, !132, !136, !141, !147, !151, !155, !157, !165, !169, !177, !179, !183, !187, !191, !195, !200, !205, !210, !211, !212, !213, !215, !216, !217, !218, !219, !220, !221, !223, !224, !225, !226, !227, !228, !229, !234, !235, !236, !237, !238, !239, !240, !241, !242, !243, !244, !245, !246, !247, !248, !249, !250, !251, !252, !253, !254, !255, !256, !257}
!11 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !13, file: !18, line: 52)
!12 = !DINamespace(name: "std", scope: null)
!13 = !DISubprogram(name: "abs", scope: !14, file: !14, line: 848, type: !15, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!14 = !DIFile(filename: "/usr/include/stdlib.h", directory: "", checksumkind: CSK_MD5, checksum: "02258fad21adf111bb9df9825e61954a")
!15 = !DISubroutineType(types: !16)
!16 = !{!17, !17}
!17 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!18 = !DIFile(filename: "/usr/lib/gcc/x86_64-linux-gnu/11/../../../../include/c++/11/bits/std_abs.h", directory: "")
!19 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !20, file: !22, line: 127)
!20 = !DIDerivedType(tag: DW_TAG_typedef, name: "div_t", file: !14, line: 63, baseType: !21)
!21 = distinct !DICompositeType(tag: DW_TAG_structure_type, file: !14, line: 59, size: 64, flags: DIFlagFwdDecl, identifier: "_ZTS5div_t")
!22 = !DIFile(filename: "/usr/lib/gcc/x86_64-linux-gnu/11/../../../../include/c++/11/cstdlib", directory: "")
!23 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !24, file: !22, line: 128)
!24 = !DIDerivedType(tag: DW_TAG_typedef, name: "ldiv_t", file: !14, line: 71, baseType: !25)
!25 = distinct !DICompositeType(tag: DW_TAG_structure_type, file: !14, line: 67, size: 128, flags: DIFlagTypePassByValue, elements: !26, identifier: "_ZTS6ldiv_t")
!26 = !{!27, !29}
!27 = !DIDerivedType(tag: DW_TAG_member, name: "quot", scope: !25, file: !14, line: 69, baseType: !28, size: 64)
!28 = !DIBasicType(name: "long", size: 64, encoding: DW_ATE_signed)
!29 = !DIDerivedType(tag: DW_TAG_member, name: "rem", scope: !25, file: !14, line: 70, baseType: !28, size: 64, offset: 64)
!30 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !31, file: !22, line: 130)
!31 = !DISubprogram(name: "abort", scope: !14, file: !14, line: 598, type: !32, flags: DIFlagPrototyped | DIFlagNoReturn, spFlags: DISPFlagOptimized)
!32 = !DISubroutineType(types: !33)
!33 = !{null}
!34 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !35, file: !22, line: 132)
!35 = !DISubprogram(name: "aligned_alloc", scope: !14, file: !14, line: 592, type: !36, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!36 = !DISubroutineType(types: !37)
!37 = !{!38, !39, !39}
!38 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64)
!39 = !DIDerivedType(tag: DW_TAG_typedef, name: "size_t", file: !40, line: 18, baseType: !41)
!40 = !DIFile(filename: ".install/llvm/lib/clang/18/include/__stddef_size_t.h", directory: "/home/randres/projects/carts", checksumkind: CSK_MD5, checksum: "2c44e821a2b1951cde2eb0fb2e656867")
!41 = !DIBasicType(name: "unsigned long", size: 64, encoding: DW_ATE_unsigned)
!42 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !43, file: !22, line: 134)
!43 = !DISubprogram(name: "atexit", scope: !14, file: !14, line: 602, type: !44, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!44 = !DISubroutineType(types: !45)
!45 = !{!17, !46}
!46 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !32, size: 64)
!47 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !48, file: !22, line: 137)
!48 = !DISubprogram(name: "at_quick_exit", scope: !14, file: !14, line: 607, type: !44, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!49 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !50, file: !22, line: 140)
!50 = !DISubprogram(name: "atof", scope: !51, file: !51, line: 25, type: !52, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!51 = !DIFile(filename: "/usr/include/x86_64-linux-gnu/bits/stdlib-float.h", directory: "", checksumkind: CSK_MD5, checksum: "adfe1626ff4efc68ac58c367ff5f206b")
!52 = !DISubroutineType(types: !53)
!53 = !{!54, !55}
!54 = !DIBasicType(name: "double", size: 64, encoding: DW_ATE_float)
!55 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !4, size: 64)
!56 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !57, file: !22, line: 141)
!57 = !DISubprogram(name: "atoi", scope: !14, file: !14, line: 362, type: !58, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!58 = !DISubroutineType(types: !59)
!59 = !{!17, !55}
!60 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !61, file: !22, line: 142)
!61 = !DISubprogram(name: "atol", scope: !14, file: !14, line: 367, type: !62, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!62 = !DISubroutineType(types: !63)
!63 = !{!28, !55}
!64 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !65, file: !22, line: 143)
!65 = !DISubprogram(name: "bsearch", scope: !66, file: !66, line: 20, type: !67, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!66 = !DIFile(filename: "/usr/include/x86_64-linux-gnu/bits/stdlib-bsearch.h", directory: "", checksumkind: CSK_MD5, checksum: "724ededa330cc3e0cbd34c5b4030a6f6")
!67 = !DISubroutineType(types: !68)
!68 = !{!38, !69, !69, !39, !39, !71}
!69 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !70, size: 64)
!70 = !DIDerivedType(tag: DW_TAG_const_type, baseType: null)
!71 = !DIDerivedType(tag: DW_TAG_typedef, name: "__compar_fn_t", file: !14, line: 816, baseType: !72)
!72 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !73, size: 64)
!73 = !DISubroutineType(types: !74)
!74 = !{!17, !69, !69}
!75 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !76, file: !22, line: 144)
!76 = !DISubprogram(name: "calloc", scope: !14, file: !14, line: 543, type: !36, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!77 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !78, file: !22, line: 145)
!78 = !DISubprogram(name: "div", scope: !14, file: !14, line: 860, type: !79, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!79 = !DISubroutineType(types: !80)
!80 = !{!20, !17, !17}
!81 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !82, file: !22, line: 146)
!82 = !DISubprogram(name: "exit", scope: !14, file: !14, line: 624, type: !83, flags: DIFlagPrototyped | DIFlagNoReturn, spFlags: DISPFlagOptimized)
!83 = !DISubroutineType(types: !84)
!84 = !{null, !17}
!85 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !86, file: !22, line: 147)
!86 = !DISubprogram(name: "free", scope: !14, file: !14, line: 555, type: !87, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!87 = !DISubroutineType(types: !88)
!88 = !{null, !38}
!89 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !90, file: !22, line: 148)
!90 = !DISubprogram(name: "getenv", scope: !14, file: !14, line: 641, type: !91, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!91 = !DISubroutineType(types: !92)
!92 = !{!93, !55}
!93 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !5, size: 64)
!94 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !95, file: !22, line: 149)
!95 = !DISubprogram(name: "labs", scope: !14, file: !14, line: 849, type: !96, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!96 = !DISubroutineType(types: !97)
!97 = !{!28, !28}
!98 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !99, file: !22, line: 150)
!99 = !DISubprogram(name: "ldiv", scope: !14, file: !14, line: 862, type: !100, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!100 = !DISubroutineType(types: !101)
!101 = !{!24, !28, !28}
!102 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !103, file: !22, line: 151)
!103 = !DISubprogram(name: "malloc", scope: !14, file: !14, line: 540, type: !104, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!104 = !DISubroutineType(types: !105)
!105 = !{!38, !39}
!106 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !107, file: !22, line: 153)
!107 = !DISubprogram(name: "mblen", scope: !14, file: !14, line: 930, type: !108, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!108 = !DISubroutineType(types: !109)
!109 = !{!17, !55, !39}
!110 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !111, file: !22, line: 154)
!111 = !DISubprogram(name: "mbstowcs", scope: !14, file: !14, line: 941, type: !112, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!112 = !DISubroutineType(types: !113)
!113 = !{!39, !114, !117, !39}
!114 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !115)
!115 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !116, size: 64)
!116 = !DIBasicType(name: "wchar_t", size: 32, encoding: DW_ATE_signed)
!117 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !55)
!118 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !119, file: !22, line: 155)
!119 = !DISubprogram(name: "mbtowc", scope: !14, file: !14, line: 933, type: !120, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!120 = !DISubroutineType(types: !121)
!121 = !{!17, !114, !117, !39}
!122 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !123, file: !22, line: 157)
!123 = !DISubprogram(name: "qsort", scope: !14, file: !14, line: 838, type: !124, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!124 = !DISubroutineType(types: !125)
!125 = !{null, !38, !39, !39, !71}
!126 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !127, file: !22, line: 160)
!127 = !DISubprogram(name: "quick_exit", scope: !14, file: !14, line: 630, type: !83, flags: DIFlagPrototyped | DIFlagNoReturn, spFlags: DISPFlagOptimized)
!128 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !129, file: !22, line: 163)
!129 = !DISubprogram(name: "rand", scope: !14, file: !14, line: 454, type: !130, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!130 = !DISubroutineType(types: !131)
!131 = !{!17}
!132 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !133, file: !22, line: 164)
!133 = !DISubprogram(name: "realloc", scope: !14, file: !14, line: 551, type: !134, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!134 = !DISubroutineType(types: !135)
!135 = !{!38, !38, !39}
!136 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !137, file: !22, line: 165)
!137 = !DISubprogram(name: "srand", scope: !14, file: !14, line: 456, type: !138, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!138 = !DISubroutineType(types: !139)
!139 = !{null, !140}
!140 = !DIBasicType(name: "unsigned int", size: 32, encoding: DW_ATE_unsigned)
!141 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !142, file: !22, line: 166)
!142 = !DISubprogram(name: "strtod", scope: !14, file: !14, line: 118, type: !143, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!143 = !DISubroutineType(types: !144)
!144 = !{!54, !117, !145}
!145 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !146)
!146 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !93, size: 64)
!147 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !148, file: !22, line: 167)
!148 = !DISubprogram(name: "strtol", scope: !14, file: !14, line: 177, type: !149, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!149 = !DISubroutineType(types: !150)
!150 = !{!28, !117, !145, !17}
!151 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !152, file: !22, line: 168)
!152 = !DISubprogram(name: "strtoul", scope: !14, file: !14, line: 181, type: !153, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!153 = !DISubroutineType(types: !154)
!154 = !{!41, !117, !145, !17}
!155 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !156, file: !22, line: 169)
!156 = !DISubprogram(name: "system", scope: !14, file: !14, line: 791, type: !58, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!157 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !158, file: !22, line: 171)
!158 = !DISubprogram(name: "wcstombs", scope: !14, file: !14, line: 945, type: !159, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!159 = !DISubroutineType(types: !160)
!160 = !{!39, !161, !162, !39}
!161 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !93)
!162 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !163)
!163 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !164, size: 64)
!164 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !116)
!165 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !166, file: !22, line: 172)
!166 = !DISubprogram(name: "wctomb", scope: !14, file: !14, line: 937, type: !167, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!167 = !DISubroutineType(types: !168)
!168 = !{!17, !93, !116}
!169 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !170, entity: !171, file: !22, line: 200)
!170 = !DINamespace(name: "__gnu_cxx", scope: null)
!171 = !DIDerivedType(tag: DW_TAG_typedef, name: "lldiv_t", file: !14, line: 81, baseType: !172)
!172 = distinct !DICompositeType(tag: DW_TAG_structure_type, file: !14, line: 77, size: 128, flags: DIFlagTypePassByValue, elements: !173, identifier: "_ZTS7lldiv_t")
!173 = !{!174, !176}
!174 = !DIDerivedType(tag: DW_TAG_member, name: "quot", scope: !172, file: !14, line: 79, baseType: !175, size: 64)
!175 = !DIBasicType(name: "long long", size: 64, encoding: DW_ATE_signed)
!176 = !DIDerivedType(tag: DW_TAG_member, name: "rem", scope: !172, file: !14, line: 80, baseType: !175, size: 64, offset: 64)
!177 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !170, entity: !178, file: !22, line: 206)
!178 = !DISubprogram(name: "_Exit", scope: !14, file: !14, line: 636, type: !83, flags: DIFlagPrototyped | DIFlagNoReturn, spFlags: DISPFlagOptimized)
!179 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !170, entity: !180, file: !22, line: 210)
!180 = !DISubprogram(name: "llabs", scope: !14, file: !14, line: 852, type: !181, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!181 = !DISubroutineType(types: !182)
!182 = !{!175, !175}
!183 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !170, entity: !184, file: !22, line: 216)
!184 = !DISubprogram(name: "lldiv", scope: !14, file: !14, line: 866, type: !185, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!185 = !DISubroutineType(types: !186)
!186 = !{!171, !175, !175}
!187 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !170, entity: !188, file: !22, line: 227)
!188 = !DISubprogram(name: "atoll", scope: !14, file: !14, line: 374, type: !189, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!189 = !DISubroutineType(types: !190)
!190 = !{!175, !55}
!191 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !170, entity: !192, file: !22, line: 228)
!192 = !DISubprogram(name: "strtoll", scope: !14, file: !14, line: 201, type: !193, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!193 = !DISubroutineType(types: !194)
!194 = !{!175, !117, !145, !17}
!195 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !170, entity: !196, file: !22, line: 229)
!196 = !DISubprogram(name: "strtoull", scope: !14, file: !14, line: 206, type: !197, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!197 = !DISubroutineType(types: !198)
!198 = !{!199, !117, !145, !17}
!199 = !DIBasicType(name: "unsigned long long", size: 64, encoding: DW_ATE_unsigned)
!200 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !170, entity: !201, file: !22, line: 231)
!201 = !DISubprogram(name: "strtof", scope: !14, file: !14, line: 124, type: !202, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!202 = !DISubroutineType(types: !203)
!203 = !{!204, !117, !145}
!204 = !DIBasicType(name: "float", size: 32, encoding: DW_ATE_float)
!205 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !170, entity: !206, file: !22, line: 232)
!206 = !DISubprogram(name: "strtold", scope: !14, file: !14, line: 127, type: !207, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!207 = !DISubroutineType(types: !208)
!208 = !{!209, !117, !145}
!209 = !DIBasicType(name: "long double", size: 128, encoding: DW_ATE_float)
!210 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !171, file: !22, line: 240)
!211 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !178, file: !22, line: 242)
!212 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !180, file: !22, line: 244)
!213 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !214, file: !22, line: 245)
!214 = !DISubprogram(name: "div", linkageName: "_ZN9__gnu_cxx3divExx", scope: !170, file: !22, line: 213, type: !185, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!215 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !184, file: !22, line: 246)
!216 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !188, file: !22, line: 248)
!217 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !201, file: !22, line: 249)
!218 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !192, file: !22, line: 250)
!219 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !196, file: !22, line: 251)
!220 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !12, entity: !206, file: !22, line: 252)
!221 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !31, file: !222, line: 38)
!222 = !DIFile(filename: "/usr/lib/gcc/x86_64-linux-gnu/11/../../../../include/c++/11/stdlib.h", directory: "", checksumkind: CSK_MD5, checksum: "0f5b773a303c24013fb112082e6d18a5")
!223 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !43, file: !222, line: 39)
!224 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !82, file: !222, line: 40)
!225 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !48, file: !222, line: 43)
!226 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !127, file: !222, line: 46)
!227 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !20, file: !222, line: 51)
!228 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !24, file: !222, line: 52)
!229 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !230, file: !222, line: 54)
!230 = !DISubprogram(name: "abs", linkageName: "_ZSt3absg", scope: !12, file: !18, line: 103, type: !231, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!231 = !DISubroutineType(types: !232)
!232 = !{!233, !233}
!233 = !DIBasicType(name: "__float128", size: 128, encoding: DW_ATE_float)
!234 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !50, file: !222, line: 55)
!235 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !57, file: !222, line: 56)
!236 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !61, file: !222, line: 57)
!237 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !65, file: !222, line: 58)
!238 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !76, file: !222, line: 59)
!239 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !214, file: !222, line: 60)
!240 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !86, file: !222, line: 61)
!241 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !90, file: !222, line: 62)
!242 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !95, file: !222, line: 63)
!243 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !99, file: !222, line: 64)
!244 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !103, file: !222, line: 65)
!245 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !107, file: !222, line: 67)
!246 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !111, file: !222, line: 68)
!247 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !119, file: !222, line: 69)
!248 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !123, file: !222, line: 71)
!249 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !129, file: !222, line: 72)
!250 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !133, file: !222, line: 73)
!251 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !137, file: !222, line: 74)
!252 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !142, file: !222, line: 75)
!253 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !148, file: !222, line: 76)
!254 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !152, file: !222, line: 77)
!255 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !156, file: !222, line: 78)
!256 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !158, file: !222, line: 80)
!257 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !166, file: !222, line: 81)
!258 = !{i32 7, !"Dwarf Version", i32 5}
!259 = !{i32 2, !"Debug Info Version", i32 3}
!260 = !{i32 1, !"wchar_size", i32 4}
!261 = !{i32 7, !"openmp", i32 51}
!262 = !{i32 8, !"PIC Level", i32 2}
!263 = !{i32 7, !"PIE Level", i32 2}
!264 = !{i32 7, !"uwtable", i32 2}
!265 = !{i32 7, !"debug-info-assignment-tracking", i1 true}
!266 = !{!"clang version 18.1.8"}
!267 = distinct !DISubprogram(name: "long_computation", linkageName: "_Z16long_computationRi", scope: !2, file: !2, line: 12, type: !268, scopeLine: 12, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !8, retainedNodes: !271)
!268 = !DISubroutineType(types: !269)
!269 = !{null, !270}
!270 = !DIDerivedType(tag: DW_TAG_reference_type, baseType: !17, size: 64)
!271 = !{!272, !273}
!272 = !DILocalVariable(name: "x", arg: 1, scope: !267, file: !2, line: 12, type: !270)
!273 = !DILocalVariable(name: "i", scope: !267, file: !2, line: 13, type: !17)
!274 = !DILocation(line: 0, scope: !267)
!275 = !{!276, !276, i64 0}
!276 = !{!"int", !277, i64 0}
!277 = !{!"omnipotent char", !278, i64 0}
!278 = !{!"Simple C++ TBAA"}
!279 = !DILocation(line: 14, column: 3, scope: !280)
!280 = distinct !DILexicalBlock(scope: !267, file: !2, line: 14, column: 3)
!281 = !DILocation(line: 15, column: 7, scope: !282)
!282 = distinct !DILexicalBlock(scope: !283, file: !2, line: 14, column: 33)
!283 = distinct !DILexicalBlock(scope: !280, file: !2, line: 14, column: 3)
!284 = !DILocation(line: 17, column: 1, scope: !267)
!285 = distinct !DISubprogram(name: "short_computation", linkageName: "_Z17short_computationRi", scope: !2, file: !2, line: 20, type: !268, scopeLine: 20, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !8, retainedNodes: !286)
!286 = !{!287, !288}
!287 = !DILocalVariable(name: "x", arg: 1, scope: !285, file: !2, line: 20, type: !270)
!288 = !DILocalVariable(name: "i", scope: !285, file: !2, line: 21, type: !17)
!289 = !DILocation(line: 0, scope: !285)
!290 = !DILocation(line: 22, column: 3, scope: !291)
!291 = distinct !DILexicalBlock(scope: !285, file: !2, line: 22, column: 3)
!292 = !DILocation(line: 23, column: 7, scope: !293)
!293 = distinct !DILexicalBlock(scope: !294, file: !2, line: 22, column: 30)
!294 = distinct !DILexicalBlock(scope: !291, file: !2, line: 22, column: 3)
!295 = !DILocation(line: 25, column: 1, scope: !285)
!296 = distinct !DISubprogram(name: "main", scope: !2, file: !2, line: 28, type: !130, scopeLine: 28, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !8, retainedNodes: !297)
!297 = !{!298, !299, !300}
!298 = !DILocalVariable(name: "x", scope: !296, file: !2, line: 30, type: !17)
!299 = !DILocalVariable(name: "y", scope: !296, file: !2, line: 30, type: !17)
!300 = !DILocalVariable(name: "res", scope: !296, file: !2, line: 30, type: !17)
!301 = distinct !DIAssignID()
!302 = !DILocation(line: 0, scope: !296)
!303 = distinct !DIAssignID()
!304 = distinct !DIAssignID()
!305 = !DILocation(line: 29, column: 9, scope: !296)
!306 = !DILocation(line: 29, column: 3, scope: !296)
!307 = !DILocation(line: 30, column: 3, scope: !296)
!308 = !DILocation(line: 30, column: 11, scope: !296)
!309 = !DILocation(line: 30, column: 7, scope: !296)
!310 = distinct !DIAssignID()
!311 = !DILocation(line: 30, column: 23, scope: !296)
!312 = !DILocation(line: 30, column: 19, scope: !296)
!313 = distinct !DIAssignID()
!314 = !DILocation(line: 30, column: 31, scope: !296)
!315 = distinct !DIAssignID()
!316 = !DILocation(line: 31, column: 3, scope: !296)
!317 = !DILocation(line: 47, column: 3, scope: !296)
!318 = !DILocation(line: 61, column: 1, scope: !296)
!319 = !DILocation(line: 60, column: 3, scope: !296)
!320 = !DISubprogram(name: "time", scope: !321, file: !321, line: 76, type: !322, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!321 = !DIFile(filename: "/usr/include/time.h", directory: "", checksumkind: CSK_MD5, checksum: "db37158473a25e1d89b19f8bc6892801")
!322 = !DISubroutineType(types: !323)
!323 = !{!324, !328}
!324 = !DIDerivedType(tag: DW_TAG_typedef, name: "time_t", file: !325, line: 10, baseType: !326)
!325 = !DIFile(filename: "/usr/include/x86_64-linux-gnu/bits/types/time_t.h", directory: "", checksumkind: CSK_MD5, checksum: "5c299a4954617c88bb03645c7864e1b1")
!326 = !DIDerivedType(tag: DW_TAG_typedef, name: "__time_t", file: !327, line: 160, baseType: !28)
!327 = !DIFile(filename: "/usr/include/x86_64-linux-gnu/bits/types.h", directory: "", checksumkind: CSK_MD5, checksum: "d108b5f93a74c50510d7d9bc0ab36df9")
!328 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !324, size: 64)
!329 = distinct !DISubprogram(linkageName: ".omp_task_entry.", scope: !2, file: !2, line: 34, type: !330, scopeLine: 34, flags: DIFlagArtificial | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !8, retainedNodes: !332)
!330 = !DISubroutineType(types: !331)
!331 = !{}
!332 = !{!333, !334}
!333 = !DILocalVariable(arg: 1, scope: !329, type: !17, flags: DIFlagArtificial)
!334 = !DILocalVariable(arg: 2, scope: !329, type: !335, flags: DIFlagArtificial)
!335 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !336)
!336 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !337, size: 64)
!337 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "kmp_task_t_with_privates", file: !2, size: 320, flags: DIFlagFwdDecl, identifier: "_ZTS24kmp_task_t_with_privates")
!338 = !DILocation(line: 0, scope: !329)
!339 = !DILocation(line: 34, column: 7, scope: !329)
!340 = !{!341, !343, i64 0}
!341 = !{!"_ZTS24kmp_task_t_with_privates", !342, i64 0}
!342 = !{!"_ZTS10kmp_task_t", !343, i64 0, !343, i64 8, !276, i64 16, !277, i64 24, !277, i64 32}
!343 = !{!"any pointer", !277, i64 0}
!344 = !{!345}
!345 = distinct !{!345, !346, !".omp_outlined.: %__context"}
!346 = distinct !{!346, !".omp_outlined."}
!347 = !DILocalVariable(name: ".global_tid.", arg: 1, scope: !348, type: !351, flags: DIFlagArtificial)
!348 = distinct !DISubprogram(name: ".omp_outlined.", scope: !2, file: !2, type: !349, scopeLine: 35, flags: DIFlagArtificial | DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !8, retainedNodes: !367)
!349 = !DISubroutineType(types: !350)
!350 = !{null, !351, !352, !355, !357, !362, !363}
!351 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !17)
!352 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !353)
!353 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !354)
!354 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !351, size: 64)
!355 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !356)
!356 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !38)
!357 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !358)
!358 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !359)
!359 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !360, size: 64)
!360 = !DISubroutineType(types: !361)
!361 = !{null, !355, null}
!362 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !38)
!363 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !364)
!364 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !365)
!365 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !366, size: 64)
!366 = !DICompositeType(tag: DW_TAG_structure_type, file: !2, line: 34, size: 192, flags: DIFlagFwdDecl)
!367 = !{!347, !368, !369, !370, !371, !372, !373, !374, !375}
!368 = !DILocalVariable(name: ".part_id.", arg: 2, scope: !348, type: !352, flags: DIFlagArtificial)
!369 = !DILocalVariable(name: ".privates.", arg: 3, scope: !348, type: !355, flags: DIFlagArtificial)
!370 = !DILocalVariable(name: ".copy_fn.", arg: 4, scope: !348, type: !357, flags: DIFlagArtificial)
!371 = !DILocalVariable(name: ".task_t.", arg: 5, scope: !348, type: !362, flags: DIFlagArtificial)
!372 = !DILocalVariable(name: "__context", arg: 6, scope: !348, type: !363, flags: DIFlagArtificial)
!373 = !DILocalVariable(name: "res", scope: !348, file: !2, line: 30, type: !17)
!374 = !DILocalVariable(name: "y", scope: !348, file: !2, line: 30, type: !17)
!375 = !DILocalVariable(name: "x", scope: !348, file: !2, line: 30, type: !17)
!376 = !DILocation(line: 0, scope: !348, inlinedAt: !377)
!377 = distinct !DILocation(line: 34, column: 7, scope: !329)
!378 = !DILocation(line: 30, column: 31, scope: !348, inlinedAt: !377)
!379 = !DILocation(line: 30, column: 19, scope: !348, inlinedAt: !377)
!380 = !DILocation(line: 30, column: 7, scope: !348, inlinedAt: !377)
!381 = !DILocation(line: 36, column: 11, scope: !382, inlinedAt: !377)
!382 = distinct !DILexicalBlock(scope: !348, file: !2, line: 35, column: 7)
!383 = !{!384, !343, i64 0}
!384 = !{!"_ZTSZ4mainE3$_0", !343, i64 0, !343, i64 8, !343, i64 16}
!385 = !DILocation(line: 36, column: 15, scope: !382, inlinedAt: !377)
!386 = !DILocation(line: 37, column: 15, scope: !382, inlinedAt: !377)
!387 = !DILocation(line: 37, column: 11, scope: !382, inlinedAt: !377)
!388 = !{!384, !343, i64 8}
!389 = !DILocation(line: 37, column: 13, scope: !382, inlinedAt: !377)
!390 = !DILocation(line: 38, column: 15, scope: !382, inlinedAt: !377)
!391 = !DILocation(line: 38, column: 11, scope: !382, inlinedAt: !377)
!392 = !{!384, !343, i64 16}
!393 = !DILocation(line: 38, column: 13, scope: !382, inlinedAt: !377)
!394 = distinct !DISubprogram(linkageName: ".omp_task_entry..2", scope: !2, file: !2, line: 40, type: !330, scopeLine: 40, flags: DIFlagArtificial | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !8, retainedNodes: !395)
!395 = !{!396, !397}
!396 = !DILocalVariable(arg: 1, scope: !394, type: !17, flags: DIFlagArtificial)
!397 = !DILocalVariable(arg: 2, scope: !394, type: !335, flags: DIFlagArtificial)
!398 = !DILocation(line: 0, scope: !394)
!399 = !DILocation(line: 40, column: 7, scope: !394)
!400 = !{!401}
!401 = distinct !{!401, !402, !".omp_outlined..1: %__context"}
!402 = distinct !{!402, !".omp_outlined..1"}
!403 = !DILocalVariable(name: ".global_tid.", arg: 1, scope: !404, type: !351, flags: DIFlagArtificial)
!404 = distinct !DISubprogram(name: ".omp_outlined..1", scope: !2, file: !2, type: !405, scopeLine: 41, flags: DIFlagArtificial | DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !8, retainedNodes: !411)
!405 = !DISubroutineType(types: !406)
!406 = !{null, !351, !352, !355, !357, !362, !407}
!407 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !408)
!408 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !409)
!409 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !410, size: 64)
!410 = !DICompositeType(tag: DW_TAG_structure_type, file: !2, line: 40, size: 64, flags: DIFlagFwdDecl)
!411 = !{!403, !412, !413, !414, !415, !416, !417}
!412 = !DILocalVariable(name: ".part_id.", arg: 2, scope: !404, type: !352, flags: DIFlagArtificial)
!413 = !DILocalVariable(name: ".privates.", arg: 3, scope: !404, type: !355, flags: DIFlagArtificial)
!414 = !DILocalVariable(name: ".copy_fn.", arg: 4, scope: !404, type: !357, flags: DIFlagArtificial)
!415 = !DILocalVariable(name: ".task_t.", arg: 5, scope: !404, type: !362, flags: DIFlagArtificial)
!416 = !DILocalVariable(name: "__context", arg: 6, scope: !404, type: !407, flags: DIFlagArtificial)
!417 = !DILocalVariable(name: "res", scope: !404, file: !2, line: 30, type: !17)
!418 = !DILocation(line: 0, scope: !404, inlinedAt: !419)
!419 = distinct !DILocation(line: 40, column: 7, scope: !394)
!420 = !DILocation(line: 30, column: 31, scope: !404, inlinedAt: !419)
!421 = !DILocation(line: 41, column: 11, scope: !404, inlinedAt: !419)
!422 = !{!423, !343, i64 0}
!423 = !{!"_ZTSZ4mainE3$_1", !343, i64 0}
!424 = !DILocation(line: 41, column: 15, scope: !404, inlinedAt: !419)
!425 = distinct !DISubprogram(linkageName: ".omp_task_entry..4", scope: !2, file: !2, line: 42, type: !330, scopeLine: 42, flags: DIFlagArtificial | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !8, retainedNodes: !426)
!426 = !{!427, !428}
!427 = !DILocalVariable(arg: 1, scope: !425, type: !17, flags: DIFlagArtificial)
!428 = !DILocalVariable(arg: 2, scope: !425, type: !335, flags: DIFlagArtificial)
!429 = !DILocation(line: 0, scope: !425)
!430 = !DILocation(line: 42, column: 7, scope: !425)
!431 = !{!432}
!432 = distinct !{!432, !433, !".omp_outlined..3: %__context"}
!433 = distinct !{!433, !".omp_outlined..3"}
!434 = !DILocalVariable(name: ".global_tid.", arg: 1, scope: !435, type: !351, flags: DIFlagArtificial)
!435 = distinct !DISubprogram(name: ".omp_outlined..3", scope: !2, file: !2, type: !436, scopeLine: 43, flags: DIFlagArtificial | DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !8, retainedNodes: !442)
!436 = !DISubroutineType(types: !437)
!437 = !{null, !351, !352, !355, !357, !362, !438}
!438 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !439)
!439 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !440)
!440 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !441, size: 64)
!441 = !DICompositeType(tag: DW_TAG_structure_type, file: !2, line: 42, size: 64, flags: DIFlagFwdDecl)
!442 = !{!434, !443, !444, !445, !446, !447, !448}
!443 = !DILocalVariable(name: ".part_id.", arg: 2, scope: !435, type: !352, flags: DIFlagArtificial)
!444 = !DILocalVariable(name: ".privates.", arg: 3, scope: !435, type: !355, flags: DIFlagArtificial)
!445 = !DILocalVariable(name: ".copy_fn.", arg: 4, scope: !435, type: !357, flags: DIFlagArtificial)
!446 = !DILocalVariable(name: ".task_t.", arg: 5, scope: !435, type: !362, flags: DIFlagArtificial)
!447 = !DILocalVariable(name: "__context", arg: 6, scope: !435, type: !438, flags: DIFlagArtificial)
!448 = !DILocalVariable(name: "x", scope: !435, file: !2, line: 30, type: !17)
!449 = !DILocation(line: 0, scope: !435, inlinedAt: !450)
!450 = distinct !DILocation(line: 42, column: 7, scope: !425)
!451 = !DILocation(line: 30, column: 7, scope: !435, inlinedAt: !450)
!452 = !DILocation(line: 43, column: 28, scope: !435, inlinedAt: !450)
!453 = !{!454, !343, i64 0}
!454 = !{!"_ZTSZ4mainE3$_2", !343, i64 0}
!455 = !DILocation(line: 43, column: 11, scope: !435, inlinedAt: !450)
!456 = distinct !DISubprogram(linkageName: ".omp_task_entry..6", scope: !2, file: !2, line: 44, type: !330, scopeLine: 44, flags: DIFlagArtificial | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !8, retainedNodes: !457)
!457 = !{!458, !459}
!458 = !DILocalVariable(arg: 1, scope: !456, type: !17, flags: DIFlagArtificial)
!459 = !DILocalVariable(arg: 2, scope: !456, type: !335, flags: DIFlagArtificial)
!460 = !DILocation(line: 0, scope: !456)
!461 = !DILocation(line: 44, column: 7, scope: !456)
!462 = !{!463}
!463 = distinct !{!463, !464, !".omp_outlined..5: %__context"}
!464 = distinct !{!464, !".omp_outlined..5"}
!465 = !DILocalVariable(name: ".global_tid.", arg: 1, scope: !466, type: !351, flags: DIFlagArtificial)
!466 = distinct !DISubprogram(name: ".omp_outlined..5", scope: !2, file: !2, type: !467, scopeLine: 45, flags: DIFlagArtificial | DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !8, retainedNodes: !473)
!467 = !DISubroutineType(types: !468)
!468 = !{null, !351, !352, !355, !357, !362, !469}
!469 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !470)
!470 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !471)
!471 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !472, size: 64)
!472 = !DICompositeType(tag: DW_TAG_structure_type, file: !2, line: 44, size: 64, flags: DIFlagFwdDecl)
!473 = !{!465, !474, !475, !476, !477, !478, !479}
!474 = !DILocalVariable(name: ".part_id.", arg: 2, scope: !466, type: !352, flags: DIFlagArtificial)
!475 = !DILocalVariable(name: ".privates.", arg: 3, scope: !466, type: !355, flags: DIFlagArtificial)
!476 = !DILocalVariable(name: ".copy_fn.", arg: 4, scope: !466, type: !357, flags: DIFlagArtificial)
!477 = !DILocalVariable(name: ".task_t.", arg: 5, scope: !466, type: !362, flags: DIFlagArtificial)
!478 = !DILocalVariable(name: "__context", arg: 6, scope: !466, type: !469, flags: DIFlagArtificial)
!479 = !DILocalVariable(name: "y", scope: !466, file: !2, line: 30, type: !17)
!480 = !DILocation(line: 0, scope: !466, inlinedAt: !481)
!481 = distinct !DILocation(line: 44, column: 7, scope: !456)
!482 = !DILocation(line: 30, column: 19, scope: !466, inlinedAt: !481)
!483 = !DILocation(line: 45, column: 29, scope: !466, inlinedAt: !481)
!484 = !{!485, !343, i64 0}
!485 = !{!"_ZTSZ4mainE3$_3", !343, i64 0}
!486 = !DILocation(line: 45, column: 11, scope: !466, inlinedAt: !481)
!487 = distinct !DISubprogram(name: "main.omp_outlined", scope: !2, file: !2, line: 31, type: !488, scopeLine: 31, flags: DIFlagArtificial | DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !8, retainedNodes: !490)
!488 = !DISubroutineType(types: !489)
!489 = !{null, !352, !352, !270, !270, !270}
!490 = !{!491, !492, !493, !494, !495}
!491 = !DILocalVariable(name: ".global_tid.", arg: 1, scope: !487, type: !352, flags: DIFlagArtificial)
!492 = !DILocalVariable(name: ".bound_tid.", arg: 2, scope: !487, type: !352, flags: DIFlagArtificial)
!493 = !DILocalVariable(name: "res", arg: 3, scope: !487, type: !270, flags: DIFlagArtificial)
!494 = !DILocalVariable(name: "x", arg: 4, scope: !487, type: !270, flags: DIFlagArtificial)
!495 = !DILocalVariable(name: "y", arg: 5, scope: !487, type: !270, flags: DIFlagArtificial)
!496 = !DILocation(line: 0, scope: !487)
!497 = !{!498}
!498 = distinct !{!498, !499, !"main.omp_outlined_debug__: %.global_tid."}
!499 = distinct !{!499, !"main.omp_outlined_debug__"}
!500 = !DILocation(line: 31, column: 3, scope: !487)
!501 = !DILocation(line: 32, column: 3, scope: !502, inlinedAt: !509)
!502 = distinct !DISubprogram(name: "main.omp_outlined_debug__", scope: !2, file: !2, line: 32, type: !488, scopeLine: 32, flags: DIFlagArtificial | DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !8, retainedNodes: !503)
!503 = !{!504, !505, !506, !507, !508}
!504 = !DILocalVariable(name: ".global_tid.", arg: 1, scope: !502, type: !352, flags: DIFlagArtificial)
!505 = !DILocalVariable(name: ".bound_tid.", arg: 2, scope: !502, type: !352, flags: DIFlagArtificial)
!506 = !DILocalVariable(name: "res", arg: 3, scope: !502, file: !2, line: 30, type: !270)
!507 = !DILocalVariable(name: "x", arg: 4, scope: !502, file: !2, line: 30, type: !270)
!508 = !DILocalVariable(name: "y", arg: 5, scope: !502, file: !2, line: 30, type: !270)
!509 = distinct !DILocation(line: 31, column: 3, scope: !487)
!510 = !DILocation(line: 0, scope: !502, inlinedAt: !509)
!511 = !DILocation(line: 34, column: 7, scope: !512, inlinedAt: !509)
!512 = distinct !DILexicalBlock(scope: !513, file: !2, line: 33, column: 3)
!513 = distinct !DILexicalBlock(scope: !502, file: !2, line: 32, column: 3)
!514 = !{i64 0, i64 8, !515, i64 8, i64 8, !515, i64 16, i64 8, !515}
!515 = !{!343, !343, i64 0}
!516 = !{i64 0, i64 8, !515, i64 8, i64 8, !515}
!517 = !{i64 0, i64 8, !515}
!518 = !{!519, !520, i64 0}
!519 = !{!"_ZTS15kmp_depend_info", !520, i64 0, !520, i64 8, !277, i64 16}
!520 = !{!"long", !277, i64 0}
!521 = !{!519, !520, i64 8}
!522 = !{!519, !277, i64 16}
!523 = !DILocation(line: 40, column: 7, scope: !512, inlinedAt: !509)
!524 = !DILocation(line: 42, column: 7, scope: !512, inlinedAt: !509)
!525 = !DILocation(line: 44, column: 7, scope: !512, inlinedAt: !509)
!526 = !DILocation(line: 46, column: 3, scope: !512, inlinedAt: !509)
!527 = !DILocation(line: 32, column: 21, scope: !513, inlinedAt: !509)
!528 = !DILocation(line: 32, column: 21, scope: !502, inlinedAt: !509)
!529 = !{!530}
!530 = !{i64 2, i64 -1, i64 -1, i1 true}
!531 = distinct !DISubprogram(linkageName: ".omp_task_entry..9", scope: !2, file: !2, line: 50, type: !330, scopeLine: 50, flags: DIFlagArtificial | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !8, retainedNodes: !532)
!532 = !{!533, !534}
!533 = !DILocalVariable(arg: 1, scope: !531, type: !17, flags: DIFlagArtificial)
!534 = !DILocalVariable(arg: 2, scope: !531, type: !335, flags: DIFlagArtificial)
!535 = !DILocation(line: 0, scope: !531)
!536 = !DILocation(line: 50, column: 7, scope: !531)
!537 = !{!538}
!538 = distinct !{!538, !539, !".omp_outlined..8: %__context"}
!539 = distinct !{!539, !".omp_outlined..8"}
!540 = !DILocalVariable(name: ".global_tid.", arg: 1, scope: !541, type: !351, flags: DIFlagArtificial)
!541 = distinct !DISubprogram(name: ".omp_outlined..8", scope: !2, file: !2, type: !542, scopeLine: 51, flags: DIFlagArtificial | DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !8, retainedNodes: !548)
!542 = !DISubroutineType(types: !543)
!543 = !{null, !351, !352, !355, !357, !362, !544}
!544 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !545)
!545 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !546)
!546 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !547, size: 64)
!547 = !DICompositeType(tag: DW_TAG_structure_type, file: !2, line: 50, size: 128, flags: DIFlagFwdDecl)
!548 = !{!540, !549, !550, !551, !552, !553, !554, !555}
!549 = !DILocalVariable(name: ".part_id.", arg: 2, scope: !541, type: !352, flags: DIFlagArtificial)
!550 = !DILocalVariable(name: ".privates.", arg: 3, scope: !541, type: !355, flags: DIFlagArtificial)
!551 = !DILocalVariable(name: ".copy_fn.", arg: 4, scope: !541, type: !357, flags: DIFlagArtificial)
!552 = !DILocalVariable(name: ".task_t.", arg: 5, scope: !541, type: !362, flags: DIFlagArtificial)
!553 = !DILocalVariable(name: "__context", arg: 6, scope: !541, type: !544, flags: DIFlagArtificial)
!554 = !DILocalVariable(name: "x", scope: !541, file: !2, line: 30, type: !17)
!555 = !DILocalVariable(name: "res", scope: !541, file: !2, line: 30, type: !17)
!556 = !DILocation(line: 0, scope: !541, inlinedAt: !557)
!557 = distinct !DILocation(line: 50, column: 7, scope: !531)
!558 = !DILocation(line: 30, column: 7, scope: !541, inlinedAt: !557)
!559 = !DILocation(line: 30, column: 31, scope: !541, inlinedAt: !557)
!560 = !DILocation(line: 52, column: 16, scope: !561, inlinedAt: !557)
!561 = distinct !DILexicalBlock(scope: !541, file: !2, line: 51, column: 7)
!562 = !{!563, !343, i64 8}
!563 = !{!"_ZTSZ4mainE3$_4", !343, i64 0, !343, i64 8}
!564 = !DILocation(line: 52, column: 9, scope: !561, inlinedAt: !557)
!565 = !{!563, !343, i64 0}
!566 = !DILocation(line: 52, column: 13, scope: !561, inlinedAt: !557)
!567 = !DILocation(line: 53, column: 10, scope: !561, inlinedAt: !557)
!568 = distinct !DISubprogram(linkageName: ".omp_task_entry..11", scope: !2, file: !2, line: 55, type: !330, scopeLine: 55, flags: DIFlagArtificial | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !8, retainedNodes: !569)
!569 = !{!570, !571}
!570 = !DILocalVariable(arg: 1, scope: !568, type: !17, flags: DIFlagArtificial)
!571 = !DILocalVariable(arg: 2, scope: !568, type: !335, flags: DIFlagArtificial)
!572 = !DILocation(line: 0, scope: !568)
!573 = !DILocation(line: 55, column: 7, scope: !568)
!574 = !{!575}
!575 = distinct !{!575, !576, !".omp_outlined..10: %__context"}
!576 = distinct !{!576, !".omp_outlined..10"}
!577 = !DILocalVariable(name: ".global_tid.", arg: 1, scope: !578, type: !351, flags: DIFlagArtificial)
!578 = distinct !DISubprogram(name: ".omp_outlined..10", scope: !2, file: !2, type: !579, scopeLine: 56, flags: DIFlagArtificial | DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !8, retainedNodes: !585)
!579 = !DISubroutineType(types: !580)
!580 = !{null, !351, !352, !355, !357, !362, !581}
!581 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !582)
!582 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !583)
!583 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !584, size: 64)
!584 = !DICompositeType(tag: DW_TAG_structure_type, file: !2, line: 55, size: 128, flags: DIFlagFwdDecl)
!585 = !{!577, !586, !587, !588, !589, !590, !591, !592}
!586 = !DILocalVariable(name: ".part_id.", arg: 2, scope: !578, type: !352, flags: DIFlagArtificial)
!587 = !DILocalVariable(name: ".privates.", arg: 3, scope: !578, type: !355, flags: DIFlagArtificial)
!588 = !DILocalVariable(name: ".copy_fn.", arg: 4, scope: !578, type: !357, flags: DIFlagArtificial)
!589 = !DILocalVariable(name: ".task_t.", arg: 5, scope: !578, type: !362, flags: DIFlagArtificial)
!590 = !DILocalVariable(name: "__context", arg: 6, scope: !578, type: !581, flags: DIFlagArtificial)
!591 = !DILocalVariable(name: "res", scope: !578, file: !2, line: 30, type: !17)
!592 = !DILocalVariable(name: "y", scope: !578, file: !2, line: 30, type: !17)
!593 = !DILocation(line: 0, scope: !578, inlinedAt: !594)
!594 = distinct !DILocation(line: 55, column: 7, scope: !568)
!595 = !DILocation(line: 30, column: 31, scope: !578, inlinedAt: !594)
!596 = !DILocation(line: 30, column: 19, scope: !578, inlinedAt: !594)
!597 = !DILocation(line: 56, column: 18, scope: !578, inlinedAt: !594)
!598 = !{!599, !343, i64 8}
!599 = !{!"_ZTSZ4mainE3$_5", !343, i64 0, !343, i64 8}
!600 = !DILocation(line: 56, column: 11, scope: !578, inlinedAt: !594)
!601 = !{!599, !343, i64 0}
!602 = !DILocation(line: 56, column: 15, scope: !578, inlinedAt: !594)
!603 = !DISubprogram(name: "printf", scope: !604, file: !604, line: 356, type: !605, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!604 = !DIFile(filename: "/usr/include/stdio.h", directory: "", checksumkind: CSK_MD5, checksum: "f31eefcc3f15835fc5a4023a625cf609")
!605 = !DISubroutineType(types: !606)
!606 = !{!17, !117, null}
!607 = distinct !DISubprogram(linkageName: ".omp_task_entry..13", scope: !2, file: !2, line: 57, type: !330, scopeLine: 57, flags: DIFlagArtificial | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !8, retainedNodes: !608)
!608 = !{!609, !610}
!609 = !DILocalVariable(arg: 1, scope: !607, type: !17, flags: DIFlagArtificial)
!610 = !DILocalVariable(arg: 2, scope: !607, type: !335, flags: DIFlagArtificial)
!611 = !DILocation(line: 0, scope: !607)
!612 = !DILocation(line: 57, column: 7, scope: !607)
!613 = !{!614}
!614 = distinct !{!614, !615, !".omp_outlined..12: %__context"}
!615 = distinct !{!615, !".omp_outlined..12"}
!616 = !DILocalVariable(name: ".global_tid.", arg: 1, scope: !617, type: !351, flags: DIFlagArtificial)
!617 = distinct !DISubprogram(name: ".omp_outlined..12", scope: !2, file: !2, type: !618, scopeLine: 58, flags: DIFlagArtificial | DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !8, retainedNodes: !624)
!618 = !DISubroutineType(types: !619)
!619 = !{null, !351, !352, !355, !357, !362, !620}
!620 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !621)
!621 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !622)
!622 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !623, size: 64)
!623 = !DICompositeType(tag: DW_TAG_structure_type, file: !2, line: 57, size: 64, flags: DIFlagFwdDecl)
!624 = !{!616, !625, !626, !627, !628, !629, !630}
!625 = !DILocalVariable(name: ".part_id.", arg: 2, scope: !617, type: !352, flags: DIFlagArtificial)
!626 = !DILocalVariable(name: ".privates.", arg: 3, scope: !617, type: !355, flags: DIFlagArtificial)
!627 = !DILocalVariable(name: ".copy_fn.", arg: 4, scope: !617, type: !357, flags: DIFlagArtificial)
!628 = !DILocalVariable(name: ".task_t.", arg: 5, scope: !617, type: !362, flags: DIFlagArtificial)
!629 = !DILocalVariable(name: "__context", arg: 6, scope: !617, type: !620, flags: DIFlagArtificial)
!630 = !DILocalVariable(name: "res", scope: !617, file: !2, line: 30, type: !17)
!631 = !DILocation(line: 0, scope: !617, inlinedAt: !632)
!632 = distinct !DILocation(line: 57, column: 7, scope: !607)
!633 = !DILocation(line: 30, column: 31, scope: !617, inlinedAt: !632)
!634 = !DILocation(line: 58, column: 24, scope: !617, inlinedAt: !632)
!635 = !{!636, !343, i64 0}
!636 = !{!"_ZTSZ4mainE3$_6", !343, i64 0}
!637 = !DILocation(line: 58, column: 11, scope: !617, inlinedAt: !632)
!638 = distinct !DISubprogram(name: "main.omp_outlined.14", scope: !2, file: !2, line: 47, type: !488, scopeLine: 47, flags: DIFlagArtificial | DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !8, retainedNodes: !639)
!639 = !{!640, !641, !642, !643, !644}
!640 = !DILocalVariable(name: ".global_tid.", arg: 1, scope: !638, type: !352, flags: DIFlagArtificial)
!641 = !DILocalVariable(name: ".bound_tid.", arg: 2, scope: !638, type: !352, flags: DIFlagArtificial)
!642 = !DILocalVariable(name: "x", arg: 3, scope: !638, type: !270, flags: DIFlagArtificial)
!643 = !DILocalVariable(name: "res", arg: 4, scope: !638, type: !270, flags: DIFlagArtificial)
!644 = !DILocalVariable(name: "y", arg: 5, scope: !638, type: !270, flags: DIFlagArtificial)
!645 = !DILocation(line: 0, scope: !638)
!646 = !{!647}
!647 = distinct !{!647, !648, !"main.omp_outlined_debug__.7: %.global_tid."}
!648 = distinct !{!648, !"main.omp_outlined_debug__.7"}
!649 = !DILocation(line: 47, column: 3, scope: !638)
!650 = !DILocation(line: 48, column: 3, scope: !651, inlinedAt: !658)
!651 = distinct !DISubprogram(name: "main.omp_outlined_debug__.7", scope: !2, file: !2, line: 48, type: !488, scopeLine: 48, flags: DIFlagArtificial | DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !8, retainedNodes: !652)
!652 = !{!653, !654, !655, !656, !657}
!653 = !DILocalVariable(name: ".global_tid.", arg: 1, scope: !651, type: !352, flags: DIFlagArtificial)
!654 = !DILocalVariable(name: ".bound_tid.", arg: 2, scope: !651, type: !352, flags: DIFlagArtificial)
!655 = !DILocalVariable(name: "x", arg: 3, scope: !651, file: !2, line: 30, type: !270)
!656 = !DILocalVariable(name: "res", arg: 4, scope: !651, file: !2, line: 30, type: !270)
!657 = !DILocalVariable(name: "y", arg: 5, scope: !651, file: !2, line: 30, type: !270)
!658 = distinct !DILocation(line: 47, column: 3, scope: !638)
!659 = !DILocation(line: 0, scope: !651, inlinedAt: !658)
!660 = !DILocation(line: 50, column: 7, scope: !661, inlinedAt: !658)
!661 = distinct !DILexicalBlock(scope: !662, file: !2, line: 49, column: 3)
!662 = distinct !DILexicalBlock(scope: !651, file: !2, line: 48, column: 3)
!663 = !DILocation(line: 55, column: 7, scope: !661, inlinedAt: !658)
!664 = !DILocation(line: 57, column: 7, scope: !661, inlinedAt: !658)
!665 = !DILocation(line: 59, column: 3, scope: !661, inlinedAt: !658)
!666 = !DILocation(line: 48, column: 21, scope: !662, inlinedAt: !658)
!667 = !DILocation(line: 48, column: 21, scope: !651, inlinedAt: !658)


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

-- Function: .omp_task_entry..6
[arts] Function: .omp_task_entry..6 doesn't have CARTS Metadata

-- Function: main.omp_outlined
[arts] Function: main.omp_outlined doesn't have CARTS Metadata

-- Function: .omp_task_entry..9
[arts] Function: .omp_task_entry..9 doesn't have CARTS Metadata

-- Function: .omp_task_entry..11
[arts] Function: .omp_task_entry..11 doesn't have CARTS Metadata

-- Function: .omp_task_entry..13
[arts] Function: .omp_task_entry..13 doesn't have CARTS Metadata

-- Function: main.omp_outlined.14
[arts] Function: main.omp_outlined.14 doesn't have CARTS Metadata

- - - - - - - - - - - - - - - - - - - - - - - - 
[arts-analysis] Computing Graph
[Attributor] Initializing AAEDTInfo: 
[Attributor] Done with 12 functions, result: unchanged.
- - - - - - - - - - - - - - - - - - - - - - - -
[arts-graph] Printing the ARTS Graph

- - - - - - - - - - - - - - - - - - - - - - - -


All EDT Guids have been reserved
opt: /home/randres/projects/carts/carts/src/analysis/ARTSAnalysisPass.cpp:1365: void (anonymous namespace)::ARTSAnalysisPass::generateCode(Module &, ARTSCache &): Assertion `EntryEDTNode && "EntryEDTNode is null!"' failed.
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: opt -load-pass-plugin=/home/randres/projects/carts/.install/carts/lib/ARTSAnalysis.so -debug-only=arts-analysis,arts,carts,arts-ir-builder,arts-graph,carts-metadata,arts-codegen -passes=arts-analysis taskwithdeps_arts_ir.bc -o taskwithdeps_arts_analysis.bc
 #0 0x00007f34fd1bd6b7 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/randres/projects/carts/.install/llvm/lib/libLLVMSupport.so.18.1+0x1926b7)
 #1 0x00007f34fd1bb21e llvm::sys::RunSignalHandlers() (/home/randres/projects/carts/.install/llvm/lib/libLLVMSupport.so.18.1+0x19021e)
 #2 0x00007f34fd1bdd8a SignalHandler(int) Signals.cpp:0:0
 #3 0x00007f34fcb08520 (/lib/x86_64-linux-gnu/libc.so.6+0x42520)
 #4 0x00007f34fcb5c9fc pthread_kill (/lib/x86_64-linux-gnu/libc.so.6+0x969fc)
 #5 0x00007f34fcb08476 gsignal (/lib/x86_64-linux-gnu/libc.so.6+0x42476)
 #6 0x00007f34fcaee7f3 abort (/lib/x86_64-linux-gnu/libc.so.6+0x287f3)
 #7 0x00007f34fcaee71b (/lib/x86_64-linux-gnu/libc.so.6+0x2871b)
 #8 0x00007f34fcaffe96 (/lib/x86_64-linux-gnu/libc.so.6+0x39e96)
 #9 0x00007f34fb85443e (/home/randres/projects/carts/.install/carts/lib/ARTSAnalysis.so+0x2343e)
#10 0x00007f34fd5c3576 llvm::PassManager<llvm::Module, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/randres/projects/carts/.install/llvm/lib/libLLVMCore.so.18.1+0x2c9576)
#11 0x0000563338160acb llvm::runPassPipeline(llvm::StringRef, llvm::Module&, llvm::TargetMachine*, llvm::TargetLibraryInfoImpl*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::ToolOutputFile*, llvm::StringRef, llvm::ArrayRef<llvm::PassPlugin>, llvm::opt_tool::OutputKind, llvm::opt_tool::VerifierKind, bool, bool, bool, bool, bool, bool, bool) (/home/randres/projects/carts/.install/llvm/bin/opt+0x19acb)
#12 0x000056333816e900 main (/home/randres/projects/carts/.install/llvm/bin/opt+0x27900)
#13 0x00007f34fcaefd90 (/lib/x86_64-linux-gnu/libc.so.6+0x29d90)
#14 0x00007f34fcaefe40 __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x29e40)
#15 0x0000563338159ef5 _start (/home/randres/projects/carts/.install/llvm/bin/opt+0x12ef5)
Aborted
make: *** [Makefile:28: taskwithdeps_arts_analysis.bc] Error 134
