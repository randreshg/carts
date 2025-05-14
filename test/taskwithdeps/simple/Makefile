
ARTS_PATH=/home/randres/projects/carts/.install/arts
CARTS_PATH=/home/randres/projects/carts/.install/carts
CLANG_INCLUDE_PATH=/usr/lib/llvm-14/lib/clang/14.0.0/include
FILENAME=taskwithdeps
SHELL = /bin/bash

all: $(FILENAME)

# Generate MLIR std dialect from C file
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
