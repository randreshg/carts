cholesky_for.c:167:9: error: call to undeclared function 'check_factorization'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]
  167 |     if (check_factorization(n, original_matrix, matrix, n, uplo, eps))
      |         ^
may not handle omp clause 80
ImplicitCastExpr 0x5571334aa930 'struct timespec *' <ArrayToPointerDecay>
`-DeclRefExpr 0x5571334aa8f0 'struct timespec[50]' lvalue Var 0x5571334a5048 'rsss' 'struct timespec[50]'
ImplicitCastExpr 0x5571334aaed8 'double **' <ArrayToPointerDecay>
`-ArraySubscriptExpr 0x5571334aae98 'double *[NB]' lvalue
  |-ImplicitCastExpr 0x5571334aae68 'double *(*)[NB]':'double *(*)[NB]' <LValueToRValue>
  | `-DeclRefExpr 0x5571334aae28 'double *(*)[NB]':'double *(*)[NB]' lvalue ParmVar 0x5571334a9820 'Ah' 'double *(*)[NB]':'double *(*)[NB]' refers_to_enclosing_variable_or_capture
  `-ImplicitCastExpr 0x5571334aae80 'int' <LValueToRValue>
    `-DeclRefExpr 0x5571334aae48 'int' lvalue Var 0x5571334aaa58 'k' 'int' refers_to_enclosing_variable_or_capture
ImplicitCastExpr 0x5571334abcd8 'double **' <ArrayToPointerDecay>
`-ArraySubscriptExpr 0x5571334abc98 'double *[NB]' lvalue
  |-ImplicitCastExpr 0x5571334abc68 'double *(*)[NB]':'double *(*)[NB]' <LValueToRValue>
  | `-DeclRefExpr 0x5571334abc28 'double *(*)[NB]':'double *(*)[NB]' lvalue ParmVar 0x5571334a9820 'Ah' 'double *(*)[NB]':'double *(*)[NB]' refers_to_enclosing_variable_or_capture
  `-ImplicitCastExpr 0x5571334abc80 'int' <LValueToRValue>
    `-DeclRefExpr 0x5571334abc48 'int' lvalue Var 0x5571334aaa58 'k' 'int' refers_to_enclosing_variable_or_capture
ImplicitCastExpr 0x5571334abdd8 'double **' <ArrayToPointerDecay>
`-ArraySubscriptExpr 0x5571334abd98 'double *[NB]' lvalue
  |-ImplicitCastExpr 0x5571334abd68 'double *(*)[NB]':'double *(*)[NB]' <LValueToRValue>
  | `-DeclRefExpr 0x5571334abd28 'double *(*)[NB]':'double *(*)[NB]' lvalue ParmVar 0x5571334a9820 'Ah' 'double *(*)[NB]':'double *(*)[NB]' refers_to_enclosing_variable_or_capture
  `-ImplicitCastExpr 0x5571334abd80 'int' <LValueToRValue>
    `-DeclRefExpr 0x5571334abd48 'int' lvalue Var 0x5571334aaa58 'k' 'int' refers_to_enclosing_variable_or_capture
ImplicitCastExpr 0x5571334af928 'double **' <ArrayToPointerDecay>
`-ArraySubscriptExpr 0x5571334af8e8 'double *[NB]' lvalue
  |-ImplicitCastExpr 0x5571334af8b8 'double *(*)[NB]':'double *(*)[NB]' <LValueToRValue>
  | `-DeclRefExpr 0x5571334af878 'double *(*)[NB]':'double *(*)[NB]' lvalue ParmVar 0x5571334a9820 'Ah' 'double *(*)[NB]':'double *(*)[NB]' refers_to_enclosing_variable_or_capture
  `-ImplicitCastExpr 0x5571334af8d0 'int' <LValueToRValue>
    `-DeclRefExpr 0x5571334af898 'int' lvalue Var 0x5571334aaa58 'k' 'int' refers_to_enclosing_variable_or_capture
ImplicitCastExpr 0x5571334afa28 'double **' <ArrayToPointerDecay>
`-ArraySubscriptExpr 0x5571334af9e8 'double *[NB]' lvalue
  |-ImplicitCastExpr 0x5571334af9b8 'double *(*)[NB]':'double *(*)[NB]' <LValueToRValue>
  | `-DeclRefExpr 0x5571334af978 'double *(*)[NB]':'double *(*)[NB]' lvalue ParmVar 0x5571334a9820 'Ah' 'double *(*)[NB]':'double *(*)[NB]' refers_to_enclosing_variable_or_capture
  `-ImplicitCastExpr 0x5571334af9d0 'int' <LValueToRValue>
    `-DeclRefExpr 0x5571334af998 'int' lvalue Var 0x5571334aaa58 'k' 'int' refers_to_enclosing_variable_or_capture
ImplicitCastExpr 0x5571334afb28 'double **' <ArrayToPointerDecay>
`-ArraySubscriptExpr 0x5571334afae8 'double *[NB]' lvalue
  |-ImplicitCastExpr 0x5571334afab8 'double *(*)[NB]':'double *(*)[NB]' <LValueToRValue>
  | `-DeclRefExpr 0x5571334afa78 'double *(*)[NB]':'double *(*)[NB]' lvalue ParmVar 0x5571334a9820 'Ah' 'double *(*)[NB]':'double *(*)[NB]' refers_to_enclosing_variable_or_capture
  `-ImplicitCastExpr 0x5571334afad0 'int' <LValueToRValue>
    `-DeclRefExpr 0x5571334afa98 'int' lvalue Var 0x5571334af658 'j' 'int'
ImplicitCastExpr 0x5571334b1ae0 'double **' <ArrayToPointerDecay>
`-ArraySubscriptExpr 0x5571334b1aa0 'double *[NB]' lvalue
  |-ImplicitCastExpr 0x5571334b1a70 'double *(*)[NB]':'double *(*)[NB]' <LValueToRValue>
  | `-DeclRefExpr 0x5571334b1a30 'double *(*)[NB]':'double *(*)[NB]' lvalue ParmVar 0x5571334a9820 'Ah' 'double *(*)[NB]':'double *(*)[NB]' refers_to_enclosing_variable_or_capture
  `-ImplicitCastExpr 0x5571334b1a88 'int' <LValueToRValue>
    `-DeclRefExpr 0x5571334b1a50 'int' lvalue Var 0x5571334aaa58 'k' 'int' refers_to_enclosing_variable_or_capture
ImplicitCastExpr 0x5571334b1c00 'double **' <ArrayToPointerDecay>
`-ArraySubscriptExpr 0x5571334b1bc0 'double *[NB]' lvalue
  |-ImplicitCastExpr 0x5571334b1b90 'double *(*)[NB]':'double *(*)[NB]' <LValueToRValue>
  | `-DeclRefExpr 0x5571334b1b30 'double *(*)[NB]':'double *(*)[NB]' lvalue ParmVar 0x5571334a9820 'Ah' 'double *(*)[NB]':'double *(*)[NB]' refers_to_enclosing_variable_or_capture
  `-ImplicitCastExpr 0x5571334b1ba8 'int' <LValueToRValue>
    `-DeclRefExpr 0x5571334b1b50 'int' lvalue Var 0x5571334af240 'l' 'int' refers_to_enclosing_variable_or_capture
ImplicitCastExpr 0x5571334b2658 'struct timespec *' <ArrayToPointerDecay>
`-DeclRefExpr 0x5571334b2618 'struct timespec[50]' lvalue Var 0x5571334a5120 'rsee' 'struct timespec[50]'
ImplicitCastExpr 0x5571334b2778 'double *' <ArrayToPointerDecay>
`-DeclRefExpr 0x5571334b2738 'double[50]' lvalue Var 0x5571334a9b60 'makespan' 'double[50]' refers_to_enclosing_variable_or_capture
ImplicitCastExpr 0x5571334b2808 'struct timespec *' <ArrayToPointerDecay>
`-DeclRefExpr 0x5571334b27c8 'struct timespec[50]' lvalue Var 0x5571334a5120 'rsee' 'struct timespec[50]'
ImplicitCastExpr 0x5571334b28c8 'struct timespec *' <ArrayToPointerDecay>
`-DeclRefExpr 0x5571334b2888 'struct timespec[50]' lvalue Var 0x5571334a5048 'rsss' 'struct timespec[50]'
ImplicitCastExpr 0x5571334b2a50 'struct timespec *' <ArrayToPointerDecay>
`-DeclRefExpr 0x5571334b2a10 'struct timespec[50]' lvalue Var 0x5571334a5120 'rsee' 'struct timespec[50]'
ImplicitCastExpr 0x5571334b2b10 'struct timespec *' <ArrayToPointerDecay>
`-DeclRefExpr 0x5571334b2ad0 'struct timespec[50]' lvalue Var 0x5571334a5048 'rsss' 'struct timespec[50]'
ImplicitCastExpr 0x5571334a4ca0 'double *' <ArrayToPointerDecay>
`-CompoundLiteralExpr 0x5571334a4c78 'double[4194304]' lvalue
  `-InitListExpr 0x5571334a4bf0 'double[4194304]'
    |-array_filler: ImplicitValueInitExpr 0x5571334a4c50 'double'
    `-ConstantExpr 0x5571334a4c60 'double'
      `-ImplicitCastExpr 0x5571334a4c30 'double' <IntegralToFloating>
        `-IntegerLiteral 0x5571334a4b78 'int' 0
cgeist: /home/randres/projects/carts/external/Polygeist/tools/cgeist/Lib/clang-mlir.cc:1644: ValueCategory MLIRScanner::CommonArrayToPointer(mlir::Location, ValueCategory): Assertion `scalar.val' failed.
PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.
Stack dump:
0.	Program arguments: cgeist cholesky_for.c -fopenmp -O3 -S -I /usr/lib/llvm-14/lib/clang/14.0.0/include
1.	<eof> parser at end of file
 #0 0x0000557128735147 llvm::sys::PrintStackTrace(llvm::raw_ostream&, int) (/home/randres/projects/carts/.install/polygeist/bin/cgeist+0x42c7147)
 #1 0x0000557128732d1e llvm::sys::RunSignalHandlers() (/home/randres/projects/carts/.install/polygeist/bin/cgeist+0x42c4d1e)
 #2 0x000055712873595a SignalHandler(int) Signals.cpp:0:0
 #3 0x00007fb2b17d9520 (/lib/x86_64-linux-gnu/libc.so.6+0x42520)
 #4 0x00007fb2b182d9fc pthread_kill (/lib/x86_64-linux-gnu/libc.so.6+0x969fc)
 #5 0x00007fb2b17d9476 gsignal (/lib/x86_64-linux-gnu/libc.so.6+0x42476)
 #6 0x00007fb2b17bf7f3 abort (/lib/x86_64-linux-gnu/libc.so.6+0x287f3)
 #7 0x00007fb2b17bf71b (/lib/x86_64-linux-gnu/libc.so.6+0x2871b)
 #8 0x00007fb2b17d0e96 (/lib/x86_64-linux-gnu/libc.so.6+0x39e96)
 #9 0x000055712758e798 MLIRScanner::CommonArrayToPointer(mlir::Location, ValueCategory) (/home/randres/projects/carts/.install/polygeist/bin/cgeist+0x3120798)
#10 0x0000557127591c1c MLIRScanner::VisitCastExpr(clang::CastExpr*) (/home/randres/projects/carts/.install/polygeist/bin/cgeist+0x3123c1c)
#11 0x0000557127584248 clang::StmtVisitorBase<std::add_pointer, MLIRScanner, ValueCategory>::Visit(clang::Stmt*) driver.cc:0:0
#12 0x000055712758d16c MLIRASTConsumer::GetOrCreateGlobal(clang::ValueDecl const*, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>, bool) (/home/randres/projects/carts/.install/polygeist/bin/cgeist+0x311f16c)
#13 0x00005571275a70e2 MLIRScanner::VisitDeclRefExpr(clang::DeclRefExpr*) (/home/randres/projects/carts/.install/polygeist/bin/cgeist+0x31390e2)
#14 0x00005571275842b4 clang::StmtVisitorBase<std::add_pointer, MLIRScanner, ValueCategory>::Visit(clang::Stmt*) driver.cc:0:0
#15 0x0000557127591c8b MLIRScanner::VisitCastExpr(clang::CastExpr*) (/home/randres/projects/carts/.install/polygeist/bin/cgeist+0x3123c8b)
#16 0x0000557127584248 clang::StmtVisitorBase<std::add_pointer, MLIRScanner, ValueCategory>::Visit(clang::Stmt*) driver.cc:0:0
#17 0x00005571275913a9 MLIRScanner::VisitCastExpr(clang::CastExpr*) (/home/randres/projects/carts/.install/polygeist/bin/cgeist+0x31233a9)
#18 0x0000557127584248 clang::StmtVisitorBase<std::add_pointer, MLIRScanner, ValueCategory>::Visit(clang::Stmt*) driver.cc:0:0
#19 0x00005571275913a9 MLIRScanner::VisitCastExpr(clang::CastExpr*) (/home/randres/projects/carts/.install/polygeist/bin/cgeist+0x31233a9)
#20 0x0000557127584248 clang::StmtVisitorBase<std::add_pointer, MLIRScanner, ValueCategory>::Visit(clang::Stmt*) driver.cc:0:0
#21 0x0000557127604c8f MLIRScanner::VisitCallExpr(clang::CallExpr*) (/home/randres/projects/carts/.install/polygeist/bin/cgeist+0x3196c8f)
#22 0x0000557127584252 clang::StmtVisitorBase<std::add_pointer, MLIRScanner, ValueCategory>::Visit(clang::Stmt*) driver.cc:0:0
#23 0x00005571275e27f6 MLIRScanner::VisitCompoundStmt(clang::CompoundStmt*) (/home/randres/projects/carts/.install/polygeist/bin/cgeist+0x31747f6)
#24 0x000055712758439e clang::StmtVisitorBase<std::add_pointer, MLIRScanner, ValueCategory>::Visit(clang::Stmt*) driver.cc:0:0
#25 0x00005571275de5ca MLIRScanner::VisitOMPParallelDirective(clang::OMPParallelDirective*) (/home/randres/projects/carts/.install/polygeist/bin/cgeist+0x31705ca)
#26 0x0000557127584406 clang::StmtVisitorBase<std::add_pointer, MLIRScanner, ValueCategory>::Visit(clang::Stmt*) driver.cc:0:0
#27 0x00005571275e27f6 MLIRScanner::VisitCompoundStmt(clang::CompoundStmt*) (/home/randres/projects/carts/.install/polygeist/bin/cgeist+0x31747f6)
#28 0x000055712758439e clang::StmtVisitorBase<std::add_pointer, MLIRScanner, ValueCategory>::Visit(clang::Stmt*) driver.cc:0:0
#29 0x000055712757c488 MLIRScanner::init(mlir::func::FuncOp, clang::FunctionDecl const*) (/home/randres/projects/carts/.install/polygeist/bin/cgeist+0x310e488)
#30 0x00005571275af8c4 MLIRASTConsumer::run() (/home/randres/projects/carts/.install/polygeist/bin/cgeist+0x31418c4)
#31 0x000055712d5ca4b4 clang::ParseAST(clang::Sema&, bool, bool) (/home/randres/projects/carts/.install/polygeist/bin/cgeist+0x915c4b4)
#32 0x0000557129fca260 clang::FrontendAction::Execute() (/home/randres/projects/carts/.install/polygeist/bin/cgeist+0x5b5c260)
#33 0x00005571275b4f04 main (/home/randres/projects/carts/.install/polygeist/bin/cgeist+0x3146f04)
#34 0x00007fb2b17c0d90 (/lib/x86_64-linux-gnu/libc.so.6+0x29d90)
#35 0x00007fb2b17c0e40 __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x29e40)
#36 0x0000557127572ce5 _start (/home/randres/projects/carts/.install/polygeist/bin/cgeist+0x3104ce5)
