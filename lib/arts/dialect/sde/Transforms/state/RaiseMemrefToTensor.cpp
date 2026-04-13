///==========================================================================///
/// File: RaiseMemrefToTensor.cpp
///
/// Scaffold for the RaiseMemrefToTensor pass described in
/// docs/compiler/raise-memref-to-tensor-rfc.md. The v1 transform converts
/// `sde.cu_task` ops whose bodies read/write shared memrefs into
/// `sde.cu_codelet` ops consuming `!sde.token` operands, threading the
/// affected memrefs as tensor SSA through the enclosing SDE regions.
///
/// This file currently only carries the pass registration and a no-op
/// `runOnOperation` body so downstream agents can wire the transform in
/// without touching the pipeline plumbing.
///==========================================================================///

#include "arts/dialect/sde/Transforms/Passes.h"
namespace mlir::arts {
#define GEN_PASS_DEF_RAISEMEMREFTOTENSOR
#include "arts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::arts

using namespace mlir;
using namespace mlir::arts;

namespace {

struct RaiseMemrefToTensorPass
    : public arts::impl::RaiseMemrefToTensorBase<RaiseMemrefToTensorPass> {
  void runOnOperation() override {
    // TODO: v1 implementation — see RFC §5.
    return;
  }
};

} // namespace

namespace mlir::arts::sde {

std::unique_ptr<Pass> createRaiseMemrefToTensorPass() {
  return std::make_unique<RaiseMemrefToTensorPass>();
}

} // namespace mlir::arts::sde
