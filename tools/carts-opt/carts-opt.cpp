//===- polygeist-opt.cpp - The polygeist-opt driver -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the 'polygeist-opt' tool, which is the polygeist analog
// of mlir-opt, used to drive compiler passes, e.g. for testing.
//
//===----------------------------------------------------------------------===//

#include "mlir/Conversion/Passes.h"
#include "mlir/InitAllDialects.h"
#include "mlir/InitAllExtensions.h"
#include "mlir/InitAllPasses.h"
#include "mlir/InitAllTranslations.h"
#include "mlir/Target/LLVMIR/Dialect/All.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Async/IR/Async.h"
#include "mlir/Dialect/DLTI/DLTI.h"
#include "mlir/Dialect/Func/Extensions/InlinerExtension.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Target/LLVMIR/Dialect/All.h"
#include "mlir/Dialect/Math/IR/Math.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Pass/PassRegistry.h"
#include "mlir/Tools/mlir-opt/MlirOptMain.h"
#include "mlir/Transforms/Passes.h"

#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"

using namespace mlir;

int main(int argc, char **argv) {
  // Create a DialectRegistry and register all required dialects.
  mlir::DialectRegistry registry;

  registry.insert<mlir::affine::AffineDialect>();
  registry.insert<mlir::arith::ArithDialect>();
  registry.insert<mlir::async::AsyncDialect>();
  registry.insert<mlir::func::FuncDialect>();
  registry.insert<mlir::LLVM::LLVMDialect>();
  registry.insert<mlir::omp::OpenMPDialect>();
  registry.insert<mlir::math::MathDialect>();
  registry.insert<mlir::memref::MemRefDialect>();
  registry.insert<mlir::scf::SCFDialect>();
  registry.insert<DLTIDialect>();
  registry.insert<mlir::arts::ArtsDialect>();

  // Register all passes and translations.
  mlir::registerArtsPasses();
  // mlir::registerAllPasses();
  mlir::func::registerInlinerExtension(registry);

  // Run the MlirOptMain driver with the registered dialects and passes.
  return mlir::failed(mlir::MlirOptMain(
      argc, argv, "Polygeist Modular Optimizer Driver", registry));
}