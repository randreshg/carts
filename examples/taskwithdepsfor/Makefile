
ARTS_PATH=/home/randres/projects/carts/.install/arts
CARTS_PATH=/home/randres/projects/carts/.install/carts
FILENAME=taskwithdeps

all: $(FILENAME)_opt

# Generate the ASTFile
%.bc: %.cpp
	clang++ -fopenmp -O1 -g3 -emit-ast -o $(FILENAME).ast $<
	clang++ -fopenmp -O1 -g3 -emit-llvm -c $< -o $@ -lstdc++
	llvm-dis $@

%.bc: %.c
	clang -fopenmp -O3 -g0 -emit-llvm -c $< -o $@
	llvm-dis $@

# Run OMP transform pass
# opt -load-pass-plugin=/home/randres/projects/carts/.install/carts/lib/OMPTransform.so -debug-only=omp-visitor,omp-transform,arts,carts,arts-ir-builder -passes="omp-transform" -ast-file=taskwithdeps.ast taskwithdeps.ll -o taskwithdeps_arts_ir.bc
$(FILENAME)_arts_ir.bc: $(FILENAME).bc
	opt -load-pass-plugin=$(CARTS_PATH)/lib/OMPTransform.so \
		-debug-only=omp-visitor,omp-transform,arts,carts,arts-ir-builder\
		-passes="omp-transform" -ast-file=$(FILENAME).ast $< -o $@
	# -debug-only=omp-transform,arts,carts,arts-ir-builder,arts-utils\
	llvm-dis $@

# Run the ARTS analysis pass
$(FILENAME)_arts_analysis.bc:  $(FILENAME)_arts_ir.bc
	opt -load-pass-plugin=$(CARTS_PATH)/lib/ARTSAnalysis.so \
			-debug-only=arts-analysis,arts,carts,arts-ir-builder,arts-graph,carts-metadata,arts-codegen\
			-passes="arts-analysis" $< -o $@
	llvm-dis $@


$(FILENAME)_opt.bc: $(FILENAME)_arts_analysis.bc
	opt -O3 $< -o $@
	llvm-dis $@

$(FILENAME)_opt: $(FILENAME)_opt.bc
	clang++ $< -O3 -g3 -march=native -o $@ -lstdc++ -I/$(ARTS_PATH)/include -L$(CARTS_PATH)/lib -larts -L/usr/lib64/librt.so/usr/lib64/libpthread.so/usr/lib64/lib -lrdmacm

clean:
	rm -f *.bc *.ll *.ast taskwithdeps_opt ;
