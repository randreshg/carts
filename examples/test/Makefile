LIBS=

all: test_opt

%.bc: %.cpp
	clang++ -fopenmp -O3 -g0 -emit-llvm -c $< -o $@ -lstdc++
	llvm-dis $@

%.bc: %.c
	clang -fopenmp -O3 -g0 -emit-llvm -c $< -o $@
	llvm-dis $@

test_arts.bc: test.bc
	opt -debug-only=arts,carts,arts-analyzer,arts-ir-builder,edt-graph,omp-transform \
		-load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so \
		-passes="omp-transform" $< -o $@
	llvm-dis $@

# test_norm.bc: test.bc
# 	noelle-norm $^ -o $@
# 	llvm-dis $@

test_with_metadata.bc: test_norm.bc
	noelle-meta-pdg-embed $^ -o $@
	llvm-dis $@

# test_opt.bc: test_with_metadata.bc
# 	# noelle-load -debug-only=arts,carts,arts-analyzer,arts-ir-builder,arts-utils -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.build/CARTS.so -CARTS $< -o $@
# 	noelle-load -debug-only=arts,carts,arts-analyzer,arts-ir-builder,edt-graph -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.build/CARTS.so -CARTS $< -o $@
# 	# noelle-load -debug-only=arts,carts,arts-analyzer -load-pass-plugin /home/rherreraguaitero/ME/ARTS-env/CARTS/.build/CARTS.so -passes=CARTS $< -o $@
# 	llvm-dis $@

# test_opt.bc: test.bc
# 	noelle-load -debug-only=carts,arts-analyzer -load /home/rherreraguaitero/ME/ARTS-env/CARTS/.build/CARTS.so -CARTS $< -o $@
# 	llvm-dis $@

test_opt: test_arts.bc
	clang++ -fopenmp $< -O3 -march=native -o $@ -lstdc++

clean:
	rm -f *.bc *.ll test_opt test_pre_prof output.prof default.* ;
