
ARTS_PATH?=home/randres/projects/carts/.install/arts
CLANG_INCLUDE_PATH=/usr/lib/llvm-14/lib/clang/14.0.0/include
FILENAME=simple
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
	clang $< -O3 -g0 -fopenmp -march=native -o $@_inspector -I/$(ARTS_PATH)/include/arts -L$(ARTS_PATH)/lib -lartsinspector -lrt -lpthread -lrdmacm

clean:
	rm -r -f *.mlir *.ll *.dot ./$(FILENAME) ./$(FILENAME)_omp ./$(FILENAME)_inspector ./counters/;  
