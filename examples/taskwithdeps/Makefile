
ARTS_PATH=/home/randres/projects/carts/.install/arts
CARTS_PATH=/home/randres/projects/carts/.install/carts
FILENAME=taskwithdeps

all: $(FILENAME)_transformed.ll

# Generate LLVM IR from C++ source file
%.ll: %.cpp
	clang++ -fopenmp $< -Xclang -plugin -Xclang omp-plugin \
		-fplugin=$(CARTS_PATH)/lib/libOpenMPPlugin.so -mllvm \
		-debug-only=omp-plugin -S
	clang++ -O0 -g3 $*_annotaded.cpp -S -emit-llvm -o $@

%.ll: %.c
	clang -fopenmp $< -Xclang -plugin -Xclang omp-plugin \
		-fplugin=$(CARTS_PATH)/lib/libOpenMPPlugin.so -mllvm \
		-debug-only=omp-plugin -S
	clang -O0 -g3 $*_annotaded.c -S -emit-llvm -o $@

# Run ARTS Transform pass
$(FILENAME)_transformed.ll: $(FILENAME).ll
	opt -load-pass-plugin=$(CARTS_PATH)/lib/ARTSTransform.so \
		-debug-only=arts-transform,arts,carts,arts-ir-builder\
		-passes="arts-transform" $< -o $@
	# -debug-only=arts-transform,arts,carts,arts-ir-builder,arts-utils\

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
	rm -f *.bc *.ll *_transformed.cpp taskwithdeps_opt ;
