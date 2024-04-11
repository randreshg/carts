[arts-transform] Run the ARTSTransform Module Pass
[arts-transform] Analyzing main function
[arts-ir-builder] Initializing ARTSIRBuilder
[arts-ir-builder] ARTSIRBuilder initialized
[arts-ir-builder] Creating EDT fib
Created ARTS runtime function artsReserveGuidRoute with type ptr (i32, i32)
Created ARTS runtime function artsEdtCreateWithGuid with type ptr (ptr, ptr, i32, ptr, i32)
[arts-ir-builder] Creating call to artsEdtCreateWithGuid: 
declare ptr @artsEdtCreateWithGuid(ptr, ptr, i32, ptr, i32)

[arts-transform] EDT: 
define internal void @fib(i32 %0, ptr %1, i32 %2, ptr %3) {
edt.entry:
  %4 = call ptr @artsReserveGuidRoute(i32 1, i32 0)
  %guid.addr = alloca ptr, align 8
  store ptr %4, ptr %guid.addr, align 8
  br label %edt.body

edt.body:                                         ; preds = %edt.entry
  %paramc = alloca i32, align 4
  store i32 2, ptr %paramc, align 4
  %paramv.addr = alloca ptr, align 8
  %depc = alloca i32, align 4
  store i32 0, ptr %depc, align 4
  %5 = load i32, ptr %paramc, align 4
  %6 = load i32, ptr %depc, align 4
  %7 = call ptr @artsEdtCreateWithGuid(ptr @fib.1, ptr %guid.addr, i32 %5, ptr %paramv.addr, i32 %6)
}

[arts-transform] Module after ARTSTransform Module Pass:
; ModuleID = 'example.cpp'
source_filename = "example.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ident_t = type { i32, i32, i32, i32, ptr }
%struct.kmp_task_t = type { ptr, ptr, i32, %union.kmp_cmplrdata_t, %union.kmp_cmplrdata_t }
%union.kmp_cmplrdata_t = type { ptr }

$__clang_call_terminate = comdat any

@.str = private unnamed_addr constant [27 x i8] c"I think the number is %d.\0A\00", align 1
@0 = private unnamed_addr constant [25 x i8] c";example.cpp;main;17;5;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 24, ptr @0 }, align 8
@2 = private unnamed_addr constant [25 x i8] c";example.cpp;main;15;3;;\00", align 1
@3 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 24, ptr @2 }, align 8

; The first thing we are gonna do, is to identify the possible regions that can become an EDT.
; My proposal is to divide the code in regions (BB) delimited by the following instructions:
; - call to relevant RTF (parallel, task, taskwait...) and functions with return values
; - br
; - ret

; After this, we can check for dominant regions and then proceed to identify the
; respective data environment. This will be done by checking the instructions in the BBs 
; that belong to the region and identifying the variables that are used and modified in each region.

; Function Attrs: mustprogress norecurse nounwind uwtable
define dso_local noundef i32 @main() #0 !dbg !10 {
entry:
  %number = alloca i32, align 4
  call void @llvm.lifetime.start.p0(i64 4, ptr %number) #7, !dbg !13
  store i32 1, ptr %number, align 4, !dbg !14, !tbaa !15
edt:
  call void (ptr, i32, ptr, ...) @__kmpc_fork_call(ptr @3, i32 1, ptr @main.omp_outlined, ptr %number), !dbg !19
edt.done:
  call void @llvm.lifetime.end.p0(i64 4, ptr %number) #7, !dbg !20
  ret i32 0, !dbg !21
}

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #1

; Function Attrs: alwaysinline norecurse nounwind uwtable
define internal void @main.omp_outlined(ptr noalias noundef %.global_tid., ptr noalias noundef %.bound_tid., ptr noundef nonnull align 4 dereferenceable(4) %number) #2 !dbg !22 {
entry:
  %0 = load i32, ptr %.global_tid., align 4, !dbg !23, !tbaa !15
edt:
  %1 = call ptr @__kmpc_omp_task_alloc(ptr @1, i32 %0, i32 1, i64 40, i64 8, ptr @.omp_task_entry.), !dbg !23
  %2 = load ptr, ptr %1, align 8, !dbg !23, !tbaa !24
  store ptr %number, ptr %2, align 8, !dbg !23, !tbaa.struct !28
  %3 = call i32 @__kmpc_omp_task(ptr @1, i32 %0, ptr %1), !dbg !23
edt.done:
  ret void, !dbg !30
}

; Function Attrs: alwaysinline nounwind uwtable
define internal void @.omp_outlined.(i32 noundef %.global_tid., ptr noalias noundef %.part_id., ptr noalias noundef %.privates., ptr noalias noundef %.copy_fn., ptr noundef %.task_t., ptr noalias noundef %__context) #3 personality ptr @__gxx_personality_v0 !dbg !31 {
entry:
  %0 = load ptr, ptr %__context, align 8, !dbg !32, !tbaa !33
  %1 = load i32, ptr %0, align 4, !dbg !32, !tbaa !15
  %call = call i32 (ptr, ...) @printf(ptr noundef @.str, i32 noundef %1), !dbg !35
  %2 = load ptr, ptr %__context, align 8, !dbg !36, !tbaa !33
  %3 = load i32, ptr %2, align 4, !dbg !37, !tbaa !15
  %inc = add nsw i32 %3, 1, !dbg !37
  store i32 %inc, ptr %2, align 4, !dbg !37, !tbaa !15
  ret void, !dbg !38
}

; Function Attrs: nofree nounwind
declare !dbg !39 noundef i32 @printf(ptr nocapture noundef readonly, ...) #4

declare i32 @__gxx_personality_v0(...)

; Function Attrs: noinline noreturn nounwind uwtable
define linkonce_odr hidden void @__clang_call_terminate(ptr noundef %0) #5 comdat {
  %2 = call ptr @__cxa_begin_catch(ptr %0) #7
  call void @_ZSt9terminatev() #9
  unreachable
}

declare ptr @__cxa_begin_catch(ptr)

declare void @_ZSt9terminatev()

; Function Attrs: norecurse uwtable
define internal noundef i32 @.omp_task_entry.(i32 noundef %0, ptr noalias noundef %1) #6 !dbg !41 {
entry:
  %2 = getelementptr inbounds %struct.kmp_task_t, ptr %1, i32 0, i32 2, !dbg !42
  %3 = load ptr, ptr %1, align 8, !dbg !42, !tbaa !24
  call void @.omp_outlined.(i32 %0, ptr %2, ptr null, ptr null, ptr %1, ptr %3) #7, !dbg !42
  ret i32 0, !dbg !42
}

; Function Attrs: nounwind
declare noalias ptr @__kmpc_omp_task_alloc(ptr, i32, i32, i64, i64, ptr) #7

; Function Attrs: mustprogress nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg) #8

; Function Attrs: nounwind
declare i32 @__kmpc_omp_task(ptr, i32, ptr) #7

; Function Attrs: nounwind
declare !callback !43 void @__kmpc_fork_call(ptr, i32, ptr, ...) #7

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #1

define internal void @fib(i32 %0, ptr %1, i32 %2, ptr %3) {
edt.entry:
  %4 = call ptr @artsReserveGuidRoute(i32 1, i32 0)
  %guid.addr = alloca ptr, align 8
  store ptr %4, ptr %guid.addr, align 8
  br label %edt.body

edt.body:                                         ; preds = %edt.entry
  %paramc = alloca i32, align 4
  store i32 2, ptr %paramc, align 4
  %paramv.addr = alloca ptr, align 8
  %depc = alloca i32, align 4
  store i32 0, ptr %depc, align 4
  %5 = load i32, ptr %paramc, align 4
  %6 = load i32, ptr %depc, align 4
  %7 = call ptr @artsEdtCreateWithGuid(ptr @fib.1, ptr %guid.addr, i32 %5, ptr %paramv.addr, i32 %6)
}

declare ptr @artsReserveGuidRoute(i32, i32)

declare void @fib.1(i32, ptr, i32, ptr)

declare ptr @artsEdtCreateWithGuid(ptr, ptr, i32, ptr, i32)

attributes #0 = { mustprogress norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { alwaysinline norecurse nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { alwaysinline nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { noinline noreturn nounwind uwtable "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #6 = { norecurse uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #7 = { nounwind }
attributes #8 = { mustprogress nocallback nofree nounwind willreturn memory(argmem: readwrite) }
attributes #9 = { noreturn nounwind }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3, !4, !5, !6, !7, !8}
!llvm.ident = !{!9}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang version 18.0.0", isOptimized: true, runtimeVersion: 0, emissionKind: LineTablesOnly, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "example.cpp", directory: "/home/rherreraguaitero/ME/ARTS-env/test/example", checksumkind: CSK_MD5, checksum: "4244ab9131d1fc62e80112bae6ec5ef5")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !{i32 1, !"wchar_size", i32 4}
!5 = !{i32 7, !"openmp", i32 51}
!6 = !{i32 8, !"PIC Level", i32 2}
!7 = !{i32 7, !"PIE Level", i32 2}
!8 = !{i32 7, !"uwtable", i32 2}
!9 = !{!"clang version 18.0.0"}
!10 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 13, type: !11, scopeLine: 13, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!11 = !DISubroutineType(types: !12)
!12 = !{}
!13 = !DILocation(line: 14, column: 3, scope: !10)
!14 = !DILocation(line: 14, column: 7, scope: !10)
!15 = !{!16, !16, i64 0}
!16 = !{!"int", !17, i64 0}
!17 = !{!"omnipotent char", !18, i64 0}
!18 = !{!"Simple C++ TBAA"}
!19 = !DILocation(line: 15, column: 3, scope: !10)
!20 = !DILocation(line: 24, column: 1, scope: !10)
!21 = !DILocation(line: 23, column: 3, scope: !10)
!22 = distinct !DISubprogram(name: "main.omp_outlined", scope: !1, file: !1, line: 15, type: !11, scopeLine: 15, flags: DIFlagArtificial | DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!23 = !DILocation(line: 17, column: 5, scope: !22)
!24 = !{!25, !27, i64 0}
!25 = !{!"_ZTS24kmp_task_t_with_privates", !26, i64 0}
!26 = !{!"_ZTS10kmp_task_t", !27, i64 0, !27, i64 8, !16, i64 16, !17, i64 24, !17, i64 32}
!27 = !{!"any pointer", !17, i64 0}
!28 = !{i64 0, i64 8, !29}
!29 = !{!27, !27, i64 0}
!30 = !DILocation(line: 22, column: 3, scope: !22)
!31 = distinct !DISubprogram(name: ".omp_outlined.", scope: !1, file: !1, type: !11, scopeLine: 18, flags: DIFlagArtificial | DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!32 = !DILocation(line: 19, column: 47, scope: !31)
!33 = !{!34, !27, i64 0}
!34 = !{!"_ZTSZ4mainE3$_0", !27, i64 0}
!35 = !DILocation(line: 19, column: 9, scope: !31)
!36 = !DILocation(line: 20, column: 9, scope: !31)
!37 = !DILocation(line: 20, column: 15, scope: !31)
!38 = !DILocation(line: 21, column: 5, scope: !31)
!39 = !DISubprogram(name: "printf", scope: !40, file: !40, line: 332, type: !11, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!40 = !DIFile(filename: "/usr/include/stdio.h", directory: "", checksumkind: CSK_MD5, checksum: "5b917eded35ce2507d1e294bf8cb74d7")
!41 = distinct !DISubprogram(linkageName: ".omp_task_entry.", scope: !1, file: !1, line: 17, type: !11, scopeLine: 17, flags: DIFlagArtificial | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!42 = !DILocation(line: 17, column: 5, scope: !41)
!43 = !{!44}
!44 = !{i64 2, i64 -1, i64 -1, i1 true}
clang-18: /home/rherreraguaitero/ME/ARTS-env/llvm-project/llvm/include/llvm/Support/Casting.h:662: decltype(auto) llvm::dyn_cast(From *) [To = llvm::BranchInst, From = llvm::Instruction]: Assertion `detail::isPresent(Val) && "dyn_cast on a non-existent value"' failed.
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace, preprocessed source, and associated run script.
Stack dump:
0.	Program arguments: /home/rherreraguaitero/ME/soft/llvm_arts/bin/clang-18 -cc1 -triple x86_64-unknown-linux-gnu -emit-obj -dumpdir example- -disable-free -clear-ast-before-backend -main-file-name example.cpp -mrelocation-model pic -pic-level 2 -pic-is-pie -mframe-pointer=none -fmath-errno -ffp-contract=on -fno-rounding-math -mconstructor-aliases -funwind-tables=2 -target-cpu x86-64 -tune-cpu generic -debug-info-kind=line-tables-only -dwarf-version=5 -debugger-tuning=gdb -fdebug-compilation-dir=/home/rherreraguaitero/ME/ARTS-env/test/example -fcoverage-compilation-dir=/home/rherreraguaitero/ME/ARTS-env/test/example -resource-dir /home/rherreraguaitero/ME/soft/llvm_arts/lib/clang/18 -I/home/videau/opt/libffi/3.2.1/lib/libffi-3.2.1/include -I/usr/include/python3.6m -c-isystem /soft/packaging/spack-builds/linux-opensuse_leap15-x86_64/gcc-10.2.0/gdb-9.2-usixmvzagpcqbitrcayzcqi4agxqiftx/include -c-isystem /soft/packaging/spack-builds/linux-opensuse_leap15-x86_64/gcc-10.2.0/source-highlight-3.1.9-q7luky6z5wjx4ebstr2o5xab2t77ta2x/include -c-isystem /soft/packaging/spack-builds/linux-opensuse_leap15-x86_64/gcc-10.2.0/boost-1.74.0-lwc57tnqmgyyj2j3fao67gaobcswdoda/include -cxx-isystem /soft/packaging/spack-builds/linux-opensuse_leap15-x86_64/gcc-10.2.0/gdb-9.2-usixmvzagpcqbitrcayzcqi4agxqiftx/include -cxx-isystem /soft/packaging/spack-builds/linux-opensuse_leap15-x86_64/gcc-10.2.0/source-highlight-3.1.9-q7luky6z5wjx4ebstr2o5xab2t77ta2x/include -cxx-isystem /soft/packaging/spack-builds/linux-opensuse_leap15-x86_64/gcc-10.2.0/boost-1.74.0-lwc57tnqmgyyj2j3fao67gaobcswdoda/include -internal-isystem /usr/lib64/gcc/x86_64-suse-linux/7/../../../../include/c++/7 -internal-isystem /usr/lib64/gcc/x86_64-suse-linux/7/../../../../include/c++/7/x86_64-suse-linux -internal-isystem /usr/lib64/gcc/x86_64-suse-linux/7/../../../../include/c++/7/backward -internal-isystem /home/rherreraguaitero/ME/soft/llvm_arts/lib/clang/18/include -internal-isystem /usr/local/include -internal-isystem /usr/lib64/gcc/x86_64-suse-linux/7/../../../../x86_64-suse-linux/include -internal-externc-isystem /include -internal-externc-isystem /usr/include -O3 -Rpass=arts-transform -fdeprecated-macro -ferror-limit 19 -fopenmp -fgnuc-version=4.2.1 -fcxx-exceptions -fexceptions -vectorize-loops -vectorize-slp -mllvm -debug-only=arts-transform,arts-ir-builder -faddrsig -D__GCC_HAVE_DWARF2_CFI_ASM=1 -o /tmp/example-4fb0b1.o -x c++ example.cpp
1.	<eof> parser at end of file
2.	Optimizer
 #0 0x00007feae19855e8 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/rherreraguaitero/ME/soft/llvm_arts/lib/libLLVMSupport.so.18git+0x18c5e8)
 #1 0x00007feae19830be llvm::sys::RunSignalHandlers() (/home/rherreraguaitero/ME/soft/llvm_arts/lib/libLLVMSupport.so.18git+0x18a0be)
 #2 0x00007feae1985c9d SignalHandler(int) Signals.cpp:0:0
 #3 0x00007feae69e58c0 __restore_rt (/lib64/libpthread.so.0+0x168c0)
 #4 0x00007feae0ebfc6b raise (/lib64/libc.so.6+0x4ac6b)
 #5 0x00007feae0ec1305 abort (/lib64/libc.so.6+0x4c305)
 #6 0x00007feae0eb7c6a __assert_fail_base (/lib64/libc.so.6+0x42c6a)
 #7 0x00007feae0eb7cf2 (/lib64/libc.so.6+0x42cf2)
 #8 0x00007feae2fb082d llvm::PredicateInfoBuilder::buildPredicateInfo() (/home/rherreraguaitero/ME/soft/llvm_arts/lib/libLLVMTransformUtils.so.18git+0x18382d)
 #9 0x00007feae2fb2840 llvm::PredicateInfo::PredicateInfo(llvm::Function&, llvm::DominatorTree&, llvm::AssumptionCache&) (/home/rherreraguaitero/ME/soft/llvm_arts/lib/libLLVMTransformUtils.so.18git+0x185840)
#10 0x00007feae2fea397 llvm::SCCPInstVisitor::addPredicateInfo(llvm::Function&, llvm::DominatorTree&, llvm::AssumptionCache&) (/home/rherreraguaitero/ME/soft/llvm_arts/lib/libLLVMTransformUtils.so.18git+0x1bd397)
#11 0x00007feae469a059 llvm::IPSCCPPass::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/rherreraguaitero/ME/soft/llvm_arts/lib/libLLVMipo.so.18git+0x2e4059)
#12 0x00007feae065275d llvm::detail::PassModel<llvm::Module, llvm::IPSCCPPass, llvm::PreservedAnalyses, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/rherreraguaitero/ME/soft/llvm_arts/lib/libLLVMPasses.so.18git+0xcf75d)
#13 0x00007feae23d8fb4 llvm::PassManager<llvm::Module, llvm::AnalysisManager<llvm::Module>>::run(llvm::Module&, llvm::AnalysisManager<llvm::Module>&) (/home/rherreraguaitero/ME/soft/llvm_arts/lib/libLLVMCore.so.18git+0x2b0fb4)
#14 0x00007feae525fd22 (anonymous namespace)::EmitAssemblyHelper::RunOptimizationPipeline(clang::BackendAction, std::unique_ptr<llvm::raw_pwrite_stream, std::default_delete<llvm::raw_pwrite_stream>>&, std::unique_ptr<llvm::ToolOutputFile, std::default_delete<llvm::ToolOutputFile>>&) BackendUtil.cpp:0:0
#15 0x00007feae52556f0 clang::EmitBackendOutput(clang::DiagnosticsEngine&, clang::HeaderSearchOptions const&, clang::CodeGenOptions const&, clang::TargetOptions const&, clang::LangOptions const&, llvm::StringRef, llvm::Module*, clang::BackendAction, llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem>, std::unique_ptr<llvm::raw_pwrite_stream, std::default_delete<llvm::raw_pwrite_stream>>) (/home/rherreraguaitero/ME/soft/llvm_arts/lib/libclangCodeGen.so.18git+0xd66f0)
#16 0x00007feae5696e61 clang::BackendConsumer::HandleTranslationUnit(clang::ASTContext&) (/home/rherreraguaitero/ME/soft/llvm_arts/lib/libclangCodeGen.so.18git+0x517e61)
#17 0x00007feade860426 clang::ParseAST(clang::Sema&, bool, bool) (/home/rherreraguaitero/ME/soft/llvm_arts/lib/libclangParse.so.18git+0x35426)
#18 0x00007feae3ea2e6f clang::FrontendAction::Execute() (/home/rherreraguaitero/ME/soft/llvm_arts/lib/libclangFrontend.so.18git+0x147e6f)
#19 0x00007feae3e11e2d clang::CompilerInstance::ExecuteAction(clang::FrontendAction&) (/home/rherreraguaitero/ME/soft/llvm_arts/lib/libclangFrontend.so.18git+0xb6e2d)
#20 0x00007feae69caba0 clang::ExecuteCompilerInvocation(clang::CompilerInstance*) (/home/rherreraguaitero/ME/soft/llvm_arts/lib/libclangFrontendTool.so.18git+0x3ba0)
#21 0x000055d3dd7a7ee0 cc1_main(llvm::ArrayRef<char const*>, char const*, void*) (/home/rherreraguaitero/ME/soft/llvm_arts/bin/clang-18+0x13ee0)
#22 0x000055d3dd7a505e ExecuteCC1Tool(llvm::SmallVectorImpl<char const*>&, llvm::ToolContext const&) driver.cpp:0:0
#23 0x000055d3dd7a3d83 clang_main(int, char**, llvm::ToolContext const&) (/home/rherreraguaitero/ME/soft/llvm_arts/bin/clang-18+0xfd83)
#24 0x000055d3dd7b3f01 main (/home/rherreraguaitero/ME/soft/llvm_arts/bin/clang-18+0x1ff01)
#25 0x00007feae0eaa24d __libc_start_main (/lib64/libc.so.6+0x3524d)
#26 0x000055d3dd7a11aa _start /home/abuild/rpmbuild/BUILD/glibc-2.31/csu/../sysdeps/x86_64/start.S:122:0
clang++: error: unable to execute command: Aborted
clang++: error: clang frontend command failed due to signal (use -v to see invocation)
clang version 18.0.0
Target: x86_64-unknown-linux-gnu
Thread model: posix
InstalledDir: /home/rherreraguaitero/ME/soft/llvm_arts/bin
clang++: note: diagnostic msg: 
********************

PLEASE ATTACH THE FOLLOWING FILES TO THE BUG REPORT:
Preprocessed source(s) and associated run script(s) are located at:
clang++: note: diagnostic msg: /tmp/example-3f22e2.cpp
clang++: note: diagnostic msg: /tmp/example-3f22e2.sh
clang++: note: diagnostic msg: 

********************
