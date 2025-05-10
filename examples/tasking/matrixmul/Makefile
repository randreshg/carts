
ARTS_PATH=/home/randres/projects/carts/.install/arts
CARTS_PATH=/home/randres/projects/carts/.install/carts
CLANG_INCLUDE_PATH=/usr/lib/llvm-14/lib/clang/14.0.0/include
FILENAME=matrixmul
SHELL = /bin/bash

all: $(FILENAME) $(FILENAME)_omp

# Generate MLIR std dialect from C/C++ file
%.mlir: %.cpp
	cgeist -std=c++17 $< -fopenmp -O0 -S -I/${CLANG_INCLUDE_PATH} > $@

%.mlir: %.c
	cgeist $< -fopenmp -O0 -S -I/${CLANG_INCLUDE_PATH} > $@

# Generate LLVM IR from MLIR std dialect
%.ll: %.mlir
	carts $< -O3 -emit-llvm -o $@

# Generate binary (CARTS version)
$(FILENAME): $(FILENAME).ll
	clang $< -O3 -g0 -fopenmp -march=native -o $@ -I/$(ARTS_PATH)/include -L$(ARTS_PATH)/lib -larts  -lrt -lpthread -lrdmacm 

# Generate binary (OpenMP version)
%_omp: %.cpp
	clang++ $< -O3 -fopenmp -g0 -march=native -o $@

%_omp: %.c
	clang $< -O3 -fopenmp -g0 -march=native -o $@

clean:
	rm -f *.mlir *.ll ./$(FILENAME) ./$(FILENAME)_omp  ;
