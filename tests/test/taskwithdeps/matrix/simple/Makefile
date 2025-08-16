
ARTS_PATH=/home/randres/projects/carts/.install/arts
CARTS_PATH=/home/randres/projects/carts/.install/carts
CLANG_INCLUDE_PATH=/usr/lib/llvm-14/lib/clang/14.0.0/include
FILENAME=matrix
SHELL = /bin/bash

all: $(FILENAME)

# Generate MLIR std dialect from C/C++ file
%.mlir: %.cpp
	cgeist -std=c++17 $< -fopenmp -O0 -S -I/${CLANG_INCLUDE_PATH} > $@

%.mlir: %.c
	cgeist $< -fopenmp -O0 -S -I/${CLANG_INCLUDE_PATH} > $@

# Generate LLVM IR from MLIR std dialect
%.ll: %.mlir
	carts $< -O3 -emit-llvm -o $@

# Generate binary	
$(FILENAME): $(FILENAME).ll
	clang $< -O3 -g0 -march=native -o $@ -I/$(ARTS_PATH)/include/arts -L$(ARTS_PATH)/lib -larts -L/usr/lib64/librt.so/usr/lib64/libpthread.so/usr/lib64/lib -lrdmacm

clean:
	rm -f *.mlir *.ll ./$(FILENAME);

# -debug-only=create-datablocks,convert-arts-to-llvm,convert-polygeist-to-llvm
# carts-opt output.mlir --cse --canonicalize --raise-scf-to-affine --canonicalize --affine-cfg --affine-expand-index-ops --affine-scalrep --affine-cfg --loop-invariant-code-motion --cse --canonicalize --lower-affine --cse --canonicalize --convert-polygeist-to-llvm --cse --canonicalize &>output_llvm.mlir