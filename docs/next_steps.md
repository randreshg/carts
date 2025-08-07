# Next Steps

This document outlines a set of tasks for future development and potential enhancements for the CARTS project.

## High Priority Tasks

*   **Expanded OpenMP Support**: Improve the coverage of the OpenMP standard, including support for more directives and clauses.
*   **Performance Optimization**: Profile and optimize the CARTS compiler and the generated code to improve performance.
*   **Pass Pipeline Alignment**: The pass pipeline described in the documentation is more detailed than the current implementation. The following tasks are needed to align the implementation with the documentation:
    *   **Clarify `EdtPass`**: The `EdtPass` in the documentation is a general pass, but the implementation has `EdtInvariantCodeMotion.cpp` and `EdtPtrRematerialization.cpp`. These should be consolidated into a single `EdtPass`.
    *   **Clarify `DBPass`**: The `DBPass` in the documentation is a general pass, but the implementation has `Db.cpp`. The functionality in `Db.cpp` should be expanded to include all the optimizations described in the documentation (shrinking, reusing, hoisting).
    *   **Align Naming**: The pass names in the code should be aligned with the documentation. For example, `ConvertDbToOpaquePtr.cpp` should be renamed to `PreprocessDbsPass.cpp`.

## Medium Priority Tasks

*   **More Examples**: Add more examples to the `examples/` directory to demonstrate the capabilities of CARTS.
*   **CI/CD Integration**: Set up a continuous integration and continuous deployment (CI/CD) pipeline to automate the build and test process.
*   **Documentation**: Continue to improve and expand the project documentation.

## Low Priority Tasks
*   **Expanded OpenMP Support**: Improve the coverage of the OpenMP standard, including support for more directives and clauses. This would require probably to modify polygeist
*   **Code Cleanup**: Refactor and clean up the codebase to improve readability and maintainability.
*   **Benchmarking**: Expand the benchmarking suite to cover a wider range of applications and use cases.

## Potential Enhancements

*   **Visualizer**: Create a tool to visualize the ARTS task graph, which would help with debugging and performance analysis.
*   **Auto-tuning**: Implement an auto-tuning framework to automatically find the best compiler and runtime settings for a given application.
*   **New Dialects**: Explore the possibility of adding new MLIR dialects to CARTS to support other programming models or hardware architectures.

[Go back to README.md](../README.md)
