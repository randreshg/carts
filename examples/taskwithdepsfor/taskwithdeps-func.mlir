-----------------------------------------
ConvertARTSToFuncsPass STARTED
-----------------------------------------
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  func.func @compute(%arg0: i32, %arg1: memref<?xf64>, %arg2: memref<?xf64>) attributes {llvm.linkage = #llvm.linkage<external>} {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c-1_i32 = arith.constant -1 : i32
    %c8 = arith.constant 8 : index
    %c8_i64 = arith.constant 8 : i64
    %cst = arith.constant 0.000000e+00 : f64
    %c0_i32 = arith.constant 0 : i32
    %c100_i32 = arith.constant 100 : i32
    %0 = arith.extsi %arg0 : i32 to i64
    %1 = arith.muli %0, %c8_i64 : i64
    %2 = arith.index_cast %1 : i64 to index
    %3 = arith.divui %2, %c8 : index
    %4 = arts.alloc(%3) : memref<?xf64>
    %5 = arts.alloc(%3) : memref<?xf64>
    %6 = call @rand() : () -> i32
    %7 = arith.remsi %6, %c100_i32 : i32
    %8 = arts.make_dep "inout", %4 : memref<?xf64> : !arts.dep
    %9 = arts.make_dep "inout", %5 : memref<?xf64> : !arts.dep
    arts.parallel parameters(%arg0, %7, %c-1_i32, %c0_i32, %cst, %c0, %c1) : (i32, i32, i32, i32, f64, index, index), dependencies(%8, %9) : (!arts.dep, !arts.dep) {
      arts.barrier
      arts.single {
        %10 = arith.index_cast %arg0 : i32 to index
        scf.for %arg3 = %c0 to %10 step %c1 {
          %11 = arith.index_cast %arg3 : index to i32
          %12 = memref.load %4[%arg3] : memref<?xf64>
          %13 = arts.make_dep "out", %12 : f64 : !arts.dep
          arts.edt parameters(%11, %7, %arg3) : (i32, i32, index), dependencies(%13) : (!arts.dep) {
            %22 = arith.sitofp %11 : i32 to f64
            %23 = arith.sitofp %7 : i32 to f64
            %24 = arith.addf %22, %23 : f64
            memref.store %24, %4[%arg3] : memref<?xf64>
          }
          %14 = memref.load %4[%arg3] : memref<?xf64>
          %15 = arith.addi %11, %c-1_i32 : i32
          %16 = arith.index_cast %15 : i32 to index
          %17 = memref.load %4[%16] : memref<?xf64>
          %18 = memref.load %5[%arg3] : memref<?xf64>
          %19 = arts.make_dep "in", %14 : f64 : !arts.dep
          %20 = arts.make_dep "in", %17 : f64 : !arts.dep
          %21 = arts.make_dep "out", %18 : f64 : !arts.dep
          arts.edt parameters(%11, %c0_i32, %cst, %arg3) : (i32, i32, f64, index), dependencies(%19, %20, %21) : (!arts.dep, !arts.dep, !arts.dep) {
            %22 = arith.cmpi sgt, %11, %c0_i32 : i32
            %23 = arith.select %22, %17, %cst : f64
            %24 = arith.addf %14, %23 : f64
            memref.store %24, %5[%arg3] : memref<?xf64>
          }
        }
      }
      arts.barrier
    }
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}
[arts-codegen] Initializing ArtsCodegen
[arts-codegen] ArtsCodegen initialized
[convert-arts-to-funcs] Starting ArtsAnalysis
[convert-arts-to-funcs] Processing MakeDepOp %8 = arts.make_dep "inout", %4 : memref<?xf64> : !arts.dep
[convert-arts-to-funcs] Processing MakeDepOp %13 = arts.make_dep "inout", %5 : memref<?xf64> : !arts.dep
[convert-arts-to-funcs] Processing MakeDepOp %21 = arts.make_dep "out", %20 : f64 : !arts.dep
[convert-arts-to-funcs] Processing MakeDepOp %31 = arts.make_dep "in", %26 : f64 : !arts.dep
[convert-arts-to-funcs] Processing MakeDepOp %36 = arts.make_dep "in", %29 : f64 : !arts.dep
[convert-arts-to-funcs] Processing MakeDepOp %41 = arts.make_dep "out", %30 : f64 : !arts.dep
[convert-arts-to-funcs] Processing parallel op
[convert-arts-to-funcs] Parallel EDT created
[arts-codegen] Old param: <block argument> of type 'i32' at index: 0
[arts-codegen] Old param: %15 = "arith.remsi"(%14, %7) : (i32, i32) -> i32
[arts-codegen] Old param: %2 = "arith.constant"() <{value = -1 : i32}> : () -> i32
[arts-codegen] Old param: %6 = "arith.constant"() <{value = 0 : i32}> : () -> i32
[arts-codegen] Old param: %5 = "arith.constant"() <{value = 0.000000e+00 : f64}> : () -> f64
carts-opt: /home/randres/projects/carts/lib/arts/Codegen/ArtsCodegen.cpp:454: mlir::Value mlir::arts::ArtsCodegen::castToInt(mlir::Type, mlir::Value, mlir::Location): Assertion `targetType.isa<IntegerType>() && "Target type should be an integer"' failed.
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: carts-opt taskwithdeps-arts.mlir --convert-arts-to-funcs -debug-only=convert-arts-to-funcs,arts-codegen
 #0 0x000055bd8f2548e7 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x98a8e7)
 #1 0x000055bd8f2524be llvm::sys::RunSignalHandlers() (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x9884be)
 #2 0x000055bd8f254f9a SignalHandler(int) Signals.cpp:0:0
 #3 0x00007fe81bf86520 (/lib/x86_64-linux-gnu/libc.so.6+0x42520)
 #4 0x00007fe81bfda9fc pthread_kill (/lib/x86_64-linux-gnu/libc.so.6+0x969fc)
 #5 0x00007fe81bf86476 gsignal (/lib/x86_64-linux-gnu/libc.so.6+0x42476)
 #6 0x00007fe81bf6c7f3 abort (/lib/x86_64-linux-gnu/libc.so.6+0x287f3)
 #7 0x00007fe81bf6c71b (/lib/x86_64-linux-gnu/libc.so.6+0x2871b)
 #8 0x00007fe81bf7de96 (/lib/x86_64-linux-gnu/libc.so.6+0x39e96)
 #9 0x000055bd8eedc421 mlir::arts::ArtsCodegen::castToInt(mlir::Type, mlir::Value, mlir::Location) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x612421)
#10 0x000055bd8eedd2f6 mlir::arts::ArtsCodegen::outlineRegion(mlir::func::FuncOp, mlir::Region&, llvm::SmallVector<mlir::Value, 6u>&, llvm::SmallVector<mlir::Value, 6u>&) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x6132f6)
#11 0x000055bd8eed672e ArtsAnalysis::processParallelOp(mlir::arts::ParallelOp) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x60c72e)
#12 0x000055bd8eed5bee void mlir::detail::walk<mlir::ForwardIterator>(mlir::Operation*, llvm::function_ref<void (mlir::Operation*)>, mlir::WalkOrder) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x60bbee)
#13 0x000055bd8eed5bee void mlir::detail::walk<mlir::ForwardIterator>(mlir::Operation*, llvm::function_ref<void (mlir::Operation*)>, mlir::WalkOrder) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x60bbee)
#14 0x000055bd8eed605b ArtsAnalysis::start() (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x60c05b)
#15 0x000055bd8eed7487 (anonymous namespace)::ConvertARTSToFuncsPass::runOnOperation() ConvertARTSToFuncs.cpp:0:0
#16 0x000055bd8f0fb254 mlir::detail::OpToOpPassAdaptor::run(mlir::Pass*, mlir::Operation*, mlir::AnalysisManager, bool, unsigned int) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x831254)
#17 0x000055bd8f0fb881 mlir::detail::OpToOpPassAdaptor::runPipeline(mlir::OpPassManager&, mlir::Operation*, mlir::AnalysisManager, bool, unsigned int, mlir::PassInstrumentor*, mlir::PassInstrumentation::PipelineParentInfo const*) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x831881)
#18 0x000055bd8f0fdd32 mlir::PassManager::run(mlir::Operation*) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x833d32)
#19 0x000055bd8eee6ae4 performActions(llvm::raw_ostream&, std::shared_ptr<llvm::SourceMgr> const&, mlir::MLIRContext*, mlir::MlirOptMainConfig const&) MlirOptMain.cpp:0:0
#20 0x000055bd8eee5d54 mlir::LogicalResult llvm::function_ref<mlir::LogicalResult (std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&)>::callback_fn<mlir::MlirOptMain(llvm::raw_ostream&, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, mlir::DialectRegistry&, mlir::MlirOptMainConfig const&)::$_2>(long, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&) MlirOptMain.cpp:0:0
#21 0x000055bd8f1f2368 mlir::splitAndProcessBuffer(std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::function_ref<mlir::LogicalResult (std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&)>, llvm::raw_ostream&, bool, bool) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x928368)
#22 0x000055bd8eee033a mlir::MlirOptMain(llvm::raw_ostream&, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, mlir::DialectRegistry&, mlir::MlirOptMainConfig const&) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x61633a)
#23 0x000055bd8eee0804 mlir::MlirOptMain(int, char**, llvm::StringRef, mlir::DialectRegistry&) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x616804)
#24 0x000055bd8e945254 main (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x7b254)
#25 0x00007fe81bf6dd90 (/lib/x86_64-linux-gnu/libc.so.6+0x29d90)
#26 0x00007fe81bf6de40 __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x29e40)
#27 0x000055bd8e944a95 _start (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x7aa95)
