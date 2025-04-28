///==========================================================================
/// File: DataBlockAnalysis.cpp
///==========================================================================

#include "arts/Analysis/DataBlockAnalysis.h"
/// Dialects
#include "arts/Analysis/LoopAnalysis.h"
#include "mlir/Dialect/Affine/Analysis/LoopAnalysis.h"
#include "mlir/Dialect/Affine/Analysis/Utils.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Transforms/DialectConversion.h"
#include "polygeist/Ops.h"
/// Arts
#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ArtsUtils.h"
/// Other
#include "mlir/IR/Dominance.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
/// Debug
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include <csignal>
#include <utility>
#define DEBUG_TYPE "datablock-analysis"
#define line "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arith;
using namespace mlir::polygeist;
using namespace mlir::scf;
using namespace mlir::affine;
using namespace mlir::arts;

//===----------------------------------------------------------------------===//
// DatablockNode
//===----------------------------------------------------------------------===//
DatablockNode::DatablockNode(DatablockGraph *DG, arts::DataBlockOp dbOp)
    : op(dbOp), DG(DG), DA(DG->DA) {
  if (!op)
    return;
  collectInfo();
}

void DatablockNode::collectInfo() {
  id = DG->nodes.size();
  mode = op.getMode();
  ptr = op.getPtr();
  indices = op.getIndices();
  sizes = op.getSizes();
  elementType = op.getElementType();
  elementTypeSize = op.getElementTypeSize();
  resultType = op.getResult().getType();

  /// Add 'hasSingleSize' attribute if the datablock has a single size of 1 or
  /// no size
  hasSingleSize = false;
  if (sizes.empty()) {
    op.setHasSingleSize();
    hasSingleSize = true;
  } else if (sizes.size() == 1) {
    if (auto cstOp = sizes[0].getDefiningOp<arith::ConstantIndexOp>()) {
      if (cstOp.value() == 1) {
        op.setHasSingleSize();
        hasSingleSize = true;
      }
    }
  }

  /// Add 'hasPtrDb' attribute if the base is a datablock.
  if (auto parentDb =
          dyn_cast_or_null<arts::DataBlockOp>(ptr.getDefiningOp())) {
    op.setHasPtrDb();
    hasPtrDb = true;
    parent = DG->getNode(parentDb);
    for (auto &eff : parent->effects)
      effects.push_back(eff);

    hasGuid = (!parent->hasSingleSize || indices.empty());
    if (hasGuid)
      op.setHasGuid();
  } else {
    hasPtrDb = false;
    /// Collect memory effects if the parent is not a datablock.
    if (auto memEff = dyn_cast<MemoryEffectOpInterface>(op.getOperation()))
      memEff.getEffects(effects);
  }

  /// Try to get the EDT parent of the datablock.
  if (auto parentOp = op->getParentOfType<arts::EdtOp>())
    edtParent = parentOp;

  /// Collect and analyze uses of the datablock.
  collectUses();

  /// Compute the region of the datablock.
  // computeRegion();
}

void DatablockNode::collectUses() {
  unsigned numUses = 0;
  /// If the only user is an EDT, it is because the datablock hasn't been
  /// rewired yet.
  if (op->hasOneUse() && isa<arts::EdtOp>(op->use_begin()->getOwner())) {
    llvm::errs() << "Datablock " << op << " hasn't been rewired yet\n";
    assert(false && "Datablock not rewired");
    return;
  }

  /// Analyze uses
  loadsAndStores.reserve(std::distance(op->user_begin(), op->user_end()));

  for (auto user : op->getUsers()) {
    numUses++;

    /// Loads/stores
    if (isa<memref::LoadOp, memref::StoreOp>(user)) {
      loadsAndStores.push_back(user);
      continue;
    }

    /// EDT user check
    if (auto edtOp = dyn_cast<arts::EdtOp>(user)) {
      if (edtUser) {
        llvm::errs() << "Datablock " << op
                     << " has multiple EDT users: " << *edtUser << "\n";
        llvm_unreachable("The datablock has multiple EDT users");
      }

      edtUser = edtOp;
      auto deps = edtUser.getDependencies();
      for (unsigned i = 0; i < deps.size(); i++) {
        if (deps[i].getDefiningOp() == op) {
          userEdtPos = i;
          break;
        }
      }
    }
  }
  useCount = numUses;
}

Value DatablockNode::createOffsetSymbolicValue(Value sym, int64_t offset,
                                               Operation *contextOp) {
  // OpBuilder builder(contextOp);
  // auto offsetConst = builder.create<arith::ConstantIndexOp>(
  //     contextOp->getLoc(), builder.getIndexAttr(offset));
  // return builder.create<arith::AddIOp>(contextOp->getLoc(), sym, offsetConst);
  return nullptr; // Placeholder for actual implementation.
}

void DatablockNode::analyzeIndexValue(Value indexVal, Operation *contextOp,
                              std::optional<int64_t> &dimMin,
                              std::optional<int64_t> &dimMax,
                              SmallVector<Value> &symbolicBounds) {
  /// Ignore null values if they somehow appear.
  if (!indexVal)
    return;

  /// Case 1: Constant index.
  if (auto constOp = indexVal.getDefiningOp<arith::ConstantOp>()) {
    if (auto intAttr = constOp.getValue().dyn_cast<IntegerAttr>()) {
      if (constOp.getType().isa<IndexType>() ||
          intAttr.getType().isSignlessInteger()) {
        int64_t c = intAttr.getInt();
        /// Update min bound: take the minimum of current min and c.
        dimMin = dimMin.has_value() ? std::min(dimMin.value(), c) : c;
        /// Update max bound: take the maximum of current max and c+1 (exclusive
        /// bound).
        dimMax = dimMax.has_value() ? std::max(dimMax.value(), c + 1) : c + 1;
      }
      // else: Non-integer constant, treat as symbolic? For now, ignore.
    }
    return; // Handled constant case.
  }

  /// Case 2: Loop induction variable (BlockArgument of a loop).
  if (auto blockArg = indexVal.dyn_cast<BlockArgument>()) {
    if (auto forOp =
            dyn_cast_or_null<scf::ForOp>(blockArg.getOwner()->getParentOp())) {
      /// If the index is the induction variable of an scf.for loop,
      /// analyze the loop bounds.
      if (blockArg == forOp.getInductionVar()) {
        auto processLoopBound = [&](Value bound, bool isLower) {
          if (auto boundConst = bound.getDefiningOp<arith::ConstantIndexOp>()) {
            int64_t val = boundConst.value();
            if (isLower) {
              dimMin = dimMin.has_value() ? std::min(dimMin.value(), val) : val;
            } else {
              /// Upper bound in scf.for is exclusive.
              dimMax = dimMax.has_value() ? std::max(dimMax.value(), val) : val;
            }
          } else {
            /// If bound is not constant, add it as a symbolic bound.
            symbolicBounds.push_back(bound);
          }
        };

        processLoopBound(forOp.getLowerBound(), /*isLower=*/true);
        processLoopBound(forOp.getUpperBound(), /*isLower=*/false);
        /// Note: Step is ignored for basic range analysis.
        return; // Handled loop IV case.
      }
    }
    /// Fallthrough: Other block arguments (e.g., function args, region args)
    /// are treated as symbolic.
    symbolicBounds.push_back(indexVal);
    return;
  }

  /// Case 3: Index Cast operation. Analyze the source of the cast.
  if (auto castOp = indexVal.getDefiningOp<arith::IndexCastOp>()) {
    /// Recursively analyze the source value before the cast.
    analyzeIndexValue(castOp.getIn(), contextOp, dimMin, dimMax,
                      symbolicBounds);
    return; // Handled cast case.
  }

  /// Case 4: Add operation (potentially `base + constant_offset`).
  if (auto addOp = indexVal.getDefiningOp<arith::AddIOp>()) {
    Value baseVal;
    std::optional<int64_t> offsetOpt;

    /// Helper to extract a constant integer value.
    auto getConstantValue = [](Value v) -> std::optional<int64_t> {
      if (auto constOp = v.getDefiningOp<arith::ConstantIndexOp>())
        return constOp.value();
      return std::nullopt;
    };

    /// Check if one operand is a constant offset.
    if (auto lhsConst = getConstantValue(addOp.getLhs())) {
      offsetOpt = lhsConst;
      baseVal = addOp.getRhs();
    } else if (auto rhsConst = getConstantValue(addOp.getRhs())) {
      offsetOpt = rhsConst;
      baseVal = addOp.getLhs();
    }

    /// If we found `base + offset` structure:
    if (baseVal && offsetOpt.has_value()) {
      int64_t offset = offsetOpt.value();
      /// Recursively analyze the base value.
      std::optional<int64_t> baseMin, baseMax;
      SmallVector<Value> baseSymbolic;
      analyzeIndexValue(baseVal, contextOp, baseMin, baseMax, baseSymbolic);

      /// Apply the offset to the constant bounds found for the base.
      if (baseMin) {
        int64_t adjustedMin = baseMin.value() + offset;
        dimMin = dimMin.has_value() ? std::min(dimMin.value(), adjustedMin)
                                    : adjustedMin;
      }
      if (baseMax) {
        int64_t adjustedMax = baseMax.value() + offset;
        dimMax = dimMax.has_value() ? std::max(dimMax.value(), adjustedMax)
                                    : adjustedMax;
      }

      /// Apply the offset to the symbolic bounds found for the base.
      for (Value sym : baseSymbolic) {
        symbolicBounds.push_back(
            offset != 0 ? createOffsetSymbolicValue(sym, offset, contextOp)
                        : sym);
      }
      return; // Handled AddI case.
    }
    /// Fallthrough: If AddI is not `base + constant`, treat result as symbolic.
  }

  /// Default Case: Treat any other operation/value as symbolic.
  /// This includes function calls, complex arithmetic, affine applies, etc.
  symbolicBounds.push_back(indexVal);
}

bool DatablockNode::computeRegion() {
  /// If there are no known loads or stores, we cannot determine a region.
  if (loadsAndStores.empty()) {
    LLVM_DEBUG(DBGS() << "No loads/stores found for DB #" << id
                      << ", cannot compute access region.\n");
    return false;
  }

  /// Initialize analysis structures based on the datablock's rank (number of
  /// dimensions).
  auto rank = sizes.size(); /// Assuming sizes corresponds to rank.
  /// Optional constant bounds [min, max) for each dimension.
  SmallVector<std::optional<int64_t>> dimMins(rank, std::nullopt);
  SmallVector<std::optional<int64_t>> dimMaxs(rank, std::nullopt);
  /// Symbolic bounds for each dimension. Using SetVector to avoid duplicates.
  SmallVector<SetVector<Value>> symbolicBoundsPerDim(rank);

  LLVM_DEBUG(DBGS() << "Computing access region for DB #" << id
                    << " (rank=" << rank << ") with " << loadsAndStores.size()
                    << " loads/stores.\n");

  /// Analyze indices for all load and store operations using this datablock.
  for (Operation *op : loadsAndStores) {
    ValueRange indices;
    if (auto loadOp = dyn_cast<memref::LoadOp>(op)) {
      indices = loadOp.getIndices();
    } else if (auto storeOp = dyn_cast<memref::StoreOp>(op)) {
      indices = storeOp.getIndices();
    } else {
      LLVM_DEBUG(DBGS() << "Warning: Unexpected operation type in "
                           "loadsAndStores for DB #"
                        << id << ": " << *op << "\n");
      continue;
    }

    /// Verify that the number of indices matches the expected rank.
    if (indices.size() != rank) {
      LLVM_DEBUG(DBGS() << "Warning: Mismatched rank in memory operation at "
                        << op->getLoc() << ". Expected " << rank << ", got "
                        << indices.size() << " indices. Op: " << *op << "\n");
      /// Skip this operation or attempt partial analysis? Skipping for now.
      continue;
    }

    /// Analyze each index dimension for this memory operation.
    for (unsigned dim = 0; dim < rank; ++dim) {
      LLVM_DEBUG(DBGS() << "  Analyzing dim " << dim
                        << ", index: " << indices[dim] << "\n");
      SmallVector<Value> currentSymbolicBounds;
      /// analyzeIndexValue updates dimMins[dim], dimMaxs[dim] and fills
      /// currentSymbolicBounds
      analyzeIndexValue(indices[dim], op, dimMins[dim], dimMaxs[dim],
                        currentSymbolicBounds);
      /// Add unique symbolic bounds found for this access to the dimension's
      /// set.
      for (Value sym : currentSymbolicBounds) {
        symbolicBoundsPerDim[dim].insert(sym);
      }
    }
  }

  // /// Store the computed bounds in the node's members.
  // accessRegionMins = std::move(dimMins);
  // accessRegionMaxs = std::move(dimMaxs);
  // /// Convert SetVectors to SmallVectors for storage.
  // accessRegionSymbols.resize(rank);
  // for (unsigned dim = 0; dim < rank; ++dim) {
  //   accessRegionSymbols[dim].assign(symbolicBoundsPerDim[dim].begin(),
  //                                   symbolicBoundsPerDim[dim].end());
  // }

  // LLVM_DEBUG({
  //   DBGS() << "Finished computing access region analysis for DB #" << id
  //          << ":\n";
  //   for (unsigned dim = 0; dim < rank; ++dim) {
  //     dbgs() << "  Dim " << dim << ": Min=";
  //     if (accessRegionMins[dim])
  //       dbgs() << *accessRegionMins[dim];
  //     else
  //       dbgs() << "N/A";
  //     dbgs() << ", Max="; // Max is exclusive
  //     if (accessRegionMaxs[dim])
  //       dbgs() << *accessRegionMaxs[dim];
  //     else
  //       dbgs() << "N/A";
  //     dbgs() << ", Symbolic=[";
  //     llvm::interleaveComma(accessRegionSymbols[dim], dbgs());
  //     dbgs() << "]\n";
  //   }
  // });

  // /// Create attribute representation of the bounds.
  // MLIRContext *context = op->getContext();
  // SmallVector<Attribute, 4> computedDomains;
  // computedDomains.reserve(rank);

  // for (unsigned dim = 0; dim < rank; ++dim) {
  //   SmallVector<NamedAttribute> dimAttrs;

  //   /// --- Constant Lower Bound ---
  //   /// Store the minimum constant value observed for this dimension's index.
  //   if (accessRegionMins[dim].has_value()) {
  //     dimAttrs.push_back(
  //         NamedAttribute(StringAttr::get(context, "min_const"),
  //                        IntegerAttr::get(IndexType::get(context),
  //                                         accessRegionMins[dim].value())));
  //   }
  //   /// If no constant lower bound is found, we don't add the attribute.
  //   /// A consumer of this attribute should handle its absence (e.g., assume 0
  //   /// or unbounded).

  //   /// --- Constant Upper Bound (Exclusive) ---
  //   /// Store the maximum constant value observed + 1 for this dimension's
  //   /// index.
  //   if (accessRegionMaxs[dim].has_value()) {
  //     dimAttrs.push_back(
  //         NamedAttribute(StringAttr::get(context, "max_exclusive_const"),
  //                        IntegerAttr::get(IndexType::get(context),
  //                                         accessRegionMaxs[dim].value())));
  //   }
  //   /// If no constant upper bound is found, we don't add the attribute.
  //   /// A consumer should handle its absence (e.g., assume unbounded or use
  //   /// datablock size).

  //   /// --- Symbolic Bounds ---
  //   /// This analysis collects all symbolic values (non-constants, complex
  //   /// expressions) that influence the index calculation for this dimension
  //   /// across all accesses. It does not currently perform advanced symbolic
  //   /// simplification or determine precise symbolic min/max expressions (e.g.,
  //   /// using affine analysis). It simply lists the contributing symbolic
  //   /// factors.
  //   if (!accessRegionSymbols[dim].empty()) {
  //     SmallVector<Attribute> symAttrs;
  //     symAttrs.reserve(accessRegionSymbols[dim].size());
  //     for (Value symVal : accessRegionSymbols[dim]) {
  //       /// Representing the Value itself directly in an attribute is not
  //       /// standard. Using its string representation provides debuggability but
  //       /// limited machine processability.
  //       std::string symStr;
  //       llvm::raw_string_ostream ss(symStr);
  //       ss << symVal; /// Includes type, potentially defining op location.
  //       symAttrs.push_back(StringAttr::get(context, ss.str()));
  //       /// Future Work: Could attempt to use FlatSymbolRefAttr if the Value
  //       /// corresponds to a named entity.
  //       /// Future Work: Integrate affine analysis to store AffineExprAttr or
  //       /// AffineMapAttr for more precise symbolic bounds derived from loops
  //       /// etc.
  //     }
  //     dimAttrs.push_back(
  //         NamedAttribute(StringAttr::get(context, "symbolic_influences"),
  //                        ArrayAttr::get(context, symAttrs)));
  //   }

  //   /// Create dictionary {min_const: ..., max_exclusive_const: ...,
  //   /// symbolic_influences: [...]} for this dimension.
  //   computedDomains.push_back(DictionaryAttr::get(context, dimAttrs));
  // }

  // /// Store the computed domain attributes array in the node.
  // /// Assumes `accessRegionDomainAttrs` is a member of DatablockNode of type
  // /// ArrayAttr.
  // accessRegionDomainAttrs = ArrayAttr::get(context, computedDomains);

  // LLVM_DEBUG({
  //   DBGS() << "Stored access region domain attributes for DB #" << id << ": "
  //          << accessRegionDomainAttrs << "\n";
  // });

  // return true; /// Indicate analysis was performed and results (potentially
  //              /// partial) are available.
}

//===----------------------------------------------------------------------===//
// DatablockGraph
//===----------------------------------------------------------------------===//
DatablockGraph::DatablockGraph(func::FuncOp func, DatablockAnalysis *DA)
    : func(func), DA(DA) {
  assert(func && "Function cannot be null");
  assert(DA && "DatablockAnalysis cannot be null");

  /// Create entry Node:
  {
    entryDbNode = new DatablockNode(this, nullptr);
    entryDbNode->id = 0;
    entryDbNode->ptr = nullptr;
    nodes.push_back(entryDbNode);
  }

  collectNodes(func.getBody());
  if (nodes.empty())
    return;

  /// Build the datablock graph from the function body.
  build();
}

SetVector<unsigned> DatablockGraph::getProducers() const {
  SetVector<unsigned> producers;
  for (auto entry : edges)
    producers.insert(entry.first);
  return producers;
}

SetVector<unsigned> DatablockGraph::getConsumers(unsigned producerID) {
  return edges[producerID];
}

bool DatablockGraph::addEdge(DatablockNode &prod, DatablockNode &cons) {
  /// If an edge from prod to cons already exists, do nothing.
  auto prodIt = edges.find(prod.id);
  if (prodIt != edges.end()) {
    if (prodIt->second.count(cons.id))
      return false;
  }
  edges[prod.id].insert(cons.id);
  return true;
}

static int analysisDepth = 0;
struct IndentScope {
  IndentScope() { ++analysisDepth; }
  ~IndentScope() { --analysisDepth; }
};

void DatablockGraph::build() {
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "- Building datablock graph for function: "
                    << func.getName() << "\n");
  /// Process the function body region with the initial environment.
  Environment env;
  processRegion(func.getBody(), env);
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "Finished building datablock graph for function: "
                    << func.getName() << "\n");
}

std::pair<Environment, bool> DatablockGraph::processRegion(Region &region,
                                                           Environment &env) {
  Environment newEnv = env;
  bool changed = false;
  for (Operation &op : region.getOps()) {
    IndentScope scope;
    std::pair<Environment, bool> result;
    if (auto edtOp = dyn_cast<EdtOp>(&op))
      result = processEdt(edtOp, newEnv);
    else if (auto ifOp = dyn_cast<scf::IfOp>(&op))
      result = processIf(ifOp, newEnv);
    else if (auto forOp = dyn_cast<scf::ForOp>(&op))
      result = processFor(forOp, newEnv);
    else if (auto callOp = dyn_cast<func::CallOp>(&op))
      result = processCall(callOp, newEnv);
    else
      continue;

    /// Merge the new environment with the current one.
    newEnv = mergeEnvironments(newEnv, result.first);
    changed = changed || result.second;
  }
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << " - Finished processing region. Environment changed: "
                    << (changed ? "true" : "false") << "\n");
  return {newEnv, changed};
}

std::pair<Environment, bool> DatablockGraph::processEdt(arts::EdtOp edtOp,
                                                        Environment &env) {
  static int edtCount = 0;
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "- Processing EDT #" << edtCount++ << "\n");
  /// Process the inputs and outputs of the EDT.
  auto edtDeps = edtOp.getDependencies();
  bool changed = false;
  Environment newEnv = env;

  /// Handle input dependencies.
  if (!edtDeps.empty()) {
    LLVM_DEBUG({
      dbgs() << std::string(analysisDepth * 2, ' ')
             << " Initial environment: {";
      for (auto entry : newEnv) {
        dbgs() << "  #" << getNode(entry.first)->id << " -> #"
               << entry.second->id << ",";
      }
      dbgs() << "}\n";
    });

    /// Reserve dbIns and dbOuts for input and output datablocks.
    SmallVector<DatablockNode *, 4> dbIns, dbOuts;
    dbIns.reserve(edtDeps.size());
    dbOuts.reserve(edtDeps.size());

    /// Iterate over the dependencies to fill dbIns and dbOuts.
    for (auto dep : edtDeps) {
      auto db = dyn_cast<DataBlockOp>(dep.getDefiningOp());
      assert(db && "Dependency must be a datablock");
      // Retrieve the datablock node and categorize based on mode.
      auto *dbNode = getNode(db);
      if (dbNode->isWriter())
        dbOuts.push_back(dbNode);
      if (dbNode->isReader())
        dbIns.push_back(dbNode);
    }

    /// Process input datablocks.
    LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                      << "  Processing EDT inputs\n");
    for (auto *dbIn : dbIns) {
      LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                        << "  - Examining DB #" << dbIn->id << " as input\n");
      auto prodDefs = findDefinition(*dbIn, newEnv);
      if (prodDefs.empty()) {
        LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                          << "    - No previous definition for DB #" << dbIn->id
                          << ", add edge from entry node\n");
        addEdge(*entryDbNode, *dbIn);
        continue;
      }
      LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ') << "Found "
                        << prodDefs.size() << " definitions for DB #"
                        << dbIn->id << "\n");
      /// Analyze dependencies from producer definitions.
      for (auto &prodDef : prodDefs) {
        bool isDirect = false;
        if (!DA->mayDepend(*prodDef, *dbIn, isDirect))
          continue;
        LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                          << "Adding edge from DB #" << prodDef->id
                          << " to DB #" << dbIn->id << "\n");
        bool res = addEdge(*prodDef, *dbIn);
        if (res) {
          LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                            << "Edge added successfully\n");
        } else {
          LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                            << "Edge already exists, skipping\n");
        }
      }
    }

    /// Process output datablocks.
    LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                      << "  Processing EDT outputs\n");
    for (auto *dbOut : dbOuts) {
      LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                        << "  - Examining DB #" << dbOut->id << " as output\n");
      auto prodDefs = findDefinition(*dbOut, newEnv);
      if (prodDefs.empty()) {
        LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                          << "    - No previous definition for DB #"
                          << dbOut->id
                          << ", updating environment with new definition\n");
        newEnv[dbOut->op] = dbOut;
        changed |= true;
        continue;
      }
      for (auto &prodDef : prodDefs) {
        bool isDirect = false;
        if (!DA->mayDepend(*prodDef, *dbOut, isDirect))
          continue;
        LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                          << "Updating environment: DB #" << dbOut->id
                          << " now defined from DB #" << prodDef->id << "\n");
        newEnv[prodDef->op] = dbOut;
        changed |= true;
      }
    }
  }

  /// Process the EDT body region.
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "  Processing EDT body region\n");
  {
    IndentScope bodyScope;
    Environment newEdtEnv;
    processRegion(edtOp.getBody(), newEdtEnv);
    // TODO: Integrate parent/child environment relationships if needed.
  }

  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "Finished processing EDT. Environment changed: "
                    << (changed ? "true" : "false") << "\n");
  return {newEnv, changed};
}

std::pair<Environment, bool> DatablockGraph::processFor(scf::ForOp forOp,
                                                        Environment &env) {
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "- Processing ForOp loop\n");
  Environment loopEnv = env;
  bool overallChanged = false;
  size_t prevEdgeCount = edges.size();
  unsigned iterationCount = 0;
  constexpr unsigned maxIterations = 5;

  // Iterate until a fixed-point is reached or maximum iterations are hit.
  while (true) {
    ++iterationCount;
    // Process the loop body region.
    auto bodyResult = processRegion(forOp->getRegion(0), loopEnv);
    Environment newLoopEnv = mergeEnvironments(loopEnv, bodyResult.first);
    overallChanged |= bodyResult.second;
    size_t currEdgeCount = edges.size();

    // Log the current iteration and edge count.
    LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ') << "  - Iteration "
                      << iterationCount << " completed: edges=" << currEdgeCount
                      << "\n");

    /// Break if no changes were made and the edge count is stable.
    if (!bodyResult.second && (currEdgeCount == prevEdgeCount))
      break;

    /// If maximum iterations reached without new edges, exit loop.
    if (iterationCount >= maxIterations && (currEdgeCount == prevEdgeCount))
      break;

    // Prepare for next iteration.
    loopEnv = std::move(newLoopEnv);
    prevEdgeCount = currEdgeCount;
    LLVM_DEBUG(
        dbgs() << std::string(analysisDepth * 2, ' ')
               << "  - Loop environment updated, iterating fixed-point...\n");
  }
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "- Finished processing ForOp loop\n");
  return {loopEnv, overallChanged};
}

std::pair<Environment, bool> DatablockGraph::processIf(scf::IfOp ifOp,
                                                       Environment &env) {
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "- Processing IfOp with then and else regions\n");
  auto thenRes = processRegion(ifOp.getThenRegion(), env);
  auto elseRes = processRegion(ifOp.getElseRegion(), env);
  Environment merged = mergeEnvironments(thenRes.first, elseRes.first);
  bool changed = thenRes.second || elseRes.second;
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "  - IfOp regions merged. Environment changed: "
                    << (changed ? "true" : "false") << "\n");
  return {merged, changed};
}

std::pair<Environment, bool> DatablockGraph::processCall(func::CallOp callOp,
                                                         Environment &env) {
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "- Processing CallOp (ignoring for now)\n");
  // TODO: Expand call handling logic for inter-procedural analysis
  return {env, false};
}

SmallVector<DatablockNode *, 4>
DatablockGraph::findDefinition(DatablockNode &dbNode, Environment &env) {
  IndentScope scope;
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "  Searching for definitions for DB #" << dbNode.id
                    << "\n");
  SmallVector<DatablockNode *, 4> defs;
  for (auto pair : env) {
    if (!DA->ptrMayAlias(*pair.second, dbNode))
      continue;
    LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                      << "  - Potential definition found: DB #"
                      << pair.second->id << "\n");
    /// Pessimistically assume alias implies potential dependency.
    defs.push_back(pair.second);
  }
  return defs;
}

Environment DatablockGraph::mergeEnvironments(const Environment &env1,
                                              const Environment &env2) {
  LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                    << "  Merging environments\n");
  Environment mergedEnv = env1;
  for (auto &pair : env2) {
    auto dbOp = pair.first;
    auto dbNode = pair.second;
    if (mergedEnv.count(dbOp)) {
      LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                        << "  - DB already exists in merged environment: "
                        << dbNode->id << "\n");
      auto node = getNode(dbOp);
      /// If they belong to the same parent
      if (dbNode->edtParent == node->edtParent) {
        LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                          << "    - Same EDT parent, "
                          << "updating definition\n");
        mergedEnv[dbOp] = dbNode;
      }
    } else {
      // Add new definition into the merged environment.
      LLVM_DEBUG(dbgs() << std::string(analysisDepth * 2, ' ')
                        << "  - Adding DB #" << dbNode->id
                        << " to merged environment\n");
      mergedEnv[dbOp] = dbNode;
    }
  }
  /// Debug final merged environment.
  LLVM_DEBUG({
    dbgs() << std::string(analysisDepth * 2, ' ')
           << "  - Final merged environment: {";
    for (auto &pair : mergedEnv) {
      dbgs() << " #" << getNode(pair.first)->id << " -> #" << pair.second->id
             << ",";
    }
    dbgs() << "}\n";
  });
  return mergedEnv;
}

void DatablockGraph::print() {
  std::string info;
  llvm::raw_string_ostream os(info);
  os << "Nodes:\n";
  for (auto *node : nodes) {
    /// Skip the entry node
    if (node == entryDbNode)
      continue;

    /// Print the node information
    auto &n = *node;
    os << "  #" << n.id << " " << n.mode << "\n";
    os << "    " << n.op << "\n";
    os << " useCount=" << n.useCount;
    os << " hasPtrDb=" << (n.hasPtrDb ? "true" : "false");
    os << " userEdtPos=" << n.userEdtPos;
    os << (n.parent ? " parent=#" + std::to_string(n.parent->id) : "") << "\n";
  }
  os << "Edges:\n";
  if (edges.empty()) {
    os << "  No edges\n";
    LLVM_DEBUG(dbgs() << os.str());
    return;
  }

  /// Print the edges for each producer node.
  for (auto &entry : edges) {
    unsigned producer = entry.first;
    for (auto consumer : entry.second) {
      os << "  #" << producer << " -> #" << consumer << "\n";
    }
  }
  os << "Total nodes: " << nodes.size() << "\n";
  LLVM_DEBUG(dbgs() << os.str());
}

void DatablockGraph::computeStatistics() {
  if (nodes.empty())
    return;
  unsigned totalUses = 0;
  unsigned totalRegions = 0;
  for (auto *node : nodes) {
    totalUses += node->useCount;
    // totalRegions += node.userRegions.size();
  }
  double avgUses = (double)totalUses / nodes.size();
  double avgRegions = (double)totalRegions / nodes.size();
  LLVM_DEBUG(DBGS() << "Average use count per datablock: " << avgUses << "\n"
                    << "Average region frequency per datablock: " << avgRegions
                    << "\n");
}

void DatablockGraph::collectNodes(Region &region) {
  /// Collect datablock nodes from the top-level region of a function.
  unsigned estimatedCount = 0;
  region.walk<mlir::WalkOrder::PreOrder>(
      [&](arts::DataBlockOp) { ++estimatedCount; });
  nodes.reserve(estimatedCount);
  region.walk<mlir::WalkOrder::PreOrder>([&](arts::DataBlockOp dbOp) {
    LLVM_DEBUG(dbgs() << "Datablock node #" << nodes.size() << ": " << dbOp
                      << "\n");
    /// Add the node to the graph.
    DatablockNode *dbNode = new DatablockNode(this, dbOp);
    assert(dbNode && "Failed to allocate DatablockNode");
    nodes.push_back(dbNode);
    nodeMap[dbOp] = dbNode;
  });
}

void DatablockGraph::fuseAdjacentNodes() {}

void DatablockGraph::detectOutOnlyNodes() {}

void DatablockGraph::deduplicateNodes() {}

bool DatablockGraph::isOnlyDependentOnEntry(DatablockNode &node) {
  for (auto &entry : edges) {
    if (entry.first == entryDbNode->id)
      continue;
    if (entry.first == node.id)
      continue;
    if (entry.second.count(node.id))
      return false;
  }
  return true;
}

//===----------------------------------------------------------------------===//
// DatablockAnalysis
//===----------------------------------------------------------------------===//
DatablockAnalysis::DatablockAnalysis(Operation *module) {
  ModuleOp mod = cast<ModuleOp>(module);
  loopAnalysis = new LoopAnalysis(mod);
  // mod->walk([&](func::FuncOp func) { getOrCreateGraph(func); });
}

DatablockAnalysis::~DatablockAnalysis() {
  // LLVM_DEBUG(DBGS() << "Deleting DatablockAnalysis\n");
  for (auto &pair : functionGraphMap)
    delete pair.second;
  functionGraphMap.clear();
  delete loopAnalysis;
}

/// Public interface
void DatablockAnalysis::printGraph(func::FuncOp func) {
  LLVM_DEBUG(dbgs() << line);
  if (functionGraphMap.count(func)) {
    LLVM_DEBUG(DBGS() << "Printing graph for function: " << func.getName()
                      << "\n");
    functionGraphMap[func]->print();
  } else
    llvm::errs() << "No graph for function: " << func.getName() << "\n";
}

DatablockGraph *DatablockAnalysis::getOrCreateGraph(func::FuncOp func) {
  if (functionGraphMap.count(func))
    return functionGraphMap[func];

  /// Checks if the function has a body, if not return null.
  if (func.getBody().empty())
    return nullptr;

  /// Create a new graph for the function and store it in the map.
  functionGraphMap[func] = new DatablockGraph(func, this);
  return functionGraphMap[func];
}

/// Dependency analysis.
bool DatablockAnalysis::mayDepend(DatablockNode &prod, DatablockNode &cons,
                                  bool &isDirect) {
  /// Verify if they belong to the same EDT parent.
  if (prod.edtParent != cons.edtParent)
    return false;

  /// Only consider writer producer and reader consumer.
  if (!prod.isWriter() || !cons.isReader())
    return false;

  /// Check if nodes are different
  auto compResult = compare(prod, cons);
  if (compResult == DatablockNodeComp::Different)
    return false;

  /// There is a direct dependency if
  isDirect = true ? (compResult == DatablockNodeComp::Equal) : false;

  /// TODO: We can make a more complex analysis here, such as checking an
  /// affine iteration space, and verifying if the producer and consumer are
  /// in the same iteration space.
  return true;
}

bool DatablockAnalysis::ptrMayAlias(DatablockNode &A, DatablockNode &B) {
  if (A.aliases.contains(B.id))
    return true;

  for (auto &eA : A.effects) {
    for (auto &eB : B.effects) {
      if (mayAlias(eA, eB)) {
        // LLVM_DEBUG(dbgs() << "    - Datablocks may alias\n");
        A.aliases.insert(B.id);
        B.aliases.insert(A.id);
        return true;
      }
    }
  }
  return false;
}

bool DatablockAnalysis::ptrMayAlias(DatablockNode &A, Value val) {
  for (auto &eA : A.effects) {
    if (mayAlias(eA, val))
      return true;
  }
  return false;
}

DatablockNodeComp DatablockAnalysis::compare(DatablockNode &A,
                                             DatablockNode &B) {
  /// If it is already known that the nodes are equivalent, return true.
  if (A.duplicates.contains(B.id) || B.duplicates.contains(A.id))
    return DatablockNodeComp::Equal;

  /// Compute it, otherwise.
  if (!ptrMayAlias(A, B))
    return DatablockNodeComp::Different;

  /// Compare indices
  if (A.indices.size() != B.indices.size())
    return DatablockNodeComp::BaseAlias;

  if (!std::equal(A.indices.begin(), A.indices.end(), B.indices.begin()))
    return DatablockNodeComp::BaseAlias;

  /// Cache the result.
  A.duplicates.insert(B.id);
  B.duplicates.insert(A.id);
  return DatablockNodeComp::Equal;
}

/// Analysis
