clang++ -fopenmp taskwithdeps.cpp -Xclang -plugin -Xclang omp-plugin \
	-fplugin=/home/randres/projects/carts/.install/carts/lib/libOpenMPPlugin.so -mllvm \
	-debug-only=omp-plugin -S
- - - - - - - - - - - - - - - - - - - 
Starting CARTS - OpenMPPluginAction...
- - - - - - - - - - - - - - - - - - - 
CreateASTConsumer with file: taskwithdeps.cpp
Found OpenMP Directive (OMPParallelDirective) at taskwithdeps.cpp:13:3
- - - - - - - - - - - - - - - - - - - 
Parallel Directive found at 13
Found OpenMP Directive (OMPSingleDirective) at taskwithdeps.cpp:15:5
- - - - - - - - - - - - - - - - - - - 
Single Directive found at 15
Found OpenMP Directive (OMPTaskDirective) at taskwithdeps.cpp:19:9
- - - - - - - - - - - - - - - - - - - 
Task Directive found at 19
Found OpenMP Directive (OMPTaskDirective) at taskwithdeps.cpp:23:9
- - - - - - - - - - - - - - - - - - - 
Task Directive found at 23
- - - - - - - - - - - - - - - - - - - 
Printing OpenMP Directive Hierarchy...
  Replaced directive at taskwithdeps.cpp:23:9 with call: edt_function_1(i, B, A);

  Replaced directive at taskwithdeps.cpp:19:9 with call: edt_function_2(i, A);

  Replaced directive at taskwithdeps.cpp:15:5 with call: edt_function_3(N, A, B);

  Replaced directive at taskwithdeps.cpp:13:3 with call: edt_function_4(N, A, B);


- - - - - - - - - - - - - - - - - -
Finished CARTS - OpenMPPluginAction
- - - - - - - - - - - - - - - - - -
Rewritten code saved to taskwithdeps_annotaded.cpp
Directive: parallel
  Location: taskwithdeps.cpp:13:3
  Directive: single
    Location: taskwithdeps.cpp:15:5
  Directive: task
    Location: taskwithdeps.cpp:19:9
    Dependencies: No
  Directive: task
    Location: taskwithdeps.cpp:23:9
    Dependencies: No
  Number of Threads: 1
clang++ -O0 -g3 taskwithdeps_annotaded.cpp -S -emit-llvm -o taskwithdeps.ll
opt -load-pass-plugin=/home/randres/projects/carts/.install/carts/lib/ARTSTransform.so \
	-debug-only=arts-transform,arts,carts,arts-ir-builder\
	-passes="arts-transform" taskwithdeps.ll -o taskwithdeps_transformed.ll

-------------------------------------------------
[arts-transform] Running ARTSTransformPass on Module: taskwithdeps.ll

-------------------------------------------------
[arts-transform] Found global annotations
[4 x { ptr, ptr, ptr, i32, ptr }] [{ ptr, ptr, ptr, i32, ptr } { ptr @_ZL14edt_function_3iPdS_, ptr @.str.3, ptr @.str.1, i32 38, ptr null }, { ptr, ptr, ptr, i32, ptr } { ptr @_ZL14edt_function_1iPdS_, ptr @.str.4, ptr @.str.1, i32 23, ptr null }, { ptr, ptr, ptr, i32, ptr } { ptr @_ZL14edt_function_2iPd, ptr @.str.5, ptr @.str.1, i32 31, ptr null }, { ptr, ptr, ptr, i32, ptr } { ptr @_ZL14edt_function_4iPdS_, ptr @.str.6, ptr @.str.1, i32 55, ptr null }]
[arts-transform] Global annotation: { ptr, ptr, ptr, i32, ptr } { ptr @_ZL14edt_function_3iPdS_, ptr @.str.3, ptr @.str.1, i32 38, ptr null }
[arts-transform] Argument 0: ; Function Attrs: mustprogress noinline optnone uwtable
define internal void @_ZL14edt_function_3iPdS_(i32 noundef %N, ptr noundef %A, ptr noundef %B) #0 !dbg !286 {
entry:
  %N.addr = alloca i32, align 4
  %A.addr = alloca ptr, align 8
  %B.addr = alloca ptr, align 8
  %i = alloca i32, align 4
  store i32 %N, ptr %N.addr, align 4
  call void @llvm.dbg.declare(metadata ptr %N.addr, metadata !287, metadata !DIExpression()), !dbg !288
  call void @llvm.var.annotation.p0.p0(ptr %N.addr, ptr @.str, ptr @.str.1, i32 39, ptr null)
  store ptr %A, ptr %A.addr, align 8
  call void @llvm.dbg.declare(metadata ptr %A.addr, metadata !289, metadata !DIExpression()), !dbg !290
  call void @llvm.var.annotation.p0.p0(ptr %A.addr, ptr @.str, ptr @.str.1, i32 40, ptr null)
  store ptr %B, ptr %B.addr, align 8
  call void @llvm.dbg.declare(metadata ptr %B.addr, metadata !291, metadata !DIExpression()), !dbg !292
  call void @llvm.var.annotation.p0.p0(ptr %B.addr, ptr @.str, ptr @.str.1, i32 41, ptr null)
  call void @llvm.dbg.declare(metadata ptr %i, metadata !293, metadata !DIExpression()), !dbg !295
  store i32 0, ptr %i, align 4, !dbg !295
  br label %for.cond, !dbg !296

for.cond:                                         ; preds = %for.inc, %entry
  %0 = load i32, ptr %i, align 4, !dbg !297
  %1 = load i32, ptr %N.addr, align 4, !dbg !299
  %cmp = icmp slt i32 %0, %1, !dbg !300
  br i1 %cmp, label %for.body, label %for.end, !dbg !301

for.body:                                         ; preds = %for.cond
  %2 = load i32, ptr %i, align 4, !dbg !302
  %3 = load ptr, ptr %A.addr, align 8, !dbg !304
  call void @_ZL14edt_function_2iPd(i32 noundef %2, ptr noundef %3), !dbg !305
  %4 = load i32, ptr %i, align 4, !dbg !306
  %5 = load ptr, ptr %B.addr, align 8, !dbg !307
  %6 = load ptr, ptr %A.addr, align 8, !dbg !308
  call void @_ZL14edt_function_1iPdS_(i32 noundef %4, ptr noundef %5, ptr noundef %6), !dbg !309
  br label %for.inc, !dbg !310

for.inc:                                          ; preds = %for.body
  %7 = load i32, ptr %i, align 4, !dbg !311
  %inc = add nsw i32 %7, 1, !dbg !311
  store i32 %inc, ptr %i, align 4, !dbg !311
  br label %for.cond, !dbg !312, !llvm.loop !313

for.end:                                          ; preds = %for.cond
  ret void, !dbg !316
}

[arts-transform] Argument 1: @.str.3 = private unnamed_addr constant [12 x i8] c"arts.single\00", section "llvm.metadata"
[arts-transform] Argument 2: @.str.1 = private unnamed_addr constant [27 x i8] c"taskwithdeps_annotaded.cpp\00", section "llvm.metadata"
[arts-transform] Argument 3: i32 38
[arts-transform] Argument 4: ptr null
[arts-transform] Global annotation: { ptr, ptr, ptr, i32, ptr } { ptr @_ZL14edt_function_1iPdS_, ptr @.str.4, ptr @.str.1, i32 23, ptr null }
[arts-transform] Argument 0: ; Function Attrs: mustprogress noinline nounwind optnone uwtable
define internal void @_ZL14edt_function_1iPdS_(i32 noundef %i, ptr noundef %B, ptr noundef %A) #3 !dbg !330 {
entry:
  %i.addr = alloca i32, align 4
  %B.addr = alloca ptr, align 8
  %A.addr = alloca ptr, align 8
  store i32 %i, ptr %i.addr, align 4
  call void @llvm.dbg.declare(metadata ptr %i.addr, metadata !331, metadata !DIExpression()), !dbg !332
  call void @llvm.var.annotation.p0.p0(ptr %i.addr, ptr @.str.2, ptr @.str.1, i32 24, ptr null)
  store ptr %B, ptr %B.addr, align 8
  call void @llvm.dbg.declare(metadata ptr %B.addr, metadata !333, metadata !DIExpression()), !dbg !334
  call void @llvm.var.annotation.p0.p0(ptr %B.addr, ptr @.str, ptr @.str.1, i32 25, ptr null)
  store ptr %A, ptr %A.addr, align 8
  call void @llvm.dbg.declare(metadata ptr %A.addr, metadata !335, metadata !DIExpression()), !dbg !336
  call void @llvm.var.annotation.p0.p0(ptr %A.addr, ptr @.str, ptr @.str.1, i32 26, ptr null)
  %0 = load ptr, ptr %A.addr, align 8, !dbg !337
  %1 = load i32, ptr %i.addr, align 4, !dbg !338
  %idxprom = sext i32 %1 to i64, !dbg !337
  %arrayidx = getelementptr inbounds double, ptr %0, i64 %idxprom, !dbg !337
  %2 = load double, ptr %arrayidx, align 8, !dbg !337
  %3 = load i32, ptr %i.addr, align 4, !dbg !339
  %cmp = icmp sgt i32 %3, 0, !dbg !340
  br i1 %cmp, label %cond.true, label %cond.false, !dbg !339

cond.true:                                        ; preds = %entry
  %4 = load ptr, ptr %A.addr, align 8, !dbg !341
  %5 = load i32, ptr %i.addr, align 4, !dbg !342
  %sub = sub nsw i32 %5, 1, !dbg !343
  %idxprom1 = sext i32 %sub to i64, !dbg !341
  %arrayidx2 = getelementptr inbounds double, ptr %4, i64 %idxprom1, !dbg !341
  %6 = load double, ptr %arrayidx2, align 8, !dbg !341
  br label %cond.end, !dbg !339

cond.false:                                       ; preds = %entry
  br label %cond.end, !dbg !339

cond.end:                                         ; preds = %cond.false, %cond.true
  %cond = phi double [ %6, %cond.true ], [ 0.000000e+00, %cond.false ], !dbg !339
  %add = fadd double %2, %cond, !dbg !344
  %7 = load ptr, ptr %B.addr, align 8, !dbg !345
  %8 = load i32, ptr %i.addr, align 4, !dbg !346
  %idxprom3 = sext i32 %8 to i64, !dbg !345
  %arrayidx4 = getelementptr inbounds double, ptr %7, i64 %idxprom3, !dbg !345
  store double %add, ptr %arrayidx4, align 8, !dbg !347
  ret void, !dbg !348
}

[arts-transform] Argument 1: @.str.4 = private unnamed_addr constant [52 x i8] c"arts.task deps(in: A[i], A[i - 1])  deps(out: B[i])\00", section "llvm.metadata"
[arts-transform] Argument 2: @.str.1 = private unnamed_addr constant [27 x i8] c"taskwithdeps_annotaded.cpp\00", section "llvm.metadata"
[arts-transform] Argument 3: i32 23
[arts-transform] Argument 4: ptr null
[arts-transform] Global annotation: { ptr, ptr, ptr, i32, ptr } { ptr @_ZL14edt_function_2iPd, ptr @.str.5, ptr @.str.1, i32 31, ptr null }
[arts-transform] Argument 0: ; Function Attrs: mustprogress noinline nounwind optnone uwtable
define internal void @_ZL14edt_function_2iPd(i32 noundef %i, ptr noundef %A) #3 !dbg !317 {
entry:
  %i.addr = alloca i32, align 4
  %A.addr = alloca ptr, align 8
  store i32 %i, ptr %i.addr, align 4
  call void @llvm.dbg.declare(metadata ptr %i.addr, metadata !320, metadata !DIExpression()), !dbg !321
  call void @llvm.var.annotation.p0.p0(ptr %i.addr, ptr @.str.2, ptr @.str.1, i32 32, ptr null)
  store ptr %A, ptr %A.addr, align 8
  call void @llvm.dbg.declare(metadata ptr %A.addr, metadata !322, metadata !DIExpression()), !dbg !323
  call void @llvm.var.annotation.p0.p0(ptr %A.addr, ptr @.str, ptr @.str.1, i32 33, ptr null)
  %0 = load i32, ptr %i.addr, align 4, !dbg !324
  %conv = sitofp i32 %0 to double, !dbg !324
  %mul = fmul double %conv, 1.000000e+00, !dbg !325
  %1 = load ptr, ptr %A.addr, align 8, !dbg !326
  %2 = load i32, ptr %i.addr, align 4, !dbg !327
  %idxprom = sext i32 %2 to i64, !dbg !326
  %arrayidx = getelementptr inbounds double, ptr %1, i64 %idxprom, !dbg !326
  store double %mul, ptr %arrayidx, align 8, !dbg !328
  ret void, !dbg !329
}

[arts-transform] Argument 1: @.str.5 = private unnamed_addr constant [26 x i8] c"arts.task deps(out: A[i])\00", section "llvm.metadata"
[arts-transform] Argument 2: @.str.1 = private unnamed_addr constant [27 x i8] c"taskwithdeps_annotaded.cpp\00", section "llvm.metadata"
[arts-transform] Argument 3: i32 31
[arts-transform] Argument 4: ptr null
[arts-transform] Global annotation: { ptr, ptr, ptr, i32, ptr } { ptr @_ZL14edt_function_4iPdS_, ptr @.str.6, ptr @.str.1, i32 55, ptr null }
[arts-transform] Argument 0: ; Function Attrs: mustprogress noinline optnone uwtable
define internal void @_ZL14edt_function_4iPdS_(i32 noundef %N, ptr noundef %A, ptr noundef %B) #0 !dbg !274 {
entry:
  %N.addr = alloca i32, align 4
  %A.addr = alloca ptr, align 8
  %B.addr = alloca ptr, align 8
  store i32 %N, ptr %N.addr, align 4
  call void @llvm.dbg.declare(metadata ptr %N.addr, metadata !275, metadata !DIExpression()), !dbg !276
  call void @llvm.var.annotation.p0.p0(ptr %N.addr, ptr @.str, ptr @.str.1, i32 56, ptr null)
  store ptr %A, ptr %A.addr, align 8
  call void @llvm.dbg.declare(metadata ptr %A.addr, metadata !277, metadata !DIExpression()), !dbg !278
  call void @llvm.var.annotation.p0.p0(ptr %A.addr, ptr @.str, ptr @.str.1, i32 57, ptr null)
  store ptr %B, ptr %B.addr, align 8
  call void @llvm.dbg.declare(metadata ptr %B.addr, metadata !279, metadata !DIExpression()), !dbg !280
  call void @llvm.var.annotation.p0.p0(ptr %B.addr, ptr @.str, ptr @.str.1, i32 58, ptr null)
  %0 = load i32, ptr %N.addr, align 4, !dbg !281
  %1 = load ptr, ptr %A.addr, align 8, !dbg !282
  %2 = load ptr, ptr %B.addr, align 8, !dbg !283
  call void @_ZL14edt_function_3iPdS_(i32 noundef %0, ptr noundef %1, ptr noundef %2), !dbg !284
  ret void, !dbg !285
}

[arts-transform] Argument 1: @.str.6 = private unnamed_addr constant [14 x i8] c"arts.parallel\00", section "llvm.metadata"
[arts-transform] Argument 2: @.str.1 = private unnamed_addr constant [27 x i8] c"taskwithdeps_annotaded.cpp\00", section "llvm.metadata"
[arts-transform] Argument 3: i32 55
[arts-transform] Argument 4: ptr null

-------------------------------------------------
[arts-transform] OmpTransformPass has finished

; ModuleID = 'taskwithdeps.ll'
source_filename = "taskwithdeps_annotaded.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [13 x i8] c"arts.default\00", section "llvm.metadata"
@.str.1 = private unnamed_addr constant [27 x i8] c"taskwithdeps_annotaded.cpp\00", section "llvm.metadata"
@.str.2 = private unnamed_addr constant [18 x i8] c"arts.firstprivate\00", section "llvm.metadata"
@.str.3 = private unnamed_addr constant [12 x i8] c"arts.single\00", section "llvm.metadata"
@.str.4 = private unnamed_addr constant [52 x i8] c"arts.task deps(in: A[i], A[i - 1])  deps(out: B[i])\00", section "llvm.metadata"
@.str.5 = private unnamed_addr constant [26 x i8] c"arts.task deps(out: A[i])\00", section "llvm.metadata"
@.str.6 = private unnamed_addr constant [14 x i8] c"arts.parallel\00", section "llvm.metadata"
@llvm.global.annotations = appending global [4 x { ptr, ptr, ptr, i32, ptr }] [{ ptr, ptr, ptr, i32, ptr } { ptr @_ZL14edt_function_3iPdS_, ptr @.str.3, ptr @.str.1, i32 38, ptr null }, { ptr, ptr, ptr, i32, ptr } { ptr @_ZL14edt_function_1iPdS_, ptr @.str.4, ptr @.str.1, i32 23, ptr null }, { ptr, ptr, ptr, i32, ptr } { ptr @_ZL14edt_function_2iPd, ptr @.str.5, ptr @.str.1, i32 31, ptr null }, { ptr, ptr, ptr, i32, ptr } { ptr @_ZL14edt_function_4iPdS_, ptr @.str.6, ptr @.str.1, i32 55, ptr null }], section "llvm.metadata"

; Function Attrs: mustprogress noinline optnone uwtable
define dso_local void @_Z7computeiPdS_(i32 noundef %N, ptr noundef %A, ptr noundef %B) #0 !dbg !258 {
entry:
  %N.addr = alloca i32, align 4
  %A.addr = alloca ptr, align 8
  %B.addr = alloca ptr, align 8
  store i32 %N, ptr %N.addr, align 4
  call void @llvm.dbg.declare(metadata ptr %N.addr, metadata !263, metadata !DIExpression()), !dbg !264
  store ptr %A, ptr %A.addr, align 8
  call void @llvm.dbg.declare(metadata ptr %A.addr, metadata !265, metadata !DIExpression()), !dbg !266
  store ptr %B, ptr %B.addr, align 8
  call void @llvm.dbg.declare(metadata ptr %B.addr, metadata !267, metadata !DIExpression()), !dbg !268
  %0 = load i32, ptr %N.addr, align 4, !dbg !269
  %1 = load ptr, ptr %A.addr, align 8, !dbg !270
  %2 = load ptr, ptr %B.addr, align 8, !dbg !271
  call void @_ZL14edt_function_4iPdS_(i32 noundef %0, ptr noundef %1, ptr noundef %2), !dbg !272
  ret void, !dbg !273
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; Function Attrs: mustprogress noinline optnone uwtable
define internal void @_ZL14edt_function_4iPdS_(i32 noundef %N, ptr noundef %A, ptr noundef %B) #0 !dbg !274 {
entry:
  %N.addr = alloca i32, align 4
  %A.addr = alloca ptr, align 8
  %B.addr = alloca ptr, align 8
  store i32 %N, ptr %N.addr, align 4
  call void @llvm.dbg.declare(metadata ptr %N.addr, metadata !275, metadata !DIExpression()), !dbg !276
  call void @llvm.var.annotation.p0.p0(ptr %N.addr, ptr @.str, ptr @.str.1, i32 56, ptr null)
  store ptr %A, ptr %A.addr, align 8
  call void @llvm.dbg.declare(metadata ptr %A.addr, metadata !277, metadata !DIExpression()), !dbg !278
  call void @llvm.var.annotation.p0.p0(ptr %A.addr, ptr @.str, ptr @.str.1, i32 57, ptr null)
  store ptr %B, ptr %B.addr, align 8
  call void @llvm.dbg.declare(metadata ptr %B.addr, metadata !279, metadata !DIExpression()), !dbg !280
  call void @llvm.var.annotation.p0.p0(ptr %B.addr, ptr @.str, ptr @.str.1, i32 58, ptr null)
  %0 = load i32, ptr %N.addr, align 4, !dbg !281
  %1 = load ptr, ptr %A.addr, align 8, !dbg !282
  %2 = load ptr, ptr %B.addr, align 8, !dbg !283
  call void @_ZL14edt_function_3iPdS_(i32 noundef %0, ptr noundef %1, ptr noundef %2), !dbg !284
  ret void, !dbg !285
}

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.var.annotation.p0.p0(ptr, ptr, ptr, i32, ptr) #2

; Function Attrs: mustprogress noinline optnone uwtable
define internal void @_ZL14edt_function_3iPdS_(i32 noundef %N, ptr noundef %A, ptr noundef %B) #0 !dbg !286 {
entry:
  %N.addr = alloca i32, align 4
  %A.addr = alloca ptr, align 8
  %B.addr = alloca ptr, align 8
  %i = alloca i32, align 4
  store i32 %N, ptr %N.addr, align 4
  call void @llvm.dbg.declare(metadata ptr %N.addr, metadata !287, metadata !DIExpression()), !dbg !288
  call void @llvm.var.annotation.p0.p0(ptr %N.addr, ptr @.str, ptr @.str.1, i32 39, ptr null)
  store ptr %A, ptr %A.addr, align 8
  call void @llvm.dbg.declare(metadata ptr %A.addr, metadata !289, metadata !DIExpression()), !dbg !290
  call void @llvm.var.annotation.p0.p0(ptr %A.addr, ptr @.str, ptr @.str.1, i32 40, ptr null)
  store ptr %B, ptr %B.addr, align 8
  call void @llvm.dbg.declare(metadata ptr %B.addr, metadata !291, metadata !DIExpression()), !dbg !292
  call void @llvm.var.annotation.p0.p0(ptr %B.addr, ptr @.str, ptr @.str.1, i32 41, ptr null)
  call void @llvm.dbg.declare(metadata ptr %i, metadata !293, metadata !DIExpression()), !dbg !295
  store i32 0, ptr %i, align 4, !dbg !295
  br label %for.cond, !dbg !296

for.cond:                                         ; preds = %for.inc, %entry
  %0 = load i32, ptr %i, align 4, !dbg !297
  %1 = load i32, ptr %N.addr, align 4, !dbg !299
  %cmp = icmp slt i32 %0, %1, !dbg !300
  br i1 %cmp, label %for.body, label %for.end, !dbg !301

for.body:                                         ; preds = %for.cond
  %2 = load i32, ptr %i, align 4, !dbg !302
  %3 = load ptr, ptr %A.addr, align 8, !dbg !304
  call void @_ZL14edt_function_2iPd(i32 noundef %2, ptr noundef %3), !dbg !305
  %4 = load i32, ptr %i, align 4, !dbg !306
  %5 = load ptr, ptr %B.addr, align 8, !dbg !307
  %6 = load ptr, ptr %A.addr, align 8, !dbg !308
  call void @_ZL14edt_function_1iPdS_(i32 noundef %4, ptr noundef %5, ptr noundef %6), !dbg !309
  br label %for.inc, !dbg !310

for.inc:                                          ; preds = %for.body
  %7 = load i32, ptr %i, align 4, !dbg !311
  %inc = add nsw i32 %7, 1, !dbg !311
  store i32 %inc, ptr %i, align 4, !dbg !311
  br label %for.cond, !dbg !312, !llvm.loop !313

for.end:                                          ; preds = %for.cond
  ret void, !dbg !316
}

; Function Attrs: mustprogress noinline nounwind optnone uwtable
define internal void @_ZL14edt_function_2iPd(i32 noundef %i, ptr noundef %A) #3 !dbg !317 {
entry:
  %i.addr = alloca i32, align 4
  %A.addr = alloca ptr, align 8
  store i32 %i, ptr %i.addr, align 4
  call void @llvm.dbg.declare(metadata ptr %i.addr, metadata !320, metadata !DIExpression()), !dbg !321
  call void @llvm.var.annotation.p0.p0(ptr %i.addr, ptr @.str.2, ptr @.str.1, i32 32, ptr null)
  store ptr %A, ptr %A.addr, align 8
  call void @llvm.dbg.declare(metadata ptr %A.addr, metadata !322, metadata !DIExpression()), !dbg !323
  call void @llvm.var.annotation.p0.p0(ptr %A.addr, ptr @.str, ptr @.str.1, i32 33, ptr null)
  %0 = load i32, ptr %i.addr, align 4, !dbg !324
  %conv = sitofp i32 %0 to double, !dbg !324
  %mul = fmul double %conv, 1.000000e+00, !dbg !325
  %1 = load ptr, ptr %A.addr, align 8, !dbg !326
  %2 = load i32, ptr %i.addr, align 4, !dbg !327
  %idxprom = sext i32 %2 to i64, !dbg !326
  %arrayidx = getelementptr inbounds double, ptr %1, i64 %idxprom, !dbg !326
  store double %mul, ptr %arrayidx, align 8, !dbg !328
  ret void, !dbg !329
}

; Function Attrs: mustprogress noinline nounwind optnone uwtable
define internal void @_ZL14edt_function_1iPdS_(i32 noundef %i, ptr noundef %B, ptr noundef %A) #3 !dbg !330 {
entry:
  %i.addr = alloca i32, align 4
  %B.addr = alloca ptr, align 8
  %A.addr = alloca ptr, align 8
  store i32 %i, ptr %i.addr, align 4
  call void @llvm.dbg.declare(metadata ptr %i.addr, metadata !331, metadata !DIExpression()), !dbg !332
  call void @llvm.var.annotation.p0.p0(ptr %i.addr, ptr @.str.2, ptr @.str.1, i32 24, ptr null)
  store ptr %B, ptr %B.addr, align 8
  call void @llvm.dbg.declare(metadata ptr %B.addr, metadata !333, metadata !DIExpression()), !dbg !334
  call void @llvm.var.annotation.p0.p0(ptr %B.addr, ptr @.str, ptr @.str.1, i32 25, ptr null)
  store ptr %A, ptr %A.addr, align 8
  call void @llvm.dbg.declare(metadata ptr %A.addr, metadata !335, metadata !DIExpression()), !dbg !336
  call void @llvm.var.annotation.p0.p0(ptr %A.addr, ptr @.str, ptr @.str.1, i32 26, ptr null)
  %0 = load ptr, ptr %A.addr, align 8, !dbg !337
  %1 = load i32, ptr %i.addr, align 4, !dbg !338
  %idxprom = sext i32 %1 to i64, !dbg !337
  %arrayidx = getelementptr inbounds double, ptr %0, i64 %idxprom, !dbg !337
  %2 = load double, ptr %arrayidx, align 8, !dbg !337
  %3 = load i32, ptr %i.addr, align 4, !dbg !339
  %cmp = icmp sgt i32 %3, 0, !dbg !340
  br i1 %cmp, label %cond.true, label %cond.false, !dbg !339

cond.true:                                        ; preds = %entry
  %4 = load ptr, ptr %A.addr, align 8, !dbg !341
  %5 = load i32, ptr %i.addr, align 4, !dbg !342
  %sub = sub nsw i32 %5, 1, !dbg !343
  %idxprom1 = sext i32 %sub to i64, !dbg !341
  %arrayidx2 = getelementptr inbounds double, ptr %4, i64 %idxprom1, !dbg !341
  %6 = load double, ptr %arrayidx2, align 8, !dbg !341
  br label %cond.end, !dbg !339

cond.false:                                       ; preds = %entry
  br label %cond.end, !dbg !339

cond.end:                                         ; preds = %cond.false, %cond.true
  %cond = phi double [ %6, %cond.true ], [ 0.000000e+00, %cond.false ], !dbg !339
  %add = fadd double %2, %cond, !dbg !344
  %7 = load ptr, ptr %B.addr, align 8, !dbg !345
  %8 = load i32, ptr %i.addr, align 4, !dbg !346
  %idxprom3 = sext i32 %8 to i64, !dbg !345
  %arrayidx4 = getelementptr inbounds double, ptr %7, i64 %idxprom3, !dbg !345
  store double %add, ptr %arrayidx4, align 8, !dbg !347
  ret void, !dbg !348
}

attributes #0 = { mustprogress noinline optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
attributes #2 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }
attributes #3 = { mustprogress noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!250, !251, !252, !253, !254, !255, !256}
!llvm.ident = !{!257}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang version 18.1.8", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, imports: !2, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "taskwithdeps_annotaded.cpp", directory: "/home/randres/projects/carts/examples/taskwithdepsfor", checksumkind: CSK_MD5, checksum: "cbdac67b2a264279c6a4351baa03e563")
!2 = !{!3, !11, !15, !22, !26, !34, !39, !41, !49, !53, !57, !67, !69, !73, !77, !81, !86, !90, !94, !98, !102, !110, !114, !118, !120, !124, !128, !133, !139, !143, !147, !149, !157, !161, !169, !171, !175, !179, !183, !187, !192, !197, !202, !203, !204, !205, !207, !208, !209, !210, !211, !212, !213, !215, !216, !217, !218, !219, !220, !221, !226, !227, !228, !229, !230, !231, !232, !233, !234, !235, !236, !237, !238, !239, !240, !241, !242, !243, !244, !245, !246, !247, !248, !249}
!3 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !5, file: !10, line: 52)
!4 = !DINamespace(name: "std", scope: null)
!5 = !DISubprogram(name: "abs", scope: !6, file: !6, line: 848, type: !7, flags: DIFlagPrototyped, spFlags: 0)
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
!23 = !DISubprogram(name: "abort", scope: !6, file: !6, line: 598, type: !24, flags: DIFlagPrototyped | DIFlagNoReturn, spFlags: 0)
!24 = !DISubroutineType(types: !25)
!25 = !{null}
!26 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !27, file: !14, line: 132)
!27 = !DISubprogram(name: "aligned_alloc", scope: !6, file: !6, line: 592, type: !28, flags: DIFlagPrototyped, spFlags: 0)
!28 = !DISubroutineType(types: !29)
!29 = !{!30, !31, !31}
!30 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64)
!31 = !DIDerivedType(tag: DW_TAG_typedef, name: "size_t", file: !32, line: 18, baseType: !33)
!32 = !DIFile(filename: ".install/llvm/lib/clang/18/include/__stddef_size_t.h", directory: "/home/randres/projects/carts", checksumkind: CSK_MD5, checksum: "2c44e821a2b1951cde2eb0fb2e656867")
!33 = !DIBasicType(name: "unsigned long", size: 64, encoding: DW_ATE_unsigned)
!34 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !35, file: !14, line: 134)
!35 = !DISubprogram(name: "atexit", scope: !6, file: !6, line: 602, type: !36, flags: DIFlagPrototyped, spFlags: 0)
!36 = !DISubroutineType(types: !37)
!37 = !{!9, !38}
!38 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !24, size: 64)
!39 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !40, file: !14, line: 137)
!40 = !DISubprogram(name: "at_quick_exit", scope: !6, file: !6, line: 607, type: !36, flags: DIFlagPrototyped, spFlags: 0)
!41 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !42, file: !14, line: 140)
!42 = !DISubprogram(name: "atof", scope: !6, file: !6, line: 102, type: !43, flags: DIFlagPrototyped, spFlags: 0)
!43 = !DISubroutineType(types: !44)
!44 = !{!45, !46}
!45 = !DIBasicType(name: "double", size: 64, encoding: DW_ATE_float)
!46 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !47, size: 64)
!47 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !48)
!48 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!49 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !50, file: !14, line: 141)
!50 = !DISubprogram(name: "atoi", scope: !6, file: !6, line: 105, type: !51, flags: DIFlagPrototyped, spFlags: 0)
!51 = !DISubroutineType(types: !52)
!52 = !{!9, !46}
!53 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !54, file: !14, line: 142)
!54 = !DISubprogram(name: "atol", scope: !6, file: !6, line: 108, type: !55, flags: DIFlagPrototyped, spFlags: 0)
!55 = !DISubroutineType(types: !56)
!56 = !{!20, !46}
!57 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !58, file: !14, line: 143)
!58 = !DISubprogram(name: "bsearch", scope: !6, file: !6, line: 828, type: !59, flags: DIFlagPrototyped, spFlags: 0)
!59 = !DISubroutineType(types: !60)
!60 = !{!30, !61, !61, !31, !31, !63}
!61 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !62, size: 64)
!62 = !DIDerivedType(tag: DW_TAG_const_type, baseType: null)
!63 = !DIDerivedType(tag: DW_TAG_typedef, name: "__compar_fn_t", file: !6, line: 816, baseType: !64)
!64 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !65, size: 64)
!65 = !DISubroutineType(types: !66)
!66 = !{!9, !61, !61}
!67 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !68, file: !14, line: 144)
!68 = !DISubprogram(name: "calloc", scope: !6, file: !6, line: 543, type: !28, flags: DIFlagPrototyped, spFlags: 0)
!69 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !70, file: !14, line: 145)
!70 = !DISubprogram(name: "div", scope: !6, file: !6, line: 860, type: !71, flags: DIFlagPrototyped, spFlags: 0)
!71 = !DISubroutineType(types: !72)
!72 = !{!12, !9, !9}
!73 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !74, file: !14, line: 146)
!74 = !DISubprogram(name: "exit", scope: !6, file: !6, line: 624, type: !75, flags: DIFlagPrototyped | DIFlagNoReturn, spFlags: 0)
!75 = !DISubroutineType(types: !76)
!76 = !{null, !9}
!77 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !78, file: !14, line: 147)
!78 = !DISubprogram(name: "free", scope: !6, file: !6, line: 555, type: !79, flags: DIFlagPrototyped, spFlags: 0)
!79 = !DISubroutineType(types: !80)
!80 = !{null, !30}
!81 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !82, file: !14, line: 148)
!82 = !DISubprogram(name: "getenv", scope: !6, file: !6, line: 641, type: !83, flags: DIFlagPrototyped, spFlags: 0)
!83 = !DISubroutineType(types: !84)
!84 = !{!85, !46}
!85 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !48, size: 64)
!86 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !87, file: !14, line: 149)
!87 = !DISubprogram(name: "labs", scope: !6, file: !6, line: 849, type: !88, flags: DIFlagPrototyped, spFlags: 0)
!88 = !DISubroutineType(types: !89)
!89 = !{!20, !20}
!90 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !91, file: !14, line: 150)
!91 = !DISubprogram(name: "ldiv", scope: !6, file: !6, line: 862, type: !92, flags: DIFlagPrototyped, spFlags: 0)
!92 = !DISubroutineType(types: !93)
!93 = !{!16, !20, !20}
!94 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !95, file: !14, line: 151)
!95 = !DISubprogram(name: "malloc", scope: !6, file: !6, line: 540, type: !96, flags: DIFlagPrototyped, spFlags: 0)
!96 = !DISubroutineType(types: !97)
!97 = !{!30, !31}
!98 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !99, file: !14, line: 153)
!99 = !DISubprogram(name: "mblen", scope: !6, file: !6, line: 930, type: !100, flags: DIFlagPrototyped, spFlags: 0)
!100 = !DISubroutineType(types: !101)
!101 = !{!9, !46, !31}
!102 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !103, file: !14, line: 154)
!103 = !DISubprogram(name: "mbstowcs", scope: !6, file: !6, line: 941, type: !104, flags: DIFlagPrototyped, spFlags: 0)
!104 = !DISubroutineType(types: !105)
!105 = !{!31, !106, !109, !31}
!106 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !107)
!107 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !108, size: 64)
!108 = !DIBasicType(name: "wchar_t", size: 32, encoding: DW_ATE_signed)
!109 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !46)
!110 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !111, file: !14, line: 155)
!111 = !DISubprogram(name: "mbtowc", scope: !6, file: !6, line: 933, type: !112, flags: DIFlagPrototyped, spFlags: 0)
!112 = !DISubroutineType(types: !113)
!113 = !{!9, !106, !109, !31}
!114 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !115, file: !14, line: 157)
!115 = !DISubprogram(name: "qsort", scope: !6, file: !6, line: 838, type: !116, flags: DIFlagPrototyped, spFlags: 0)
!116 = !DISubroutineType(types: !117)
!117 = !{null, !30, !31, !31, !63}
!118 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !119, file: !14, line: 160)
!119 = !DISubprogram(name: "quick_exit", scope: !6, file: !6, line: 630, type: !75, flags: DIFlagPrototyped | DIFlagNoReturn, spFlags: 0)
!120 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !121, file: !14, line: 163)
!121 = !DISubprogram(name: "rand", scope: !6, file: !6, line: 454, type: !122, flags: DIFlagPrototyped, spFlags: 0)
!122 = !DISubroutineType(types: !123)
!123 = !{!9}
!124 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !125, file: !14, line: 164)
!125 = !DISubprogram(name: "realloc", scope: !6, file: !6, line: 551, type: !126, flags: DIFlagPrototyped, spFlags: 0)
!126 = !DISubroutineType(types: !127)
!127 = !{!30, !30, !31}
!128 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !129, file: !14, line: 165)
!129 = !DISubprogram(name: "srand", scope: !6, file: !6, line: 456, type: !130, flags: DIFlagPrototyped, spFlags: 0)
!130 = !DISubroutineType(types: !131)
!131 = !{null, !132}
!132 = !DIBasicType(name: "unsigned int", size: 32, encoding: DW_ATE_unsigned)
!133 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !134, file: !14, line: 166)
!134 = !DISubprogram(name: "strtod", scope: !6, file: !6, line: 118, type: !135, flags: DIFlagPrototyped, spFlags: 0)
!135 = !DISubroutineType(types: !136)
!136 = !{!45, !109, !137}
!137 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !138)
!138 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !85, size: 64)
!139 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !140, file: !14, line: 167)
!140 = !DISubprogram(name: "strtol", scope: !6, file: !6, line: 177, type: !141, flags: DIFlagPrototyped, spFlags: 0)
!141 = !DISubroutineType(types: !142)
!142 = !{!20, !109, !137, !9}
!143 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !144, file: !14, line: 168)
!144 = !DISubprogram(name: "strtoul", scope: !6, file: !6, line: 181, type: !145, flags: DIFlagPrototyped, spFlags: 0)
!145 = !DISubroutineType(types: !146)
!146 = !{!33, !109, !137, !9}
!147 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !148, file: !14, line: 169)
!148 = !DISubprogram(name: "system", scope: !6, file: !6, line: 791, type: !51, flags: DIFlagPrototyped, spFlags: 0)
!149 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !150, file: !14, line: 171)
!150 = !DISubprogram(name: "wcstombs", scope: !6, file: !6, line: 945, type: !151, flags: DIFlagPrototyped, spFlags: 0)
!151 = !DISubroutineType(types: !152)
!152 = !{!31, !153, !154, !31}
!153 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !85)
!154 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !155)
!155 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !156, size: 64)
!156 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !108)
!157 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !158, file: !14, line: 172)
!158 = !DISubprogram(name: "wctomb", scope: !6, file: !6, line: 937, type: !159, flags: DIFlagPrototyped, spFlags: 0)
!159 = !DISubroutineType(types: !160)
!160 = !{!9, !85, !108}
!161 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !162, entity: !163, file: !14, line: 200)
!162 = !DINamespace(name: "__gnu_cxx", scope: null)
!163 = !DIDerivedType(tag: DW_TAG_typedef, name: "lldiv_t", file: !6, line: 81, baseType: !164)
!164 = distinct !DICompositeType(tag: DW_TAG_structure_type, file: !6, line: 77, size: 128, flags: DIFlagTypePassByValue, elements: !165, identifier: "_ZTS7lldiv_t")
!165 = !{!166, !168}
!166 = !DIDerivedType(tag: DW_TAG_member, name: "quot", scope: !164, file: !6, line: 79, baseType: !167, size: 64)
!167 = !DIBasicType(name: "long long", size: 64, encoding: DW_ATE_signed)
!168 = !DIDerivedType(tag: DW_TAG_member, name: "rem", scope: !164, file: !6, line: 80, baseType: !167, size: 64, offset: 64)
!169 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !162, entity: !170, file: !14, line: 206)
!170 = !DISubprogram(name: "_Exit", scope: !6, file: !6, line: 636, type: !75, flags: DIFlagPrototyped | DIFlagNoReturn, spFlags: 0)
!171 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !162, entity: !172, file: !14, line: 210)
!172 = !DISubprogram(name: "llabs", scope: !6, file: !6, line: 852, type: !173, flags: DIFlagPrototyped, spFlags: 0)
!173 = !DISubroutineType(types: !174)
!174 = !{!167, !167}
!175 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !162, entity: !176, file: !14, line: 216)
!176 = !DISubprogram(name: "lldiv", scope: !6, file: !6, line: 866, type: !177, flags: DIFlagPrototyped, spFlags: 0)
!177 = !DISubroutineType(types: !178)
!178 = !{!163, !167, !167}
!179 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !162, entity: !180, file: !14, line: 227)
!180 = !DISubprogram(name: "atoll", scope: !6, file: !6, line: 113, type: !181, flags: DIFlagPrototyped, spFlags: 0)
!181 = !DISubroutineType(types: !182)
!182 = !{!167, !46}
!183 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !162, entity: !184, file: !14, line: 228)
!184 = !DISubprogram(name: "strtoll", scope: !6, file: !6, line: 201, type: !185, flags: DIFlagPrototyped, spFlags: 0)
!185 = !DISubroutineType(types: !186)
!186 = !{!167, !109, !137, !9}
!187 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !162, entity: !188, file: !14, line: 229)
!188 = !DISubprogram(name: "strtoull", scope: !6, file: !6, line: 206, type: !189, flags: DIFlagPrototyped, spFlags: 0)
!189 = !DISubroutineType(types: !190)
!190 = !{!191, !109, !137, !9}
!191 = !DIBasicType(name: "unsigned long long", size: 64, encoding: DW_ATE_unsigned)
!192 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !162, entity: !193, file: !14, line: 231)
!193 = !DISubprogram(name: "strtof", scope: !6, file: !6, line: 124, type: !194, flags: DIFlagPrototyped, spFlags: 0)
!194 = !DISubroutineType(types: !195)
!195 = !{!196, !109, !137}
!196 = !DIBasicType(name: "float", size: 32, encoding: DW_ATE_float)
!197 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !162, entity: !198, file: !14, line: 232)
!198 = !DISubprogram(name: "strtold", scope: !6, file: !6, line: 127, type: !199, flags: DIFlagPrototyped, spFlags: 0)
!199 = !DISubroutineType(types: !200)
!200 = !{!201, !109, !137}
!201 = !DIBasicType(name: "long double", size: 128, encoding: DW_ATE_float)
!202 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !163, file: !14, line: 240)
!203 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !170, file: !14, line: 242)
!204 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !172, file: !14, line: 244)
!205 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !206, file: !14, line: 245)
!206 = !DISubprogram(name: "div", linkageName: "_ZN9__gnu_cxx3divExx", scope: !162, file: !14, line: 213, type: !177, flags: DIFlagPrototyped, spFlags: 0)
!207 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !176, file: !14, line: 246)
!208 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !180, file: !14, line: 248)
!209 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !193, file: !14, line: 249)
!210 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !184, file: !14, line: 250)
!211 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !188, file: !14, line: 251)
!212 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !4, entity: !198, file: !14, line: 252)
!213 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !23, file: !214, line: 38)
!214 = !DIFile(filename: "/usr/lib/gcc/x86_64-linux-gnu/11/../../../../include/c++/11/stdlib.h", directory: "", checksumkind: CSK_MD5, checksum: "0f5b773a303c24013fb112082e6d18a5")
!215 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !35, file: !214, line: 39)
!216 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !74, file: !214, line: 40)
!217 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !40, file: !214, line: 43)
!218 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !119, file: !214, line: 46)
!219 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !12, file: !214, line: 51)
!220 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !16, file: !214, line: 52)
!221 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !222, file: !214, line: 54)
!222 = !DISubprogram(name: "abs", linkageName: "_ZSt3absg", scope: !4, file: !10, line: 103, type: !223, flags: DIFlagPrototyped, spFlags: 0)
!223 = !DISubroutineType(types: !224)
!224 = !{!225, !225}
!225 = !DIBasicType(name: "__float128", size: 128, encoding: DW_ATE_float)
!226 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !42, file: !214, line: 55)
!227 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !50, file: !214, line: 56)
!228 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !54, file: !214, line: 57)
!229 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !58, file: !214, line: 58)
!230 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !68, file: !214, line: 59)
!231 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !206, file: !214, line: 60)
!232 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !78, file: !214, line: 61)
!233 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !82, file: !214, line: 62)
!234 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !87, file: !214, line: 63)
!235 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !91, file: !214, line: 64)
!236 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !95, file: !214, line: 65)
!237 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !99, file: !214, line: 67)
!238 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !103, file: !214, line: 68)
!239 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !111, file: !214, line: 69)
!240 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !115, file: !214, line: 71)
!241 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !121, file: !214, line: 72)
!242 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !125, file: !214, line: 73)
!243 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !129, file: !214, line: 74)
!244 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !134, file: !214, line: 75)
!245 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !140, file: !214, line: 76)
!246 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !144, file: !214, line: 77)
!247 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !148, file: !214, line: 78)
!248 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !150, file: !214, line: 80)
!249 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !0, entity: !158, file: !214, line: 81)
!250 = !{i32 7, !"Dwarf Version", i32 5}
!251 = !{i32 2, !"Debug Info Version", i32 3}
!252 = !{i32 1, !"wchar_size", i32 4}
!253 = !{i32 8, !"PIC Level", i32 2}
!254 = !{i32 7, !"PIE Level", i32 2}
!255 = !{i32 7, !"uwtable", i32 2}
!256 = !{i32 7, !"frame-pointer", i32 2}
!257 = !{!"clang version 18.1.8"}
!258 = distinct !DISubprogram(name: "compute", linkageName: "_Z7computeiPdS_", scope: !1, file: !1, line: 18, type: !259, scopeLine: 18, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !262)
!259 = !DISubroutineType(types: !260)
!260 = !{null, !9, !261, !261}
!261 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !45, size: 64)
!262 = !{}
!263 = !DILocalVariable(name: "N", arg: 1, scope: !258, file: !1, line: 18, type: !9)
!264 = !DILocation(line: 18, column: 18, scope: !258)
!265 = !DILocalVariable(name: "A", arg: 2, scope: !258, file: !1, line: 18, type: !261)
!266 = !DILocation(line: 18, column: 29, scope: !258)
!267 = !DILocalVariable(name: "B", arg: 3, scope: !258, file: !1, line: 18, type: !261)
!268 = !DILocation(line: 18, column: 40, scope: !258)
!269 = !DILocation(line: 19, column: 18, scope: !258)
!270 = !DILocation(line: 19, column: 21, scope: !258)
!271 = !DILocation(line: 19, column: 24, scope: !258)
!272 = !DILocation(line: 19, column: 3, scope: !258)
!273 = !DILocation(line: 21, column: 1, scope: !258)
!274 = distinct !DISubprogram(name: "edt_function_4", linkageName: "_ZL14edt_function_4iPdS_", scope: !1, file: !1, line: 55, type: !259, scopeLine: 58, flags: DIFlagPrototyped, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition, unit: !0, retainedNodes: !262)
!275 = !DILocalVariable(name: "N", arg: 1, scope: !274, file: !1, line: 56, type: !9)
!276 = !DILocation(line: 56, column: 7, scope: !274)
!277 = !DILocalVariable(name: "A", arg: 2, scope: !274, file: !1, line: 57, type: !261)
!278 = !DILocation(line: 57, column: 12, scope: !274)
!279 = !DILocalVariable(name: "B", arg: 3, scope: !274, file: !1, line: 58, type: !261)
!280 = !DILocation(line: 58, column: 12, scope: !274)
!281 = !DILocation(line: 60, column: 20, scope: !274)
!282 = !DILocation(line: 60, column: 23, scope: !274)
!283 = !DILocation(line: 60, column: 26, scope: !274)
!284 = !DILocation(line: 60, column: 5, scope: !274)
!285 = !DILocation(line: 63, column: 1, scope: !274)
!286 = distinct !DISubprogram(name: "edt_function_3", linkageName: "_ZL14edt_function_3iPdS_", scope: !1, file: !1, line: 38, type: !259, scopeLine: 41, flags: DIFlagPrototyped, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition, unit: !0, retainedNodes: !262)
!287 = !DILocalVariable(name: "N", arg: 1, scope: !286, file: !1, line: 39, type: !9)
!288 = !DILocation(line: 39, column: 7, scope: !286)
!289 = !DILocalVariable(name: "A", arg: 2, scope: !286, file: !1, line: 40, type: !261)
!290 = !DILocation(line: 40, column: 12, scope: !286)
!291 = !DILocalVariable(name: "B", arg: 3, scope: !286, file: !1, line: 41, type: !261)
!292 = !DILocation(line: 41, column: 12, scope: !286)
!293 = !DILocalVariable(name: "i", scope: !294, file: !1, line: 43, type: !9)
!294 = distinct !DILexicalBlock(scope: !286, file: !1, line: 43, column: 7)
!295 = !DILocation(line: 43, column: 16, scope: !294)
!296 = !DILocation(line: 43, column: 12, scope: !294)
!297 = !DILocation(line: 43, column: 23, scope: !298)
!298 = distinct !DILexicalBlock(scope: !294, file: !1, line: 43, column: 7)
!299 = !DILocation(line: 43, column: 27, scope: !298)
!300 = !DILocation(line: 43, column: 25, scope: !298)
!301 = !DILocation(line: 43, column: 7, scope: !294)
!302 = !DILocation(line: 45, column: 24, scope: !303)
!303 = distinct !DILexicalBlock(scope: !298, file: !1, line: 43, column: 35)
!304 = !DILocation(line: 45, column: 27, scope: !303)
!305 = !DILocation(line: 45, column: 9, scope: !303)
!306 = !DILocation(line: 49, column: 24, scope: !303)
!307 = !DILocation(line: 49, column: 27, scope: !303)
!308 = !DILocation(line: 49, column: 30, scope: !303)
!309 = !DILocation(line: 49, column: 9, scope: !303)
!310 = !DILocation(line: 51, column: 7, scope: !303)
!311 = !DILocation(line: 43, column: 31, scope: !298)
!312 = !DILocation(line: 43, column: 7, scope: !298)
!313 = distinct !{!313, !301, !314, !315}
!314 = !DILocation(line: 51, column: 7, scope: !294)
!315 = !{!"llvm.loop.mustprogress"}
!316 = !DILocation(line: 53, column: 1, scope: !286)
!317 = distinct !DISubprogram(name: "edt_function_2", linkageName: "_ZL14edt_function_2iPd", scope: !1, file: !1, line: 31, type: !318, scopeLine: 33, flags: DIFlagPrototyped, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition, unit: !0, retainedNodes: !262)
!318 = !DISubroutineType(types: !319)
!319 = !{null, !9, !261}
!320 = !DILocalVariable(name: "i", arg: 1, scope: !317, file: !1, line: 32, type: !9)
!321 = !DILocation(line: 32, column: 7, scope: !317)
!322 = !DILocalVariable(name: "A", arg: 2, scope: !317, file: !1, line: 33, type: !261)
!323 = !DILocation(line: 33, column: 12, scope: !317)
!324 = !DILocation(line: 35, column: 18, scope: !317)
!325 = !DILocation(line: 35, column: 20, scope: !317)
!326 = !DILocation(line: 35, column: 11, scope: !317)
!327 = !DILocation(line: 35, column: 13, scope: !317)
!328 = !DILocation(line: 35, column: 16, scope: !317)
!329 = !DILocation(line: 36, column: 1, scope: !317)
!330 = distinct !DISubprogram(name: "edt_function_1", linkageName: "_ZL14edt_function_1iPdS_", scope: !1, file: !1, line: 23, type: !259, scopeLine: 26, flags: DIFlagPrototyped, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition, unit: !0, retainedNodes: !262)
!331 = !DILocalVariable(name: "i", arg: 1, scope: !330, file: !1, line: 24, type: !9)
!332 = !DILocation(line: 24, column: 7, scope: !330)
!333 = !DILocalVariable(name: "B", arg: 2, scope: !330, file: !1, line: 25, type: !261)
!334 = !DILocation(line: 25, column: 12, scope: !330)
!335 = !DILocalVariable(name: "A", arg: 3, scope: !330, file: !1, line: 26, type: !261)
!336 = !DILocation(line: 26, column: 12, scope: !330)
!337 = !DILocation(line: 28, column: 18, scope: !330)
!338 = !DILocation(line: 28, column: 20, scope: !330)
!339 = !DILocation(line: 28, column: 26, scope: !330)
!340 = !DILocation(line: 28, column: 28, scope: !330)
!341 = !DILocation(line: 28, column: 34, scope: !330)
!342 = !DILocation(line: 28, column: 36, scope: !330)
!343 = !DILocation(line: 28, column: 38, scope: !330)
!344 = !DILocation(line: 28, column: 23, scope: !330)
!345 = !DILocation(line: 28, column: 11, scope: !330)
!346 = !DILocation(line: 28, column: 13, scope: !330)
!347 = !DILocation(line: 28, column: 16, scope: !330)
!348 = !DILocation(line: 29, column: 1, scope: !330)


-------------------------------------------------
# -debug-only=arts-transform,arts,carts,arts-ir-builder,arts-utils\

