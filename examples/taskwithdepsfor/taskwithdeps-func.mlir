-----------------------------------------
ConvertARTSToFuncsPass STARTED
-----------------------------------------
[arts-codegen] Initializing ArtsCodegen
[arts-codegen] ArtsCodegen initialized
EDT GUID: %12 = "func.call"(%10, %11) <{callee = @artsReserveGuidRoute}> : (i32, i32) -> i64
[arts-codegen] EDT function pointer: %f = func.constant @__arts_edt_1 : (i32, !llvm.ptr, i32, !llvm.ptr) -> !llvm.void
[arts-codegen] EDT parameters count: %14 = "arith.constant"() <{value = 3 : i32}> : () -> i32
[arts-codegen] EDT parameters: %15 = "llvm.alloca"(%14) : (i32) -> !llvm.ptr
[arts-codegen] EDT dependencies count: %16 = "arith.constant"() <{value = 1 : i32}> : () -> i32
#map = affine_map<(d0) -> (d0)>
#map1 = affine_map<(d0) -> (d0 - 1)>
#map2 = affine_map<() -> (0)>
#map3 = affine_map<()[s0] -> (s0)>
#set = affine_set<(d0) : (d0 - 1 >= 0)>
"builtin.module"() ({
  "func.func"() <{function_type = ((i32, !llvm.ptr, i32, !llvm.ptr) -> !llvm.void, i64, i32, !llvm.ptr, i32) -> i64, sym_name = "artsEdtCreateWithGuid", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i32, i32) -> i64, sym_name = "artsReserveGuidRoute", sym_visibility = "private"}> ({
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i32, memref<?xf64>, memref<?xf64>) -> (), sym_name = "compute"}> ({
  ^bb0(%arg0: i32, %arg1: memref<?xf64>, %arg2: memref<?xf64>):
    %0 = "arith.constant"() <{value = 100 : i32}> : () -> i32
    %1 = "arith.constant"() <{value = 0.000000e+00 : f64}> : () -> f64
    %2 = "arith.index_cast"(%arg0) : (i32) -> index
    %3 = "func.call"() <{callee = @rand}> : () -> i32
    %4 = "arith.remsi"(%3, %0) : (i32, i32) -> i32
    %5 = "arith.sitofp"(%4) : (i32) -> f64
    %6 = "arts.make_dep"(%arg1) <{mode = "inout"}> : (memref<?xf64>) -> !arts.dep
    %7 = "arts.make_dep"(%arg2) <{mode = "inout"}> : (memref<?xf64>) -> !arts.dep
    "arts.parallel"(%5, %1, %2, %6, %7) <{operandSegmentSizes = array<i32: 3, 2>}> ({
      "arts.barrier"() : () -> ()
      "arts.single"() ({
        "affine.for"(%2) ({
        ^bb0(%arg3: index):
          %8 = "arith.index_cast"(%arg3) : (index) -> i32
          %9 = "arts.make_dep"(%arg1) <{affine_map = #map, mode = "out"}> : (memref<?xf64>) -> !arts.dep
          %10 = "arith.constant"() <{value = 1 : i32}> : () -> i32
          %11 = "arith.constant"() <{value = 0 : i32}> : () -> i32
          %12 = "func.call"(%10, %11) <{callee = @artsReserveGuidRoute}> : (i32, i32) -> i64
          %13 = "func.constant"() <{value = @__arts_edt_1}> : () -> ((i32, !llvm.ptr, i32, !llvm.ptr) -> !llvm.void)
          %14 = "arith.constant"() <{value = 3 : i32}> : () -> i32
          %15 = "llvm.alloca"(%14) : (i32) -> !llvm.ptr
          %16 = "arith.constant"() <{value = 1 : i32}> : () -> i32
          %17 = "func.call"(%13, %12, %14, %15, %16) <{callee = @artsEdtCreateWithGuid}> : ((i32, !llvm.ptr, i32, !llvm.ptr) -> !llvm.void, i64, i32, !llvm.ptr, i32) -> i64
          "arts.edt"(%8, %5, %arg3, %9) <{operandSegmentSizes = array<i32: 3, 1>}> ({
            %21 = "arith.sitofp"(%8) : (i32) -> f64
            %22 = "arith.addf"(%21, %5) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
            "affine.store"(%22, %arg1, %arg3) <{map = #map}> : (f64, memref<?xf64>, index) -> ()
            "arts.yield"() : () -> ()
          }) : (i32, f64, index, !arts.dep) -> ()
          %18 = "arts.make_dep"(%arg1) <{affine_map = #map, mode = "in"}> : (memref<?xf64>) -> !arts.dep
          %19 = "arts.make_dep"(%arg1) <{affine_map = #map1, mode = "in"}> : (memref<?xf64>) -> !arts.dep
          %20 = "arts.make_dep"(%arg2) <{affine_map = #map, mode = "out"}> : (memref<?xf64>) -> !arts.dep
          "arts.edt"(%arg3, %1, %18, %19, %20) <{operandSegmentSizes = array<i32: 2, 3>}> ({
            %21 = "affine.load"(%arg1, %arg3) <{map = #map}> : (memref<?xf64>, index) -> f64
            %22 = "affine.if"(%arg3) ({
              %24 = "affine.load"(%arg1, %arg3) <{map = #map1}> : (memref<?xf64>, index) -> f64
              "affine.yield"(%24) : (f64) -> ()
            }, {
              "affine.yield"(%1) : (f64) -> ()
            }) {condition = #set} : (index) -> f64
            %23 = "arith.addf"(%21, %22) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
            "affine.store"(%23, %arg2, %arg3) <{map = #map}> : (f64, memref<?xf64>, index) -> ()
            "arts.yield"() : () -> ()
          }) : (index, f64, !arts.dep, !arts.dep, !arts.dep) -> ()
          "affine.yield"() : () -> ()
        }) {lower_bound = #map2, step = 1 : index, upper_bound = #map3} : (index) -> ()
        "arts.yield"() : () -> ()
      }) : () -> ()
      "arts.barrier"() : () -> ()
      "artcarts-opt: /home/randres/projects/carts/external/Polygeist/llvm-project/mlir/lib/Transforms/Utils/DialectConversion.cpp:2009: mlir::LogicalResult (anonymous namespace)::OperationLegalizer::legalizePatternResult(mlir::Operation *, const mlir::Pattern &, mlir::ConversionPatternRewriter &, (anonymous namespace)::RewriterState &): Assertion `(replacedRoot() || updatedRootInPlace()) && "expected pattern to replace the root operation"' failed.
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: carts-opt taskwithdeps-arts.mlir --convert-arts-to-funcs -debug-only=convert-arts-to-funcs,arts-codegen
 #0 0x00005606e4496d67 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x959d67)
 #1 0x00005606e449493e llvm::sys::RunSignalHandlers() (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x95793e)
 #2 0x00005606e449741a SignalHandler(int) Signals.cpp:0:0
 #3 0x00007f7a46c27520 (/lib/x86_64-linux-gnu/libc.so.6+0x42520)
 #4 0x00007f7a46c7b9fc pthread_kill (/lib/x86_64-linux-gnu/libc.so.6+0x969fc)
 #5 0x00007f7a46c27476 gsignal (/lib/x86_64-linux-gnu/libc.so.6+0x42476)
 #6 0x00007f7a46c0d7f3 abort (/lib/x86_64-linux-gnu/libc.so.6+0x287f3)
 #7 0x00007f7a46c0d71b (/lib/x86_64-linux-gnu/libc.so.6+0x2871b)
 #8 0x00007f7a46c1ee96 (/lib/x86_64-linux-gnu/libc.so.6+0x39e96)
 #9 0x00005606e419765c mlir::LogicalResult llvm::function_ref<mlir::LogicalResult (mlir::Pattern const&)>::callback_fn<(anonymous namespace)::OperationLegalizer::legalizeWithPattern(mlir::Operation*, mlir::ConversionPatternRewriter&)::$_20>(long, mlir::Pattern const&) DialectConversion.cpp:0:0
#10 0x00005606e41b31e8 void llvm::function_ref<void ()>::callback_fn<mlir::PatternApplicator::matchAndRewrite(mlir::Operation*, mlir::PatternRewriter&, llvm::function_ref<bool (mlir::Pattern const&)>, llvm::function_ref<void (mlir::Pattern const&)>, llvm::function_ref<mlir::LogicalResult (mlir::Pattern const&)>)::$_2>(long) PatternApplicator.cpp:0:0
#11 0x00005606e41afd3f mlir::PatternApplicator::matchAndRewrite(mlir::Operation*, mlir::PatternRewriter&, llvm::function_ref<bool (mlir::Pattern const&)>, llvm::function_ref<void (mlir::Pattern const&)>, llvm::function_ref<mlir::LogicalResult (mlir::Pattern const&)>) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x672d3f)
#12 0x00005606e41950a5 (anonymous namespace)::OperationLegalizer::legalize(mlir::Operation*, mlir::ConversionPatternRewriter&) DialectConversion.cpp:0:0
#13 0x00005606e4189d07 (anonymous namespace)::OperationConverter::convertOperations(llvm::ArrayRef<mlir::Operation*>, llvm::function_ref<void (mlir::Diagnostic&)>) DialectConversion.cpp:0:0
#14 0x00005606e418a5c0 mlir::applyPartialConversion(mlir::Operation*, mlir::ConversionTarget const&, mlir::FrozenRewritePatternSet const&, llvm::DenseSet<mlir::Operation*, llvm::DenseMapInfo<mlir::Operation*, void>>*) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x64d5c0)
#15 0x00005606e4161d3f (anonymous namespace)::ConvertARTSToFuncsPass::runOnOperation() ConvertARTSToFuncs.cpp:0:0
#16 0x00005606e4328d84 mlir::detail::OpToOpPassAdaptor::run(mlir::Pass*, mlir::Operation*, mlir::AnalysisManager, bool, unsigned int) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x7ebd84)
#17 0x00005606e43293b1 mlir::detail::OpToOpPassAdaptor::runPipeline(mlir::OpPassManager&, mlir::Operation*, mlir::AnalysisManager, bool, unsigned int, mlir::PassInstrumentor*, mlir::PassInstrumentation::PipelineParentInfo const*) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x7ec3b1)
#18 0x00005606e432b862 mlir::PassManager::run(mlir::Operation*) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x7ee862)
#19 0x00005606e416e9e4 performActions(llvm::raw_ostream&, std::shared_ptr<llvm::SourceMgr> const&, mlir::MLIRContext*, mlir::MlirOptMainConfig const&) MlirOptMain.cpp:0:0
#20 0x00005606e416dc54 mlir::LogicalResult llvm::function_ref<mlir::LogicalResult (std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&)>::callback_fn<mlir::MlirOptMain(llvm::raw_ostream&, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, mlir::DialectRegistry&, mlir::MlirOptMainConfig const&)::$_2>(long, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&) MlirOptMain.cpp:0:0
#21 0x00005606e44349b8 mlir::splitAndProcessBuffer(std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::function_ref<mlir::LogicalResult (std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&)>, llvm::raw_ostream&, bool, bool) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x8f79b8)
#22 0x00005606e416823a mlir::MlirOptMain(llvm::raw_ostream&, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, mlir::DialectRegistry&, mlir::MlirOptMainConfig const&) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x62b23a)
#23 0x00005606e4168704 mlir::MlirOptMain(int, char**, llvm::StringRef, mlir::DialectRegistry&) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x62b704)
#24 0x00005606e3bb5efc main (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x78efc)
#25 0x00007f7a46c0ed90 (/lib/x86_64-linux-gnu/libc.so.6+0x29d90)
#26 0x00007f7a46c0ee40 __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x29e40)
#27 0x00005606e3bb59c5 _start (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x789c5)
