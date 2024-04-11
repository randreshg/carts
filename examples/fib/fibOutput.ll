[arts-transform] Run the ARTSTransform Module Pass
[arts-transform] Analyzing main function
[arts-transform] Identifying EDT regions

[arts-transform] Module: 
; ModuleID = 'fib.cpp'
source_filename = "fib.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }
%struct.kmp_task_t_with_privates = type { %struct.kmp_task_t, %struct..kmp_privates.t }
%struct.kmp_task_t = type { ptr, ptr, i32, %union.kmp_cmplrdata_t, %union.kmp_cmplrdata_t }
%union.kmp_cmplrdata_t = type { ptr }
%struct..kmp_privates.t = type { i32 }
%struct.kmp_task_t_with_privates.1 = type { %struct.kmp_task_t, %struct..kmp_privates.t.2 }
%struct..kmp_privates.t.2 = type { i32 }

$__clang_call_terminate = comdat any

@0 = private unnamed_addr constant [20 x i8] c";fib.cpp;fib;22;8;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 19, ptr @0 }, align 8
@2 = private unnamed_addr constant [20 x i8] c";fib.cpp;fib;25;8;;\00", align 1
@3 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 19, ptr @2 }, align 8
@4 = private unnamed_addr constant [20 x i8] c";fib.cpp;fib;28;8;;\00", align 1
@5 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 19, ptr @4 }, align 8
@6 = private unnamed_addr constant [21 x i8] c";fib.cpp;main;42;5;;\00", align 1
@7 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 20, ptr @6 }, align 8
@.str = private unnamed_addr constant [14 x i8] c"fib(%d) = %d\0A\00", align 1
@8 = private unnamed_addr constant %struct.ident_t { i32 0, i32 322, i32 0, i32 20, ptr @6 }, align 8
@9 = private unnamed_addr constant [21 x i8] c";fib.cpp;main;40;3;;\00", align 1
@10 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 20, ptr @9 }, align 8
@.str.4 = private unnamed_addr constant [9 x i8] c"Done %d\0A\00", align 1

; Function Attrs: mustprogress nounwind uwtable
define dso_local noundef i32 @_Z3fibi(i32 noundef %n) #0 !dbg !10 {
entry:
  %i = alloca i32, align 4
  %j = alloca i32, align 4
  %0 = call i32 @__kmpc_global_thread_num(ptr @1)
  call void @llvm.lifetime.start.p0(i64 4, ptr %i) #6, !dbg !13
  call void @llvm.lifetime.start.p0(i64 4, ptr %j) #6, !dbg !13
  %cmp = icmp slt i32 %n, 2, !dbg !14
  br i1 %cmp, label %if.then, label %if.else, !dbg !15

if.then:                                          ; preds = %entry
  br label %cleanup, !dbg !16

if.else:                                          ; preds = %entry
  br label %task.region.0, !dbg !17

task.region.0:                                    ; preds = %if.else
  %1 = call ptr @__kmpc_omp_task_alloc(ptr @1, i32 %0, i32 1, i64 48, i64 8, ptr @.omp_task_entry.), !dbg !17
  %2 = load ptr, ptr %1, align 8, !dbg !17, !tbaa !18
  store ptr %i, ptr %2, align 8, !dbg !17, !tbaa.struct !26
  %3 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr %1, i32 0, i32 1, !dbg !17
  store i32 %n, ptr %3, align 8, !dbg !17, !tbaa !28
  %4 = call i32 @__kmpc_omp_task(ptr @1, i32 %0, ptr %1), !dbg !17
  br label %task.done.0, !dbg !29

task.done.0:                                      ; preds = %task.region.0
  br label %task.region.1, !dbg !29

task.region.1:                                    ; preds = %task.done.0
  %5 = call ptr @__kmpc_omp_task_alloc(ptr @3, i32 %0, i32 1, i64 48, i64 8, ptr @.omp_task_entry..3), !dbg !29
  %6 = load ptr, ptr %5, align 8, !dbg !29, !tbaa !18
  store ptr %j, ptr %6, align 8, !dbg !29, !tbaa.struct !26
  %7 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, ptr %5, i32 0, i32 1, !dbg !29
  store i32 %n, ptr %7, align 8, !dbg !29, !tbaa !28
  %8 = call i32 @__kmpc_omp_task(ptr @3, i32 %0, ptr %5), !dbg !29
  br label %task.done.1, !dbg !30

task.done.1:                                      ; preds = %task.region.1
  %9 = call i32 @__kmpc_omp_taskwait(ptr @5, i32 %0), !dbg !30
  %10 = load i32, ptr %i, align 4, !dbg !31, !tbaa !32
  %11 = load i32, ptr %j, align 4, !dbg !33, !tbaa !32
  %add = add nsw i32 %10, %11, !dbg !34
  br label %cleanup, !dbg !35

cleanup:                                          ; preds = %task.done.1, %if.then
  %retval.0 = phi i32 [ %n, %if.then ], [ %add, %task.done.1 ], !dbg !36
  call void @llvm.lifetime.end.p0(i64 4, ptr %j) #6, !dbg !37
  call void @llvm.lifetime.end.p0(i64 4, ptr %i) #6, !dbg !37
  ret i32 %retval.0, !dbg !37
}

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: alwaysinline nounwind uwtable
define internal void @.omp_outlined.(i32 noundef %.global_tid., ptr noalias noundef %.part_id., ptr noalias noundef %.privates., ptr noalias noundef %.copy_fn., ptr noundef %.task_t., ptr noalias noundef %__context) #2 personality ptr @__gxx_personality_v0 !dbg !38 {
entry:
  %.firstpriv.ptr.addr = alloca ptr, align 8
  call void %.copy_fn.(ptr %.privates., ptr %.firstpriv.ptr.addr), !dbg !39
  %0 = load ptr, ptr %.firstpriv.ptr.addr, align 8, !dbg !40
  %1 = load i32, ptr %0, align 4, !dbg !41, !tbaa !32
  %sub = sub nsw i32 %1, 1, !dbg !42
  %call = call noundef i32 @_Z3fibi(i32 noundef %sub), !dbg !43
  %2 = load ptr, ptr %__context, align 8, !dbg !40, !tbaa !44
  store i32 %call, ptr %2, align 4, !dbg !46, !tbaa !32
  ret void, !dbg !47
}

declare i32 @__gxx_personality_v0(...)

; Function Attrs: noinline noreturn nounwind uwtable
define linkonce_odr hidden void @__clang_call_terminate(ptr noundef %0) #3 comdat {
  %2 = call ptr @__cxa_begin_catch(ptr %0) #6
  call void @_ZSt9terminatev() #13
  unreachable
}

declare ptr @__cxa_begin_catch(ptr)

declare void @_ZSt9terminatev()

; Function Attrs: alwaysinline uwtable
define internal void @.omp_task_privates_map.(ptr noalias noundef %0, ptr noalias noundef %1) #4 !dbg !48 {
entry:
  store ptr %0, ptr %1, align 8, !dbg !49, !tbaa !27
  ret void, !dbg !49
}

; Function Attrs: norecurse uwtable
define internal noundef i32 @.omp_task_entry.(i32 noundef %0, ptr noalias noundef %1) #5 !dbg !50 {
entry:
  %2 = getelementptr inbounds %struct.kmp_task_t, ptr %1, i32 0, i32 2, !dbg !51
  %3 = load ptr, ptr %1, align 8, !dbg !51, !tbaa !18
  %4 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr %1, i32 0, i32 1, !dbg !51
  call void @.omp_outlined.(i32 %0, ptr %2, ptr %4, ptr @.omp_task_privates_map., ptr %1, ptr %3) #6, !dbg !51
  ret i32 0, !dbg !51
}

; Function Attrs: nounwind
declare i32 @__kmpc_global_thread_num(ptr) #6

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) #6

; Function Attrs: mustprogress nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg) #7

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(ptr, i32, ptr) #6

; Function Attrs: alwaysinline nounwind uwtable
define internal void @.omp_outlined..1(i32 noundef %.global_tid., ptr noalias noundef %.part_id., ptr noalias noundef %.privates., ptr noalias noundef %.copy_fn., ptr noundef %.task_t., ptr noalias noundef %__context) #2 personality ptr @__gxx_personality_v0 !dbg !52 {
entry:
  %.firstpriv.ptr.addr = alloca ptr, align 8
  call void %.copy_fn.(ptr %.privates., ptr %.firstpriv.ptr.addr), !dbg !53
  %0 = load ptr, ptr %.firstpriv.ptr.addr, align 8, !dbg !54
  %1 = load i32, ptr %0, align 4, !dbg !55, !tbaa !32
  %sub = sub nsw i32 %1, 2, !dbg !56
  %call = call noundef i32 @_Z3fibi(i32 noundef %sub), !dbg !57
  %2 = load ptr, ptr %__context, align 8, !dbg !54, !tbaa !58
  store i32 %call, ptr %2, align 4, !dbg !60, !tbaa !32
  ret void, !dbg !61
}

; Function Attrs: alwaysinline uwtable
define internal void @.omp_task_privates_map..2(ptr noalias noundef %0, ptr noalias noundef %1) #4 !dbg !62 {
entry:
  store ptr %0, ptr %1, align 8, !dbg !63, !tbaa !27
  ret void, !dbg !63
}

; Function Attrs: norecurse uwtable
define internal noundef i32 @.omp_task_entry..3(i32 noundef %0, ptr noalias noundef %1) #5 !dbg !64 {
entry:
  %2 = getelementptr inbounds %struct.kmp_task_t, ptr %1, i32 0, i32 2, !dbg !65
  %3 = load ptr, ptr %1, align 8, !dbg !65, !tbaa !18
  %4 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, ptr %1, i32 0, i32 1, !dbg !65
  call void @.omp_outlined..1(i32 %0, ptr %2, ptr %4, ptr @.omp_task_privates_map..2, ptr %1, ptr %3) #6, !dbg !65
  ret i32 0, !dbg !65
}

; Function Attrs: convergent nounwind
declare i32 @__kmpc_omp_taskwait(ptr, i32) #8

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: mustprogress norecurse uwtable
define dso_local noundef i32 @main() #9 !dbg !66 {
entry:
  %call = call i32 @rand() #6, !dbg !67
  call void @omp_set_dynamic(i32 noundef 0), !dbg !68
  call void @omp_set_num_threads(i32 noundef 4), !dbg !69
  %n.casted.sroa.0.0.insert.ext = zext i32 %call to i64, !dbg !70
  br label %par.region.0, !dbg !70

par.region.0:                                     ; preds = %entry
  call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr @10, i32 1, ptr @main.omp_outlined, i64 %n.casted.sroa.0.0.insert.ext), !dbg !70
  br label %par.done.0, !dbg !71

par.done.0:                                       ; preds = %par.region.0
  %call1 = call i32 (ptr, ...) @printf(ptr noundef @.str.4, i32 noundef %call), !dbg !71
  ret i32 0, !dbg !72
}

; Function Attrs: nounwind
declare !dbg !73 i32 @rand() #10

; Function Attrs: nounwind
declare !dbg !75 void @omp_set_dynamic(i32 noundef) #10

; Function Attrs: nounwind
declare !dbg !77 void @omp_set_num_threads(i32 noundef) #10

; Function Attrs: alwaysinline norecurse nounwind uwtable
define internal void @main.omp_outlined(ptr noalias noundef %.global_tid., ptr noalias noundef %.bound_tid., i64 noundef %n) #11 personality ptr @__gxx_personality_v0 !dbg !78 {
entry:
  %n.addr.sroa.0.0.extract.trunc = trunc i64 %n to i32
  %n.addr.sroa.3.0.extract.shift = lshr i64 %n, 32
  %0 = load i32, ptr %.global_tid., align 4, !dbg !79, !tbaa !32
  %1 = call i32 @__kmpc_single(ptr @7, i32 %0), !dbg !79
  %2 = icmp ne i32 %1, 0, !dbg !79
  br i1 %2, label %omp_if.then, label %omp_if.end, !dbg !79

omp_if.then:                                      ; preds = %entry
  %call = call noundef i32 @_Z3fibi(i32 noundef %n.addr.sroa.0.0.extract.trunc), !dbg !80
  %call1 = call i32 (ptr, ...) @printf(ptr noundef @.str, i32 noundef %n.addr.sroa.0.0.extract.trunc, i32 noundef %call), !dbg !81
  call void @__kmpc_end_single(ptr @7, i32 %0), !dbg !81
  br label %omp_if.end, !dbg !81

omp_if.end:                                       ; preds = %omp_if.then, %entry
  call void @__kmpc_barrier(ptr @8, i32 %0), !dbg !82
  ret void, !dbg !83
}

; Function Attrs: convergent nounwind
declare i32 @__kmpc_single(ptr, i32) #8

; Function Attrs: convergent nounwind
declare void @__kmpc_end_single(ptr, i32) #8

; Function Attrs: nofree nounwind
declare !dbg !84 noundef i32 @printf(ptr nocapture noundef readonly, ...) #12

; Function Attrs: convergent nounwind
declare void @__kmpc_barrier(ptr, i32) #8

; Function Attrs: nounwind
declare !callback !86 void @__kmpc_fork_call(ptr, i32, ptr, ...) #6

attributes #0 = { mustprogress nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { alwaysinline nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { noinline noreturn nounwind uwtable "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { alwaysinline uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { norecurse uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #6 = { nounwind }
attributes #7 = { mustprogress nocallback nofree nounwind willreturn memory(argmem: readwrite) }
attributes #8 = { convergent nounwind }
attributes #9 = { mustprogress norecurse uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #10 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #11 = { alwaysinline norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #12 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #13 = { noreturn nounwind }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3, !4, !5, !6, !7, !8}
!llvm.ident = !{!9}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang version 18.0.0", isOptimized: true, runtimeVersion: 0, emissionKind: LineTablesOnly, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "fib.cpp", directory: "/home/rherreraguaitero/ME/ARTS-env/test/fib", checksumkind: CSK_MD5, checksum: "6eed027092152b6be8e8fc1f62e9fa3c")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !{i32 1, !"wchar_size", i32 4}
!5 = !{i32 7, !"openmp", i32 51}
!6 = !{i32 8, !"PIC Level", i32 2}
!7 = !{i32 7, !"PIE Level", i32 2}
!8 = !{i32 7, !"uwtable", i32 2}
!9 = !{!"clang version 18.0.0"}
!10 = distinct !DISubprogram(name: "fib", scope: !1, file: !1, line: 15, type: !11, scopeLine: 16, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!11 = !DISubroutineType(types: !12)
!12 = !{}
!13 = !DILocation(line: 17, column: 3, scope: !10)
!14 = !DILocation(line: 18, column: 8, scope: !10)
!15 = !DILocation(line: 18, column: 7, scope: !10)
!16 = !DILocation(line: 19, column: 5, scope: !10)
!17 = !DILocation(line: 22, column: 8, scope: !10)
!18 = !{!19, !21, i64 0}
!19 = !{!"_ZTS24kmp_task_t_with_privates", !20, i64 0, !25, i64 40}
!20 = !{!"_ZTS10kmp_task_t", !21, i64 0, !21, i64 8, !24, i64 16, !22, i64 24, !22, i64 32}
!21 = !{!"any pointer", !22, i64 0}
!22 = !{!"omnipotent char", !23, i64 0}
!23 = !{!"Simple C++ TBAA"}
!24 = !{!"int", !22, i64 0}
!25 = !{!"_ZTS15.kmp_privates.t", !24, i64 0}
!26 = !{i64 0, i64 8, !27}
!27 = !{!21, !21, i64 0}
!28 = !{!19, !24, i64 40}
!29 = !DILocation(line: 25, column: 8, scope: !10)
!30 = !DILocation(line: 28, column: 8, scope: !10)
!31 = !DILocation(line: 29, column: 15, scope: !10)
!32 = !{!24, !24, i64 0}
!33 = !DILocation(line: 29, column: 17, scope: !10)
!34 = !DILocation(line: 29, column: 16, scope: !10)
!35 = !DILocation(line: 29, column: 8, scope: !10)
!36 = !DILocation(line: 0, scope: !10)
!37 = !DILocation(line: 31, column: 1, scope: !10)
!38 = distinct !DISubprogram(name: ".omp_outlined.", scope: !1, file: !1, type: !11, scopeLine: 23, flags: DIFlagArtificial | DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!39 = !DILocation(line: 22, column: 8, scope: !38)
!40 = !DILocation(line: 23, column: 8, scope: !38)
!41 = !DILocation(line: 23, column: 14, scope: !38)
!42 = !DILocation(line: 23, column: 15, scope: !38)
!43 = !DILocation(line: 23, column: 10, scope: !38)
!44 = !{!45, !21, i64 0}
!45 = !{!"_ZTSZ3fibiE3$_0", !21, i64 0}
!46 = !DILocation(line: 23, column: 9, scope: !38)
!47 = !DILocation(line: 23, column: 17, scope: !38)
!48 = distinct !DISubprogram(linkageName: ".omp_task_privates_map.", scope: !1, file: !1, line: 22, type: !11, scopeLine: 22, flags: DIFlagArtificial | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!49 = !DILocation(line: 22, column: 8, scope: !48)
!50 = distinct !DISubprogram(linkageName: ".omp_task_entry.", scope: !1, file: !1, line: 22, type: !11, scopeLine: 22, flags: DIFlagArtificial | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!51 = !DILocation(line: 22, column: 8, scope: !50)
!52 = distinct !DISubprogram(name: ".omp_outlined..1", scope: !1, file: !1, type: !11, scopeLine: 26, flags: DIFlagArtificial | DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!53 = !DILocation(line: 25, column: 8, scope: !52)
!54 = !DILocation(line: 26, column: 8, scope: !52)
!55 = !DILocation(line: 26, column: 14, scope: !52)
!56 = !DILocation(line: 26, column: 15, scope: !52)
!57 = !DILocation(line: 26, column: 10, scope: !52)
!58 = !{!59, !21, i64 0}
!59 = !{!"_ZTSZ3fibiE3$_1", !21, i64 0}
!60 = !DILocation(line: 26, column: 9, scope: !52)
!61 = !DILocation(line: 26, column: 17, scope: !52)
!62 = distinct !DISubprogram(linkageName: ".omp_task_privates_map..2", scope: !1, file: !1, line: 25, type: !11, scopeLine: 25, flags: DIFlagArtificial | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!63 = !DILocation(line: 25, column: 8, scope: !62)
!64 = distinct !DISubprogram(linkageName: ".omp_task_entry..3", scope: !1, file: !1, line: 25, type: !11, scopeLine: 25, flags: DIFlagArtificial | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!65 = !DILocation(line: 25, column: 8, scope: !64)
!66 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 33, type: !11, scopeLine: 34, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!67 = !DILocation(line: 35, column: 11, scope: !66)
!68 = !DILocation(line: 37, column: 3, scope: !66)
!69 = !DILocation(line: 38, column: 3, scope: !66)
!70 = !DILocation(line: 40, column: 3, scope: !66)
!71 = !DILocation(line: 46, column: 3, scope: !66)
!72 = !DILocation(line: 47, column: 1, scope: !66)
!73 = !DISubprogram(name: "rand", scope: !74, file: !74, line: 453, type: !11, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!74 = !DIFile(filename: "/usr/include/stdlib.h", directory: "", checksumkind: CSK_MD5, checksum: "f0db66726d35051e5af2525f5b33bd81")
!75 = !DISubprogram(name: "omp_set_dynamic", scope: !76, file: !76, line: 58, type: !11, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!76 = !DIFile(filename: "soft/llvm_arts/lib/clang/18/include/omp.h", directory: "/home/rherreraguaitero/ME", checksumkind: CSK_MD5, checksum: "7f7de6561f4ac3a0481be022bc074bdf")
!77 = !DISubprogram(name: "omp_set_num_threads", scope: !76, file: !76, line: 57, type: !11, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!78 = distinct !DISubprogram(name: "main.omp_outlined", scope: !1, file: !1, line: 40, type: !11, scopeLine: 40, flags: DIFlagArtificial | DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!79 = !DILocation(line: 42, column: 5, scope: !78)
!80 = !DILocation(line: 43, column: 34, scope: !78)
!81 = !DILocation(line: 43, column: 5, scope: !78)
!82 = !DILocation(line: 42, column: 23, scope: !78)
!83 = !DILocation(line: 44, column: 3, scope: !78)
!84 = !DISubprogram(name: "printf", scope: !85, file: !85, line: 332, type: !11, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!85 = !DIFile(filename: "/usr/include/stdio.h", directory: "", checksumkind: CSK_MD5, checksum: "5b917eded35ce2507d1e294bf8cb74d7")
!86 = !{!87}
!87 = !{i64 2, i64 -1, i64 -1, i1 true}

[arts-transform] Module after ARTSTransform Module Pass:
; ModuleID = 'fib.cpp'
source_filename = "fib.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }
%struct.kmp_task_t_with_privates = type { %struct.kmp_task_t, %struct..kmp_privates.t }
%struct.kmp_task_t = type { ptr, ptr, i32, %union.kmp_cmplrdata_t, %union.kmp_cmplrdata_t }
%union.kmp_cmplrdata_t = type { ptr }
%struct..kmp_privates.t = type { i32 }
%struct.kmp_task_t_with_privates.1 = type { %struct.kmp_task_t, %struct..kmp_privates.t.2 }
%struct..kmp_privates.t.2 = type { i32 }

$__clang_call_terminate = comdat any

@0 = private unnamed_addr constant [20 x i8] c";fib.cpp;fib;22;8;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 19, ptr @0 }, align 8
@2 = private unnamed_addr constant [20 x i8] c";fib.cpp;fib;25;8;;\00", align 1
@3 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 19, ptr @2 }, align 8
@4 = private unnamed_addr constant [20 x i8] c";fib.cpp;fib;28;8;;\00", align 1
@5 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 19, ptr @4 }, align 8
@6 = private unnamed_addr constant [21 x i8] c";fib.cpp;main;42;5;;\00", align 1
@7 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 20, ptr @6 }, align 8
@.str = private unnamed_addr constant [14 x i8] c"fib(%d) = %d\0A\00", align 1
@8 = private unnamed_addr constant %struct.ident_t { i32 0, i32 322, i32 0, i32 20, ptr @6 }, align 8
@9 = private unnamed_addr constant [21 x i8] c";fib.cpp;main;40;3;;\00", align 1
@10 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 20, ptr @9 }, align 8
@.str.4 = private unnamed_addr constant [9 x i8] c"Done %d\0A\00", align 1

; Function Attrs: mustprogress nounwind uwtable
define dso_local noundef i32 @_Z3fibi(i32 noundef %n) #0 !dbg !10 {
entry:
  %i = alloca i32, align 4
  %j = alloca i32, align 4
  %0 = call i32 @__kmpc_global_thread_num(ptr @1)
  call void @llvm.lifetime.start.p0(i64 4, ptr %i) #6, !dbg !13
  call void @llvm.lifetime.start.p0(i64 4, ptr %j) #6, !dbg !13
  %cmp = icmp slt i32 %n, 2, !dbg !14
  br i1 %cmp, label %if.then, label %if.else, !dbg !15

if.then:                                          ; preds = %entry
  br label %cleanup, !dbg !16

if.else:                                          ; preds = %entry
  br label %task.region.0, !dbg !17

task.region.0:                                    ; preds = %if.else
  %1 = call ptr @__kmpc_omp_task_alloc(ptr @1, i32 %0, i32 1, i64 48, i64 8, ptr @.omp_task_entry.), !dbg !17
  %2 = load ptr, ptr %1, align 8, !dbg !17, !tbaa !18
  store ptr %i, ptr %2, align 8, !dbg !17, !tbaa.struct !26
  %3 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr %1, i32 0, i32 1, !dbg !17
  store i32 %n, ptr %3, align 8, !dbg !17, !tbaa !28
  %4 = call i32 @__kmpc_omp_task(ptr @1, i32 %0, ptr %1), !dbg !17
  br label %task.done.0, !dbg !29

task.done.0:                                      ; preds = %task.region.0
  br label %task.region.1, !dbg !29

task.region.1:                                    ; preds = %task.done.0
  %5 = call ptr @__kmpc_omp_task_alloc(ptr @3, i32 %0, i32 1, i64 48, i64 8, ptr @.omp_task_entry..3), !dbg !29
  %6 = load ptr, ptr %5, align 8, !dbg !29, !tbaa !18
  store ptr %j, ptr %6, align 8, !dbg !29, !tbaa.struct !26
  %7 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, ptr %5, i32 0, i32 1, !dbg !29
  store i32 %n, ptr %7, align 8, !dbg !29, !tbaa !28
  %8 = call i32 @__kmpc_omp_task(ptr @3, i32 %0, ptr %5), !dbg !29
  br label %task.done.1, !dbg !30

task.done.1:                                      ; preds = %task.region.1
  %9 = call i32 @__kmpc_omp_taskwait(ptr @5, i32 %0), !dbg !30
  %10 = load i32, ptr %i, align 4, !dbg !31, !tbaa !32
  %11 = load i32, ptr %j, align 4, !dbg !33, !tbaa !32
  %add = add nsw i32 %10, %11, !dbg !34
  br label %cleanup, !dbg !35

cleanup:                                          ; preds = %task.done.1, %if.then
  %retval.0 = phi i32 [ %n, %if.then ], [ %add, %task.done.1 ], !dbg !36
  call void @llvm.lifetime.end.p0(i64 4, ptr %j) #6, !dbg !37
  call void @llvm.lifetime.end.p0(i64 4, ptr %i) #6, !dbg !37
  ret i32 %retval.0, !dbg !37
}

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: alwaysinline nounwind uwtable
define internal void @.omp_outlined.(i32 noundef %.global_tid., ptr noalias noundef %.part_id., ptr noalias noundef %.privates., ptr noalias noundef %.copy_fn., ptr noundef %.task_t., ptr noalias noundef %__context) #2 personality ptr @__gxx_personality_v0 !dbg !38 {
entry:
  %.firstpriv.ptr.addr = alloca ptr, align 8
  call void %.copy_fn.(ptr %.privates., ptr %.firstpriv.ptr.addr), !dbg !39
  %0 = load ptr, ptr %.firstpriv.ptr.addr, align 8, !dbg !40
  %1 = load i32, ptr %0, align 4, !dbg !41, !tbaa !32
  %sub = sub nsw i32 %1, 1, !dbg !42
  %call = call noundef i32 @_Z3fibi(i32 noundef %sub), !dbg !43
  %2 = load ptr, ptr %__context, align 8, !dbg !40, !tbaa !44
  store i32 %call, ptr %2, align 4, !dbg !46, !tbaa !32
  ret void, !dbg !47
}

declare i32 @__gxx_personality_v0(...)

; Function Attrs: noinline noreturn nounwind uwtable
define linkonce_odr hidden void @__clang_call_terminate(ptr noundef %0) #3 comdat {
  %2 = call ptr @__cxa_begin_catch(ptr %0) #6
  call void @_ZSt9terminatev() #13
  unreachable
}

declare ptr @__cxa_begin_catch(ptr)

declare void @_ZSt9terminatev()

; Function Attrs: alwaysinline uwtable
define internal void @.omp_task_privates_map.(ptr noalias noundef %0, ptr noalias noundef %1) #4 !dbg !48 {
entry:
  store ptr %0, ptr %1, align 8, !dbg !49, !tbaa !27
  ret void, !dbg !49
}

; Function Attrs: norecurse uwtable
define internal noundef i32 @.omp_task_entry.(i32 noundef %0, ptr noalias noundef %1) #5 !dbg !50 {
entry:
  %2 = getelementptr inbounds %struct.kmp_task_t, ptr %1, i32 0, i32 2, !dbg !51
  %3 = load ptr, ptr %1, align 8, !dbg !51, !tbaa !18
  %4 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr %1, i32 0, i32 1, !dbg !51
  call void @.omp_outlined.(i32 %0, ptr %2, ptr %4, ptr @.omp_task_privates_map., ptr %1, ptr %3) #6, !dbg !51
  ret i32 0, !dbg !51
}

; Function Attrs: nounwind
declare i32 @__kmpc_global_thread_num(ptr) #6

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) #6

; Function Attrs: mustprogress nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg) #7

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(ptr, i32, ptr) #6

; Function Attrs: alwaysinline nounwind uwtable
define internal void @.omp_outlined..1(i32 noundef %.global_tid., ptr noalias noundef %.part_id., ptr noalias noundef %.privates., ptr noalias noundef %.copy_fn., ptr noundef %.task_t., ptr noalias noundef %__context) #2 personality ptr @__gxx_personality_v0 !dbg !52 {
entry:
  %.firstpriv.ptr.addr = alloca ptr, align 8
  call void %.copy_fn.(ptr %.privates., ptr %.firstpriv.ptr.addr), !dbg !53
  %0 = load ptr, ptr %.firstpriv.ptr.addr, align 8, !dbg !54
  %1 = load i32, ptr %0, align 4, !dbg !55, !tbaa !32
  %sub = sub nsw i32 %1, 2, !dbg !56
  %call = call noundef i32 @_Z3fibi(i32 noundef %sub), !dbg !57
  %2 = load ptr, ptr %__context, align 8, !dbg !54, !tbaa !58
  store i32 %call, ptr %2, align 4, !dbg !60, !tbaa !32
  ret void, !dbg !61
}

; Function Attrs: alwaysinline uwtable
define internal void @.omp_task_privates_map..2(ptr noalias noundef %0, ptr noalias noundef %1) #4 !dbg !62 {
entry:
  store ptr %0, ptr %1, align 8, !dbg !63, !tbaa !27
  ret void, !dbg !63
}

; Function Attrs: norecurse uwtable
define internal noundef i32 @.omp_task_entry..3(i32 noundef %0, ptr noalias noundef %1) #5 !dbg !64 {
entry:
  %2 = getelementptr inbounds %struct.kmp_task_t, ptr %1, i32 0, i32 2, !dbg !65
  %3 = load ptr, ptr %1, align 8, !dbg !65, !tbaa !18
  %4 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, ptr %1, i32 0, i32 1, !dbg !65
  call void @.omp_outlined..1(i32 %0, ptr %2, ptr %4, ptr @.omp_task_privates_map..2, ptr %1, ptr %3) #6, !dbg !65
  ret i32 0, !dbg !65
}

; Function Attrs: convergent nounwind
declare i32 @__kmpc_omp_taskwait(ptr, i32) #8

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: mustprogress norecurse uwtable
define dso_local noundef i32 @main() #9 !dbg !66 {
entry:
  %call = call i32 @rand() #6, !dbg !67
  call void @omp_set_dynamic(i32 noundef 0), !dbg !68
  call void @omp_set_num_threads(i32 noundef 4), !dbg !69
  %n.casted.sroa.0.0.insert.ext = zext i32 %call to i64, !dbg !70
  br label %par.region.0, !dbg !70

par.region.0:                                     ; preds = %entry
  call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr @10, i32 1, ptr @main.omp_outlined, i64 %n.casted.sroa.0.0.insert.ext), !dbg !70
  br label %par.done.0, !dbg !71

par.done.0:                                       ; preds = %par.region.0
  %call1 = call i32 (ptr, ...) @printf(ptr noundef @.str.4, i32 noundef %call), !dbg !71
  ret i32 0, !dbg !72
}

; Function Attrs: nounwind
declare !dbg !73 i32 @rand() #10

; Function Attrs: nounwind
declare !dbg !75 void @omp_set_dynamic(i32 noundef) #10

; Function Attrs: nounwind
declare !dbg !77 void @omp_set_num_threads(i32 noundef) #10

; Function Attrs: alwaysinline norecurse nounwind uwtable
define internal void @main.omp_outlined(ptr noalias noundef %.global_tid., ptr noalias noundef %.bound_tid., i64 noundef %n) #11 personality ptr @__gxx_personality_v0 !dbg !78 {
entry:
  %n.addr.sroa.0.0.extract.trunc = trunc i64 %n to i32
  %n.addr.sroa.3.0.extract.shift = lshr i64 %n, 32
  %0 = load i32, ptr %.global_tid., align 4, !dbg !79, !tbaa !32
  %1 = call i32 @__kmpc_single(ptr @7, i32 %0), !dbg !79
  %2 = icmp ne i32 %1, 0, !dbg !79
  br i1 %2, label %omp_if.then, label %omp_if.end, !dbg !79

omp_if.then:                                      ; preds = %entry
  %call = call noundef i32 @_Z3fibi(i32 noundef %n.addr.sroa.0.0.extract.trunc), !dbg !80
  %call1 = call i32 (ptr, ...) @printf(ptr noundef @.str, i32 noundef %n.addr.sroa.0.0.extract.trunc, i32 noundef %call), !dbg !81
  call void @__kmpc_end_single(ptr @7, i32 %0), !dbg !81
  br label %omp_if.end, !dbg !81

omp_if.end:                                       ; preds = %omp_if.then, %entry
  call void @__kmpc_barrier(ptr @8, i32 %0), !dbg !82
  ret void, !dbg !83
}

; Function Attrs: convergent nounwind
declare i32 @__kmpc_single(ptr, i32) #8

; Function Attrs: convergent nounwind
declare void @__kmpc_end_single(ptr, i32) #8

; Function Attrs: nofree nounwind
declare !dbg !84 noundef i32 @printf(ptr nocapture noundef readonly, ...) #12

; Function Attrs: convergent nounwind
declare void @__kmpc_barrier(ptr, i32) #8

; Function Attrs: nounwind
declare !callback !86 void @__kmpc_fork_call(ptr, i32, ptr, ...) #6

attributes #0 = { mustprogress nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { alwaysinline nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { noinline noreturn nounwind uwtable "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { alwaysinline uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { norecurse uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #6 = { nounwind }
attributes #7 = { mustprogress nocallback nofree nounwind willreturn memory(argmem: readwrite) }
attributes #8 = { convergent nounwind }
attributes #9 = { mustprogress norecurse uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #10 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #11 = { alwaysinline norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #12 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #13 = { noreturn nounwind }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3, !4, !5, !6, !7, !8}
!llvm.ident = !{!9}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang version 18.0.0", isOptimized: true, runtimeVersion: 0, emissionKind: LineTablesOnly, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "fib.cpp", directory: "/home/rherreraguaitero/ME/ARTS-env/test/fib", checksumkind: CSK_MD5, checksum: "6eed027092152b6be8e8fc1f62e9fa3c")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !{i32 1, !"wchar_size", i32 4}
!5 = !{i32 7, !"openmp", i32 51}
!6 = !{i32 8, !"PIC Level", i32 2}
!7 = !{i32 7, !"PIE Level", i32 2}
!8 = !{i32 7, !"uwtable", i32 2}
!9 = !{!"clang version 18.0.0"}
!10 = distinct !DISubprogram(name: "fib", scope: !1, file: !1, line: 15, type: !11, scopeLine: 16, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!11 = !DISubroutineType(types: !12)
!12 = !{}
!13 = !DILocation(line: 17, column: 3, scope: !10)
!14 = !DILocation(line: 18, column: 8, scope: !10)
!15 = !DILocation(line: 18, column: 7, scope: !10)
!16 = !DILocation(line: 19, column: 5, scope: !10)
!17 = !DILocation(line: 22, column: 8, scope: !10)
!18 = !{!19, !21, i64 0}
!19 = !{!"_ZTS24kmp_task_t_with_privates", !20, i64 0, !25, i64 40}
!20 = !{!"_ZTS10kmp_task_t", !21, i64 0, !21, i64 8, !24, i64 16, !22, i64 24, !22, i64 32}
!21 = !{!"any pointer", !22, i64 0}
!22 = !{!"omnipotent char", !23, i64 0}
!23 = !{!"Simple C++ TBAA"}
!24 = !{!"int", !22, i64 0}
!25 = !{!"_ZTS15.kmp_privates.t", !24, i64 0}
!26 = !{i64 0, i64 8, !27}
!27 = !{!21, !21, i64 0}
!28 = !{!19, !24, i64 40}
!29 = !DILocation(line: 25, column: 8, scope: !10)
!30 = !DILocation(line: 28, column: 8, scope: !10)
!31 = !DILocation(line: 29, column: 15, scope: !10)
!32 = !{!24, !24, i64 0}
!33 = !DILocation(line: 29, column: 17, scope: !10)
!34 = !DILocation(line: 29, column: 16, scope: !10)
!35 = !DILocation(line: 29, column: 8, scope: !10)
!36 = !DILocation(line: 0, scope: !10)
!37 = !DILocation(line: 31, column: 1, scope: !10)
!38 = distinct !DISubprogram(name: ".omp_outlined.", scope: !1, file: !1, type: !11, scopeLine: 23, flags: DIFlagArtificial | DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!39 = !DILocation(line: 22, column: 8, scope: !38)
!40 = !DILocation(line: 23, column: 8, scope: !38)
!41 = !DILocation(line: 23, column: 14, scope: !38)
!42 = !DILocation(line: 23, column: 15, scope: !38)
!43 = !DILocation(line: 23, column: 10, scope: !38)
!44 = !{!45, !21, i64 0}
!45 = !{!"_ZTSZ3fibiE3$_0", !21, i64 0}
!46 = !DILocation(line: 23, column: 9, scope: !38)
!47 = !DILocation(line: 23, column: 17, scope: !38)
!48 = distinct !DISubprogram(linkageName: ".omp_task_privates_map.", scope: !1, file: !1, line: 22, type: !11, scopeLine: 22, flags: DIFlagArtificial | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!49 = !DILocation(line: 22, column: 8, scope: !48)
!50 = distinct !DISubprogram(linkageName: ".omp_task_entry.", scope: !1, file: !1, line: 22, type: !11, scopeLine: 22, flags: DIFlagArtificial | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!51 = !DILocation(line: 22, column: 8, scope: !50)
!52 = distinct !DISubprogram(name: ".omp_outlined..1", scope: !1, file: !1, type: !11, scopeLine: 26, flags: DIFlagArtificial | DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!53 = !DILocation(line: 25, column: 8, scope: !52)
!54 = !DILocation(line: 26, column: 8, scope: !52)
!55 = !DILocation(line: 26, column: 14, scope: !52)
!56 = !DILocation(line: 26, column: 15, scope: !52)
!57 = !DILocation(line: 26, column: 10, scope: !52)
!58 = !{!59, !21, i64 0}
!59 = !{!"_ZTSZ3fibiE3$_1", !21, i64 0}
!60 = !DILocation(line: 26, column: 9, scope: !52)
!61 = !DILocation(line: 26, column: 17, scope: !52)
!62 = distinct !DISubprogram(linkageName: ".omp_task_privates_map..2", scope: !1, file: !1, line: 25, type: !11, scopeLine: 25, flags: DIFlagArtificial | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!63 = !DILocation(line: 25, column: 8, scope: !62)
!64 = distinct !DISubprogram(linkageName: ".omp_task_entry..3", scope: !1, file: !1, line: 25, type: !11, scopeLine: 25, flags: DIFlagArtificial | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!65 = !DILocation(line: 25, column: 8, scope: !64)
!66 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 33, type: !11, scopeLine: 34, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!67 = !DILocation(line: 35, column: 11, scope: !66)
!68 = !DILocation(line: 37, column: 3, scope: !66)
!69 = !DILocation(line: 38, column: 3, scope: !66)
!70 = !DILocation(line: 40, column: 3, scope: !66)
!71 = !DILocation(line: 46, column: 3, scope: !66)
!72 = !DILocation(line: 47, column: 1, scope: !66)
!73 = !DISubprogram(name: "rand", scope: !74, file: !74, line: 453, type: !11, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!74 = !DIFile(filename: "/usr/include/stdlib.h", directory: "", checksumkind: CSK_MD5, checksum: "f0db66726d35051e5af2525f5b33bd81")
!75 = !DISubprogram(name: "omp_set_dynamic", scope: !76, file: !76, line: 58, type: !11, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!76 = !DIFile(filename: "soft/llvm_arts/lib/clang/18/include/omp.h", directory: "/home/rherreraguaitero/ME", checksumkind: CSK_MD5, checksum: "7f7de6561f4ac3a0481be022bc074bdf")
!77 = !DISubprogram(name: "omp_set_num_threads", scope: !76, file: !76, line: 57, type: !11, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!78 = distinct !DISubprogram(name: "main.omp_outlined", scope: !1, file: !1, line: 40, type: !11, scopeLine: 40, flags: DIFlagArtificial | DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!79 = !DILocation(line: 42, column: 5, scope: !78)
!80 = !DILocation(line: 43, column: 34, scope: !78)
!81 = !DILocation(line: 43, column: 5, scope: !78)
!82 = !DILocation(line: 42, column: 23, scope: !78)
!83 = !DILocation(line: 44, column: 3, scope: !78)
!84 = !DISubprogram(name: "printf", scope: !85, file: !85, line: 332, type: !11, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!85 = !DIFile(filename: "/usr/include/stdio.h", directory: "", checksumkind: CSK_MD5, checksum: "5b917eded35ce2507d1e294bf8cb74d7")
!86 = !{!87}
!87 = !{i64 2, i64 -1, i64 -1, i1 true}
