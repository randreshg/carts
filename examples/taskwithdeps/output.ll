clang++ -fopenmp -O3 -g0 -emit-llvm -c taskwithdeps.cpp -o taskwithdeps.bc -lstdc++
clang++: warning: -Z-reserved-lib-stdc++: 'linker' input unused [-Wunused-command-line-argument]
llvm-dis taskwithdeps.bc
opt -load-pass-plugin=/home/randres/projects/carts/.install/carts/lib/OMPVisitor.so \
	-debug-only=omp-visitor\
	-passes="omp-visitor" -ast-file=taskwithdeps.ast taskwithdeps.bc -o taskwithdeps_visitor.bc

-------------------------------------------------
[omp-visitor] Running OmpVisitorPass on Module: 
taskwithdeps.bc

-------------------------------------------------
Found OpenMP directive: OMPParallelDirective at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:31:3
Found OpenMP directive: OMPSingleDirective at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:32:3
Found OpenMP directive: OMPTaskDirective at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:40:7
  This is an OpenMP Task Directive
    Clause: depend
      Dependency type: 1
      Dependency variable: res
Found OpenMP directive: OMPTaskDirective at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:42:7
  This is an OpenMP Task Directive
    Clause: depend
      Dependency type: 1
      Dependency variable: x
Found OpenMP directive: OMPTaskDirective at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:44:7
  This is an OpenMP Task Directive
    Clause: depend
      Dependency type: 1
      Dependency variable: y
Found OpenMP directive: OMPTaskDirective at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:46:7
  This is an OpenMP Task Directive
    Clause: depend
      Dependency type: 0
      Dependency variable: x
Found OpenMP directive: OMPTaskDirective at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:51:7
  This is an OpenMP Task Directive
    Clause: depend
      Dependency type: 0
      Dependency variable: y
Found OpenMP directive: OMPTaskDirective at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:53:7
  This is an OpenMP Task Directive
    Clause: depend
      Dependency type: 0
      Dependency variable: res
Found OpenMP directive: OMPParallelDirective at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:31:3
Found OpenMP directive: OMPSingleDirective at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:32:3
Found OpenMP directive: OMPTaskDirective at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:40:7
  This is an OpenMP Task Directive
    Clause: depend
      Dependency type: 1
      Dependency variable: res
Found OpenMP directive: OMPTaskDirective at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:42:7
  This is an OpenMP Task Directive
    Clause: depend
      Dependency type: 1
      Dependency variable: x
Found OpenMP directive: OMPTaskDirective at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:44:7
  This is an OpenMP Task Directive
    Clause: depend
      Dependency type: 1
      Dependency variable: y
Found OpenMP directive: OMPTaskDirective at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:46:7
  This is an OpenMP Task Directive
    Clause: depend
      Dependency type: 0
      Dependency variable: x
Found OpenMP directive: OMPTaskDirective at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:51:7
  This is an OpenMP Task Directive
    Clause: depend
      Dependency type: 0
      Dependency variable: y
Found OpenMP directive: OMPTaskDirective at /home/randres/projects/carts/examples/taskwithdeps/taskwithdeps.cpp:53:7
  This is an OpenMP Task Directive
    Clause: depend
      Dependency type: 0
      Dependency variable: res
# opt -load ./libOpenMPParserPass.so -openmp-parser-pass -ast-file=your_ast_file.ast your_module.bc
# -debug-only=omp-visitor,arts,carts,arts-ir-builder,arts-utils\
llvm-dis taskwithdeps_visitor.bc
