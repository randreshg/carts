////===----------------------------------------------------------------------===////
/// ArtsMetadataManager.cpp
////===----------------------------------------------------------------------===////

#include "arts/Analysis/Metadata/ArtsMetadataManager.h"
#include "arts/ArtsDialect.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "arts/Utils/ArtsDebug.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

ARTS_DEBUG_SETUP(metadata_manager);

using namespace mlir;
using namespace mlir::arts;

////===----------------------------------------------------------------------===////
/// Metadata Addition
////===----------------------------------------------------------------------===////

/// Add loop metadata for an operation
LoopMetadata *ArtsMetadataManager::addLoopMetadata(Operation *op) {
  return addMetadata<LoopMetadata>(op);
}

/// Add memref metadata for an allocation operation
MemrefMetadata *ArtsMetadataManager::addMemrefMetadata(Operation *op) {
  return addMetadata<MemrefMetadata>(op);
}

/// Add value metadata for an operation
ValueMetadata *ArtsMetadataManager::addValueMetadata(Operation *op) {
  return addMetadata<ValueMetadata>(op);
}

///===----------------------------------------------------------------------===///
// Metadata Retrieval
///===----------------------------------------------------------------------===///

LoopMetadata *ArtsMetadataManager::getLoopMetadata(Operation *op) {
  return getMetadata<LoopMetadata>(op);
}

const LoopMetadata *ArtsMetadataManager::getLoopMetadata(Operation *op) const {
  return getMetadata<LoopMetadata>(op);
}

MemrefMetadata *ArtsMetadataManager::getMemrefMetadata(Operation *op) {
  return getMetadata<MemrefMetadata>(op);
}

const MemrefMetadata *
ArtsMetadataManager::getMemrefMetadata(Operation *op) const {
  return getMetadata<MemrefMetadata>(op);
}

ValueMetadata *ArtsMetadataManager::getValueMetadata(Operation *op) {
  return getMetadata<ValueMetadata>(op);
}

const ValueMetadata *
ArtsMetadataManager::getValueMetadata(Operation *op) const {
  return getMetadata<ValueMetadata>(op);
}

///===----------------------------------------------------------------------===///
// Batch Operations
///===----------------------------------------------------------------------===///

/// Walk all operations and import metadata from those with arts.* attributes
void ArtsMetadataManager::importFromOperations(ModuleOp module) {
  ARTS_DEBUG_HEADER(ArtsMetadataManager::importFromOperations);

  size_t importCount = 0;
  module.walk([&](Operation *op) {
    size_t before = metadataMap_.size();
    importFromOperation(op);
    if (metadataMap_.size() > before)
      importCount++;
  });

  ARTS_DEBUG("Imported metadata for " << importCount << " operations");
  ARTS_DEBUG_FOOTER(ArtsMetadataManager::importFromOperations);
}

/// Export all metadata objects to their respective operations
void ArtsMetadataManager::exportToOperations() {
  for (auto &entry : metadataMap_)
    entry.second->exportToOp();
}

/// Walk the module and create metadata objects for operations with arts.*
/// attributes
void ArtsMetadataManager::collectFromModule(ModuleOp module) {
  ARTS_DEBUG_HEADER(ArtsMetadataManager::collectFromModule);

  size_t opsWithArtsAttrs = 0;
  size_t importedCount = 0;

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
    size_t before = metadataMap_.size();

    /// Try to import metadata based on operation type
    importFromOperation(op);

    if (metadataMap_.size() > before)
      importedCount++;
  });

  ARTS_DEBUG("Found " << opsWithArtsAttrs << " operations with arts.* attributes");
  ARTS_DEBUG("Successfully imported metadata for " << importedCount << " operations");
  ARTS_DEBUG_FOOTER(ArtsMetadataManager::collectFromModule);
}

///===----------------------------------------------------------------------===///
// JSON Import/Export
///===----------------------------------------------------------------------===///

bool ArtsMetadataManager::importFromJson(llvm::StringRef jsonStr) {
  ARTS_DEBUG_HEADER(ArtsMetadataManager::importFromJson);

  // Parse JSON
  auto json = llvm::json::parse(jsonStr);
  if (!json) {
    ARTS_ERROR("Failed to parse JSON: " << toString(json.takeError()));
    return false;
  }

  auto *root = json->getAsObject();
  if (!root) {
    ARTS_ERROR("JSON root is not an object");
    return false;
  }

  /// Import allocations
  if (auto *allocations = root->getObject(AttrNames::Metadata::Allocations)) {
    for (const auto &_ : *allocations) {
      ARTS_WARN("importFromJson requires operations to attach metadata to");
      ARTS_WARN("Use importFromOperations after loading from JSON file");
      return false;
    }
  }

  ARTS_DEBUG_FOOTER(ArtsMetadataManager::importFromJson);
  return true;
}

bool ArtsMetadataManager::importFromJsonFile(llvm::StringRef filename) {
  ARTS_DEBUG("Importing metadata from JSON file: " << filename);

  auto fileOrErr = llvm::MemoryBuffer::getFile(filename);
  if (!fileOrErr) {
    ARTS_ERROR("Error opening metadata file: " << filename);
    return false;
  }

  return importFromJson((*fileOrErr)->getBuffer());
}

std::string ArtsMetadataManager::exportToJson() const {
  llvm::json::Object root;
  llvm::json::Object loops;
  llvm::json::Object memrefs;
  llvm::json::Object locations;
  llvm::json::Object values;

  // Export all metadata objects to JSON
  for (const auto &entry : metadataMap_) {
    Operation *op = entry.first;
    const ArtsMetadata *metadata = entry.second.get();

    llvm::json::Object metadataJson;
    metadata->exportToJson(metadataJson);

    // Categorize by metadata type using getMetadataName() instead of
    // dynamic_cast
    StringRef metadataName = metadata->getMetadataName();
    if (metadataName == AttrNames::LoopMetadata::Name)
      loops[metadata->toString().str()] = std::move(metadataJson);
    else if (metadataName == AttrNames::MemrefMetadata::Name)
      memrefs[metadata->toString().str()] = std::move(metadataJson);
    else if (metadataName == AttrNames::ValueMetadata::Name)
      values[metadata->toString().str()] = std::move(metadataJson);
  }

  /// Add all categories to root
  root[AttrNames::Metadata::Version.str()] = "1.0";
  if (!loops.empty())
    root[AttrNames::Metadata::Loops.str()] = std::move(loops);
  if (!memrefs.empty())
    root[AttrNames::Metadata::Memrefs.str()] = std::move(memrefs);
  if (!locations.empty())
    root[AttrNames::Metadata::Locations.str()] = std::move(locations);
  if (!values.empty())
    root[AttrNames::Metadata::Values.str()] = std::move(values);

  // Convert to string
  std::string jsonStr;
  llvm::raw_string_ostream os(jsonStr);
  os << llvm::json::Value(std::move(root));
  return os.str();
}

bool ArtsMetadataManager::exportToJsonFile(llvm::StringRef filename) const {
  ARTS_DEBUG("Exporting metadata to JSON file: " << filename);

  std::string jsonStr = exportToJson();

  // Write to file
  std::error_code ec;
  llvm::raw_fd_ostream file(filename, ec);
  if (ec) {
    ARTS_ERROR("Error opening file for writing: " << filename);
    return false;
  }

  file << jsonStr;
  ARTS_DEBUG("Successfully exported " << metadataMap_.size() << " metadata entries");
  return true;
}

///===----------------------------------------------------------------------===///
// Iteration and Inspection
///===----------------------------------------------------------------------===///

SmallVector<Operation *>
ArtsMetadataManager::getOperationsWithMetadata() const {
  SmallVector<Operation *> ops;
  ops.reserve(metadataMap_.size());

  for (const auto &entry : metadataMap_)
    ops.push_back(entry.first);

  return ops;
}

SmallVector<Operation *> ArtsMetadataManager::getLoopOperations() const {
  SmallVector<Operation *> ops;

  for (const auto &entry : metadataMap_) {
    if (entry.second->getMetadataName() == AttrNames::LoopMetadata::Name)
      ops.push_back(entry.first);
  }

  return ops;
}

SmallVector<Operation *> ArtsMetadataManager::getMemrefOperations() const {
  SmallVector<Operation *> ops;

  for (const auto &entry : metadataMap_) {
    if (entry.second->getMetadataName() == AttrNames::MemrefMetadata::Name)
      ops.push_back(entry.first);
  }

  return ops;
}

///===----------------------------------------------------------------------===///
// Debug and Utilities
///===----------------------------------------------------------------------===///

void ArtsMetadataManager::printStatistics(llvm::raw_ostream &os) const {
  size_t loopCount = 0, memrefCount = 0, locationCount = 0, valueCount = 0;

  for (const auto &entry : metadataMap_) {
    StringRef name = entry.second->getMetadataName();
    if (name == AttrNames::LoopMetadata::Name)
      ++loopCount;
    else if (name == AttrNames::MemrefMetadata::Name)
      ++memrefCount;
    else if (name == AttrNames::ValueMetadata::Name)
      ++valueCount;
  }

  os << "ArtsMetadataManager Statistics:\n";
  os << "  Total metadata entries: " << metadataMap_.size() << "\n";
  os << "  Loop metadata: " << loopCount << "\n";
  os << "  Memref metadata: " << memrefCount << "\n";
  os << "  Location metadata: " << locationCount << "\n";
  os << "  Value metadata: " << valueCount << "\n";
}

void ArtsMetadataManager::dump() const {
  llvm::outs() << "ArtsMetadataManager Dump:\n";
  llvm::outs() << "=========================\n\n";

  printStatistics(llvm::outs());

  llvm::outs() << "\nDetailed Metadata:\n";
  llvm::outs() << "------------------\n";

  for (const auto &entry : metadataMap_) {
    Operation *op = entry.first;
    const ArtsMetadata *metadata = entry.second.get();

    llvm::outs() << "Operation: ";
    op->print(llvm::outs());
    llvm::outs() << "\n";

    llvm::outs() << "  Metadata type: " << metadata->getMetadataName() << "\n";
    llvm::outs() << "  toString: " << metadata->toString() << "\n";

    llvm::json::Object jsonObj;
    metadata->exportToJson(jsonObj);
    llvm::outs() << "  JSON: " << llvm::json::Value(std::move(jsonObj))
                 << "\n\n";
  }
}

///===----------------------------------------------------------------------===///
// Helper Methods
///===----------------------------------------------------------------------===///

bool ArtsMetadataManager::isLoopOp(Operation *op) {
  return isa<affine::AffineForOp, scf::ForOp, scf::ParallelOp, scf::ForallOp>(
      op);
}

bool ArtsMetadataManager::isMemrefAllocOp(Operation *op) {
  return isa<memref::AllocOp, memref::AllocaOp>(op);
}

void ArtsMetadataManager::importFromOperation(Operation *op) {
  /// Skip if already have metadata for this operation
  if (hasMetadata(op))
    return;

  /// Try to create and import appropriate metadata type
  std::unique_ptr<ArtsMetadata> metadata;

  if (isLoopOp(op)) {
    auto loopMeta = std::make_unique<LoopMetadata>(op);
    if (loopMeta->importFromOp()) {
      ARTS_DEBUG_REGION({
        ARTS_DEBUG("Imported LoopMetadata for operation: " << op->getName());
      });
      metadataMap_[op] = std::move(loopMeta);
    }
  } else if (isMemrefAllocOp(op)) {
    auto memrefMeta = std::make_unique<MemrefMetadata>(op);
    if (memrefMeta->importFromOp()) {
      ARTS_DEBUG_REGION({
        ARTS_DEBUG("Imported MemrefMetadata for operation: " << op->getName());
      });
      metadataMap_[op] = std::move(memrefMeta);
    }
  }
}
