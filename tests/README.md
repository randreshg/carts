# CARTS Test Infrastructure

This directory contains the test infrastructure for the CARTS compiler framework using LLVM/MLIR lit-style testing.

## Directory Structure

The test structure mirrors the main library organization:

```
tests/
├── lit.cfg.py              # Main lit configuration
├── arts/                   # ARTS-specific tests (mirrors lib/arts/)
│   ├── Analysis/           # Analysis pass tests
│   │   ├── Db/            # DataBlock analysis tests
│   │   ├── Edt/           # EDT analysis tests
│   │   └── Graphs/        # Graph analysis tests
│   ├── Passes/            # Compiler pass tests
│   │   ├── ArtsInliner.mlir
│   │   ├── ConvertOpenMPToArts.mlir
│   │   ├── ConvertArtsToLLVM.mlir
│   │   ├── CreateDbs.mlir
│   │   ├── Db.mlir
│   │   ├── Edt.mlir
│   │   ├── CreateEpochs.mlir
│   │   ├── ConvertDbToOpaquePtr.mlir
│   │   └── EdtLowering.mlir
│   ├── Transforms/        # Transform pass tests
│   │   ├── EdtInvariantCodeMotion.mlir
│   │   └── EdtPtrRematerialization.mlir
│   ├── Codegen/           # Code generation tests
│   ├── Utils/             # Utility tests
│   └── end-to-end-pipeline.mlir  # Complete pipeline test
└── README.md              # This file
```

## Running Tests

### All Tests
```bash
carts check                    # Run all tests
carts check -v                # Run with verbose output
```

### Specific Test Suites
```bash
carts check --suite arts      # Run only ARTS tests
carts check --suite all       # Run all test suites (default)
```

### Using CMake
```bash
make check                     # Run all tests
make check-arts               # Run ARTS tests only
make check-verbose            # Run tests with verbose output
```

### Direct llvm-lit Usage
```bash
llvm-lit -v tests/            # All tests
llvm-lit -v tests/arts/       # ARTS tests only
llvm-lit -v tests/arts/Passes/  # Pass tests only
```

## Test Format

Tests use the standard MLIR/LLVM lit format:

```mlir
// RUN: %carts opt %s --pass-name | %filecheck %s --check-prefix=PREFIX

// Test description

module {
  // Test MLIR code
}

// PREFIX: expected-pattern
// PREFIX-NOT: unwanted-pattern
```

## Tool Substitutions

The following substitutions are available in tests:

- `%carts` → `tools/carts` (main wrapper script)
- `%mlir_translate` → `.install/llvm/bin/mlir-translate`
- `%filecheck` → `.install/llvm/bin/FileCheck`
- `%mlir_opt` → `.install/llvm/bin/mlir-opt`

## Pass Names Reference

| Pass Name | Test Location | Description |
|-----------|---------------|-------------|
| `arts-inliner` | `arts/Passes/ArtsInliner.mlir` | Function inlining |
| `convert-openmp-to-arts` | `arts/Passes/ConvertOpenMPToArts.mlir` | OpenMP → ARTS conversion |
| `edt` | `arts/Passes/Edt.mlir` | EDT optimization |
| `create-dbs` | `arts/Passes/CreateDbs.mlir` | DataBlock creation |
| `db` | `arts/Passes/Db.mlir` | DataBlock optimization |
| `create-epochs` | `arts/Passes/CreateEpochs.mlir` | Epoch synchronization |
| `convert-db-to-opaque-ptr` | `arts/Passes/ConvertDbToOpaquePtr.mlir` | DB → opaque pointer |
| `edt-lowering` | `arts/Passes/EdtLowering.mlir` | EDT outlining |
| `convert-arts-to-llvm` | `arts/Passes/ConvertArtsToLLVM.mlir` | ARTS → LLVM conversion |
| `edt-invariant-code-motion` | `arts/Transforms/EdtInvariantCodeMotion.mlir` | Loop invariant code motion |
| `edt-ptr-rematerialization` | `arts/Transforms/EdtPtrRematerialization.mlir` | Pointer rematerialization |

## Requirements

- CARTS must be built: `carts build`
- LLVM tools must be installed in `.install/llvm/bin/`
- FileCheck and llvm-lit must be available

## Adding New Tests

1. **For Pass Tests**: Create `.mlir` file in appropriate `arts/*/` directory
2. **For Analysis Tests**: Add to `arts/Analysis/*/` directory
3. **For Integration Tests**: Add to `arts/` directory
4. **Test Naming**: Use the pass filename (e.g., `Edt.mlir` for the EDT pass)

### Test Template
```mlir
// RUN: %carts opt %s --your-pass-name | %filecheck %s --check-prefix=YOUR_PREFIX

// Brief description of what this test verifies

module {
  func.func @test_function(%arg0: memref<10xi32>) {
    // Minimal test case that exercises the pass
    return
  }
}

// YOUR_PREFIX: expected output pattern
// YOUR_PREFIX-NOT: pattern that should not appear
```

## Best Practices

1. **Keep tests minimal** - Focus on the essential functionality
2. **Use stable CHECK patterns** - Avoid full IR dumps, match specific anchors
3. **Test both positive and negative cases** when relevant
4. **Follow naming conventions** - Match source file names
5. **Include comprehensive comments** - Explain what's being tested
6. **Use appropriate CHECK prefixes** - Make them descriptive (e.g., CONVERT, EDT, DB_OPT)

## Troubleshooting

### Common Issues
- **Tools not found**: Run `carts build` to install all required tools
- **Tests fail to run**: Check that `.install/llvm/bin/llvm-lit` exists
- **Wrong pass names**: Refer to `include/arts/Passes/ArtsPasses.td` for exact names
- **Path issues**: Tests run from project root, paths should be relative to that

### Debug Tips
```bash
# Run a single test with verbose output
llvm-lit -v tests/arts/Passes/Edt.mlir

# Check tool substitutions
llvm-lit --show-substitutions tests/

# Run without FileCheck to see raw output
carts opt tests/arts/Passes/Edt.mlir --edt
```
