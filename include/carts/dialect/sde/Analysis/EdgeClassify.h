///==========================================================================///
/// File: EdgeClassify.h
///
/// SDE redistribution edge classification: decides the movement family (halo /
/// reduce-scatter / all-to-all / none) and owns the A1 same-owner re-tile
/// fail-closed gate. Carved verbatim from the correctness-base @782988ad1
/// RedistributionEdges analysis. The v4 @0b9338f07 removal of the EdgeClassify
/// enum + readerOnlyRetilesSameOwners + classifyRedistributionEdge is
/// deliberately NOT followed.
///==========================================================================///

#ifndef CARTS_DIALECT_SDE_ANALYSIS_EDGE_CLASSIFY_H
#define CARTS_DIALECT_SDE_ANALYSIS_EDGE_CLASSIFY_H

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/dialect/sde/IR/SdeDialect.h"

#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"

#include <cstdint>
#include <optional>

namespace mlir::carts::sde::redist {

/// The committed home (producer) layout of one array root.
struct HomeLayout {
  SdeSuIterateOp writer;
  ArrayLayoutKind layoutKind = ArrayLayoutKind::replicated;
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> blockShape;
};

enum class EdgeClassify {
  None,
  Halo,
  ReduceScatter,
  AllToAll,
};

std::optional<LayoutGraphFact> findLayoutFact(SdeSuIterateOp su,
                                              int64_t arrayId);

const ArrayAccessProfile *
findProfileForRoot(const ModuleSuAccessRelations &relations, Value root);

std::optional<SmallVector<int64_t, 4>>
getCommittedHaloShape(SdeSuIterateOp reader);

bool sameOwnerDimSet(ArrayRef<int64_t> lhs, ArrayRef<int64_t> rhs);

ArrayRef<int64_t> committedBlockShape(const LayoutGraphFact &fact);

bool readerCommittedLayoutDisagreesWithHome(
    const std::optional<LayoutGraphFact> &readerFact, const HomeLayout &home);

bool readerOnlyRetilesSameOwners(
    const std::optional<LayoutGraphFact> &readerFact, const HomeLayout &home);

EdgeClassify classifyRedistributionEdge(
    const HomeLayout &home, const std::optional<LayoutGraphFact> &readerFact,
    bool hasOwnerReduction, bool committedContractionLayout,
    bool committedHaloLayout);

bool producerConsumerMuTypesDisagree(Value writerRoot, Value readerRoot);

bool readerNeedsRedistributionMovement(SdeSuIterateOp reader, int64_t arrayId,
                                       const HomeLayout &home,
                                       const ModuleSuAccessRelations &relations,
                                       std::optional<unsigned> readerSuId,
                                       Value root);

void collectReaderRedistributionCandidates(
    SdeSuIterateOp reader, llvm::SmallVector<int64_t, 4> &arrayIds);

} // namespace mlir::carts::sde::redist

#endif // CARTS_DIALECT_SDE_ANALYSIS_EDGE_CLASSIFY_H
