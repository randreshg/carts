; ModuleID = 'example.cpp'
source_filename = "example.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }
%struct.anon = type { ptr }
%struct.anon.0 = type { ptr }
%struct.kmp_task_t_with_privates = type { %struct.kmp_task_t, %struct..kmp_privates.t }
%struct.kmp_task_t = type { ptr, ptr, i32, %union.kmp_cmplrdata_t, %union.kmp_cmplrdata_t }
%union.kmp_cmplrdata_t = type { ptr }
%struct..kmp_privates.t = type { i32 }
%struct.kmp_task_t_with_privates.1 = type { %struct.kmp_task_t, %struct..kmp_privates.t.2 }
%struct..kmp_privates.t.2 = type { i32 }

$__clang_call_terminate = comdat any

@0 = private unnamed_addr constant [23 x i8] c";unknown;unknown;0;0;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 22, ptr @0 }, align 8
@.str = private unnamed_addr constant [14 x i8] c"fib(%d) = %d\0A\00", align 1
@2 = private unnamed_addr constant %struct.ident_t { i32 0, i32 322, i32 0, i32 22, ptr @0 }, align 8

; Function Attrs: mustprogress noinline nounwind optnone uwtable
define dso_local noundef i32 @_Z3fibi(i32 noundef %n) #0 {
entry:
  %retval = alloca i32, align 4
  %n.addr = alloca i32, align 4
  %i = alloca i32, align 4
  %j = alloca i32, align 4
  %agg.captured = alloca %struct.anon, align 8
  %agg.captured1 = alloca %struct.anon.0, align 8
  %0 = call i32 @__kmpc_global_thread_num(ptr @1)
  store i32 %n, ptr %n.addr, align 4
  %1 = load i32, ptr %n.addr, align 4
  %cmp = icmp slt i32 %1, 2
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %2 = load i32, ptr %n.addr, align 4
  store i32 %2, ptr %retval, align 4
  br label %return

if.else:                                          ; preds = %entry
  %3 = getelementptr inbounds %struct.anon, ptr %agg.captured, i32 0, i32 0
  store ptr %i, ptr %3, align 8
  %4 = call ptr @__kmpc_omp_task_alloc(ptr @1, i32 %0, i32 1, i64 48, i64 8, ptr @.omp_task_entry.)
  %5 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr %4, i32 0, i32 0
  %6 = getelementptr inbounds %struct.kmp_task_t, ptr %5, i32 0, i32 0
  %7 = load ptr, ptr %6, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %7, ptr align 8 %agg.captured, i64 8, i1 false)
  %8 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr %4, i32 0, i32 1
  %9 = getelementptr inbounds %struct..kmp_privates.t, ptr %8, i32 0, i32 0
  %10 = load i32, ptr %n.addr, align 4
  store i32 %10, ptr %9, align 8
  %11 = call i32 @__kmpc_omp_task(ptr @1, i32 %0, ptr %4)
  %12 = getelementptr inbounds %struct.anon.0, ptr %agg.captured1, i32 0, i32 0
  store ptr %j, ptr %12, align 8
  %13 = call ptr @__kmpc_omp_task_alloc(ptr @1, i32 %0, i32 1, i64 48, i64 8, ptr @.omp_task_entry..3)
  %14 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, ptr %13, i32 0, i32 0
  %15 = getelementptr inbounds %struct.kmp_task_t, ptr %14, i32 0, i32 0
  %16 = load ptr, ptr %15, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %16, ptr align 8 %agg.captured1, i64 8, i1 false)
  %17 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, ptr %13, i32 0, i32 1
  %18 = getelementptr inbounds %struct..kmp_privates.t.2, ptr %17, i32 0, i32 0
  %19 = load i32, ptr %n.addr, align 4
  store i32 %19, ptr %18, align 8
  %20 = call i32 @__kmpc_omp_task(ptr @1, i32 %0, ptr %13)
  %21 = call i32 @__kmpc_omp_taskwait(ptr @1, i32 %0)
  %22 = load i32, ptr %i, align 4
  %23 = load i32, ptr %j, align 4
  %add = add nsw i32 %22, %23
  store i32 %add, ptr %retval, align 4
  br label %return

return:                                           ; preds = %if.else, %if.then
  %24 = load i32, ptr %retval, align 4
  ret i32 %24
}

declare i32 @__gxx_personality_v0(...)

; Function Attrs: noinline noreturn nounwind
define linkonce_odr hidden void @__clang_call_terminate(ptr noundef %0) #1 comdat {
  %2 = call ptr @__cxa_begin_catch(ptr %0) #4
  call void @_ZSt9terminatev() #11
  unreachable
}

declare ptr @__cxa_begin_catch(ptr)

declare void @_ZSt9terminatev()

; Function Attrs: noinline uwtable
define internal void @.omp_task_privates_map.(ptr noalias noundef %0, ptr noalias noundef %1) #2 {
entry:
  %.addr = alloca ptr, align 8
  %.addr1 = alloca ptr, align 8
  store ptr %0, ptr %.addr, align 8
  store ptr %1, ptr %.addr1, align 8
  %2 = load ptr, ptr %.addr, align 8
  %3 = getelementptr inbounds %struct..kmp_privates.t, ptr %2, i32 0, i32 0
  %4 = load ptr, ptr %.addr1, align 8
  store ptr %3, ptr %4, align 8
  ret void
}

; Function Attrs: noinline norecurse uwtable
define internal noundef i32 @.omp_task_entry.(i32 noundef %0, ptr noalias noundef %1) #3 personality ptr @__gxx_personality_v0 {
entry:
  %.global_tid..addr.i = alloca i32, align 4
  %.part_id..addr.i = alloca ptr, align 8
  %.privates..addr.i = alloca ptr, align 8
  %.copy_fn..addr.i = alloca ptr, align 8
  %.task_t..addr.i = alloca ptr, align 8
  %__context.addr.i = alloca ptr, align 8
  %.firstpriv.ptr.addr.i = alloca ptr, align 8
  %.addr = alloca i32, align 4
  %.addr1 = alloca ptr, align 8
  store i32 %0, ptr %.addr, align 4
  store ptr %1, ptr %.addr1, align 8
  %2 = load i32, ptr %.addr, align 4
  %3 = load ptr, ptr %.addr1, align 8
  %4 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr %3, i32 0, i32 0
  %5 = getelementptr inbounds %struct.kmp_task_t, ptr %4, i32 0, i32 2
  %6 = getelementptr inbounds %struct.kmp_task_t, ptr %4, i32 0, i32 0
  %7 = load ptr, ptr %6, align 8
  %8 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr %3, i32 0, i32 1
  call void @llvm.experimental.noalias.scope.decl(metadata !7)
  call void @llvm.experimental.noalias.scope.decl(metadata !10)
  call void @llvm.experimental.noalias.scope.decl(metadata !12)
  call void @llvm.experimental.noalias.scope.decl(metadata !14)
  store i32 %2, ptr %.global_tid..addr.i, align 4, !noalias !16
  store ptr %5, ptr %.part_id..addr.i, align 8, !noalias !16
  store ptr %8, ptr %.privates..addr.i, align 8, !noalias !16
  store ptr @.omp_task_privates_map., ptr %.copy_fn..addr.i, align 8, !noalias !16
  store ptr %3, ptr %.task_t..addr.i, align 8, !noalias !16
  store ptr %7, ptr %__context.addr.i, align 8, !noalias !16
  %9 = load ptr, ptr %__context.addr.i, align 8, !noalias !16
  %10 = load ptr, ptr %.copy_fn..addr.i, align 8, !noalias !16
  %11 = load ptr, ptr %.privates..addr.i, align 8, !noalias !16
  call void %10(ptr %11, ptr %.firstpriv.ptr.addr.i) #4
  %12 = load ptr, ptr %.firstpriv.ptr.addr.i, align 8, !noalias !16
  %13 = load i32, ptr %12, align 4
  %sub.i = sub nsw i32 %13, 1
  %call.i = invoke noundef i32 @_Z3fibi(i32 noundef %sub.i)
          to label %.omp_outlined..exit unwind label %terminate.lpad.i

terminate.lpad.i:                                 ; preds = %entry
  %14 = landingpad { ptr, i32 }
          catch ptr null
  %15 = extractvalue { ptr, i32 } %14, 0
  call void @__clang_call_terminate(ptr %15) #11
  unreachable

.omp_outlined..exit:                              ; preds = %entry
  %16 = load ptr, ptr %9, align 8
  store i32 %call.i, ptr %16, align 4
  ret i32 0
}

; Function Attrs: nounwind
declare i32 @__kmpc_global_thread_num(ptr) #4

; Function Attrs: nounwind
declare ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) #4

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg) #5

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(ptr, i32, ptr) #4

; Function Attrs: noinline uwtable
define internal void @.omp_task_privates_map..2(ptr noalias noundef %0, ptr noalias noundef %1) #2 {
entry:
  %.addr = alloca ptr, align 8
  %.addr1 = alloca ptr, align 8
  store ptr %0, ptr %.addr, align 8
  store ptr %1, ptr %.addr1, align 8
  %2 = load ptr, ptr %.addr, align 8
  %3 = getelementptr inbounds %struct..kmp_privates.t.2, ptr %2, i32 0, i32 0
  %4 = load ptr, ptr %.addr1, align 8
  store ptr %3, ptr %4, align 8
  ret void
}

; Function Attrs: noinline norecurse uwtable
define internal noundef i32 @.omp_task_entry..3(i32 noundef %0, ptr noalias noundef %1) #3 personality ptr @__gxx_personality_v0 {
entry:
  %.global_tid..addr.i = alloca i32, align 4
  %.part_id..addr.i = alloca ptr, align 8
  %.privates..addr.i = alloca ptr, align 8
  %.copy_fn..addr.i = alloca ptr, align 8
  %.task_t..addr.i = alloca ptr, align 8
  %__context.addr.i = alloca ptr, align 8
  %.firstpriv.ptr.addr.i = alloca ptr, align 8
  %.addr = alloca i32, align 4
  %.addr1 = alloca ptr, align 8
  store i32 %0, ptr %.addr, align 4
  store ptr %1, ptr %.addr1, align 8
  %2 = load i32, ptr %.addr, align 4
  %3 = load ptr, ptr %.addr1, align 8
  %4 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, ptr %3, i32 0, i32 0
  %5 = getelementptr inbounds %struct.kmp_task_t, ptr %4, i32 0, i32 2
  %6 = getelementptr inbounds %struct.kmp_task_t, ptr %4, i32 0, i32 0
  %7 = load ptr, ptr %6, align 8
  %8 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, ptr %3, i32 0, i32 1
  call void @llvm.experimental.noalias.scope.decl(metadata !17)
  call void @llvm.experimental.noalias.scope.decl(metadata !20)
  call void @llvm.experimental.noalias.scope.decl(metadata !22)
  call void @llvm.experimental.noalias.scope.decl(metadata !24)
  store i32 %2, ptr %.global_tid..addr.i, align 4, !noalias !26
  store ptr %5, ptr %.part_id..addr.i, align 8, !noalias !26
  store ptr %8, ptr %.privates..addr.i, align 8, !noalias !26
  store ptr @.omp_task_privates_map..2, ptr %.copy_fn..addr.i, align 8, !noalias !26
  store ptr %3, ptr %.task_t..addr.i, align 8, !noalias !26
  store ptr %7, ptr %__context.addr.i, align 8, !noalias !26
  %9 = load ptr, ptr %__context.addr.i, align 8, !noalias !26
  %10 = load ptr, ptr %.copy_fn..addr.i, align 8, !noalias !26
  %11 = load ptr, ptr %.privates..addr.i, align 8, !noalias !26
  call void %10(ptr %11, ptr %.firstpriv.ptr.addr.i) #4
  %12 = load ptr, ptr %.firstpriv.ptr.addr.i, align 8, !noalias !26
  %13 = load i32, ptr %12, align 4
  %sub.i = sub nsw i32 %13, 2
  %call.i = invoke noundef i32 @_Z3fibi(i32 noundef %sub.i)
          to label %.omp_outlined..1.exit unwind label %terminate.lpad.i

terminate.lpad.i:                                 ; preds = %entry
  %14 = landingpad { ptr, i32 }
          catch ptr null
  %15 = extractvalue { ptr, i32 } %14, 0
  call void @__clang_call_terminate(ptr %15) #11
  unreachable

.omp_outlined..1.exit:                            ; preds = %entry
  %16 = load ptr, ptr %9, align 8
  store i32 %call.i, ptr %16, align 4
  ret i32 0
}

; Function Attrs: convergent nounwind
declare i32 @__kmpc_omp_taskwait(ptr, i32) #6

; Function Attrs: mustprogress noinline norecurse optnone uwtable
define dso_local noundef i32 @main() #7 {
entry:
  %n = alloca i32, align 4
  store i32 10, ptr %n, align 4
  call void @omp_set_dynamic(i32 noundef 0)
  call void @omp_set_num_threads(i32 noundef 4)
  call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr @1, i32 1, ptr @.omp_outlined..4, ptr %n)
  ret i32 0
}

declare void @omp_set_dynamic(i32 noundef) #8

declare void @omp_set_num_threads(i32 noundef) #8

; Function Attrs: noinline norecurse nounwind optnone uwtable
define internal void @.omp_outlined..4(ptr noalias noundef %.global_tid., ptr noalias noundef %.bound_tid., ptr noundef nonnull align 4 dereferenceable(4) %n) #9 personality ptr @__gxx_personality_v0 {
entry:
  %.global_tid..addr = alloca ptr, align 8
  %.bound_tid..addr = alloca ptr, align 8
  %n.addr = alloca ptr, align 8
  store ptr %.global_tid., ptr %.global_tid..addr, align 8
  store ptr %.bound_tid., ptr %.bound_tid..addr, align 8
  store ptr %n, ptr %n.addr, align 8
  %0 = load ptr, ptr %n.addr, align 8
  %1 = load ptr, ptr %.global_tid..addr, align 8
  %2 = load i32, ptr %1, align 4
  %3 = call i32 @__kmpc_single(ptr @1, i32 %2)
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %omp_if.then, label %omp_if.end

omp_if.then:                                      ; preds = %entry
  %5 = load i32, ptr %0, align 4
  %6 = load i32, ptr %0, align 4
  %call = call noundef i32 @_Z3fibi(i32 noundef %6)
  %call1 = invoke i32 (ptr, ...) @printf(ptr noundef @.str, i32 noundef %5, i32 noundef %call)
          to label %invoke.cont unwind label %terminate.lpad

invoke.cont:                                      ; preds = %omp_if.then
  call void @__kmpc_end_single(ptr @1, i32 %2)
  br label %omp_if.end

omp_if.end:                                       ; preds = %invoke.cont, %entry
  call void @__kmpc_barrier(ptr @2, i32 %2)
  ret void

terminate.lpad:                                   ; preds = %omp_if.then
  %7 = landingpad { ptr, i32 }
          catch ptr null
  %8 = extractvalue { ptr, i32 } %7, 0
  call void @__clang_call_terminate(ptr %8) #11
  unreachable
}

; ARTS TYPES AND FUNCTION DECLARATIONS

; typedef intptr_t artsGuid_t; /**< GUID type */    


artsGuid_t artsEdtCreate(artsEdt_t funcPtr, unsigned int route, uint32_t paramc, uint64_t * paramv, uint32_t depc);
declare dso_local i64 @artsEdtCreate(i64 (i64)*, i32, i32, i64*, i32) #8



; Function Attrs: convergent nounwind
declare void @__kmpc_end_single(ptr, i32) #6

; Function Attrs: convergent nounwind
declare i32 @__kmpc_single(ptr, i32) #6

declare i32 @printf(ptr noundef, ...) #8

; Function Attrs: convergent nounwind
declare void @__kmpc_barrier(ptr, i32) #6

; Function Attrs: nounwind
declare !callback !27 void @__kmpc_fork_call(ptr, i32, ptr, ...) #4

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.experimental.noalias.scope.decl(metadata) #10

attributes #0 = { mustprogress noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { noinline noreturn nounwind "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #2 = { noinline uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { noinline norecurse uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { nounwind }
attributes #5 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }
attributes #6 = { convergent nounwind }
attributes #7 = { mustprogress noinline norecurse optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #8 = { "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #9 = { noinline norecurse nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #10 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }
attributes #11 = { noreturn nounwind }

!llvm.module.flags = !{!0, !1, !2, !3, !4, !5}
!llvm.ident = !{!6}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"openmp", i32 50}
!2 = !{i32 8, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
!4 = !{i32 7, !"uwtable", i32 2}
!5 = !{i32 7, !"frame-pointer", i32 2}
!6 = !{!"clang version 16.0.0 (https://github.com/llvm/llvm-project.git 08d094a0e457360ad8b94b017d2dc277e697ca76)"}
!7 = !{!8}
!8 = distinct !{!8, !9, !".omp_outlined.: %.part_id."}
!9 = distinct !{!9, !".omp_outlined."}
!10 = !{!11}
!11 = distinct !{!11, !9, !".omp_outlined.: %.privates."}
!12 = !{!13}
!13 = distinct !{!13, !9, !".omp_outlined.: %.copy_fn."}
!14 = !{!15}
!15 = distinct !{!15, !9, !".omp_outlined.: %__context"}
!16 = !{!8, !11, !13, !15}
!17 = !{!18}
!18 = distinct !{!18, !19, !".omp_outlined..1: %.part_id."}
!19 = distinct !{!19, !".omp_outlined..1"}
!20 = !{!21}
!21 = distinct !{!21, !19, !".omp_outlined..1: %.privates."}
!22 = !{!23}
!23 = distinct !{!23, !19, !".omp_outlined..1: %.copy_fn."}
!24 = !{!25}
!25 = distinct !{!25, !19, !".omp_outlined..1: %__context"}
!26 = !{!18, !21, !23, !25}
!27 = !{!28}
!28 = !{i64 2, i64 -1, i64 -1, i1 true}
