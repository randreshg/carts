# LLVM OpenMP Metadata Tool
This project analyses a OpenMP C/C++ file's Abstract Syntax Tree (AST) and Intermediate Representation (IR) clang dumps to gather metadata and stores the OpenMP related metadata in an output IR file. 
## Usage:

 1. Extract AST and IR information from your source file:
    1. Extract AST file in text format. Requires O3. **Example:** `clang++ -c -O3 -fopenmp -Xclang -ast-dump example.cpp &> AST_FILE.txt`
	2.  Extract IR file in text format. Requires O0. **Example:** `clang++ -S -fopenmp -emit-llvm -g3 -O0 -fno-discard-value-names example.cpp -o output.ll`
	3. Extract IR file in bitcode format. Requires O0. Used for `opt`. **Example:** `clang++ -S -fopenmp -emit-llvm -g3 -O0 -fno-discard-value-names example.cpp -o output.bc`
	4. Note: Make sure the clang you are using supports OpenMP. 
	5. Note: Make sure you use `clang++` for C++ files, and `clang` for C files.
2. Set AST/IR paths in relevant header files:
	1. In `ast_parser.hpp`, set `INTERNAL_AST_PATH`to the AST path in 1.1).
	2. In `ir_parser.hpp`, set `INTERNAL_IR_PATH` to the IR (text) path in 1.2).
	3. In `omp_metadata.hpp`, set `GLOBAL_AST_PATH` and `GLOBAL_IR_PATH` to the same paths in 1.1) and 1.2) respectively. 
3. Add the the project to the LLVM pipeline. **(NOTE: LLVM 16 EXAMPLE.)**
    1. Go to `llvm_src/your_llvm_project/llvm/lib/Transforms`
	2. Create a new folder, say `OMP_PASS`.
	3. Still inside of `Transforms`, add to its `CMakeLists.txt` the line `add_subdirectory(OMP_PASS)`, or another 3.2) folder name of your choosing.
	4. Now `cd` into the new folder. Create a new `CMakeLists.txt` file with the contents:
	```
	add_llvm_library( <YourPassName> MODULE
		omp_metadata_pass.cpp
		omp_metadata.cpp
		ir_parser.cpp
		ast_parser.cpp
		
		PLUGIN_TOOL
		opt
		)
	```
	5. Now move all of the project files to this new folder. (This includes: `ast_parser.cpp`, `ast_parser.hpp`, `ir_parser.cpp`, `ir_parser.hpp`, `omp_metadata.cpp`, `omp_metadata.hpp`, and `omp_metadata_pass.cpp`.)
4.  Build new files added to LLVM pipeline. **Example:**   `cd` into `llvm_src/your_llvm_project/build` and run `cmake --build .`
5.  You should now see a shared object (`.so`) file called `<YourPassName>.so` inside of `llvm_src/your_llvm_project/build/lib`. 
6. Run the pass with `opt`. 
	1. Ensure you set an output IR (text) file so we don't lose metadata changes. 
	2. **Example**: `opt -load /path/to/<YourPassName>.so -OMP_PASS -enable-new-pm=0 -S /path/to/IR/bitcode/input.bc -o /desired/output/IR/file/path/output.ll`
	3. Note: Make sure you keep `-OMP_PASS`  consistent with the struct name inside of `omp_metadata_pass.cpp` when loading.
				