; ModuleID = 'taskwithdeps_arts_ir.bc'
source_filename = "taskwithdeps.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }
%struct.kmp_depend_info = type { i64, i64, i8 }
%struct.kmp_task_t_with_privates = type { %struct.kmp_task_t, %struct..kmp_privates.t }
%struct.kmp_task_t = type { ptr, ptr, i32, %union.kmp_cmplrdata_t, %union.kmp_cmplrdata_t }
%union.kmp_cmplrdata_t = type { ptr }
%struct..kmp_privates.t = type { ptr, i32 }
%struct.kmp_task_t_with_privates.1 = type { %struct.kmp_task_t, %struct..kmp_privates.t.2 }
%struct..kmp_privates.t.2 = type { ptr, ptr, i32 }

@0 = private unnamed_addr constant [34 x i8] c";taskwithdeps.cpp;compute;42;13;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 33, ptr @0 }, align 8
@2 = private unnamed_addr constant [34 x i8] c";taskwithdeps.cpp;compute;46;13;;\00", align 1
@3 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 33, ptr @2 }, align 8

; Function Attrs: mustprogress nofree norecurse nosync nounwind memory(argmem: write) uwtable
define dso_local void @_Z4funciPd(i32 noundef %N, ptr nocapture noundef writeonly %A) local_unnamed_addr #0 !dbg !261 {
entry:
  tail call void @llvm.dbg.value(metadata i32 %N, metadata !266, metadata !DIExpression()), !dbg !270
  tail call void @llvm.dbg.value(metadata ptr %A, metadata !267, metadata !DIExpression()), !dbg !270
  tail call void @llvm.dbg.value(metadata i32 0, metadata !268, metadata !DIExpression()), !dbg !271
  %cmp4 = icmp sgt i32 %N, 0, !dbg !272
  br i1 %cmp4, label %for.body.preheader, label %for.cond.cleanup, !dbg !274

for.body.preheader:                               ; preds = %entry
  %wide.trip.count = zext nneg i32 %N to i64, !dbg !272
  br label %for.body, !dbg !274

for.cond.cleanup:                                 ; preds = %for.body, %entry
  ret void, !dbg !275

for.body:                                         ; preds = %for.body.preheader, %for.body
  %indvars.iv = phi i64 [ 0, %for.body.preheader ], [ %indvars.iv.next, %for.body ]
  tail call void @llvm.dbg.value(metadata i64 %indvars.iv, metadata !268, metadata !DIExpression()), !dbg !271
  %0 = trunc i64 %indvars.iv to i32, !dbg !276
  %conv = sitofp i32 %0 to double, !dbg !276
  %arrayidx = getelementptr inbounds double, ptr %A, i64 %indvars.iv, !dbg !278
  store double %conv, ptr %arrayidx, align 8, !dbg !279, !tbaa !280
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1, !dbg !284
  tail call void @llvm.dbg.value(metadata i64 %indvars.iv.next, metadata !268, metadata !DIExpression()), !dbg !271
  %exitcond.not = icmp eq i64 %indvars.iv.next, %wide.trip.count, !dbg !272
  br i1 %exitcond.not, label %for.cond.cleanup, label %for.body, !dbg !274, !llvm.loop !285
}

; Function Attrs: mustprogress nounwind uwtable
define dso_local void @_Z7computeiPdS_(i32 noundef %N, ptr noundef %A, ptr noundef %B) local_unnamed_addr #1 !dbg !289 {
entry:
  %.dep.arr.addr = alloca [1 x %struct.kmp_depend_info], align 8
  %.dep.arr.addr2 = alloca [3 x %struct.kmp_depend_info], align 8
  %0 = tail call i32 @__kmpc_global_thread_num(ptr nonnull @1), !dbg !298
  tail call void @llvm.dbg.value(metadata i32 %N, metadata !293, metadata !DIExpression()), !dbg !302
  tail call void @llvm.dbg.value(metadata ptr %A, metadata !294, metadata !DIExpression()), !dbg !302
  tail call void @llvm.dbg.value(metadata ptr %B, metadata !295, metadata !DIExpression()), !dbg !302
  tail call void @llvm.dbg.value(metadata i32 0, metadata !296, metadata !DIExpression()), !dbg !303
  %cmp22 = icmp sgt i32 %N, 0, !dbg !304
  br i1 %cmp22, label %for.body.lr.ph, label %for.cond.cleanup, !dbg !305

for.body.lr.ph:                                   ; preds = %entry
  %1 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr, i64 0, i32 1
  %2 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr, i64 0, i32 2
  %3 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr2, i64 0, i32 1
  %4 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr2, i64 0, i32 2
  %5 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr2, i64 1
  %6 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr2, i64 1, i32 1
  %7 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr2, i64 1, i32 2
  %8 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr2, i64 2
  %9 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr2, i64 2, i32 1
  %10 = getelementptr inbounds %struct.kmp_depend_info, ptr %.dep.arr.addr2, i64 2, i32 2
  %wide.trip.count = zext nneg i32 %N to i64, !dbg !304
  br label %for.body, !dbg !305

for.cond.cleanup:                                 ; preds = %for.body, %entry
  ret void, !dbg !306

for.body:                                         ; preds = %for.body.lr.ph, %for.body
  %indvars.iv = phi i64 [ 0, %for.body.lr.ph ], [ %indvars.iv.next, %for.body ]
  tail call void @llvm.dbg.value(metadata i64 %indvars.iv, metadata !296, metadata !DIExpression()), !dbg !303
  %11 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @1, i32 %0, i32 1, i64 56, i64 1, ptr nonnull @.omp_task_entry.), !dbg !307
  %12 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr %11, i64 0, i32 1, !dbg !307
  store ptr %A, ptr %12, align 8, !dbg !307, !tbaa !308
  %13 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr %11, i64 0, i32 1, i32 1, !dbg !307
  %14 = trunc i64 %indvars.iv to i32, !dbg !307
  store i32 %14, ptr %13, align 8, !dbg !307, !tbaa !314
  %arrayidx = getelementptr double, ptr %A, i64 %indvars.iv, !dbg !315
  %15 = ptrtoint ptr %arrayidx to i64, !dbg !307
  store i64 %15, ptr %.dep.arr.addr, align 8, !dbg !307, !tbaa !316
  store i64 8, ptr %1, align 8, !dbg !307, !tbaa !319
  store i8 3, ptr %2, align 8, !dbg !307, !tbaa !320
  %16 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @1, i32 %0, ptr %11, i32 1, ptr nonnull %.dep.arr.addr, i32 0, ptr null), !dbg !307
  %17 = call ptr @__kmpc_omp_task_alloc(ptr nonnull @3, i32 %0, i32 1, i64 64, i64 1, ptr nonnull @.omp_task_entry..3), !dbg !321
  %18 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, ptr %17, i64 0, i32 1, !dbg !321
  store ptr %B, ptr %18, align 8, !dbg !321, !tbaa !322
  %19 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, ptr %17, i64 0, i32 1, i32 1, !dbg !321
  store ptr %A, ptr %19, align 8, !dbg !321, !tbaa !325
  %20 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, ptr %17, i64 0, i32 1, i32 2, !dbg !321
  %21 = trunc i64 %indvars.iv to i32, !dbg !321
  store i32 %21, ptr %20, align 8, !dbg !321, !tbaa !326
  store i64 %15, ptr %.dep.arr.addr2, align 8, !dbg !321, !tbaa !316
  store i64 8, ptr %3, align 8, !dbg !321, !tbaa !319
  store i8 1, ptr %4, align 8, !dbg !321, !tbaa !320
  %arrayidx6 = getelementptr double, ptr %arrayidx, i64 -1, !dbg !327
  %22 = ptrtoint ptr %arrayidx6 to i64, !dbg !321
  store i64 %22, ptr %5, align 8, !dbg !321, !tbaa !316
  store i64 8, ptr %6, align 8, !dbg !321, !tbaa !319
  store i8 1, ptr %7, align 8, !dbg !321, !tbaa !320
  %arrayidx8 = getelementptr inbounds double, ptr %B, i64 %indvars.iv, !dbg !329
  %23 = ptrtoint ptr %arrayidx8 to i64, !dbg !321
  store i64 %23, ptr %8, align 8, !dbg !321, !tbaa !316
  store i64 8, ptr %9, align 8, !dbg !321, !tbaa !319
  store i8 3, ptr %10, align 8, !dbg !321, !tbaa !320
  %24 = call i32 @__kmpc_omp_task_with_deps(ptr nonnull @3, i32 %0, ptr %17, i32 3, ptr nonnull %.dep.arr.addr2, i32 0, ptr null), !dbg !321
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1, !dbg !330
  tail call void @llvm.dbg.value(metadata i64 %indvars.iv.next, metadata !296, metadata !DIExpression()), !dbg !303
  %exitcond.not = icmp eq i64 %indvars.iv.next, %wide.trip.count, !dbg !304
  br i1 %exitcond.not, label %for.cond.cleanup, label %for.body, !dbg !305, !llvm.loop !331
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(write, argmem: readwrite, inaccessiblemem: none) uwtable
define internal noundef i32 @.omp_task_entry.(i32 %0, ptr noalias nocapture noundef readonly %1) #2 !dbg !333 {
entry:
  tail call void @llvm.dbg.value(metadata i32 poison, metadata !337, metadata !DIExpression()), !dbg !342
  tail call void @llvm.dbg.value(metadata ptr %1, metadata !338, metadata !DIExpression()), !dbg !342
  %2 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr %1, i64 0, i32 1, !dbg !343
  call void @llvm.dbg.value(metadata i32 poison, metadata !344, metadata !DIExpression()), !dbg !372
  call void @llvm.dbg.value(metadata ptr %1, metadata !365, metadata !DIExpression(DW_OP_plus_uconst, 16, DW_OP_stack_value)), !dbg !372
  call void @llvm.dbg.value(metadata ptr %2, metadata !366, metadata !DIExpression()), !dbg !372
  call void @llvm.dbg.value(metadata !335, metadata !367, metadata !DIExpression()), !dbg !372
  call void @llvm.dbg.value(metadata ptr %1, metadata !368, metadata !DIExpression()), !dbg !372
  call void @llvm.dbg.value(metadata ptr poison, metadata !369, metadata !DIExpression()), !dbg !372
  tail call void @llvm.dbg.value(metadata ptr %2, metadata !374, metadata !DIExpression()), !dbg !391
  tail call void @llvm.dbg.value(metadata ptr undef, metadata !377, metadata !DIExpression()), !dbg !391
  tail call void @llvm.dbg.value(metadata ptr undef, metadata !382, metadata !DIExpression()), !dbg !391
  %3 = getelementptr inbounds %struct.kmp_task_t_with_privates, ptr %1, i64 0, i32 1, i32 1, !dbg !393
  call void @llvm.dbg.value(metadata ptr %2, metadata !370, metadata !DIExpression(DW_OP_deref)), !dbg !372
  call void @llvm.dbg.value(metadata ptr %3, metadata !371, metadata !DIExpression(DW_OP_deref)), !dbg !372
  %4 = load i32, ptr %3, align 4, !dbg !394, !tbaa !395
  %conv.i = sitofp i32 %4 to double, !dbg !394
  %5 = load ptr, ptr %2, align 8, !dbg !396, !tbaa !397
  %idxprom.i = sext i32 %4 to i64, !dbg !396
  %arrayidx.i = getelementptr inbounds double, ptr %5, i64 %idxprom.i, !dbg !396
  store double %conv.i, ptr %arrayidx.i, align 8, !dbg !398, !tbaa !280
  ret i32 0, !dbg !343
}

; Function Attrs: nounwind
declare i32 @__kmpc_global_thread_num(ptr) local_unnamed_addr #3

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) local_unnamed_addr #3

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task_with_deps(ptr, i32, ptr, i32, ptr, i32, ptr) local_unnamed_addr #3

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(readwrite, inaccessiblemem: none) uwtable
define internal noundef i32 @.omp_task_entry..3(i32 %0, ptr noalias nocapture noundef readonly %1) #4 !dbg !399 {
entry:
  tail call void @llvm.dbg.value(metadata i32 poison, metadata !401, metadata !DIExpression()), !dbg !403
  tail call void @llvm.dbg.value(metadata ptr %1, metadata !402, metadata !DIExpression()), !dbg !403
  call void @llvm.dbg.value(metadata i32 poison, metadata !404, metadata !DIExpression()), !dbg !421
  call void @llvm.dbg.value(metadata ptr %1, metadata !413, metadata !DIExpression(DW_OP_plus_uconst, 16, DW_OP_stack_value)), !dbg !421
  call void @llvm.dbg.value(metadata ptr %1, metadata !414, metadata !DIExpression(DW_OP_plus_uconst, 40, DW_OP_stack_value)), !dbg !421
  call void @llvm.dbg.value(metadata !335, metadata !415, metadata !DIExpression()), !dbg !421
  call void @llvm.dbg.value(metadata ptr %1, metadata !416, metadata !DIExpression()), !dbg !421
  call void @llvm.dbg.value(metadata ptr poison, metadata !417, metadata !DIExpression()), !dbg !421
  tail call void @llvm.dbg.value(metadata ptr %1, metadata !423, metadata !DIExpression(DW_OP_plus_uconst, 40, DW_OP_stack_value)), !dbg !429
  tail call void @llvm.dbg.value(metadata ptr undef, metadata !426, metadata !DIExpression()), !dbg !429
  tail call void @llvm.dbg.value(metadata ptr undef, metadata !427, metadata !DIExpression()), !dbg !429
  tail call void @llvm.dbg.value(metadata ptr undef, metadata !428, metadata !DIExpression()), !dbg !429
  %2 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, ptr %1, i64 0, i32 1, i32 1, !dbg !431
  %3 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, ptr %1, i64 0, i32 1, i32 2, !dbg !431
  call void @llvm.dbg.value(metadata ptr %1, metadata !418, metadata !DIExpression(DW_OP_plus_uconst, 40, DW_OP_deref, DW_OP_stack_value)), !dbg !421
  call void @llvm.dbg.value(metadata ptr %3, metadata !419, metadata !DIExpression(DW_OP_deref)), !dbg !421
  call void @llvm.dbg.value(metadata ptr %2, metadata !420, metadata !DIExpression(DW_OP_deref)), !dbg !421
  %4 = load ptr, ptr %2, align 8, !dbg !432, !tbaa !397
  %5 = load i32, ptr %3, align 4, !dbg !433, !tbaa !395
  %idxprom.i = sext i32 %5 to i64, !dbg !432
  %arrayidx.i = getelementptr inbounds double, ptr %4, i64 %idxprom.i, !dbg !432
  %6 = load double, ptr %arrayidx.i, align 8, !dbg !432, !tbaa !280
  %cmp.i = icmp sgt i32 %5, 0, !dbg !434
  br i1 %cmp.i, label %cond.true.i, label %.omp_outlined..1.exit, !dbg !435

cond.true.i:                                      ; preds = %entry
  %7 = zext nneg i32 %5 to i64, !dbg !436
  %8 = getelementptr double, ptr %4, i64 %7, !dbg !436
  %arrayidx4.i = getelementptr double, ptr %8, i64 -1, !dbg !436
  %9 = load double, ptr %arrayidx4.i, align 8, !dbg !436, !tbaa !280
  br label %.omp_outlined..1.exit, !dbg !435

.omp_outlined..1.exit:                            ; preds = %entry, %cond.true.i
  %cond.i = phi double [ %9, %cond.true.i ], [ 0.000000e+00, %entry ], !dbg !435
  %10 = getelementptr inbounds %struct.kmp_task_t_with_privates.1, ptr %1, i64 0, i32 1, !dbg !437
  call void @llvm.dbg.value(metadata ptr %10, metadata !414, metadata !DIExpression()), !dbg !421
  tail call void @llvm.dbg.value(metadata ptr %10, metadata !423, metadata !DIExpression()), !dbg !429
  call void @llvm.dbg.value(metadata ptr %10, metadata !418, metadata !DIExpression(DW_OP_deref)), !dbg !421
  %add.i = fadd double %6, %cond.i, !dbg !438
  %11 = load ptr, ptr %10, align 8, !dbg !439, !tbaa !397
  %arrayidx6.i = getelementptr inbounds double, ptr %11, i64 %idxprom.i, !dbg !439
  store double %add.i, ptr %arrayidx6.i, align 8, !dbg !440, !tbaa !280
  ret i32 0, !dbg !437
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.value(metadata, metadata, metadata) #5

attributes #0 = { mustprogress nofree norecurse nosync nounwind memory(argmem: write) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { mustprogress nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #2 = { mustprogress nofree norecurse nosync nounwind willreturn memory(write, argmem: readwrite, inaccessiblemem: none) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { nounwind }
attributes #4 = { mustprogress nofree norecurse nosync nounwind willreturn memory(readwrite, inaccessiblemem: none) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!252, !253, !254, !255, !256, !257, !258, !259}
!llvm.ident = !{!260}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang version 18.1.8", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, imports: !2, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "taskwithdeps.cpp", directory: "/home/randres/projects/carts/examples/taskwithdeps", checksumkind: CSK_MD5, checksum: "d8ce449c25b4c18f6b523a48af3c1b31")
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
!261 = distinct !DISubprogram(name: "func", linkageName: "_Z4funciPd", scope: !1, file: !1, line: 29, type: !262, scopeLine: 29, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !265)
!262 = !DISubroutineType(types: !263)
!263 = !{null, !9, !264}
!264 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !46, size: 64)
!265 = !{!266, !267, !268}
!266 = !DILocalVariable(name: "N", arg: 1, scope: !261, file: !1, line: 29, type: !9)
!267 = !DILocalVariable(name: "A", arg: 2, scope: !261, file: !1, line: 29, type: !264)
!268 = !DILocalVariable(name: "i", scope: !269, file: !1, line: 30, type: !9)
!269 = distinct !DILexicalBlock(scope: !261, file: !1, line: 30, column: 3)
!270 = !DILocation(line: 0, scope: !261)
!271 = !DILocation(line: 0, scope: !269)
!272 = !DILocation(line: 30, column: 21, scope: !273)
!273 = distinct !DILexicalBlock(scope: !269, file: !1, line: 30, column: 3)
!274 = !DILocation(line: 30, column: 3, scope: !269)
!275 = !DILocation(line: 33, column: 1, scope: !261)
!276 = !DILocation(line: 31, column: 12, scope: !277)
!277 = distinct !DILexicalBlock(scope: !273, file: !1, line: 30, column: 31)
!278 = !DILocation(line: 31, column: 5, scope: !277)
!279 = !DILocation(line: 31, column: 10, scope: !277)
!280 = !{!281, !281, i64 0}
!281 = !{!"double", !282, i64 0}
!282 = !{!"omnipotent char", !283, i64 0}
!283 = !{!"Simple C++ TBAA"}
!284 = !DILocation(line: 30, column: 26, scope: !273)
!285 = distinct !{!285, !274, !286, !287, !288}
!286 = !DILocation(line: 32, column: 3, scope: !269)
!287 = !{!"llvm.loop.mustprogress"}
!288 = !{!"llvm.loop.unroll.disable"}
!289 = distinct !DISubprogram(name: "compute", linkageName: "_Z7computeiPdS_", scope: !1, file: !1, line: 35, type: !290, scopeLine: 35, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !292)
!290 = !DISubroutineType(types: !291)
!291 = !{null, !9, !264, !264}
!292 = !{!293, !294, !295, !296}
!293 = !DILocalVariable(name: "N", arg: 1, scope: !289, file: !1, line: 35, type: !9)
!294 = !DILocalVariable(name: "A", arg: 2, scope: !289, file: !1, line: 35, type: !264)
!295 = !DILocalVariable(name: "B", arg: 3, scope: !289, file: !1, line: 35, type: !264)
!296 = !DILocalVariable(name: "i", scope: !297, file: !1, line: 40, type: !9)
!297 = distinct !DILexicalBlock(scope: !289, file: !1, line: 40, column: 9)
!298 = !DILocation(line: 42, column: 13, scope: !299)
!299 = distinct !DILexicalBlock(scope: !300, file: !1, line: 42, column: 13)
!300 = distinct !DILexicalBlock(scope: !301, file: !1, line: 40, column: 37)
!301 = distinct !DILexicalBlock(scope: !297, file: !1, line: 40, column: 9)
!302 = !DILocation(line: 0, scope: !289)
!303 = !DILocation(line: 0, scope: !297)
!304 = !DILocation(line: 40, column: 27, scope: !301)
!305 = !DILocation(line: 40, column: 9, scope: !297)
!306 = !DILocation(line: 51, column: 1, scope: !289)
!307 = !DILocation(line: 42, column: 13, scope: !300)
!308 = !{!309, !311, i64 40}
!309 = !{!"_ZTS24kmp_task_t_with_privates", !310, i64 0, !313, i64 40}
!310 = !{!"_ZTS10kmp_task_t", !311, i64 0, !311, i64 8, !312, i64 16, !282, i64 24, !282, i64 32}
!311 = !{!"any pointer", !282, i64 0}
!312 = !{!"int", !282, i64 0}
!313 = !{!"_ZTS15.kmp_privates.t", !311, i64 0, !312, i64 8}
!314 = !{!309, !312, i64 48}
!315 = !DILocation(line: 42, column: 42, scope: !299)
!316 = !{!317, !318, i64 0}
!317 = !{!"_ZTS15kmp_depend_info", !318, i64 0, !318, i64 8, !282, i64 16}
!318 = !{!"long", !282, i64 0}
!319 = !{!317, !318, i64 8}
!320 = !{!317, !282, i64 16}
!321 = !DILocation(line: 46, column: 13, scope: !300)
!322 = !{!323, !311, i64 40}
!323 = !{!"_ZTS24kmp_task_t_with_privates", !310, i64 0, !324, i64 40}
!324 = !{!"_ZTS15.kmp_privates.t", !311, i64 0, !311, i64 8, !312, i64 16}
!325 = !{!323, !311, i64 48}
!326 = !{!323, !312, i64 56}
!327 = !DILocation(line: 46, column: 47, scope: !328)
!328 = distinct !DILexicalBlock(scope: !300, file: !1, line: 46, column: 13)
!329 = !DILocation(line: 46, column: 67, scope: !328)
!330 = !DILocation(line: 40, column: 33, scope: !301)
!331 = distinct !{!331, !305, !332, !287, !288}
!332 = !DILocation(line: 48, column: 9, scope: !297)
!333 = distinct !DISubprogram(linkageName: ".omp_task_entry.", scope: !1, file: !1, line: 42, type: !334, scopeLine: 42, flags: DIFlagArtificial | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !336)
!334 = !DISubroutineType(types: !335)
!335 = !{}
!336 = !{!337, !338}
!337 = !DILocalVariable(arg: 1, scope: !333, type: !9, flags: DIFlagArtificial)
!338 = !DILocalVariable(arg: 2, scope: !333, type: !339, flags: DIFlagArtificial)
!339 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !340)
!340 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !341, size: 64)
!341 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "kmp_task_t_with_privates", file: !1, size: 448, flags: DIFlagFwdDecl, identifier: "_ZTS24kmp_task_t_with_privates")
!342 = !DILocation(line: 0, scope: !333)
!343 = !DILocation(line: 42, column: 13, scope: !333)
!344 = !DILocalVariable(name: ".global_tid.", arg: 1, scope: !345, type: !348, flags: DIFlagArtificial)
!345 = distinct !DISubprogram(name: ".omp_outlined.", scope: !1, file: !1, type: !346, scopeLine: 43, flags: DIFlagArtificial | DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !364)
!346 = !DISubroutineType(types: !347)
!347 = !{null, !348, !349, !352, !354, !359, !360}
!348 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !9)
!349 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !350)
!350 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !351)
!351 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !348, size: 64)
!352 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !353)
!353 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !30)
!354 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !355)
!355 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !356)
!356 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !357, size: 64)
!357 = !DISubroutineType(types: !358)
!358 = !{null, !352, null}
!359 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !30)
!360 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !361)
!361 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !362)
!362 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !363, size: 64)
!363 = !DICompositeType(tag: DW_TAG_structure_type, scope: !289, file: !1, line: 42, size: 8, flags: DIFlagFwdDecl)
!364 = !{!344, !365, !366, !367, !368, !369, !370, !371}
!365 = !DILocalVariable(name: ".part_id.", arg: 2, scope: !345, type: !349, flags: DIFlagArtificial)
!366 = !DILocalVariable(name: ".privates.", arg: 3, scope: !345, type: !352, flags: DIFlagArtificial)
!367 = !DILocalVariable(name: ".copy_fn.", arg: 4, scope: !345, type: !354, flags: DIFlagArtificial)
!368 = !DILocalVariable(name: ".task_t.", arg: 5, scope: !345, type: !359, flags: DIFlagArtificial)
!369 = !DILocalVariable(name: "__context", arg: 6, scope: !345, type: !360, flags: DIFlagArtificial)
!370 = !DILocalVariable(name: "A", scope: !345, file: !1, line: 35, type: !264)
!371 = !DILocalVariable(name: "i", scope: !345, file: !1, line: 40, type: !9)
!372 = !DILocation(line: 0, scope: !345, inlinedAt: !373)
!373 = distinct !DILocation(line: 42, column: 13, scope: !333)
!374 = !DILocalVariable(arg: 1, scope: !375, type: !387, flags: DIFlagArtificial)
!375 = distinct !DISubprogram(linkageName: ".omp_task_privates_map.", scope: !1, file: !1, line: 42, type: !334, scopeLine: 42, flags: DIFlagArtificial | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !376)
!376 = !{!374, !377, !382}
!377 = !DILocalVariable(arg: 2, scope: !375, type: !378, flags: DIFlagArtificial)
!378 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !379)
!379 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !380)
!380 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !381, size: 64)
!381 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !264, size: 64)
!382 = !DILocalVariable(arg: 3, scope: !375, type: !383, flags: DIFlagArtificial)
!383 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !384)
!384 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !385)
!385 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !386, size: 64)
!386 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !9, size: 64)
!387 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !388)
!388 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !389)
!389 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !390, size: 64)
!390 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: ".kmp_privates.t", file: !1, size: 128, flags: DIFlagFwdDecl, identifier: "_ZTS15.kmp_privates.t")
!391 = !DILocation(line: 0, scope: !375, inlinedAt: !392)
!392 = distinct !DILocation(line: 42, column: 13, scope: !345, inlinedAt: !373)
!393 = !DILocation(line: 42, column: 13, scope: !375, inlinedAt: !392)
!394 = !DILocation(line: 43, column: 20, scope: !345, inlinedAt: !373)
!395 = !{!312, !312, i64 0}
!396 = !DILocation(line: 43, column: 13, scope: !345, inlinedAt: !373)
!397 = !{!311, !311, i64 0}
!398 = !DILocation(line: 43, column: 18, scope: !345, inlinedAt: !373)
!399 = distinct !DISubprogram(linkageName: ".omp_task_entry..3", scope: !1, file: !1, line: 46, type: !334, scopeLine: 46, flags: DIFlagArtificial | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !400)
!400 = !{!401, !402}
!401 = !DILocalVariable(arg: 1, scope: !399, type: !9, flags: DIFlagArtificial)
!402 = !DILocalVariable(arg: 2, scope: !399, type: !339, flags: DIFlagArtificial)
!403 = !DILocation(line: 0, scope: !399)
!404 = !DILocalVariable(name: ".global_tid.", arg: 1, scope: !405, type: !348, flags: DIFlagArtificial)
!405 = distinct !DISubprogram(name: ".omp_outlined..1", scope: !1, file: !1, type: !406, scopeLine: 47, flags: DIFlagArtificial | DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !412)
!406 = !DISubroutineType(types: !407)
!407 = !{null, !348, !349, !352, !354, !359, !408}
!408 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !409)
!409 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !410)
!410 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !411, size: 64)
!411 = !DICompositeType(tag: DW_TAG_structure_type, scope: !289, file: !1, line: 46, size: 8, flags: DIFlagFwdDecl)
!412 = !{!404, !413, !414, !415, !416, !417, !418, !419, !420}
!413 = !DILocalVariable(name: ".part_id.", arg: 2, scope: !405, type: !349, flags: DIFlagArtificial)
!414 = !DILocalVariable(name: ".privates.", arg: 3, scope: !405, type: !352, flags: DIFlagArtificial)
!415 = !DILocalVariable(name: ".copy_fn.", arg: 4, scope: !405, type: !354, flags: DIFlagArtificial)
!416 = !DILocalVariable(name: ".task_t.", arg: 5, scope: !405, type: !359, flags: DIFlagArtificial)
!417 = !DILocalVariable(name: "__context", arg: 6, scope: !405, type: !408, flags: DIFlagArtificial)
!418 = !DILocalVariable(name: "B", scope: !405, file: !1, line: 35, type: !264)
!419 = !DILocalVariable(name: "i", scope: !405, file: !1, line: 40, type: !9)
!420 = !DILocalVariable(name: "A", scope: !405, file: !1, line: 35, type: !264)
!421 = !DILocation(line: 0, scope: !405, inlinedAt: !422)
!422 = distinct !DILocation(line: 46, column: 13, scope: !399)
!423 = !DILocalVariable(arg: 1, scope: !424, type: !387, flags: DIFlagArtificial)
!424 = distinct !DISubprogram(linkageName: ".omp_task_privates_map..2", scope: !1, file: !1, line: 46, type: !334, scopeLine: 46, flags: DIFlagArtificial | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !425)
!425 = !{!423, !426, !427, !428}
!426 = !DILocalVariable(arg: 2, scope: !424, type: !378, flags: DIFlagArtificial)
!427 = !DILocalVariable(arg: 3, scope: !424, type: !383, flags: DIFlagArtificial)
!428 = !DILocalVariable(arg: 4, scope: !424, type: !378, flags: DIFlagArtificial)
!429 = !DILocation(line: 0, scope: !424, inlinedAt: !430)
!430 = distinct !DILocation(line: 46, column: 13, scope: !405, inlinedAt: !422)
!431 = !DILocation(line: 46, column: 13, scope: !424, inlinedAt: !430)
!432 = !DILocation(line: 47, column: 20, scope: !405, inlinedAt: !422)
!433 = !DILocation(line: 47, column: 22, scope: !405, inlinedAt: !422)
!434 = !DILocation(line: 47, column: 30, scope: !405, inlinedAt: !422)
!435 = !DILocation(line: 47, column: 28, scope: !405, inlinedAt: !422)
!436 = !DILocation(line: 47, column: 36, scope: !405, inlinedAt: !422)
!437 = !DILocation(line: 46, column: 13, scope: !399)
!438 = !DILocation(line: 47, column: 25, scope: !405, inlinedAt: !422)
!439 = !DILocation(line: 47, column: 13, scope: !405, inlinedAt: !422)
!440 = !DILocation(line: 47, column: 18, scope: !405, inlinedAt: !422)
