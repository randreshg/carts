LIBS=

all: test_opt

# Emit LLVM IR -> TODO: Add OpenMP Metadata
%.bc: %.cpp
	clang++ -fopenmp -O3 -g0 -emit-llvm -c $< -o $@ -lstdc++
	llvm-dis $@

%.bc: %.c
	clang -fopenmp -O3 -g0 -emit-llvm -c $< -o $@
	llvm-dis $@

# Run OMP transform pass
test_arts_ir.bc: test.bc
	opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so \
		-passes="omp-transform" $< -o $@
	# -debug-only=omp-transform,arts,carts,arts-ir-builder,arts-utils\
	llvm-dis $@

# Run the ARTS analysis pass
test_arts_analysis.bc:  test_arts_ir.bc
	opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so \
			-debug-only=arts-analysis,arts,carts,arts-ir-builder,edt-graph,carts-metadata,arts-codegen,arts-utils\
			-passes="arts-analysis" $< -o $@
	llvm-dis $@


test_opt.bc: test_arts_analysis.bc
	opt -O3 $< -o $@
	llvm-dis $@

test_opt: test_opt.bc
	clang++ $< -O3 -g3 -march=native -o $@ -lstdc++ -I/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/arts/include -L/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/arts/lib -larts -L/usr/lib64/librt.so/usr/lib64/libpthread.so/usr/lib64/lib -lrdmacm


clean:
	rm -f *.bc *.ll test_opt test_pre_prof output.prof core.* default.* ;
