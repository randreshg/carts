clang++ -fopenmp -O3 -g0 -emit-llvm -c fib.cpp -o fib.bc -lstdc++
clang++: warning: -Z-reserved-lib-stdc++: 'linker' input unused [-Wunused-command-line-argument]
llvm-dis fib.bc
opt -load-pass-plugin=/home/randres/projects/carts/.install/carts/lib/OMPTransform.so \
	-debug-only=omp-transform,arts,carts,arts-ir-builder\
	-passes="omp-transform" fib.bc -o fib_arts_ir.bc

-------------------------------------------------
[omp-transform] Running OmpTransformPass on Module: 
fib.bc

-------------------------------------------------

-------------------------------------------------
[omp-transform] Running OmpTransform on Module: 
SCC: 
SCC: __kmpc_global_thread_num 
SCC: llvm.lifetime.start.p0 
SCC: __kmpc_omp_task_alloc 
SCC: __kmpc_omp_task 
SCC: __kmpc_omp_taskwait 
SCC: llvm.lifetime.end.p0 
SCC: _Z3fibi 
SCC: __gxx_personality_v0 
SCC: llvm.experimental.noalias.scope.decl 
SCC: .omp_task_entry. 
SCC: .omp_task_entry..3 
SCC: rand 
SCC: omp_set_dynamic 
SCC: omp_set_num_threads 
SCC: __kmpc_fork_call 
SCC: __kmpc_single 
SCC: printf 
SCC: __kmpc_end_single 
SCC: __kmpc_barrier 
SCC: main.omp_outlined 
SCC: main 
SCC: 
[omp-transform] [Attributor] Done with 5 functions, result: changed.

-------------------------------------------------
[omp-transform] OmpTransformPass has finished

; ModuleID = 'fib.bc'
source_filename = "fib.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }
%struct.kmp_task_t_with_privates = type { %struct.kmp_task_t, %struct..kmp_privates.t }
%struct.kmp_task_t = type { ptr, ptr, i32, %union.kmp_cmplrdata_t, %union.kmp_cmplrdata_t }
%union.kmp_cmplrdata_t = type { ptr }
%struct..kmp_privates.t = type { i32 }
%struct.kmp_task_t_with_privates.1 = type { %struct.kmp_task_t, %struct..kmp_privates.t.2 }
%struct..kmp_privates.t.2 = type { i32 }

@0 = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, ptr @0 }, align 8
@.str = private unnamed_addr constant [14 x i8] c"fib(%d) = %d\0A\00", align 1
@2 = private unnamed_addr constant %struct.ident_t { i32 0, i32 322, i32 0, i32 22, ptr @0 }, align 8
@.str.4 = private unnamed_addr constant [13 x i8] c"Done %d, %d\0A\00", align 1

; Function Attrs: mustprogress nounwind uwtable
define dso_local noundef i32 @_Z3fibi(i32 noundef %n) local_unnamed_addr #0 {
entry:
  %i = alloca i32, align 4
  %j = alloca i32, align 4
  %0 = tail call i32 @__kmpc_global_thread_num(ptr nonnull @1)
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %i) #3
  call void @llvm.lifetime.start.p0(i64 4, ptr nonnull %j) #3
  %cmp = icmp slt i32 %n, 2
  br i1 %cmp, label %cleanup, label %if.else

if.else:                                          ; preds = %entry
  %1 = tail call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 %0, i32 1, i64 48, i64 8, ptr nonnull @.omp_task_entry.)
  %2 = load ptr, ptr %1, align 8, !tbaa !6
  store ptr %i, ptr %2, align 8, !tbaa.struct !14
  %3 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr %1, i64 0, i32 1
  store i32 %n, ptr %3, align 8, !tbaa !16
  %4 = call i32 @__kmpc_omp_task(ptr nonnull @1, i32 %0, ptr nonnull %1)
  %5 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 %0, i32 1, i64 48, i64 8, ptr nonnull @.omp_task_entry..3)
  %6 = load ptr, ptr %5, align 8, !tbaa !6
  store ptr %j, ptr %6, align 8, !tbaa.struct !14
  %7 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, ptr %5, i64 0, i32 1
  store i32 %n, ptr %7, align 8, !tbaa !16
  %8 = call i32 @__kmpc_omp_task(ptr nonnull @1, i32 %0, ptr nonnull %5)
  %9 = call i32 @__kmpc_omp_taskwait(ptr nonnull @1, i32 %0)
  %10 = load i32, ptr %i, align 4, !tbaa !17
  %11 = load i32, ptr %j, align 4, !tbaa !17
  %add = add nsw i32 %11, %10
  br label %cleanup

cleanup:                                          ; preds = %entry, %if.else
  %retval.0 = phi i32 [ %add, %if.else ], [ %n, %entry ]
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %j) #3
  call void @llvm.lifetime.end.p0(i64 4, ptr nonnull %i) #3
  ret i32 %retval.0
}

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #1

declare i32 @__gxx_personality_v0(...)

; Function Attrs: norecurse nounwind uwtable
define internal noundef i32 @.omp_task_entry.(i32 %0, ptr noalias nocapture noundef readonly %1) #2 personality ptr @__gxx_personality_v0 {
entry:
  %2 = load ptr, ptr %1, align 8, !tbaa !6
  %3 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr %1, i64 0, i32 1
  tail call void @llvm.experimental.noalias.scope.decl(metadata !18)
  %4 = load i32, ptr %3, align 4, !tbaa !17, !noalias !18
  %sub.i = add nsw i32 %4, -1
  %call.i = tail call noundef i32 @_Z3fibi(i32 noundef %sub.i), !noalias !18
  %5 = load ptr, ptr %2, align 8, !tbaa !21, !alias.scope !18, !noalias !23
  store i32 %call.i, ptr %5, align 4, !tbaa !17, !noalias !18
  ret i32 0
}

; Function Attrs: nounwind
declare i32 @__kmpc_global_thread_num(ptr) local_unnamed_addr #3

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #3

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(ptr, i32, ptr) local_unnamed_addr #3

; Function Attrs: norecurse nounwind uwtable
define internal noundef i32 @.omp_task_entry..3(i32 %0, ptr noalias nocapture noundef readonly %1) #2 personality ptr @__gxx_personality_v0 {
entry:
  %2 = load ptr, ptr %1, align 8, !tbaa !6
  %3 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, ptr %1, i64 0, i32 1
  tail call void @llvm.experimental.noalias.scope.decl(metadata !25)
  %4 = load i32, ptr %3, align 4, !tbaa !17, !noalias !25
  %sub.i = add nsw i32 %4, -2
  %call.i = tail call noundef i32 @_Z3fibi(i32 noundef %sub.i), !noalias !25
  %5 = load ptr, ptr %2, align 8, !tbaa !28, !alias.scope !25, !noalias !30
  store i32 %call.i, ptr %5, align 4, !tbaa !17, !noalias !25
  ret i32 0
}

; Function Attrs: convergent nounwind
declare i32 @__kmpc_omp_taskwait(ptr, i32) local_unnamed_addr #4

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: mustprogress norecurse nounwind uwtable
define dso_local noundef i32 @main() local_unnamed_addr #5 {
entry:
  %call = tail call i32 @rand() #3
  %call1 = tail call i32 @rand() #3
  tail call void @omp_set_dynamic(i32 noundef 0)
  tail call void @omp_set_num_threads(i32 noundef 4)
  %n.casted.sroa.0.0.insert.ext = zext i32 %call to i64
  tail call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr nonnull @1, i32 1, ptr nonnull @main.omp_outlined, i64 %n.casted.sroa.0.0.insert.ext)
  %call2 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.4, i32 noundef %call, i32 noundef %call1)
  ret i32 0
}

; Function Attrs: nounwind
declare i32 @rand() local_unnamed_addr #6

; Function Attrs: nounwind
declare void @omp_set_dynamic(i32 noundef) local_unnamed_addr #6

; Function Attrs: nounwind
declare void @omp_set_num_threads(i32 noundef) local_unnamed_addr #6

; Function Attrs: alwaysinline norecurse nounwind uwtable
define internal void @main.omp_outlined(ptr noalias nocapture noundef readonly %.global_tid., ptr noalias nocapture readnone %.bound_tid., i64 noundef %n) #7 personality ptr @__gxx_personality_v0 {
entry:
  %0 = load i32, ptr %.global_tid., align 4, !tbaa !17
  %1 = tail call i32 @__kmpc_single(ptr nonnull @1, i32 %0) #3
  %.not = icmp eq i32 %1, 0
  br i1 %.not, label %omp_if.end, label %omp_if.then

omp_if.then:                                      ; preds = %entry
  %n.addr.sroa.0.0.extract.trunc = trunc i64 %n to i32
  %call = tail call noundef i32 @_Z3fibi(i32 noundef %n.addr.sroa.0.0.extract.trunc)
  %call1 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i32 noundef %n.addr.sroa.0.0.extract.trunc, i32 noundef %call)
  tail call void @__kmpc_end_single(ptr nonnull @1, i32 %0)
  br label %omp_if.end

omp_if.end:                                       ; preds = %omp_if.then, %entry
  tail call void @__kmpc_barrier(ptr nonnull @2, i32 %0)
  ret void
}

; Function Attrs: convergent nounwind
declare i32 @__kmpc_single(ptr, i32) local_unnamed_addr #4

; Function Attrs: convergent nounwind
declare void @__kmpc_end_single(ptr, i32) local_unnamed_addr #4

; Function Attrs: nofree nounwind
declare noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #8

; Function Attrs: convergent nounwind
declare void @__kmpc_barrier(ptr, i32) local_unnamed_addr #4

; Function Attrs: nounwind
declare !callback !32 void @__kmpc_fork_call(ptr, i32, ptr, ...) local_unnamed_addr #3

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #9

attributes #0 = { mustprogress nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { nounwind }
attributes #4 = { convergent nounwind }
attributes #5 = { mustprogress norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #6 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #7 = { alwaysinline norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #8 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #9 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"openmp", i32 51}
!2 = !{i32 8, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
!4 = !{i32 7, !"uwtable", i32 2}
!5 = !{!"clang version 18.1.8"}
!6 = !{!7, !9, i64 0}
!7 = !{!"_ZTS24kmp_task_t_with_privates", !8, i64 0, !13, i64 40}
!8 = !{!"_ZTS10kmp_task_t", !9, i64 0, !9, i64 8, !12, i64 16, !10, i64 24, !10, i64 32}
!9 = !{!"any pointer", !10, i64 0}
!10 = !{!"omnipotent char", !11, i64 0}
!11 = !{!"Simple C++ TBAA"}
!12 = !{!"int", !10, i64 0}
!13 = !{!"_ZTS15.kmp_privates.t", !12, i64 0}
!14 = !{i64 0, i64 8, !15}
!15 = !{!9, !9, i64 0}
!16 = !{!7, !12, i64 40}
!17 = !{!12, !12, i64 0}
!18 = !{!19}
!19 = distinct !{!19, !20, !".omp_outlined.: %__context"}
!20 = distinct !{!20, !".omp_outlined."}
!21 = !{!22, !9, i64 0}
!22 = !{!"_ZTSZ3fibiE3$_0", !9, i64 0}
!23 = !{!24}
!24 = distinct !{!24, !20, !".omp_outlined.: %.privates."}
!25 = !{!26}
!26 = distinct !{!26, !27, !".omp_outlined..1: %__context"}
!27 = distinct !{!27, !".omp_outlined..1"}
!28 = !{!29, !9, i64 0}
!29 = !{!"_ZTSZ3fibiE3$_1", !9, i64 0}
!30 = !{!31}
!31 = distinct !{!31, !27, !".omp_outlined..1: %.privates."}
!32 = !{!33}
!33 = !{i64 2, i64 -1, i64 -1, i1 true}


-------------------------------------------------
# -debug-only=omp-transform,arts,carts,arts-ir-builder,arts-utils\
llvm-dis fib_arts_ir.bc
