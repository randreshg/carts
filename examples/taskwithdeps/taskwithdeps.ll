; ModuleID = 'taskwithdeps_annotaded.cpp'
source_filename = "taskwithdeps_annotaded.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [13 x i8] c"arts.default\00", section "llvm.metadata"
@.str.1 = private unnamed_addr constant [27 x i8] c"taskwithdeps_annotaded.cpp\00", section "llvm.metadata"
@.str.2 = private unnamed_addr constant [12 x i8] c"arts.shared\00", section "llvm.metadata"
@.str.3 = private unnamed_addr constant [24 x i8] c"Task 1: Initializing a\0A\00", align 1, !dbg !0
@.str.4 = private unnamed_addr constant [37 x i8] c"Task 2: Reading a=%d and updating b\0A\00", align 1, !dbg !8
@.str.5 = private unnamed_addr constant [29 x i8] c"Task 3: Final value of b=%d\0A\00", align 1, !dbg !13
@.str.6 = private unnamed_addr constant [12 x i8] c"arts.single\00", section "llvm.metadata"
@.str.7 = private unnamed_addr constant [35 x i8] c"arts.task deps(in: a) deps(out: b)\00", section "llvm.metadata"
@.str.8 = private unnamed_addr constant [14 x i8] c"arts.parallel\00", section "llvm.metadata"
@.str.9 = private unnamed_addr constant [22 x i8] c"arts.task deps(in: b)\00", section "llvm.metadata"
@.str.10 = private unnamed_addr constant [23 x i8] c"arts.task deps(out: a)\00", section "llvm.metadata"
@llvm.global.annotations = appending global [5 x { ptr, ptr, ptr, i32, ptr }] [{ ptr, ptr, ptr, i32, ptr } { ptr @_ZL14edt_function_4ii, ptr @.str.6, ptr @.str.1, i32 68, ptr null }, { ptr, ptr, ptr, i32, ptr } { ptr @_ZL14edt_function_2ii, ptr @.str.7, ptr @.str.1, i32 51, ptr null }, { ptr, ptr, ptr, i32, ptr } { ptr @_ZL14edt_function_5ii, ptr @.str.8, ptr @.str.1, i32 86, ptr null }, { ptr, ptr, ptr, i32, ptr } { ptr @_ZL14edt_function_1i, ptr @.str.9, ptr @.str.1, i32 44, ptr null }, { ptr, ptr, ptr, i32, ptr } { ptr @_ZL14edt_function_3i, ptr @.str.10, ptr @.str.1, i32 60, ptr null }], section "llvm.metadata"

; Function Attrs: mustprogress noinline nounwind optnone uwtable
define dso_local void @_Z16long_computationRi(ptr noundef nonnull align 4 dereferenceable(4) %x) #0 !dbg !274 {
entry:
  %x.addr = alloca ptr, align 8
  %i = alloca i32, align 4
  store ptr %x, ptr %x.addr, align 8
  call void @llvm.dbg.declare(metadata ptr %x.addr, metadata !279, metadata !DIExpression()), !dbg !280
  call void @llvm.dbg.declare(metadata ptr %i, metadata !281, metadata !DIExpression()), !dbg !282
  store i32 0, ptr %i, align 4, !dbg !283
  br label %for.cond, !dbg !285

for.cond:                                         ; preds = %for.inc, %entry
  %0 = load i32, ptr %i, align 4, !dbg !286
  %cmp = icmp slt i32 %0, 1000000, !dbg !288
  br i1 %cmp, label %for.body, label %for.end, !dbg !289

for.body:                                         ; preds = %for.cond
  %1 = load i32, ptr %i, align 4, !dbg !290
  %2 = load ptr, ptr %x.addr, align 8, !dbg !292
  %3 = load i32, ptr %2, align 4, !dbg !293
  %add = add nsw i32 %3, %1, !dbg !293
  store i32 %add, ptr %2, align 4, !dbg !293
  br label %for.inc, !dbg !294

for.inc:                                          ; preds = %for.body
  %4 = load i32, ptr %i, align 4, !dbg !295
  %inc = add nsw i32 %4, 1, !dbg !295
  store i32 %inc, ptr %i, align 4, !dbg !295
  br label %for.cond, !dbg !296, !llvm.loop !297

for.end:                                          ; preds = %for.cond
  ret void, !dbg !300
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; Function Attrs: mustprogress noinline nounwind optnone uwtable
define dso_local void @_Z17short_computationRi(ptr noundef nonnull align 4 dereferenceable(4) %x) #0 !dbg !301 {
entry:
  %x.addr = alloca ptr, align 8
  %i = alloca i32, align 4
  store ptr %x, ptr %x.addr, align 8
  call void @llvm.dbg.declare(metadata ptr %x.addr, metadata !302, metadata !DIExpression()), !dbg !303
  call void @llvm.dbg.declare(metadata ptr %i, metadata !304, metadata !DIExpression()), !dbg !305
  store i32 0, ptr %i, align 4, !dbg !306
  br label %for.cond, !dbg !308

for.cond:                                         ; preds = %for.inc, %entry
  %0 = load i32, ptr %i, align 4, !dbg !309
  %cmp = icmp slt i32 %0, 1000, !dbg !311
  br i1 %cmp, label %for.body, label %for.end, !dbg !312

for.body:                                         ; preds = %for.cond
  %1 = load i32, ptr %i, align 4, !dbg !313
  %2 = load ptr, ptr %x.addr, align 8, !dbg !315
  %3 = load i32, ptr %2, align 4, !dbg !316
  %add = add nsw i32 %3, %1, !dbg !316
  store i32 %add, ptr %2, align 4, !dbg !316
  br label %for.inc, !dbg !317

for.inc:                                          ; preds = %for.body
  %4 = load i32, ptr %i, align 4, !dbg !318
  %inc = add nsw i32 %4, 1, !dbg !318
  store i32 %inc, ptr %i, align 4, !dbg !318
  br label %for.cond, !dbg !319, !llvm.loop !320

for.end:                                          ; preds = %for.cond
  ret void, !dbg !322
}

; Function Attrs: mustprogress noinline optnone uwtable
define dso_local noundef i32 @_Z7computev() #2 !dbg !323 {
entry:
  %a = alloca i32, align 4
  %b = alloca i32, align 4
  call void @llvm.dbg.declare(metadata ptr %a, metadata !324, metadata !DIExpression()), !dbg !325
  store i32 0, ptr %a, align 4, !dbg !325
  call void @llvm.dbg.declare(metadata ptr %b, metadata !326, metadata !DIExpression()), !dbg !327
  store i32 0, ptr %b, align 4, !dbg !327
  %0 = load i32, ptr %a, align 4, !dbg !328
  %1 = load i32, ptr %b, align 4, !dbg !329
  call void @_ZL14edt_function_5ii(i32 noundef %0, i32 noundef %1), !dbg !330
  ret i32 0, !dbg !331
}

; Function Attrs: mustprogress noinline optnone uwtable
define internal void @_ZL14edt_function_5ii(i32 noundef %a, i32 noundef %b) #2 !dbg !332 {
entry:
  %a.addr = alloca i32, align 4
  %b.addr = alloca i32, align 4
  store i32 %a, ptr %a.addr, align 4
  call void @llvm.dbg.declare(metadata ptr %a.addr, metadata !335, metadata !DIExpression()), !dbg !336
  call void @llvm.var.annotation.p0.p0(ptr %a.addr, ptr @.str, ptr @.str.1, i32 87, ptr null)
  store i32 %b, ptr %b.addr, align 4
  call void @llvm.dbg.declare(metadata ptr %b.addr, metadata !337, metadata !DIExpression()), !dbg !338
  call void @llvm.var.annotation.p0.p0(ptr %b.addr, ptr @.str, ptr @.str.1, i32 88, ptr null)
  %0 = load i32, ptr %a.addr, align 4, !dbg !339
  %1 = load i32, ptr %b.addr, align 4, !dbg !340
  call void @_ZL14edt_function_4ii(i32 noundef %0, i32 noundef %1), !dbg !341
  ret void, !dbg !342
}

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.var.annotation.p0.p0(ptr, ptr, ptr, i32, ptr) #3

; Function Attrs: mustprogress noinline optnone uwtable
define internal void @_ZL14edt_function_4ii(i32 noundef %a, i32 noundef %b) #2 !dbg !343 {
entry:
  %a.addr = alloca i32, align 4
  %b.addr = alloca i32, align 4
  store i32 %a, ptr %a.addr, align 4
  call void @llvm.dbg.declare(metadata ptr %a.addr, metadata !344, metadata !DIExpression()), !dbg !345
  call void @llvm.var.annotation.p0.p0(ptr %a.addr, ptr @.str, ptr @.str.1, i32 69, ptr null)
  store i32 %b, ptr %b.addr, align 4
  call void @llvm.dbg.declare(metadata ptr %b.addr, metadata !346, metadata !DIExpression()), !dbg !347
  call void @llvm.var.annotation.p0.p0(ptr %b.addr, ptr @.str, ptr @.str.1, i32 70, ptr null)
  %0 = load i32, ptr %a.addr, align 4, !dbg !348
  call void @_ZL14edt_function_3i(i32 noundef %0), !dbg !349
  %1 = load i32, ptr %a.addr, align 4, !dbg !350
  %2 = load i32, ptr %b.addr, align 4, !dbg !351
  call void @_ZL14edt_function_2ii(i32 noundef %1, i32 noundef %2), !dbg !352
  %3 = load i32, ptr %b.addr, align 4, !dbg !353
  call void @_ZL14edt_function_1i(i32 noundef %3), !dbg !354
  ret void, !dbg !355
}

; Function Attrs: mustprogress noinline optnone uwtable
define internal void @_ZL14edt_function_3i(i32 noundef %a) #2 !dbg !356 {
entry:
  %a.addr = alloca i32, align 4
  store i32 %a, ptr %a.addr, align 4
  call void @llvm.dbg.declare(metadata ptr %a.addr, metadata !357, metadata !DIExpression()), !dbg !358
  call void @llvm.var.annotation.p0.p0(ptr %a.addr, ptr @.str.2, ptr @.str.1, i32 61, ptr null)
  %call = call i32 (ptr, ...) @printf(ptr noundef @.str.3), !dbg !359
  store i32 10, ptr %a.addr, align 4, !dbg !360
  ret void, !dbg !361
}

; Function Attrs: mustprogress noinline optnone uwtable
define internal void @_ZL14edt_function_2ii(i32 noundef %a, i32 noundef %b) #2 !dbg !362 {
entry:
  %a.addr = alloca i32, align 4
  %b.addr = alloca i32, align 4
  store i32 %a, ptr %a.addr, align 4
  call void @llvm.dbg.declare(metadata ptr %a.addr, metadata !363, metadata !DIExpression()), !dbg !364
  call void @llvm.var.annotation.p0.p0(ptr %a.addr, ptr @.str.2, ptr @.str.1, i32 52, ptr null)
  store i32 %b, ptr %b.addr, align 4
  call void @llvm.dbg.declare(metadata ptr %b.addr, metadata !365, metadata !DIExpression()), !dbg !366
  call void @llvm.var.annotation.p0.p0(ptr %b.addr, ptr @.str.2, ptr @.str.1, i32 53, ptr null)
  %0 = load i32, ptr %a.addr, align 4, !dbg !367
  %call = call i32 (ptr, ...) @printf(ptr noundef @.str.4, i32 noundef %0), !dbg !368
  %1 = load i32, ptr %a.addr, align 4, !dbg !369
  %add = add nsw i32 %1, 5, !dbg !370
  store i32 %add, ptr %b.addr, align 4, !dbg !371
  ret void, !dbg !372
}

; Function Attrs: mustprogress noinline optnone uwtable
define internal void @_ZL14edt_function_1i(i32 noundef %b) #2 !dbg !373 {
entry:
  %b.addr = alloca i32, align 4
  store i32 %b, ptr %b.addr, align 4
  call void @llvm.dbg.declare(metadata ptr %b.addr, metadata !374, metadata !DIExpression()), !dbg !375
  call void @llvm.var.annotation.p0.p0(ptr %b.addr, ptr @.str.2, ptr @.str.1, i32 45, ptr null)
  %0 = load i32, ptr %b.addr, align 4, !dbg !376
  %call = call i32 (ptr, ...) @printf(ptr noundef @.str.5, i32 noundef %0), !dbg !377
  ret void, !dbg !378
}

declare i32 @printf(ptr noundef, ...) #4

attributes #0 = { mustprogress noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
attributes #2 = { mustprogress noinline optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }
attributes #4 = { "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.dbg.cu = !{!18}
!llvm.module.flags = !{!266, !267, !268, !269, !270, !271, !272}
!llvm.ident = !{!273}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(scope: null, file: !2, line: 63, type: !3, isLocal: true, isDefinition: true)
!2 = !DIFile(filename: "taskwithdeps_annotaded.cpp", directory: "/home/randres/projects/carts/examples/taskwithdeps", checksumkind: CSK_MD5, checksum: "dc80e86cb52fb28822a002bf876558b1")
!3 = !DICompositeType(tag: DW_TAG_array_type, baseType: !4, size: 192, elements: !6)
!4 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !5)
!5 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!6 = !{!7}
!7 = !DISubrange(count: 24)
!8 = !DIGlobalVariableExpression(var: !9, expr: !DIExpression())
!9 = distinct !DIGlobalVariable(scope: null, file: !2, line: 55, type: !10, isLocal: true, isDefinition: true)
!10 = !DICompositeType(tag: DW_TAG_array_type, baseType: !4, size: 296, elements: !11)
!11 = !{!12}
!12 = !DISubrange(count: 37)
!13 = !DIGlobalVariableExpression(var: !14, expr: !DIExpression())
!14 = distinct !DIGlobalVariable(scope: null, file: !2, line: 47, type: !15, isLocal: true, isDefinition: true)
!15 = !DICompositeType(tag: DW_TAG_array_type, baseType: !4, size: 232, elements: !16)
!16 = !{!17}
!17 = !DISubrange(count: 29)
!18 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !2, producer: "clang version 18.1.8", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, globals: !19, imports: !20, splitDebugInlining: false, nameTableKind: None)
!19 = !{!0, !8, !13}
!20 = !{!21, !29, !33, !40, !44, !52, !57, !59, !65, !69, !73, !83, !85, !89, !93, !97, !102, !106, !110, !114, !118, !126, !130, !134, !136, !140, !144, !149, !155, !159, !163, !165, !173, !177, !185, !187, !191, !195, !199, !203, !208, !213, !218, !219, !220, !221, !223, !224, !225, !226, !227, !228, !229, !231, !232, !233, !234, !235, !236, !237, !242, !243, !244, !245, !246, !247, !248, !249, !250, !251, !252, !253, !254, !255, !256, !257, !258, !259, !260, !261, !262, !263, !264, !265}
!21 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !23, file: !28, line: 52)
!22 = !DINamespace(name: "std", scope: null)
!23 = !DISubprogram(name: "abs", scope: !24, file: !24, line: 848, type: !25, flags: DIFlagPrototyped, spFlags: 0)
!24 = !DIFile(filename: "/usr/include/stdlib.h", directory: "", checksumkind: CSK_MD5, checksum: "02258fad21adf111bb9df9825e61954a")
!25 = !DISubroutineType(types: !26)
!26 = !{!27, !27}
!27 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!28 = !DIFile(filename: "/usr/lib/gcc/x86_64-linux-gnu/11/../../../../include/c++/11/bits/std_abs.h", directory: "")
!29 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !30, file: !32, line: 127)
!30 = !DIDerivedType(tag: DW_TAG_typedef, name: "div_t", file: !24, line: 63, baseType: !31)
!31 = !DICompositeType(tag: DW_TAG_structure_type, file: !24, line: 59, size: 64, flags: DIFlagFwdDecl, identifier: "_ZTS5div_t")
!32 = !DIFile(filename: "/usr/lib/gcc/x86_64-linux-gnu/11/../../../../include/c++/11/cstdlib", directory: "")
!33 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !34, file: !32, line: 128)
!34 = !DIDerivedType(tag: DW_TAG_typedef, name: "ldiv_t", file: !24, line: 71, baseType: !35)
!35 = distinct !DICompositeType(tag: DW_TAG_structure_type, file: !24, line: 67, size: 128, flags: DIFlagTypePassByValue, elements: !36, identifier: "_ZTS6ldiv_t")
!36 = !{!37, !39}
!37 = !DIDerivedType(tag: DW_TAG_member, name: "quot", scope: !35, file: !24, line: 69, baseType: !38, size: 64)
!38 = !DIBasicType(name: "long", size: 64, encoding: DW_ATE_signed)
!39 = !DIDerivedType(tag: DW_TAG_member, name: "rem", scope: !35, file: !24, line: 70, baseType: !38, size: 64, offset: 64)
!40 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !41, file: !32, line: 130)
!41 = !DISubprogram(name: "abort", scope: !24, file: !24, line: 598, type: !42, flags: DIFlagPrototyped | DIFlagNoReturn, spFlags: 0)
!42 = !DISubroutineType(types: !43)
!43 = !{null}
!44 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !45, file: !32, line: 132)
!45 = !DISubprogram(name: "aligned_alloc", scope: !24, file: !24, line: 592, type: !46, flags: DIFlagPrototyped, spFlags: 0)
!46 = !DISubroutineType(types: !47)
!47 = !{!48, !49, !49}
!48 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64)
!49 = !DIDerivedType(tag: DW_TAG_typedef, name: "size_t", file: !50, line: 18, baseType: !51)
!50 = !DIFile(filename: ".install/llvm/lib/clang/18/include/__stddef_size_t.h", directory: "/home/randres/projects/carts", checksumkind: CSK_MD5, checksum: "2c44e821a2b1951cde2eb0fb2e656867")
!51 = !DIBasicType(name: "unsigned long", size: 64, encoding: DW_ATE_unsigned)
!52 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !53, file: !32, line: 134)
!53 = !DISubprogram(name: "atexit", scope: !24, file: !24, line: 602, type: !54, flags: DIFlagPrototyped, spFlags: 0)
!54 = !DISubroutineType(types: !55)
!55 = !{!27, !56}
!56 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !42, size: 64)
!57 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !58, file: !32, line: 137)
!58 = !DISubprogram(name: "at_quick_exit", scope: !24, file: !24, line: 607, type: !54, flags: DIFlagPrototyped, spFlags: 0)
!59 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !60, file: !32, line: 140)
!60 = !DISubprogram(name: "atof", scope: !24, file: !24, line: 102, type: !61, flags: DIFlagPrototyped, spFlags: 0)
!61 = !DISubroutineType(types: !62)
!62 = !{!63, !64}
!63 = !DIBasicType(name: "double", size: 64, encoding: DW_ATE_float)
!64 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !4, size: 64)
!65 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !66, file: !32, line: 141)
!66 = !DISubprogram(name: "atoi", scope: !24, file: !24, line: 105, type: !67, flags: DIFlagPrototyped, spFlags: 0)
!67 = !DISubroutineType(types: !68)
!68 = !{!27, !64}
!69 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !70, file: !32, line: 142)
!70 = !DISubprogram(name: "atol", scope: !24, file: !24, line: 108, type: !71, flags: DIFlagPrototyped, spFlags: 0)
!71 = !DISubroutineType(types: !72)
!72 = !{!38, !64}
!73 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !74, file: !32, line: 143)
!74 = !DISubprogram(name: "bsearch", scope: !24, file: !24, line: 828, type: !75, flags: DIFlagPrototyped, spFlags: 0)
!75 = !DISubroutineType(types: !76)
!76 = !{!48, !77, !77, !49, !49, !79}
!77 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !78, size: 64)
!78 = !DIDerivedType(tag: DW_TAG_const_type, baseType: null)
!79 = !DIDerivedType(tag: DW_TAG_typedef, name: "__compar_fn_t", file: !24, line: 816, baseType: !80)
!80 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !81, size: 64)
!81 = !DISubroutineType(types: !82)
!82 = !{!27, !77, !77}
!83 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !84, file: !32, line: 144)
!84 = !DISubprogram(name: "calloc", scope: !24, file: !24, line: 543, type: !46, flags: DIFlagPrototyped, spFlags: 0)
!85 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !86, file: !32, line: 145)
!86 = !DISubprogram(name: "div", scope: !24, file: !24, line: 860, type: !87, flags: DIFlagPrototyped, spFlags: 0)
!87 = !DISubroutineType(types: !88)
!88 = !{!30, !27, !27}
!89 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !90, file: !32, line: 146)
!90 = !DISubprogram(name: "exit", scope: !24, file: !24, line: 624, type: !91, flags: DIFlagPrototyped | DIFlagNoReturn, spFlags: 0)
!91 = !DISubroutineType(types: !92)
!92 = !{null, !27}
!93 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !94, file: !32, line: 147)
!94 = !DISubprogram(name: "free", scope: !24, file: !24, line: 555, type: !95, flags: DIFlagPrototyped, spFlags: 0)
!95 = !DISubroutineType(types: !96)
!96 = !{null, !48}
!97 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !98, file: !32, line: 148)
!98 = !DISubprogram(name: "getenv", scope: !24, file: !24, line: 641, type: !99, flags: DIFlagPrototyped, spFlags: 0)
!99 = !DISubroutineType(types: !100)
!100 = !{!101, !64}
!101 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !5, size: 64)
!102 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !103, file: !32, line: 149)
!103 = !DISubprogram(name: "labs", scope: !24, file: !24, line: 849, type: !104, flags: DIFlagPrototyped, spFlags: 0)
!104 = !DISubroutineType(types: !105)
!105 = !{!38, !38}
!106 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !107, file: !32, line: 150)
!107 = !DISubprogram(name: "ldiv", scope: !24, file: !24, line: 862, type: !108, flags: DIFlagPrototyped, spFlags: 0)
!108 = !DISubroutineType(types: !109)
!109 = !{!34, !38, !38}
!110 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !111, file: !32, line: 151)
!111 = !DISubprogram(name: "malloc", scope: !24, file: !24, line: 540, type: !112, flags: DIFlagPrototyped, spFlags: 0)
!112 = !DISubroutineType(types: !113)
!113 = !{!48, !49}
!114 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !115, file: !32, line: 153)
!115 = !DISubprogram(name: "mblen", scope: !24, file: !24, line: 930, type: !116, flags: DIFlagPrototyped, spFlags: 0)
!116 = !DISubroutineType(types: !117)
!117 = !{!27, !64, !49}
!118 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !119, file: !32, line: 154)
!119 = !DISubprogram(name: "mbstowcs", scope: !24, file: !24, line: 941, type: !120, flags: DIFlagPrototyped, spFlags: 0)
!120 = !DISubroutineType(types: !121)
!121 = !{!49, !122, !125, !49}
!122 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !123)
!123 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !124, size: 64)
!124 = !DIBasicType(name: "wchar_t", size: 32, encoding: DW_ATE_signed)
!125 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !64)
!126 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !127, file: !32, line: 155)
!127 = !DISubprogram(name: "mbtowc", scope: !24, file: !24, line: 933, type: !128, flags: DIFlagPrototyped, spFlags: 0)
!128 = !DISubroutineType(types: !129)
!129 = !{!27, !122, !125, !49}
!130 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !131, file: !32, line: 157)
!131 = !DISubprogram(name: "qsort", scope: !24, file: !24, line: 838, type: !132, flags: DIFlagPrototyped, spFlags: 0)
!132 = !DISubroutineType(types: !133)
!133 = !{null, !48, !49, !49, !79}
!134 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !135, file: !32, line: 160)
!135 = !DISubprogram(name: "quick_exit", scope: !24, file: !24, line: 630, type: !91, flags: DIFlagPrototyped | DIFlagNoReturn, spFlags: 0)
!136 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !137, file: !32, line: 163)
!137 = !DISubprogram(name: "rand", scope: !24, file: !24, line: 454, type: !138, flags: DIFlagPrototyped, spFlags: 0)
!138 = !DISubroutineType(types: !139)
!139 = !{!27}
!140 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !141, file: !32, line: 164)
!141 = !DISubprogram(name: "realloc", scope: !24, file: !24, line: 551, type: !142, flags: DIFlagPrototyped, spFlags: 0)
!142 = !DISubroutineType(types: !143)
!143 = !{!48, !48, !49}
!144 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !145, file: !32, line: 165)
!145 = !DISubprogram(name: "srand", scope: !24, file: !24, line: 456, type: !146, flags: DIFlagPrototyped, spFlags: 0)
!146 = !DISubroutineType(types: !147)
!147 = !{null, !148}
!148 = !DIBasicType(name: "unsigned int", size: 32, encoding: DW_ATE_unsigned)
!149 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !150, file: !32, line: 166)
!150 = !DISubprogram(name: "strtod", scope: !24, file: !24, line: 118, type: !151, flags: DIFlagPrototyped, spFlags: 0)
!151 = !DISubroutineType(types: !152)
!152 = !{!63, !125, !153}
!153 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !154)
!154 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !101, size: 64)
!155 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !156, file: !32, line: 167)
!156 = !DISubprogram(name: "strtol", scope: !24, file: !24, line: 177, type: !157, flags: DIFlagPrototyped, spFlags: 0)
!157 = !DISubroutineType(types: !158)
!158 = !{!38, !125, !153, !27}
!159 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !160, file: !32, line: 168)
!160 = !DISubprogram(name: "strtoul", scope: !24, file: !24, line: 181, type: !161, flags: DIFlagPrototyped, spFlags: 0)
!161 = !DISubroutineType(types: !162)
!162 = !{!51, !125, !153, !27}
!163 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !164, file: !32, line: 169)
!164 = !DISubprogram(name: "system", scope: !24, file: !24, line: 791, type: !67, flags: DIFlagPrototyped, spFlags: 0)
!165 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !166, file: !32, line: 171)
!166 = !DISubprogram(name: "wcstombs", scope: !24, file: !24, line: 945, type: !167, flags: DIFlagPrototyped, spFlags: 0)
!167 = !DISubroutineType(types: !168)
!168 = !{!49, !169, !170, !49}
!169 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !101)
!170 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !171)
!171 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !172, size: 64)
!172 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !124)
!173 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !174, file: !32, line: 172)
!174 = !DISubprogram(name: "wctomb", scope: !24, file: !24, line: 937, type: !175, flags: DIFlagPrototyped, spFlags: 0)
!175 = !DISubroutineType(types: !176)
!176 = !{!27, !101, !124}
!177 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !178, entity: !179, file: !32, line: 200)
!178 = !DINamespace(name: "__gnu_cxx", scope: null)
!179 = !DIDerivedType(tag: DW_TAG_typedef, name: "lldiv_t", file: !24, line: 81, baseType: !180)
!180 = distinct !DICompositeType(tag: DW_TAG_structure_type, file: !24, line: 77, size: 128, flags: DIFlagTypePassByValue, elements: !181, identifier: "_ZTS7lldiv_t")
!181 = !{!182, !184}
!182 = !DIDerivedType(tag: DW_TAG_member, name: "quot", scope: !180, file: !24, line: 79, baseType: !183, size: 64)
!183 = !DIBasicType(name: "long long", size: 64, encoding: DW_ATE_signed)
!184 = !DIDerivedType(tag: DW_TAG_member, name: "rem", scope: !180, file: !24, line: 80, baseType: !183, size: 64, offset: 64)
!185 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !178, entity: !186, file: !32, line: 206)
!186 = !DISubprogram(name: "_Exit", scope: !24, file: !24, line: 636, type: !91, flags: DIFlagPrototyped | DIFlagNoReturn, spFlags: 0)
!187 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !178, entity: !188, file: !32, line: 210)
!188 = !DISubprogram(name: "llabs", scope: !24, file: !24, line: 852, type: !189, flags: DIFlagPrototyped, spFlags: 0)
!189 = !DISubroutineType(types: !190)
!190 = !{!183, !183}
!191 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !178, entity: !192, file: !32, line: 216)
!192 = !DISubprogram(name: "lldiv", scope: !24, file: !24, line: 866, type: !193, flags: DIFlagPrototyped, spFlags: 0)
!193 = !DISubroutineType(types: !194)
!194 = !{!179, !183, !183}
!195 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !178, entity: !196, file: !32, line: 227)
!196 = !DISubprogram(name: "atoll", scope: !24, file: !24, line: 113, type: !197, flags: DIFlagPrototyped, spFlags: 0)
!197 = !DISubroutineType(types: !198)
!198 = !{!183, !64}
!199 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !178, entity: !200, file: !32, line: 228)
!200 = !DISubprogram(name: "strtoll", scope: !24, file: !24, line: 201, type: !201, flags: DIFlagPrototyped, spFlags: 0)
!201 = !DISubroutineType(types: !202)
!202 = !{!183, !125, !153, !27}
!203 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !178, entity: !204, file: !32, line: 229)
!204 = !DISubprogram(name: "strtoull", scope: !24, file: !24, line: 206, type: !205, flags: DIFlagPrototyped, spFlags: 0)
!205 = !DISubroutineType(types: !206)
!206 = !{!207, !125, !153, !27}
!207 = !DIBasicType(name: "unsigned long long", size: 64, encoding: DW_ATE_unsigned)
!208 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !178, entity: !209, file: !32, line: 231)
!209 = !DISubprogram(name: "strtof", scope: !24, file: !24, line: 124, type: !210, flags: DIFlagPrototyped, spFlags: 0)
!210 = !DISubroutineType(types: !211)
!211 = !{!212, !125, !153}
!212 = !DIBasicType(name: "float", size: 32, encoding: DW_ATE_float)
!213 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !178, entity: !214, file: !32, line: 232)
!214 = !DISubprogram(name: "strtold", scope: !24, file: !24, line: 127, type: !215, flags: DIFlagPrototyped, spFlags: 0)
!215 = !DISubroutineType(types: !216)
!216 = !{!217, !125, !153}
!217 = !DIBasicType(name: "long double", size: 128, encoding: DW_ATE_float)
!218 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !179, file: !32, line: 240)
!219 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !186, file: !32, line: 242)
!220 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !188, file: !32, line: 244)
!221 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !222, file: !32, line: 245)
!222 = !DISubprogram(name: "div", linkageName: "_ZN9__gnu_cxx3divExx", scope: !178, file: !32, line: 213, type: !193, flags: DIFlagPrototyped, spFlags: 0)
!223 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !192, file: !32, line: 246)
!224 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !196, file: !32, line: 248)
!225 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !209, file: !32, line: 249)
!226 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !200, file: !32, line: 250)
!227 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !204, file: !32, line: 251)
!228 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !22, entity: !214, file: !32, line: 252)
!229 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !41, file: !230, line: 38)
!230 = !DIFile(filename: "/usr/lib/gcc/x86_64-linux-gnu/11/../../../../include/c++/11/stdlib.h", directory: "", checksumkind: CSK_MD5, checksum: "0f5b773a303c24013fb112082e6d18a5")
!231 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !53, file: !230, line: 39)
!232 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !90, file: !230, line: 40)
!233 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !58, file: !230, line: 43)
!234 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !135, file: !230, line: 46)
!235 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !30, file: !230, line: 51)
!236 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !34, file: !230, line: 52)
!237 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !238, file: !230, line: 54)
!238 = !DISubprogram(name: "abs", linkageName: "_ZSt3absg", scope: !22, file: !28, line: 103, type: !239, flags: DIFlagPrototyped, spFlags: 0)
!239 = !DISubroutineType(types: !240)
!240 = !{!241, !241}
!241 = !DIBasicType(name: "__float128", size: 128, encoding: DW_ATE_float)
!242 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !60, file: !230, line: 55)
!243 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !66, file: !230, line: 56)
!244 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !70, file: !230, line: 57)
!245 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !74, file: !230, line: 58)
!246 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !84, file: !230, line: 59)
!247 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !222, file: !230, line: 60)
!248 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !94, file: !230, line: 61)
!249 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !98, file: !230, line: 62)
!250 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !103, file: !230, line: 63)
!251 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !107, file: !230, line: 64)
!252 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !111, file: !230, line: 65)
!253 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !115, file: !230, line: 67)
!254 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !119, file: !230, line: 68)
!255 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !127, file: !230, line: 69)
!256 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !131, file: !230, line: 71)
!257 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !137, file: !230, line: 72)
!258 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !141, file: !230, line: 73)
!259 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !145, file: !230, line: 74)
!260 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !150, file: !230, line: 75)
!261 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !156, file: !230, line: 76)
!262 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !160, file: !230, line: 77)
!263 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !164, file: !230, line: 78)
!264 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !166, file: !230, line: 80)
!265 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !18, entity: !174, file: !230, line: 81)
!266 = !{i32 7, !"Dwarf Version", i32 5}
!267 = !{i32 2, !"Debug Info Version", i32 3}
!268 = !{i32 1, !"wchar_size", i32 4}
!269 = !{i32 8, !"PIC Level", i32 2}
!270 = !{i32 7, !"PIE Level", i32 2}
!271 = !{i32 7, !"uwtable", i32 2}
!272 = !{i32 7, !"frame-pointer", i32 2}
!273 = !{!"clang version 18.1.8"}
!274 = distinct !DISubprogram(name: "long_computation", linkageName: "_Z16long_computationRi", scope: !2, file: !2, line: 20, type: !275, scopeLine: 20, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !18, retainedNodes: !278)
!275 = !DISubroutineType(types: !276)
!276 = !{null, !277}
!277 = !DIDerivedType(tag: DW_TAG_reference_type, baseType: !27, size: 64)
!278 = !{}
!279 = !DILocalVariable(name: "x", arg: 1, scope: !274, file: !2, line: 20, type: !277)
!280 = !DILocation(line: 20, column: 28, scope: !274)
!281 = !DILocalVariable(name: "i", scope: !274, file: !2, line: 21, type: !27)
!282 = !DILocation(line: 21, column: 7, scope: !274)
!283 = !DILocation(line: 22, column: 10, scope: !284)
!284 = distinct !DILexicalBlock(scope: !274, file: !2, line: 22, column: 3)
!285 = !DILocation(line: 22, column: 8, scope: !284)
!286 = !DILocation(line: 22, column: 15, scope: !287)
!287 = distinct !DILexicalBlock(scope: !284, file: !2, line: 22, column: 3)
!288 = !DILocation(line: 22, column: 17, scope: !287)
!289 = !DILocation(line: 22, column: 3, scope: !284)
!290 = !DILocation(line: 23, column: 10, scope: !291)
!291 = distinct !DILexicalBlock(scope: !287, file: !2, line: 22, column: 33)
!292 = !DILocation(line: 23, column: 5, scope: !291)
!293 = !DILocation(line: 23, column: 7, scope: !291)
!294 = !DILocation(line: 24, column: 3, scope: !291)
!295 = !DILocation(line: 22, column: 29, scope: !287)
!296 = !DILocation(line: 22, column: 3, scope: !287)
!297 = distinct !{!297, !289, !298, !299}
!298 = !DILocation(line: 24, column: 3, scope: !284)
!299 = !{!"llvm.loop.mustprogress"}
!300 = !DILocation(line: 25, column: 1, scope: !274)
!301 = distinct !DISubprogram(name: "short_computation", linkageName: "_Z17short_computationRi", scope: !2, file: !2, line: 28, type: !275, scopeLine: 28, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !18, retainedNodes: !278)
!302 = !DILocalVariable(name: "x", arg: 1, scope: !301, file: !2, line: 28, type: !277)
!303 = !DILocation(line: 28, column: 29, scope: !301)
!304 = !DILocalVariable(name: "i", scope: !301, file: !2, line: 29, type: !27)
!305 = !DILocation(line: 29, column: 7, scope: !301)
!306 = !DILocation(line: 30, column: 10, scope: !307)
!307 = distinct !DILexicalBlock(scope: !301, file: !2, line: 30, column: 3)
!308 = !DILocation(line: 30, column: 8, scope: !307)
!309 = !DILocation(line: 30, column: 15, scope: !310)
!310 = distinct !DILexicalBlock(scope: !307, file: !2, line: 30, column: 3)
!311 = !DILocation(line: 30, column: 17, scope: !310)
!312 = !DILocation(line: 30, column: 3, scope: !307)
!313 = !DILocation(line: 31, column: 10, scope: !314)
!314 = distinct !DILexicalBlock(scope: !310, file: !2, line: 30, column: 30)
!315 = !DILocation(line: 31, column: 5, scope: !314)
!316 = !DILocation(line: 31, column: 7, scope: !314)
!317 = !DILocation(line: 32, column: 3, scope: !314)
!318 = !DILocation(line: 30, column: 26, scope: !310)
!319 = !DILocation(line: 30, column: 3, scope: !310)
!320 = distinct !{!320, !312, !321, !299}
!321 = !DILocation(line: 32, column: 3, scope: !307)
!322 = !DILocation(line: 33, column: 1, scope: !301)
!323 = distinct !DISubprogram(name: "compute", linkageName: "_Z7computev", scope: !2, file: !2, line: 36, type: !138, scopeLine: 36, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !18, retainedNodes: !278)
!324 = !DILocalVariable(name: "a", scope: !323, file: !2, line: 37, type: !27)
!325 = !DILocation(line: 37, column: 7, scope: !323)
!326 = !DILocalVariable(name: "b", scope: !323, file: !2, line: 38, type: !27)
!327 = !DILocation(line: 38, column: 7, scope: !323)
!328 = !DILocation(line: 40, column: 18, scope: !323)
!329 = !DILocation(line: 40, column: 21, scope: !323)
!330 = !DILocation(line: 40, column: 3, scope: !323)
!331 = !DILocation(line: 42, column: 3, scope: !323)
!332 = distinct !DISubprogram(name: "edt_function_5", linkageName: "_ZL14edt_function_5ii", scope: !2, file: !2, line: 86, type: !333, scopeLine: 88, flags: DIFlagPrototyped, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition, unit: !18, retainedNodes: !278)
!333 = !DISubroutineType(types: !334)
!334 = !{null, !27, !27}
!335 = !DILocalVariable(name: "a", arg: 1, scope: !332, file: !2, line: 87, type: !27)
!336 = !DILocation(line: 87, column: 7, scope: !332)
!337 = !DILocalVariable(name: "b", arg: 2, scope: !332, file: !2, line: 88, type: !27)
!338 = !DILocation(line: 88, column: 7, scope: !332)
!339 = !DILocation(line: 90, column: 20, scope: !332)
!340 = !DILocation(line: 90, column: 23, scope: !332)
!341 = !DILocation(line: 90, column: 5, scope: !332)
!342 = !DILocation(line: 93, column: 1, scope: !332)
!343 = distinct !DISubprogram(name: "edt_function_4", linkageName: "_ZL14edt_function_4ii", scope: !2, file: !2, line: 68, type: !333, scopeLine: 70, flags: DIFlagPrototyped, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition, unit: !18, retainedNodes: !278)
!344 = !DILocalVariable(name: "a", arg: 1, scope: !343, file: !2, line: 69, type: !27)
!345 = !DILocation(line: 69, column: 7, scope: !343)
!346 = !DILocalVariable(name: "b", arg: 2, scope: !343, file: !2, line: 70, type: !27)
!347 = !DILocation(line: 70, column: 7, scope: !343)
!348 = !DILocation(line: 73, column: 22, scope: !343)
!349 = !DILocation(line: 73, column: 7, scope: !343)
!350 = !DILocation(line: 77, column: 22, scope: !343)
!351 = !DILocation(line: 77, column: 25, scope: !343)
!352 = !DILocation(line: 77, column: 7, scope: !343)
!353 = !DILocation(line: 81, column: 22, scope: !343)
!354 = !DILocation(line: 81, column: 7, scope: !343)
!355 = !DILocation(line: 84, column: 1, scope: !343)
!356 = distinct !DISubprogram(name: "edt_function_3", linkageName: "_ZL14edt_function_3i", scope: !2, file: !2, line: 60, type: !91, scopeLine: 61, flags: DIFlagPrototyped, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition, unit: !18, retainedNodes: !278)
!357 = !DILocalVariable(name: "a", arg: 1, scope: !356, file: !2, line: 61, type: !27)
!358 = !DILocation(line: 61, column: 7, scope: !356)
!359 = !DILocation(line: 63, column: 11, scope: !356)
!360 = !DILocation(line: 64, column: 13, scope: !356)
!361 = !DILocation(line: 66, column: 1, scope: !356)
!362 = distinct !DISubprogram(name: "edt_function_2", linkageName: "_ZL14edt_function_2ii", scope: !2, file: !2, line: 51, type: !333, scopeLine: 53, flags: DIFlagPrototyped, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition, unit: !18, retainedNodes: !278)
!363 = !DILocalVariable(name: "a", arg: 1, scope: !362, file: !2, line: 52, type: !27)
!364 = !DILocation(line: 52, column: 7, scope: !362)
!365 = !DILocalVariable(name: "b", arg: 2, scope: !362, file: !2, line: 53, type: !27)
!366 = !DILocation(line: 53, column: 7, scope: !362)
!367 = !DILocation(line: 55, column: 59, scope: !362)
!368 = !DILocation(line: 55, column: 11, scope: !362)
!369 = !DILocation(line: 56, column: 15, scope: !362)
!370 = !DILocation(line: 56, column: 17, scope: !362)
!371 = !DILocation(line: 56, column: 13, scope: !362)
!372 = !DILocation(line: 58, column: 1, scope: !362)
!373 = distinct !DISubprogram(name: "edt_function_1", linkageName: "_ZL14edt_function_1i", scope: !2, file: !2, line: 44, type: !91, scopeLine: 45, flags: DIFlagPrototyped, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition, unit: !18, retainedNodes: !278)
!374 = !DILocalVariable(name: "b", arg: 1, scope: !373, file: !2, line: 45, type: !27)
!375 = !DILocation(line: 45, column: 7, scope: !373)
!376 = !DILocation(line: 47, column: 51, scope: !373)
!377 = !DILocation(line: 47, column: 11, scope: !373)
!378 = !DILocation(line: 49, column: 1, scope: !373)
