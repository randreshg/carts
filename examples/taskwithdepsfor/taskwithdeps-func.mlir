--- ConvertArtsToFuncsPass START ---
Preprocessing datablocks
module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<i64, dense<64> : vector<2xi32>>, #dlti.dl_entry<f80, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<270>, dense<32> : vector<4xi32>>, #dlti.dl_entry<f128, dense<128> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr<272>, dense<64> : vector<4xi32>>, #dlti.dl_entry<!llvm.ptr<271>, dense<32> : vector<4xi32>>, #dlti.dl_entry<i32, dense<32> : vector<2xi32>>, #dlti.dl_entry<f16, dense<16> : vector<2xi32>>, #dlti.dl_entry<f64, dense<64> : vector<2xi32>>, #dlti.dl_entry<i1, dense<8> : vector<2xi32>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi32>>, #dlti.dl_entry<i16, dense<16> : vector<2xi32>>, #dlti.dl_entry<i8, dense<8> : vector<2xi32>>, #dlti.dl_entry<"dlti.stack_alignment", 128 : i32>, #dlti.dl_entry<"dlti.endianness", "little">>, llvm.data_layout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128", llvm.target_triple = "x86_64-unknown-linux-gnu", "polygeist.target-cpu" = "x86-64", "polygeist.target-features" = "+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87", "polygeist.tune-cpu" = "generic"} {
  llvm.mlir.global internal constant @str0("A[0] = %f\0A\00") {addr_space = 0 : i32}
  llvm.func @printf(!llvm.ptr, ...) -> i32
  func.func @compute() attributes {llvm.linkage = #llvm.linkage<external>} {
    %c100 = arith.constant 100 : index
    %c1 = arith.constant 1 : index
    %c0 = arith.constant 0 : index
    %c-1_i32 = arith.constant -1 : i32
    %cst = arith.constant 0.000000e+00 : f64
    %c0_i32 = arith.constant 0 : i32
    %c100_i32 = arith.constant 100 : i32
    %alloca = memref.alloca() : memref<100xf64>
    %alloca_0 = memref.alloca() : memref<100xf64>
    %0 = call @rand() : () -> i32
    %1 = arith.remsi %0, %c100_i32 : i32
    %2 = arts.datablock "inout", %alloca_0 : memref<100xf64>[%c0] [%c100] [%c1] : memref<?xmemref<?xf64>>
    %3 = arts.datablock "inout", %alloca : memref<100xf64>[%c0] [%c100] [%c1] : memref<?xmemref<?xf64>>
    arts.edt parameters(%1) : (i32), constants(%c1, %c0, %c-1_i32, %c0_i32, %cst, %c100) : (index, index, i32, i32, f64, index), dependencies(%2, %3) : (memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>) attributes {parallel} {
      arts.barrier
      arts.edt attributes {single} {
        %8 = arith.sitofp %1 : i32 to f64
        scf.for %arg0 = %c0 to %c100 step %c1 {
          %9 = arith.index_cast %arg0 : index to i32
          %10 = arts.datablock "out", %2 : memref<?xmemref<?xf64>>[%arg0] [%c1] [%c1] {isLoad} : memref<?xmemref<?xf64>>
          arts.edt parameters(%9, %8, %arg0) : (i32, f64, index), dependencies(%10) : (memref<?xmemref<?xf64>>) {
            %16 = arith.sitofp %9 : i32 to f64
            %17 = arith.addf %16, %8 : f64
            %18 = memref.load %10[%c0] : memref<?xmemref<?xf64>>
            %c0_1 = arith.constant 0 : index
            memref.store %17, %18[%c0_1] : memref<?xf64>
            arts.yield
          }
          %11 = arith.addi %9, %c-1_i32 : i32
          %12 = arith.index_cast %11 : i32 to index
          %13 = arts.datablock "in", %2 : memref<?xmemref<?xf64>>[%arg0] [%c1] [%c1] {isLoad} : memref<?xmemref<?xf64>>
          %14 = arts.datablock "in", %2 : memref<?xmemref<?xf64>>[%12] [%c1] [%c1] {isLoad} : memref<?xmemref<?xf64>>
          %15 = arts.datablock "out", %3 : memref<?xmemref<?xf64>>[%arg0] [%c1] [%c1] {isLoad} : memref<?xmemref<?xf64>>
          arts.edt parameters(%9, %arg0, %12) : (i32, index, index), constants(%c0_i32, %cst) : (i32, f64), dependencies(%13, %14, %15) : (memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>, memref<?xmemref<?xf64>>) {
            %16 = memref.load %13[%c0] : memref<?xmemref<?xf64>>
            %c0_1 = arith.constant 0 : index
            %17 = memref.load %16[%c0_1] : memref<?xf64>
            %18 = memref.load %14[%c0] : memref<?xmemref<?xf64>>
            %c0_2 = arith.constant 0 : index
            %19 = memref.load %18[%c0_2] : memref<?xf64>
            %20 = arith.cmpi sgt, %9, %c0_i32 : i32
            %21 = arith.select %20, %19, %cst : f64
            %22 = arith.addf %17, %21 : f64
            %23 = memref.load %15[%c0] : memref<?xmemref<?xf64>>
            %c0_3 = arith.constant 0 : index
            memref.store %22, %23[%c0_3] : memref<?xf64>
            arts.yield
          }
        }
        arts.yield
      }
      arts.barrier
      arts.yield
    }
    %4 = llvm.mlir.addressof @str0 : !llvm.ptr
    %5 = llvm.getelementptr %4[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<11 x i8>
    %6 = affine.load %alloca_0[0] : memref<100xf64>
    %7 = llvm.call @printf(%5, %6) vararg(!llvm.func<i32 (ptr, ...)>) : (!llvm.ptr, f64) -> i32
    return
  }
  func.func private @rand() -> i32 attributes {llvm.linkage = #llvm.linkage<external>}
}
[convert-arts-to-funcs] Lowering arts.parallel

 - Datablock: %2 = arts.datablock "inout", %alloca_0 : memref<100xf64>[%c0] [%c100] [%c1] : memref<?xmemref<?xf64>>
 - Datablock: %8 = arts.datablock "inout", %alloca : memref<100xf64>[%c0] [%c100] [%c1] : memref<?xmemref<?xf64>>
 - Array of Datablocks: %8 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<100xf64>
 - Type: f64
 - Array of Datablocks: %7 = "memref.alloca"() <{operandSegmentSizes = array<i32: 0, 0>}> : () -> memref<100xf64>
 - Type: f64
[convert-arts-to-funcs] Lowering arts.edt: arts.edt parameters(%17, %16, %arg4) : (i32, f64, index), dependencies(%18) : (memref<?xmemref<?xf64>>) {
  %24 = arith.sitofp %17 : i32 to f64
  %25 = arith.addf %24, %16 : f64
  %26 = memref.load %18[%c0] : memref<?xmemref<?xf64>>
  %c0_7 = arith.constant 0 : index
  memref.store %25, %26[%c0_7] : memref<?xf64>
  arts.yield
}
[convert-arts-to-funcs] Processing edt op
 - Datablock: %18 = arts.datablock "out", %alloca : memref<?xmemref<?xf64>>[%arg4] [%c1] [%c1] {isLoad} : memref<?xmemref<?xf64>>
LLVM ERROR: neither the scoping op nor the type class provide data layout information for memref<?xf64>
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: carts-opt taskwithdeps-arts.mlir --convert-arts-to-funcs --cse --canonicalize -debug-only=convert-arts-to-funcs,arts-codegen
 #0 0x000055c71c58b1b7 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0xab11b7)
 #1 0x000055c71c588d8e llvm::sys::RunSignalHandlers() (/home/randres/projects/carts/.install/carts/bin/carts-opt+0xaaed8e)
 #2 0x000055c71c58b86a SignalHandler(int) Signals.cpp:0:0
 #3 0x00007fd3f47af520 (/lib/x86_64-linux-gnu/libc.so.6+0x42520)
 #4 0x00007fd3f48039fc pthread_kill (/lib/x86_64-linux-gnu/libc.so.6+0x969fc)
 #5 0x00007fd3f47af476 gsignal (/lib/x86_64-linux-gnu/libc.so.6+0x42476)
 #6 0x00007fd3f47957f3 abort (/lib/x86_64-linux-gnu/libc.so.6+0x287f3)
 #7 0x000055c71c55648c llvm::report_fatal_error(llvm::Twine const&, bool) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0xa7c48c)
 #8 0x000055c71c44b9c4 (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x9719c4)
 #9 0x000055c71c44b4a6 mlir::detail::getDefaultTypeSizeInBits(mlir::Type, mlir::DataLayout const&, llvm::ArrayRef<mlir::DataLayoutEntryInterface>) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x9714a6)
#10 0x000055c71c44f28b unsigned int llvm::function_ref<unsigned int (mlir::Type)>::callback_fn<mlir::DataLayout::getTypeSize(mlir::Type) const::$_3>(long, mlir::Type) DataLayoutInterfaces.cpp:0:0
#11 0x000055c71c44d27d cachedLookup(mlir::Type, llvm::DenseMap<mlir::Type, unsigned int, llvm::DenseMapInfo<mlir::Type, void>, llvm::detail::DenseMapPair<mlir::Type, unsigned int>>&, llvm::function_ref<unsigned int (mlir::Type)>) DataLayoutInterfaces.cpp:0:0
#12 0x000055c71c44b93c mlir::DataLayout::getTypeSize(mlir::Type) const (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x97193c)
#13 0x000055c71c102a40 mlir::arts::DataBlockCodegen::create(mlir::arts::DataBlockOp, mlir::Location) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x628a40)
#14 0x000055c71c10a635 mlir::arts::ArtsCodegen::createDatablock(mlir::arts::DataBlockOp, mlir::Location) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x630635)
#15 0x000055c71c0fc289 (anonymous namespace)::EdtOpLowering::matchAndRewrite(mlir::arts::EdtOp, mlir::arts::EdtOpAdaptor, mlir::ConversionPatternRewriter&) const ConvertArtsToFuncs.cpp:0:0
#16 0x000055c71c0fb90d mlir::OpConversionPattern<mlir::arts::EdtOp>::matchAndRewrite(mlir::Operation*, llvm::ArrayRef<mlir::Value>, mlir::ConversionPatternRewriter&) const (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x62190d)
#17 0x000055c71c27fecb mlir::ConversionPattern::matchAndRewrite(mlir::Operation*, mlir::PatternRewriter&) const (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x7a5ecb)
#18 0x000055c71c2af997 void llvm::function_ref<void ()>::callback_fn<mlir::PatternApplicator::matchAndRewrite(mlir::Operation*, mlir::PatternRewriter&, llvm::function_ref<bool (mlir::Pattern const&)>, llvm::function_ref<void (mlir::Pattern const&)>, llvm::function_ref<mlir::LogicalResult (mlir::Pattern const&)>)::$_2>(long) PatternApplicator.cpp:0:0
#19 0x000055c71c2ac2af mlir::PatternApplicator::matchAndRewrite(mlir::Operation*, mlir::PatternRewriter&, llvm::function_ref<bool (mlir::Pattern const&)>, llvm::function_ref<void (mlir::Pattern const&)>, llvm::function_ref<mlir::LogicalResult (mlir::Pattern const&)>) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x7d22af)
#20 0x000055c71c28d3d5 (anonymous namespace)::OperationLegalizer::legalize(mlir::Operation*, mlir::ConversionPatternRewriter&) DialectConversion.cpp:0:0
#21 0x000055c71c282157 (anonymous namespace)::OperationConverter::convertOperations(llvm::ArrayRef<mlir::Operation*>, llvm::function_ref<void (mlir::Diagnostic&)>) DialectConversion.cpp:0:0
#22 0x000055c71c282a10 mlir::applyPartialConversion(mlir::Operation*, mlir::ConversionTarget const&, mlir::FrozenRewritePatternSet const&, llvm::DenseSet<mlir::Operation*, llvm::DenseMapInfo<mlir::Operation*, void>>*) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x7a8a10)
#23 0x000055c71c0f9af5 (anonymous namespace)::ConvertArtsToFuncsPass::runOnOperation() ConvertArtsToFuncs.cpp:0:0
#24 0x000055c71c424f14 mlir::detail::OpToOpPassAdaptor::run(mlir::Pass*, mlir::Operation*, mlir::AnalysisManager, bool, unsigned int) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x94af14)
#25 0x000055c71c425541 mlir::detail::OpToOpPassAdaptor::runPipeline(mlir::OpPassManager&, mlir::Operation*, mlir::AnalysisManager, bool, unsigned int, mlir::PassInstrumentor*, mlir::PassInstrumentation::PipelineParentInfo const*) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x94b541)
#26 0x000055c71c4279f2 mlir::PassManager::run(mlir::Operation*) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x94d9f2)
#27 0x000055c71c126ef4 performActions(llvm::raw_ostream&, std::shared_ptr<llvm::SourceMgr> const&, mlir::MLIRContext*, mlir::MlirOptMainConfig const&) MlirOptMain.cpp:0:0
#28 0x000055c71c126164 mlir::LogicalResult llvm::function_ref<mlir::LogicalResult (std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&)>::callback_fn<mlir::MlirOptMain(llvm::raw_ostream&, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, mlir::DialectRegistry&, mlir::MlirOptMainConfig const&)::$_2>(long, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&) MlirOptMain.cpp:0:0
#29 0x000055c71c528c68 mlir::splitAndProcessBuffer(std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::function_ref<mlir::LogicalResult (std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&)>, llvm::raw_ostream&, bool, bool) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0xa4ec68)
#30 0x000055c71c12074a mlir::MlirOptMain(llvm::raw_ostream&, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, mlir::DialectRegistry&, mlir::MlirOptMainConfig const&) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x64674a)
#31 0x000055c71c120c14 mlir::MlirOptMain(int, char**, llvm::StringRef, mlir::DialectRegistry&) (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x646c14)
#32 0x000055c71bb61441 main (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x87441)
#33 0x00007fd3f4796d90 (/lib/x86_64-linux-gnu/libc.so.6+0x29d90)
#34 0x00007fd3f4796e40 __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x29e40)
#35 0x000055c71bb60a95 _start (/home/randres/projects/carts/.install/carts/bin/carts-opt+0x86a95)
