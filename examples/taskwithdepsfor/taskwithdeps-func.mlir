-----------------------------------------
ConvertARTSToFuncsPass STARTED
-----------------------------------------
[arts-codegen] Initializing ArtsCodegen
[arts-codegen] ArtsCodegen initialized
EDT GUID: %6 = "func.call"(%4, %5) <{callee = @artsReserveGuidRoute}> : (i32, i32) -> i64
  - Parameter: %2 = "arith.index_cast"(%arg3) : (index) -> i32
  - Parameter: <block argument> of type 'index' at index: 0
[arts-codegen] EDT function pointer: %f = func.constant @__arts_edt_1 : (i32, !llvm.ptr, i32, !llvm.ptr) -> !llvm.void
[arts-codegen] EDT parameters count: %8 = "arith.constant"() <{value = 2 : i32}> : () -> i32
[arts-codegen] EDT parameters: %9 = "llvm.alloca"(%8) : (i32) -> !llvm.ptr
[arts-codegen] EDT dependencies count: %10 = "arith.constant"() <{value = 1 : i32}> : () -> i32
EDT call: %11 = "func.call"(%7, %6, %8, %9, %10) <{callee = @artsEdtCreateWithGuid}> : ((i32, !llvm.ptr, i32, !llvm.ptr) -> !llvm.void, i64, i32, !llvm.ptr, i32) -> i64
#map = affine_map<(d0) -> (d0)>
#map1 = affine_map<(d0) -> (d0 - 1)>
#map2 = affine_map<() -> (0)>
#map3 = affine_map<()[s0] -> (s0)>
#set = affine_set<(d0) : (d0 - 1 >= 0)>
"builtin.module"() ({
  "func.func"() <{function_type = ((i32, !llvm.ptr, i32, !llvm.ptr) -> !llvm.void, i64, i32, !llvm.ptr, i32) -> i64, sym_name = "artsEdtCreateWithGuid"}> ({
  }) : () -> ()
  "func.func"() <{function_type = (i32, i32) -> i64, sym_name = "artsReserveGuidRoute"}> ({
  }) : () -> ()
  "func.func"() <{function_type = (i32, memref<?xf64>, memref<?xf64>) -> (), sym_name = "compute"}> ({
  ^bb0(%arg0: i32, %arg1: memref<?xf64>, %arg2: memref<?xf64>):
    %0 = "arith.constant"() <{value = 0.000000e+00 : f64}> : () -> f64
    %1 = "arith.index_cast"(%arg0) : (i32) -> index
    "arts.parallel"() ({
      "arts.barrier"() : () -> ()
      "arts.single"() ({
        "affine.for"(%1) ({
        ^bb0(%arg3: index):
          %2 = "arith.index_cast"(%arg3) : (index) -> i32
          %3 = "arts.make_dep"(%arg1) <{affine_map = #map, mode = "out"}> : (memref<?xf64>) -> !arts.dep
          %4 = "arith.constant"() <{value = 1 : i32}> : () -> i32
          %5 = "arith.constant"() <{value = 0 : i32}> : () -> i32
          %6 = "func.call"(%4, %5) <{callee = @artsReserveGuidRoute}> : (i32, i32) -> i64
          %7 = "func.constant"() <{value = @__arts_edt_1}> : () -> ((i32, !llvm.ptr, i32, !llvm.ptr) -> !llvm.void)
          %8 = "arith.constant"() <{value = 2 : i32}> : () -> i32
          %9 = "llvm.alloca"(%8) : (i32) -> !llvm.ptr
          %10 = "arith.constant"() <{value = 1 : i32}> : () -> i32
          %11 = "func.call"(%7, %6, %8, %9, %10) <{callee = @artsEdtCreateWithGuid}> : ((i32, !llvm.ptr, i32, !llvm.ptr) -> !llvm.void, i64, i32, !llvm.ptr, i32) -> i64
          "arts.edt"(%2, %arg3, %3) <{operandSegmentSizes = array<i32: 2, 1>}> ({
            %15 = "arith.sitofp"(%2) : (i32) -> f64
            "affine.store"(%15, %arg1, %arg3) <{map = #map}> : (f64, memref<?xf64>, index) -> ()
            "arts.yield"() : () -> ()
          }) : (i32, index, !arts.dep) -> ()
          %12 = "arts.make_dep"(%arg1) <{affine_map = #map, mode = "in"}> : (memref<?xf64>) -> !arts.dep
          %13 = "arts.make_dep"(%arg1) <{affine_map = #map1, mode = "in"}> : (memref<?xf64>) -> !arts.dep
          %14 = "arts.make_dep"(%arg2) <{affine_map = #map, mode = "out"}> : (memref<?xf64>) -> !arts.dep
          "arts.edt"(%arg3, %0, %12, %13, %14) <{operandSegmentSizes = array<i32: 2, 3>}> ({
            %15 = "affine.load"(%arg1, %arg3) <{map = #map}> : (memref<?xf64>, index) -> f64
            %16 = "affine.if"(%arg3) ({
              %18 = "affine.load"(%arg1, %arg3) <{map = #map1}> : (memref<?xf64>, index) -> f64
              "affine.yield"(%18) : (f64) -> ()
            }, {
              "affine.yield"(%0) : (f64) -> ()
            }) {condition = #set} : (index) -> f64
            %17 = "arith.addf"(%15, %16) <{fastmath = #arith.fastmath<none>}> : (f64, f64) -> f64
            "affine.store"(%17, %arg2, %arg3) <{map = #map}> : (f64, memref<?xf64>, index) -> ()
            "arts.yield"() : () -> ()
          }) : (index, f64, !arts.dep, !arts.dep, !arts.dep) -> ()
          "affine.yield"() : () -> ()
        }) {lower_bound = #map2, step = 1 : index, upper_bound = #map3} : (index) -> ()
        "arts.yield"() : () -> ()
      }) : () -> ()
      "arts.barrier"() : () -> ()
      "arts.yield"() : () -> ()
    }) : () -> ()
    "func.return"() : () -> ()
  }) {llvm.linkage = #llvm.linkage<external>} : () -> ()
  "func.func"() <{function_type = (i32, !llvm.ptr, i32, !llvm.ptr) -> !llvm.void, sym_name = "__arts_edt_1"}> ({
  }) : () -> ()
}) {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> carts-opt: /home/randres/projects/carts/external/Polygeist/llvm-project/mlir/lib/Transforms/Utils/DialectConversion.cpp:2009: mlir::LogicalResult (anonymous namespace)::OperationLegalizer::legalizePatternResult(mlir::Operation *, const mlir::Pattern &, mlir::ConversionPatternRewriter &, (anonymous namespace)::RewriterState &): Assertion `(replacedRoot() || updatedRootInPlace()) && "expected pattern to replace the root operation"' failed.
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: carts-opt taskwithdeps-arts.mlir --convert-arts-to-funcs -debug-only=convert-arts-to-funcs,arts-codegen
 #0 0x00005584ec386307 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x957307)
 #1 0x00005584ec383ede llvm::sys::RunSignalHandlers() (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x954ede)
 #2 0x00005584ec3869ba SignalHandler(int) Signals.cpp:0:0
 #3 0x00007f464274e520 (/lib/x86_64-linux-gnu/libc.so.6+0x42520)
 #4 0x00007f46427a29fc pthread_kill (/lib/x86_64-linux-gnu/libc.so.6+0x969fc)
 #5 0x00007f464274e476 gsignal (/lib/x86_64-linux-gnu/libc.so.6+0x42476)
 #6 0x00007f46427347f3 abort (/lib/x86_64-linux-gnu/libc.so.6+0x287f3)
 #7 0x00007f464273471b (/lib/x86_64-linux-gnu/libc.so.6+0x2871b)
 #8 0x00007f4642745e96 (/lib/x86_64-linux-gnu/libc.so.6+0x39e96)
 #9 0x00005584ec086f1c mlir::LogicalResult llvm::function_ref<mlir::LogicalResult (mlir::Pattern const&)>::callback_fn<(anonymous namespace)::OperationLegalizer::legalizeWithPattern(mlir::Operation*, mlir::ConversionPatternRewriter&)::$_20>(long, mlir::Pattern const&) DialectConversion.cpp:0:0
#10 0x00005584ec0a2628 void llvm::function_ref<void ()>::callback_fn<mlir::PatternApplicator::matchAndRewrite(mlir::Operation*, mlir::PatternRewriter&, llvm::function_ref<bool (mlir::Pattern const&)>, llvm::function_ref<void (mlir::Pattern const&)>, llvm::function_ref<mlir::LogicalResult (mlir::Pattern const&)>)::$_2>(long) PatternApplicator.cpp:0:0
#11 0x00005584ec09f17f mlir::PatternApplicator::matchAndRewrite(mlir::Operation*, mlir::PatternRewriter&, llvm::function_ref<bool (mlir::Pattern const&)>, llvm::function_ref<void (mlir::Pattern const&)>, llvm::function_ref<mlir::LogicalResult (mlir::Pattern const&)>) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x67017f)
#12 0x00005584ec084965 (anonymous namespace)::OperationLegalizer::legalize(mlir::Operation*, mlir::ConversionPatternRewriter&) DialectConversion.cpp:0:0
#13 0x00005584ec0795c7 (anonymous namespace)::OperationConverter::convertOperations(llvm::ArrayRef<mlir::Operation*>, llvm::function_ref<void (mlir::Diagnostic&)>) DialectConversion.cpp:0:0
#14 0x00005584ec079e80 mlir::applyPartialConversion(mlir::Operation*, mlir::ConversionTarget const&, mlir::FrozenRewritePatternSet const&, llvm::DenseSet<mlir::Operation*, llvm::DenseMapInfo<mlir::Operation*, void>>*) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x64ae80)
#15 0x00005584ec0513ef (anonymous namespace)::ConvertARTSToFuncsPass::runOnOperation() ConvertARTSToFuncs.cpp:0:0
#16 0x00005584ec218364 mlir::detail::OpToOpPassAdaptor::run(mlir::Pass*, mlir::Operation*, mlir::AnalysisManager, bool, unsigned int) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x7e9364)
#17 0x00005584ec218991 mlir::detail::OpToOpPassAdaptor::runPipeline(mlir::OpPassManager&, mlir::Operation*, mlir::AnalysisManager, bool, unsigned int, mlir::PassInstrumentor*, mlir::PassInstrumentation::PipelineParentInfo const*) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x7e9991)
#18 0x00005584ec21ae42 mlir::PassManager::run(mlir::Operation*) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x7ebe42)
#19 0x00005584ec05e1f4 performActions(llvm::raw_ostream&, std::shared_ptr<llvm::SourceMgr> const&, mlir::MLIRContext*, mlir::MlirOptMainConfig const&) MlirOptMain.cpp:0:0
#20 0x00005584ec05d464 mlir::LogicalResult llvm::function_ref<mlir::LogicalResult (std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&)>::callback_fn<mlir::MlirOptMain(llvm::raw_ostream&, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, mlir::DialectRegistry&, mlir::MlirOptMainConfig const&)::$_2>(long, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&) MlirOptMain.cpp:0:0
#21 0x00005584ec323f58 mlir::splitAndProcessBuffer(std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::function_ref<mlir::LogicalResult (std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&)>, llvm::raw_ostream&, bool, bool) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x8f4f58)
#22 0x00005584ec057a4a mlir::MlirOptMain(llvm::raw_ostream&, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, mlir::DialectRegistry&, mlir::MlirOptMainConfig const&) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x628a4a)
#23 0x00005584ec057f14 mlir::MlirOptMain(int, char**, llvm::StringRef, mlir::DialectRegistry&) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x628f14)
#24 0x00005584ebaa7efc main (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x78efc)
#25 0x00007f4642735d90 (/lib/x86_64-linux-gnu/libc.so.6+0x29d90)
#26 0x00007f4642735e40 __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x29e40)
#27 0x00005584ebaa79c5 _start (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x789c5)
