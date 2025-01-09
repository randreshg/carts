polygeist-opt: /home/randres/projects/carts/external/Polygeist/llvm-project/llvm/include/llvm/Support/Casting.h:566: decltype(auto) llvm::cast(const From &) [To = mlir::detail::TypedValue<mlir::MemRefType>, From = mlir::Value]: Assertion `isa<To>(Val) && "cast<Ty>() argument of incompatible type!"' failed.
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: polygeist-opt taskwithdeps.mlir --convert-polygeist-to-llvm
 #0 0x000055826259bd07 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/randres/projects/carts/.install/polygeist/bin/polygeist-opt+0x18ead07)
 #1 0x00005582625998de llvm::sys::RunSignalHandlers() (/home/randres/projects/carts/.install/polygeist/bin/polygeist-opt+0x18e88de)
 #2 0x000055826259c3ba SignalHandler(int) Signals.cpp:0:0
 #3 0x00007f0d5b22a520 (/lib/x86_64-linux-gnu/libc.so.6+0x42520)
 #4 0x00007f0d5b27e9fc pthread_kill (/lib/x86_64-linux-gnu/libc.so.6+0x969fc)
 #5 0x00007f0d5b22a476 gsignal (/lib/x86_64-linux-gnu/libc.so.6+0x42476)
 #6 0x00007f0d5b2107f3 abort (/lib/x86_64-linux-gnu/libc.so.6+0x287f3)
 #7 0x00007f0d5b21071b (/lib/x86_64-linux-gnu/libc.so.6+0x2871b)
 #8 0x00007f0d5b221e96 (/lib/x86_64-linux-gnu/libc.so.6+0x39e96)
 #9 0x00005582614a45aa (/home/randres/projects/carts/.install/polygeist/bin/polygeist-opt+0x7f35aa)
#10 0x00005582614ce596 mlir::LogicalResult mlir::Op<mlir::affine::AffineLoadOp, mlir::OpTrait::ZeroRegions, mlir::OpTrait::OneResult, mlir::OpTrait::OneTypedResult<mlir::Type>::Impl, mlir::OpTrait::ZeroSuccessors, mlir::OpTrait::AtLeastNOperands<1u>::Impl, mlir::OpTrait::OpInvariants, mlir::BytecodeOpInterface::Trait, mlir::affine::AffineReadOpInterface::Trait, mlir::affine::AffineMapAccessInterface::Trait, mlir::OpTrait::MemRefsNormalizable, mlir::MemoryEffectOpInterface::Trait>::foldSingleResultHook<mlir::affine::AffineLoadOp>(mlir::Operation*, llvm::ArrayRef<mlir::Attribute>, llvm::SmallVectorImpl<mlir::OpFoldResult>&) AffineOps.cpp:0:0
#11 0x00005582614cce94 mlir::RegisteredOperationName::Model<mlir::affine::AffineLoadOp>::foldHook(mlir::Operation*, llvm::ArrayRef<mlir::Attribute>, llvm::SmallVectorImpl<mlir::OpFoldResult>&) AffineOps.cpp:0:0
#12 0x0000558262507af0 mlir::Operation::fold(llvm::SmallVectorImpl<mlir::OpFoldResult>&) (/home/randres/projects/carts/.install/polygeist/bin/polygeist-opt+0x1856af0)
#13 0x000055826248ad0e mlir::OpBuilder::tryFold(mlir::Operation*, llvm::SmallVectorImpl<mlir::Value>&) (/home/randres/projects/carts/.install/polygeist/bin/polygeist-opt+0x17d9d0e)
#14 0x0000558262274993 (anonymous namespace)::OperationLegalizer::legalize(mlir::Operation*, mlir::ConversionPatternRewriter&) DialectConversion.cpp:0:0
#15 0x0000558262276db3 mlir::LogicalResult llvm::function_ref<mlir::LogicalResult (mlir::Pattern const&)>::callback_fn<(anonymous namespace)::OperationLegalizer::legalizeWithPattern(mlir::Operation*, mlir::ConversionPatternRewriter&)::$_20>(long, mlir::Pattern const&) DialectConversion.cpp:0:0
#16 0x000055826236b8f8 void llvm::function_ref<void ()>::callback_fn<mlir::PatternApplicator::matchAndRewrite(mlir::Operation*, mlir::PatternRewriter&, llvm::function_ref<bool (mlir::Pattern const&)>, llvm::function_ref<void (mlir::Pattern const&)>, llvm::function_ref<mlir::LogicalResult (mlir::Pattern const&)>)::$_2>(long) PatternApplicator.cpp:0:0
#17 0x000055826236844f mlir::PatternApplicator::matchAndRewrite(mlir::Operation*, mlir::PatternRewriter&, llvm::function_ref<bool (mlir::Pattern const&)>, llvm::function_ref<void (mlir::Pattern const&)>, llvm::function_ref<mlir::LogicalResult (mlir::Pattern const&)>) (/home/randres/projects/carts/.install/polygeist/bin/polygeist-opt+0x16b744f)
#18 0x0000558262274dc5 (anonymous namespace)::OperationLegalizer::legalize(mlir::Operation*, mlir::ConversionPatternRewriter&) DialectConversion.cpp:0:0
#19 0x0000558262269f67 (anonymous namespace)::OperationConverter::convertOperations(llvm::ArrayRef<mlir::Operation*>, llvm::function_ref<void (mlir::Diagnostic&)>) DialectConversion.cpp:0:0
#20 0x000055826226a820 mlir::applyPartialConversion(mlir::Operation*, mlir::ConversionTarget const&, mlir::FrozenRewritePatternSet const&, llvm::DenseSet<mlir::Operation*, llvm::DenseMapInfo<mlir::Operation*, void>>*) (/home/randres/projects/carts/.install/polygeist/bin/polygeist-opt+0x15b9820)
#21 0x0000558261e559d7 (anonymous namespace)::ConvertPolygeistToLLVMPass::convertModule(mlir::ModuleOp, bool) ConvertPolygeistToLLVM.cpp:0:0
#22 0x0000558261e4e28c (anonymous namespace)::ConvertPolygeistToLLVMPass::runOnOperation() ConvertPolygeistToLLVM.cpp:0:0
#23 0x00005582623ced84 mlir::detail::OpToOpPassAdaptor::run(mlir::Pass*, mlir::Operation*, mlir::AnalysisManager, bool, unsigned int) (/home/randres/projects/carts/.install/polygeist/bin/polygeist-opt+0x171dd84)
#24 0x00005582623cf3b1 mlir::detail::OpToOpPassAdaptor::runPipeline(mlir::OpPassManager&, mlir::Operation*, mlir::AnalysisManager, bool, unsigned int, mlir::PassInstrumentor*, mlir::PassInstrumentation::PipelineParentInfo const*) (/home/randres/projects/carts/.install/polygeist/bin/polygeist-opt+0x171e3b1)
#25 0x00005582623d1862 mlir::PassManager::run(mlir::Operation*) (/home/randres/projects/carts/.install/polygeist/bin/polygeist-opt+0x1720862)
#26 0x0000558261fac214 performActions(llvm::raw_ostream&, std::shared_ptr<llvm::SourceMgr> const&, mlir::MLIRContext*, mlir::MlirOptMainConfig const&) MlirOptMain.cpp:0:0
#27 0x0000558261fab484 mlir::LogicalResult llvm::function_ref<mlir::LogicalResult (std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&)>::callback_fn<mlir::MlirOptMain(llvm::raw_ostream&, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, mlir::DialectRegistry&, mlir::MlirOptMainConfig const&)::$_2>(long, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&) MlirOptMain.cpp:0:0
#28 0x0000558262530ba8 mlir::splitAndProcessBuffer(std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::function_ref<mlir::LogicalResult (std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, llvm::raw_ostream&)>, llvm::raw_ostream&, bool, bool) (/home/randres/projects/carts/.install/polygeist/bin/polygeist-opt+0x187fba8)
#29 0x0000558261fa651a mlir::MlirOptMain(llvm::raw_ostream&, std::unique_ptr<llvm::MemoryBuffer, std::default_delete<llvm::MemoryBuffer>>, mlir::DialectRegistry&, mlir::MlirOptMainConfig const&) (/home/randres/projects/carts/.install/polygeist/bin/polygeist-opt+0x12f551a)
#30 0x0000558261fa69e4 mlir::MlirOptMain(int, char**, llvm::StringRef, mlir::DialectRegistry&) (/home/randres/projects/carts/.install/polygeist/bin/polygeist-opt+0x12f59e4)
#31 0x000055826148f35b main (/home/randres/projects/carts/.install/polygeist/bin/polygeist-opt+0x7de35b)
#32 0x00007f0d5b211d90 (/lib/x86_64-linux-gnu/libc.so.6+0x29d90)
#33 0x00007f0d5b211e40 __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x29e40)
#34 0x000055826148e975 _start (/home/randres/projects/carts/.install/polygeist/bin/polygeist-opt+0x7dd975)
