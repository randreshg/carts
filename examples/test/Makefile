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
	opt -debug-only=omp-transform,arts,carts,arts-ir-builder,arts-utils\
		-load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so \
		-passes="omp-transform" $< -o $@
	llvm-dis $@

# Normalize the IR with NOELLE
test_norm.bc: test_arts_ir.bc
	noelle-norm $^ -o $@
	llvm-dis $@

# Embed NOELLE PDG metadata in the IR
test_with_metadata.bc: test_norm.bc
	noelle-meta-pdg-embed $^ -o $@
	llvm-dis $@

# Run the ARTS analysis pass
test_arts_analysis.bc: test_with_metadata.bc
	noelle-load -debug-only=arts-analyzer,arts,carts,arts-ir-builder,edt-graph \
		-load /home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so -ARTSAnalysisPass $< -o $@
	llvm-dis $@

# noelle-load -debug-only=arts,carts,arts-analyzer,arts-ir-builder,arts-utils -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.build/CARTS.so -CARTS $< -o $@
# noelle-load -debug-only=arts,carts,arts-analyzer -load-pass-plugin /home/rherreraguaitero/ME/ARTS-env/CARTS/.build/CARTS.so -passes=CARTS $< -o $@



test_opt: test_arts_analysis.bc
	clang++ -fopenmp $< -O3 -march=native -o $@ -lstdc++

clean:
	rm -f *.bc *.ll test_opt test_pre_prof output.prof default.* ;
