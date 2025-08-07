# Dependencies

CARTS relies on several key external projects to function. These dependencies are managed through the project's CMake build system.

## LLVM/MLIR

*   **Description**: The LLVM Project is a collection of modular and reusable compiler and toolchain technologies. MLIR (Multi-Level IR) is a sub-project of LLVM that provides a flexible and extensible intermediate representation (IR) for building compilers.
*   **Usage**: CARTS uses LLVM and MLIR as the foundation for its compiler infrastructure. The ARTS dialect is an extension of MLIR, and the project leverages MLIR's pass framework for transformations and optimizations. The final output of the CARTS compiler is LLVM IR.
*   **Location**: The build system expects to find LLVM/MLIR in `./.install/llvm`.

## Clang

*   **Description**: Clang is the C/C++/Objective-C compiler front-end for the LLVM project.
*   **Usage**: CARTS uses Clang to parse C/C++ source code and to compile the generated LLVM IR into executables.
*   **Location**: The build system expects to find Clang in `./.install/llvm`.

## Polygeist

*   **Description**: Polygeist is a tool that raises C/C++ code to MLIR, enabling MLIR-based compilation of C/C++ code.
*   **Usage**: CARTS uses Polygeist to convert C/C++ code (including OpenMP directives) into the MLIR representation. This is the first step in the CARTS compilation pipeline.
*   **Location**: The build system expects to find Polygeist in `./.install/polygeist`.

## ARTS (Asynchronous Runtime System)

*   **Description**: ARTS is a lightweight, asynchronous runtime system designed for task-based parallelism on distributed and memory-disaggregated architectures.
*   **Usage**: CARTS is the compiler for the ARTS runtime. The ARTS dialect in CARTS maps directly to the concepts and functionalities of the ARTS runtime. The compiled executables are linked against the ARTS runtime library.
*   **Location**: The ARTS runtime is located in `./external/arts` and is built into `./.install/arts`.

[Go back to README.md](../README.md)