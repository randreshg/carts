////===----------------------------------------------------------------------===////
/// MetadataRegistry.cpp
////===----------------------------------------------------------------------===////

#include "arts/analysis/metadata/MetadataRegistry.h"
#include "arts/Dialect.h"
#include "arts/utils/Debug.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/LoopUtils.h"
#include "arts/utils/metadata/LocationMetadata.h"
#include "arts/utils/metadata/LoopMetadata.h"
#include "arts/utils/metadata/MemrefMetadata.h"
#include "arts/utils/metadata/ValueMetadata.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"

ARTS_DEBUG_SETUP(metadata_registry);

using namespace mlir;
using namespace mlir::arts;

namespace {

static bool refreshLoopMetadataFactsImpl(LoopMetadata &metadata,
                                         Operation *op) {
  if (!op)
    return false;

  metadata.setOperation(op);
  metadata.locationMetadata = LocationMetadata::fromLocation(op->getLoc());
  metadata.nestingLevel = static_cast<int64_t>(getLoopDepth(op));

  if (std::optional<int64_t> tripCount = getStaticTripCount(op))
    metadata.tripCount = *tripCount;
  else
    metadata.tripCount.reset();

  return true;
}

static void seedLoopMetadataDefaults(LoopMetadata &metadata, Operation *op) {
  if (!op)
    return;

  if (isa<arts::ForOp, scf::ParallelOp, scf::ForallOp, omp::WsloopOp>(op)) {
    metadata.potentiallyParallel = true;
    metadata.parallelClassification =
        LoopMetadata::ParallelClassification::Likely;
  }

  if (auto artsFor = dyn_cast<arts::ForOp>(op)) {
    if (!artsFor.getReductionAccumulators().empty())
      metadata.hasReductions = true;
  }
}

static bool refreshMemrefMetadataFactsImpl(MemrefMetadata &metadata,
                                           Operation *op) {
  if (!op)
    return false;
  metadata.setOperation(op);
  return true;
}

static bool refreshValueMetadataFactsImpl(ValueMetadata &metadata,
                                          Operation *op) {
  if (!op)
    return false;
  metadata.setOperation(op);
  metadata.location = LocationMetadata::fromLocation(op->getLoc());
  return true;
}

static bool refreshMetadataFactsImpl(ArtsMetadata &metadata, Operation *op) {
  StringRef metadataName = metadata.getMetadataName();
  if (metadataName == AttrNames::LoopMetadata::Name)
    return refreshLoopMetadataFactsImpl(static_cast<LoopMetadata &>(metadata),
                                        op);
  if (metadataName == AttrNames::MemrefMetadata::Name)
    return refreshMemrefMetadataFactsImpl(
        static_cast<MemrefMetadata &>(metadata), op);
  if (metadataName == AttrNames::ValueMetadata::Name)
    return refreshValueMetadataFactsImpl(static_cast<ValueMetadata &>(metadata),
                                         op);
  metadata.setOperation(op);
  return true;
}

static std::unique_ptr<ArtsMetadata>
createMetadataEntryForOperation(Operation *op) {
  if (!op)
    return nullptr;

  if (MetadataRegistry::isLoopOp(op)) {
    auto metadata = std::make_unique<LoopMetadata>(op);
    if (op->hasAttr(AttrNames::LoopMetadata::Name))
      metadata->importFromOp();
    else
      seedLoopMetadataDefaults(*metadata, op);
    return metadata;
  }

  if (MetadataRegistry::isMemrefAllocOp(op)) {
    auto metadata = std::make_unique<MemrefMetadata>(op);
    if (op->hasAttr(AttrNames::MemrefMetadata::Name))
      metadata->importFromOp();
    return metadata;
  }

  if (op->hasAttr(AttrNames::ValueMetadata::Name)) {
    auto metadata = std::make_unique<ValueMetadata>(op);
    metadata->importFromOp();
    return metadata;
  }

  return nullptr;
}

} // namespace

////===----------------------------------------------------------------------===////
/// Metadata Addition
////===----------------------------------------------------------------------===////

LoopMetadata *MetadataRegistry::addLoopMetadata(Operation *op) {
  return addMetadata<LoopMetadata>(op);
}

MemrefMetadata *MetadataRegistry::addMemrefMetadata(Operation *op) {
  return addMetadata<MemrefMetadata>(op);
}

ValueMetadata *MetadataRegistry::addValueMetadata(Operation *op) {
  return addMetadata<ValueMetadata>(op);
}

///===----------------------------------------------------------------------===///
/// Metadata Retrieval
///===----------------------------------------------------------------------===///

LoopMetadata *MetadataRegistry::getLoopMetadata(Operation *op) {
  return getMetadata<LoopMetadata>(op);
}

const LoopMetadata *MetadataRegistry::getLoopMetadata(Operation *op) const {
  return getMetadata<LoopMetadata>(op);
}

MemrefMetadata *MetadataRegistry::getMemrefMetadata(Operation *op) {
  return getMetadata<MemrefMetadata>(op);
}

const MemrefMetadata *MetadataRegistry::getMemrefMetadata(Operation *op) const {
  return getMetadata<MemrefMetadata>(op);
}

MemrefMetadata *MetadataRegistry::getMemrefMetadataById(int64_t artsId) {
  for (auto &entry : metadataMap) {
    if (entry.second->getMetadataName() != AttrNames::MemrefMetadata::Name)
      continue;
    if (entry.second->getMetadataId() == artsId)
      return static_cast<MemrefMetadata *>(entry.second.get());
  }
  return nullptr;
}

const MemrefMetadata *
MetadataRegistry::getMemrefMetadataById(int64_t artsId) const {
  for (const auto &entry : metadataMap) {
    if (entry.second->getMetadataName() != AttrNames::MemrefMetadata::Name)
      continue;
    if (entry.second->getMetadataId() == artsId)
      return static_cast<const MemrefMetadata *>(entry.second.get());
  }
  return nullptr;
}

ValueMetadata *MetadataRegistry::getValueMetadata(Operation *op) {
  return getMetadata<ValueMetadata>(op);
}

const ValueMetadata *MetadataRegistry::getValueMetadata(Operation *op) const {
  return getMetadata<ValueMetadata>(op);
}

///===----------------------------------------------------------------------===///
/// Batch Operations
///===----------------------------------------------------------------------===///

void MetadataRegistry::importFromOperations(ModuleOp module) {
  ARTS_DEBUG_HEADER(MetadataRegistry::importFromOperations);

  uint64_t importCount = 0;
  module.walk([&](Operation *op) {
    uint64_t before = metadataMap.size();
    importFromOperation(op);
    if (metadataMap.size() > before)
      importCount++;
  });

  ARTS_DEBUG("Imported metadata for " << importCount << " operations");
  ARTS_DEBUG_FOOTER(MetadataRegistry::importFromOperations);
}

void MetadataRegistry::exportToOperations() {
  for (auto &entry : metadataMap)
    entry.second->exportToOp();
}

void MetadataRegistry::collectFromModule(ModuleOp module) {
  ARTS_DEBUG_HEADER(MetadataRegistry::collectFromModule);

  /// This importer is used to reconstruct the in-memory registry from the
  /// current IR attributes. Rebuilding from scratch avoids leaving stale
  /// Operation* keys behind after earlier passes rewrote or erased ops.
  metadataMap.clear();

  uint64_t opsWithArtsAttrs = 0;
  uint64_t importedCount = 0;

  module.walk([&](Operation *op) {
    bool hasArtsAttr = false;
    for (auto namedAttr : op->getAttrs()) {
      if (namedAttr.getName().strref().starts_with("arts.")) {
        hasArtsAttr = true;
        break;
      }
    }

    if (!hasArtsAttr)
      return;

    opsWithArtsAttrs++;
    uint64_t before = metadataMap.size();

    importFromOperation(op);

    if (metadataMap.size() > before)
      importedCount++;
  });

  ARTS_DEBUG("Found " << opsWithArtsAttrs
                      << " operations with arts.* attributes");
  ARTS_DEBUG("Successfully imported metadata for " << importedCount
                                                   << " operations");
  ARTS_DEBUG_FOOTER(MetadataRegistry::collectFromModule);
}

///===----------------------------------------------------------------------===///
/// Iteration and Inspection
///===----------------------------------------------------------------------===///

SmallVector<Operation *> MetadataRegistry::getOperationsWithMetadata() const {
  SmallVector<Operation *> ops;
  ops.reserve(metadataMap.size());

  for (const auto &entry : metadataMap)
    ops.push_back(entry.first);

  return ops;
}

SmallVector<Operation *> MetadataRegistry::getLoopOperations() const {
  SmallVector<Operation *> ops;

  for (const auto &entry : metadataMap) {
    if (entry.second->getMetadataName() == AttrNames::LoopMetadata::Name)
      ops.push_back(entry.first);
  }

  return ops;
}

SmallVector<Operation *> MetadataRegistry::getMemrefOperations() const {
  SmallVector<Operation *> ops;

  for (const auto &entry : metadataMap) {
    if (entry.second->getMetadataName() == AttrNames::MemrefMetadata::Name)
      ops.push_back(entry.first);
  }

  return ops;
}

///===----------------------------------------------------------------------===///
/// Metadata Helpers
///===----------------------------------------------------------------------===///

void MetadataRegistry::initializeMetadata(ArtsMetadata *metadata) {
  if (!metadata)
    return;
  Operation *op = metadata->getOperation();
  assert(op && "Operation must always be available");
  ArtsId id = metadata->getMetadataId();

  /// If metadata already has an ID (e.g., from JSON), preserve it
  if (id.has_value()) {
    /// Set the arts.id attribute on the operation if it doesn't have one
    if (!op->getAttrOfType<IntegerAttr>(AttrNames::Operation::ArtsId))
      idRegistry.set(op, id.value());
  } else {
    /// No ID in metadata, try to get/assign from operation
    id = assignOperationId(op);
  }

  /// If still no ID, assign a new one (should not happen if op has location)
  if (!id.has_value())
    id = ArtsId(idRegistry.allocateSequential());

  metadata->setMetadataId(id);
  idRegistry.recordUsedId(id.value());
}

ArtsId MetadataRegistry::assignOperationId(Operation *op) {
  assert(op && "Operation must always be available");

  /// Try to get or create a deterministic ID based on location
  int64_t id = idRegistry.getOrCreate(op);

  /// If no deterministic ID available (no location), use sequential fallback
  if (id == 0) {
    id = idRegistry.allocateSequential();
    idRegistry.set(op, id);
  }

  idRegistry.recordUsedId(id);
  return ArtsId(id);
}

bool MetadataRegistry::transferMetadata(Operation *sourceOp,
                                        Operation *targetOp) {
  if (!sourceOp || !targetOp)
    return false;

  /// Check if source has metadata
  auto it = metadataMap.find(sourceOp);
  if (it == metadataMap.end())
    return false;

  /// Get the metadata and preserve arts_id
  std::unique_ptr<ArtsMetadata> metadata = std::move(it->second);
  ArtsId preservedId = metadata->getMetadataId();

  /// Update metadata to point to target operation
  metadata->setOperation(targetOp);

  /// Preserve arts_id if it exists
  if (preservedId.has_value()) {
    metadata->setMetadataId(preservedId);
    /// Also set the arts.id attribute on the target operation
    idRegistry.set(targetOp, preservedId.value());
  } else {
    /// Assign new ID if source didn't have one
    ArtsId newId = assignOperationId(targetOp);
    metadata->setMetadataId(newId);
  }

  /// Move metadata to target operation in the map
  metadataMap.erase(it);
  metadataMap[targetOp] = std::move(metadata);

  /// Export metadata to target operation
  metadataMap[targetOp]->exportToOp();

  ArtsId newArtsId = metadataMap[targetOp]->getMetadataId();
  ARTS_DEBUG(
      "Transferred metadata from "
      << *sourceOp << " to " << *targetOp << " (arts_id="
      << (newArtsId.has_value() ? std::to_string(newArtsId.value()) : "none")
      << ")");

  return true;
}

bool MetadataRegistry::replaceMetadataForRewrite(Operation *sourceOp,
                                                 Operation *targetOp) {
  if (!sourceOp || !targetOp)
    return false;

  auto it = metadataMap.find(sourceOp);
  if (it == metadataMap.end())
    return false;

  std::unique_ptr<ArtsMetadata> metadata = std::move(it->second);
  metadataMap.erase(it);
  if (!metadata)
    return false;

  if (!refreshMetadataFactsImpl(*metadata, targetOp))
    return false;

  ArtsId preservedId = metadata->getMetadataId();
  if (preservedId.has_value())
    idRegistry.set(targetOp, preservedId.value());
  else
    metadata->setMetadataId(assignOperationId(targetOp));

  metadata->exportToOp();
  metadataMap[targetOp] = std::move(metadata);
  return true;
}

bool MetadataRegistry::refreshMetadata(Operation *op) {
  if (!op)
    return false;

  auto it = metadataMap.find(op);
  if (it == metadataMap.end()) {
    std::unique_ptr<ArtsMetadata> created = createMetadataEntryForOperation(op);
    if (!created)
      return false;

    initializeMetadata(created.get());
    if (!refreshMetadataFactsImpl(*created, op))
      return false;
    created->exportToOp();
    metadataMap[op] = std::move(created);
    return true;
  }

  if (!refreshMetadataFactsImpl(*it->second, op))
    return false;

  it->second->exportToOp();
  return true;
}

///===----------------------------------------------------------------------===///
/// Operation Classification
///===----------------------------------------------------------------------===///

bool MetadataRegistry::isLoopOp(Operation *op) {
  return isa<affine::AffineForOp, scf::ForOp, scf::ParallelOp, scf::ForallOp,
             omp::WsloopOp, arts::ForOp>(op);
}

bool MetadataRegistry::isMemrefAllocOp(Operation *op) {
  return isa<memref::AllocOp, memref::AllocaOp>(op);
}

///===----------------------------------------------------------------------===///
/// Helper Methods
///===----------------------------------------------------------------------===///

void MetadataRegistry::importFromOperation(Operation *op) {
  /// Skip if already have metadata for this operation
  if (hasMetadata(op))
    return;

  /// Try to create and import appropriate metadata type
  if (isLoopOp(op)) {
    auto loopMeta = std::make_unique<LoopMetadata>(op);
    if (loopMeta->importFromOp()) {
      initializeMetadata(loopMeta.get());
      ARTS_DEBUG("Imported LoopMetadata for operation: " << op->getName());
      metadataMap[op] = std::move(loopMeta);
    }
  } else if (isMemrefAllocOp(op)) {
    auto memrefMeta = std::make_unique<MemrefMetadata>(op);
    if (memrefMeta->importFromOp()) {
      initializeMetadata(memrefMeta.get());
      ARTS_DEBUG("Imported MemrefMetadata for operation: " << op->getName());
      metadataMap[op] = std::move(memrefMeta);
    }
  } else if (op->hasAttr(AttrNames::ValueMetadata::Name)) {
    auto valueMeta = std::make_unique<ValueMetadata>(op);
    if (valueMeta->importFromOp()) {
      initializeMetadata(valueMeta.get());
      ARTS_DEBUG("Imported ValueMetadata for operation: " << op->getName());
      metadataMap[op] = std::move(valueMeta);
    }
  }
}

///===----------------------------------------------------------------------===///
/// Debug and Utilities
///===----------------------------------------------------------------------===///

void MetadataRegistry::printStatistics(llvm::raw_ostream &os) const {
  uint64_t loopCount = 0, memrefCount = 0;

  for (const auto &entry : metadataMap) {
    StringRef name = entry.second->getMetadataName();
    if (name == AttrNames::LoopMetadata::Name)
      ++loopCount;
    else if (name == AttrNames::MemrefMetadata::Name)
      ++memrefCount;
  }

  os << "MetadataRegistry Statistics:\n";
  os << "  Total metadata entries: " << metadataMap.size() << "\n";
  os << "  Loop metadata: " << loopCount << "\n";
  os << "  Memref metadata: " << memrefCount << "\n";
}

void MetadataRegistry::dump() const {
  ARTS_DEBUG_REGION({
    ARTS_DBGS() << "MetadataRegistry Dump:\n";
    ARTS_DBGS() << "=========================\n\n";

    printStatistics(ARTS_DBGS());

    ARTS_DBGS() << "\nDetailed Metadata:\n";
    ARTS_DBGS() << "------------------\n";

    for (const auto &entry : metadataMap) {
      Operation *op = entry.first;
      const ArtsMetadata *metadata = entry.second.get();

      ARTS_DBGS() << "Operation: " << op << "\n";
      ARTS_DBGS() << "  Metadata type: " << metadata->getMetadataName() << "\n";
      ARTS_DBGS() << "  toString: " << metadata->toString() << "\n";

      llvm::json::Object jsonObj;
      metadata->exportToJson(jsonObj);
      ARTS_DBGS() << "  JSON: " << llvm::json::Value(std::move(jsonObj))
                  << "\n\n";
    }
  });
}
