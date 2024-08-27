LIBS=

all: taskwait_opt

# Emit LLVM IR -> TODO: Add OpenMP Metadata
%.bc: %.cpp
	clang++ -fopenmp -O3 -g0 -emit-llvm -c $< -o $@ -lstdc++
	llvm-dis $@

%.bc: %.c
	clang -fopenmp -O3 -g0 -emit-llvm -c $< -o $@
	llvm-dis $@

# Run OMP transform pass
taskwait_arts_ir.bc: taskwait.bc
	opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/OMPTransform.so \
		-passes="omp-transform" $< -o $@
	# -debug-only=omp-transform,arts,carts,arts-ir-builder,arts-utils\
	llvm-dis $@

# Run the ARTS analysis pass
taskwait_arts_analysis.bc:  taskwait_arts_ir.bc
	opt -load-pass-plugin=/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/carts/lib/ARTSAnalysis.so \
			-debug-only=arts-analysis,arts,carts,arts-ir-builder,arts-graph,carts-metadata,arts-codegen\
			-passes="arts-analysis" $< -o $@
	llvm-dis $@


taskwait_opt.bc: taskwait_arts_analysis.bc
	opt -O3 $< -o $@
	llvm-dis $@

taskwait_opt: taskwait_opt.bc
	clang++ $< -O3 -g3 -march=native -o $@ -lstdc++ -I/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/arts/include -L/home/rherreraguaitero/ME/ARTS-env/CARTS/.install/arts/lib -larts -L/usr/lib64/librt.so/usr/lib64/libpthread.so/usr/lib64/lib -lrdmacm


clean:
	rm -f *.bc *.ll taskwait_opt ;
