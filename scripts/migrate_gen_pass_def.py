#!/usr/bin/env python3
"""Migrate GEN_PASS_CLASSES → GEN_PASS_DEF_* for CARTS passes.

For each .cpp file that includes PassDetails.h:
1. Replace `#include "arts/passes/PassDetails.h"` with
   `#define GEN_PASS_DEF_PASSNAME` + minimal headers + `#include "arts/passes/Passes.h.inc"`
2. Replace `PassNameBase<` with `impl::PassNameBase<`
"""
import re
import os
import sys

# All pass names from Passes.td
PASS_NAMES = [
    "AliasScopeGen", "ArtsInliner", "BlockLoopStripMining", "CollectMetadata",
    "Concurrency", "ContractValidation", "ConvertArtsToLLVM",
    "ConvertOpenMPToArts", "CreateDbs", "CreateEpochs", "DataPtrHoisting",
    "DbDistributedOwnership", "DbLowering", "DbModeTightening",
    "DbPartitioning", "DbScratchElimination", "DbTransforms",
    "DeadCodeElimination", "DepTransforms", "DistributedHostLoopOutlining",
    "EdtAllocaSinking", "EdtDistribution", "EdtICM", "EdtLowering",
    "EdtPtrRematerialization", "EdtStructuralOpt", "EdtTransforms",
    "EpochLowering", "EpochOpt", "ForLowering", "ForOpt",
    "GuidRangCallOpt", "HandleDeps", "Hoisting", "KernelTransforms",
    "LoopFusion", "LoopNormalization", "LoopReordering",
    "LoopVectorizationHints", "LoweringContractCleanup",
    "ParallelEdtLowering", "PatternDiscovery",
    "RaiseMemRefDimensionality", "RuntimeCallOpt", "ScalarReplacement",
    "StencilBoundaryPeeling", "VerifyDbCreated", "VerifyForLowered",
    "VerifyLowered", "VerifyMetadata", "VerifyPreLowered",
]

# Build uppercase mapping: PassName -> PASSNAME
def to_upper(name):
    """Convert CamelCase to UPPERCASE for GEN_PASS_DEF."""
    return name.upper()

PASS_UPPER = {name: to_upper(name) for name in PASS_NAMES}

# PassDetails.h provides these headers — we need to preserve them
PASSDETAILS_HEADERS = [
    '#include "arts/Dialect.h"',
    '#include "arts/passes/Passes.h"',
    '#include "mlir/Dialect/Affine/IR/AffineOps.h"',
    '#include "mlir/Dialect/Func/IR/FuncOps.h"',
    '#include "mlir/Pass/Pass.h"',
]


def find_passes_in_file(text):
    """Find which pass names are defined in this file."""
    found = []
    for name in PASS_NAMES:
        # Look for struct PassNamePass : ... PassNameBase<
        if re.search(rf'\b{name}Base<', text):
            found.append(name)
    return found


def process_file(filepath):
    """Process a single .cpp file."""
    with open(filepath, 'r') as f:
        text = f.read()

    if '#include "arts/passes/PassDetails.h"' not in text:
        return False

    # Find which passes are in this file
    passes = find_passes_in_file(text)
    if not passes:
        print(f'  WARNING: {filepath} includes PassDetails.h but has no PassNameBase<> — skipping')
        return False

    # Build GEN_PASS_DEF defines
    defines = '\n'.join(f'#define GEN_PASS_DEF_{PASS_UPPER[p]}' for p in passes)

    # Replace PassDetails.h include with GEN_PASS_DEF + headers + Passes.h.inc
    replacement = f"""{defines}
#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "mlir/Pass/Pass.h"
#include "arts/passes/Passes.h.inc\""""

    text = text.replace(
        '#include "arts/passes/PassDetails.h"',
        replacement
    )

    # Replace PassNameBase< with impl::PassNameBase< (handle both arts:: and bare)
    for name in passes:
        # arts::PassNameBase< → arts::impl::PassNameBase<
        text = re.sub(
            rf'\barts::(?!impl::){name}Base<',
            f'arts::impl::{name}Base<',
            text
        )
        # Bare PassNameBase< (not preceded by :: or impl::)
        text = re.sub(
            rf'(?<!::)(?<!impl::)\b{name}Base<',
            f'impl::{name}Base<',
            text
        )

    with open(filepath, 'w') as f:
        f.write(text)

    print(f'Updated: {filepath} (passes: {", ".join(passes)})')
    return True


def main():
    base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    pass_dir = os.path.join(base, 'lib', 'arts', 'passes')

    total = 0
    for root, dirs, files in os.walk(pass_dir):
        for fn in files:
            if fn.endswith('.cpp'):
                filepath = os.path.join(root, fn)
                if process_file(filepath):
                    total += 1

    print(f'\nTotal files updated: {total}')


if __name__ == '__main__':
    main()
