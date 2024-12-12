LIBS=

all: simple_opt

# Emit LLVM IR -> TODO: Add OpenMP Metadata
%.bc: %.cpp
	clang++ -fopenmp -O3 -g0 -emit-llvm -c $< -o $@ -lstdc++
	llvm-dis $@

%.bc: %.c
	clang -fopenmp -O3 -g0 -emit-llvm -c $< -o $@
	llvm-dis $@

# Run OMP transform pass
simple_arts_ir.bc: simple.bc
	opt -load-pass-plugin=/home/randres/projects/carts/.install/carts/lib/ARTSTransform.so \
		-debug-only=omp-transform,arts,carts,arts-ir-builder\
		-passes="omp-transform" $< -o $@
	# -debug-only=omp-transform,arts,carts,arts-ir-builder,arts-utils\
	llvm-dis $@

# Run the ARTS analysis pass
simple_arts_analysis.bc:  simple_arts_ir.bc
	opt -load-pass-plugin=/home/randres/projects/carts/.install/carts/lib/ARTSAnalysis.so \
			-debug-only=arts-analysis,arts,carts,arts-ir-builder,arts-graph,carts-metadata,arts-codegen\
			-passes="arts-analysis" $< -o $@
	llvm-dis $@


simple_opt.bc: simple_arts_analysis.bc
	opt -O3 $< -o $@
	llvm-dis $@

simple_opt: simple_opt.bc
	clang++ $< -O3 -g3 -march=native -o $@ -I/home/randres/projects/carts/carts/include -L/home/randres/projects/carts/.install/arts/lib -larts -lrdmacm


clean:
	rm -f *.bc *.ll simple_opt ;
