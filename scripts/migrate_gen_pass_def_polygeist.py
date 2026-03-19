#!/usr/bin/env python3
"""Migrate GEN_PASS_CLASSES → GEN_PASS_DEF_* for Polygeist passes."""
import re
import os

# All pass names from polygeist/Passes/Passes.td
PASS_NAMES = [
    "AffineCFG", "AffineReduction", "CollectKernelStatistics",
    "ConvertCudaRTtoCPU", "ConvertCudaRTtoGPU", "ConvertCudaRTtoHipRT",
    "ConvertParallelToGPU1", "ConvertParallelToGPU2",
    "ConvertPolygeistToLLVM", "ConvertToOpaquePtrPass",
    "FixGPUFunc", "ForBreakToWhile", "InnerSerialization",
    "LoopRestructure", "LowerAlternatives", "MergeGPUModulesPass",
    "OpenMPOptPass", "ParallelLICM", "ParallelLower",
    "PolygeistCanonicalize", "PolygeistMem2Reg", "PolyhedralOpt",
    "RemoveTrivialUse", "SCFBarrierRemovalContinuation",
    "SCFCPUify", "SCFCanonicalizeFor", "SCFParallelLoopUnroll",
    "SCFRaiseToAffine", "Serialization",
    # Also check for old-style names that might appear as base classes
    "SCFParallelLoopDistribute",
]

PASS_UPPER = {name: name.upper() for name in PASS_NAMES}


def find_passes_in_file(text):
    found = []
    for name in PASS_NAMES:
        if re.search(rf'\b{name}Base<', text):
            found.append(name)
    return found


def process_file(filepath):
    with open(filepath, 'r') as f:
        text = f.read()

    if '#include "PassDetails.h"' not in text:
        return False

    passes = find_passes_in_file(text)
    if not passes:
        print(f'  WARNING: {filepath} has no PassNameBase<> — doing header swap only')
        defines = '// No GEN_PASS_DEF needed — uses PassWrapper'
    else:
        defines = '\n'.join(f'#define GEN_PASS_DEF_{PASS_UPPER[p]}' for p in passes)

    replacement = f"""{defines}
#include "mlir/Pass/Pass.h"
#include "polygeist/Ops.h"
#include "polygeist/Passes/Passes.h"
#include "polygeist/Passes/Passes.h.inc\""""

    text = text.replace('#include "PassDetails.h"', replacement)

    for name in passes:
        # polygeist::PassNameBase< → polygeist::impl::PassNameBase<
        text = re.sub(
            rf'\bpolygeist::(?!impl::){name}Base<',
            f'polygeist::impl::{name}Base<',
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

    print(f'Updated: {filepath} (passes: {", ".join(passes) if passes else "none"})')
    return True


def main():
    base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    pass_dir = os.path.join(base, 'external', 'Polygeist', 'lib', 'polygeist', 'Passes')

    total = 0
    for fn in sorted(os.listdir(pass_dir)):
        if fn.endswith('.cpp') or fn.endswith('.h'):
            filepath = os.path.join(pass_dir, fn)
            if process_file(filepath):
                total += 1

    print(f'\nTotal files updated: {total}')


if __name__ == '__main__':
    main()
