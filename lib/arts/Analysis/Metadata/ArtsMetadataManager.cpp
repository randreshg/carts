////===----------------------------------------------------------------------===////
/// ArtsMetadataManager.cpp
////===----------------------------------------------------------------------===////

#include "arts/Analysis/Metadata/ArtsMetadataManager.h"
#include "arts/ArtsDialect.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include <cmath>
#include <limits>
#include <system_error>

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
// Metadata Helpers
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

MemrefMetadata *ArtsMetadataManager::getMemrefMetadataById(int64_t artsId) {
  for (auto &entry : metadataMap) {
    if (entry.second->getMetadataName() != AttrNames::MemrefMetadata::Name)
      continue;
    if (entry.second->getMetadataId() == artsId)
      return static_cast<MemrefMetadata *>(entry.second.get());
  }
  return nullptr;
}

const MemrefMetadata *
ArtsMetadataManager::getMemrefMetadataById(int64_t artsId) const {
  for (const auto &entry : metadataMap) {
    if (entry.second->getMetadataName() != AttrNames::MemrefMetadata::Name)
      continue;
    if (entry.second->getMetadataId() == artsId)
      return static_cast<const MemrefMetadata *>(entry.second.get());
  }
  return nullptr;
}

ValueMetadata *ArtsMetadataManager::getValueMetadata(Operation *op) {
  return getMetadata<ValueMetadata>(op);
}

const ValueMetadata *
ArtsMetadataManager::getValueMetadata(Operation *op) const {
  return getMetadata<ValueMetadata>(op);
}

void ArtsMetadataManager::setMetadataFile(llvm::StringRef filename) {
  if (filename.empty())
    return;
  metadataFilePath = filename.str();
  jsonCacheInitialized = false;
  loopJsonCache.clear();
  memrefJsonCache.clear();
  serializedMetadataCache.clear();
}

bool ArtsMetadataManager::ensureLoopMetadata(Operation *op) {
  if (!op)
    return false;

  ARTS_DEBUG("ensureLoopMetadata: " << op->getName());

  /// 1. Check if already in memory
  if (getLoopMetadata(op)) {
    ARTS_DEBUG("  → already in memory");
    return true;
  }

  /// 2. Check if op has metadata attribute
  if (op->getAttr(AttrNames::LoopMetadata::Name)) {
    ARTS_DEBUG("  → has arts.loop attr, importing");
    importFromOperation(op);
    return getLoopMetadata(op) != nullptr;
  }

  /// 3. Try to attach from cache using arts.id or location
  LocationMetadata loc = LocationMetadata::fromLocation(op->getLoc());
  ARTS_DEBUG("  → extracted location: " << loc.getKey()
             << " (file=" << loc.file << ", line=" << loc.line << ")");
  std::string key;
  if (auto idAttr = op->getAttrOfType<IntegerAttr>(ArtsMetadata::IdAttrName))
    key = std::to_string(idAttr.getInt());
  if (key.empty())
    key = loc.getKey().str();
  ARTS_DEBUG("  → trying exact match with key: " << key);
  if (!key.empty() &&
      attachMetadataFromJson(op, key, loopJsonCache, /*isLoop=*/true)) {
    ARTS_DEBUG("  → exact match found!");
    return true;
  }

  ARTS_DEBUG("  → no exact match, trying near-location match");
  if (!loc.isValid()) {
    ARTS_DEBUG("  → location invalid, giving up");
    return false;
  }
  return attachLoopMetadataNearLocation(op, loc);
}

bool ArtsMetadataManager::ensureMemrefMetadata(Operation *op) {
  if (!op)
    return false;

  /// 1. Check if already in memory
  if (getMemrefMetadata(op))
    return true;

  /// 2. Check if op has metadata attribute
  if (op->getAttr(AttrNames::MemrefMetadata::Name)) {
    importFromOperation(op);
    return getMemrefMetadata(op) != nullptr;
  }

  /// 3. Try to attach from cache using arts.id or location
  std::string key;
  if (auto idAttr = op->getAttrOfType<IntegerAttr>(ArtsMetadata::IdAttrName))
    key = std::to_string(idAttr.getInt());
  if (key.empty())
    key = LocationMetadata::fromLocation(op->getLoc()).getKey().str();
  if (key.empty())
    return false;
  return attachMetadataFromJson(op, key, memrefJsonCache, /*isLoop=*/false);
}

///===----------------------------------------------------------------------===///
/// Batch Operations
///===----------------------------------------------------------------------===///

/// Walk all operations and import metadata from those with arts.* attributes
void ArtsMetadataManager::importFromOperations(ModuleOp module) {
  ARTS_DEBUG_HEADER(ArtsMetadataManager::importFromOperations);

  uint64_t importCount = 0;
  module.walk([&](Operation *op) {
    uint64_t before = metadataMap.size();
    importFromOperation(op);
    if (metadataMap.size() > before)
      importCount++;
  });

  ARTS_DEBUG("Imported metadata for " << importCount << " operations");
  ARTS_DEBUG_FOOTER(ArtsMetadataManager::importFromOperations);
}

/// Export all metadata objects to their respective operations
void ArtsMetadataManager::exportToOperations() {
  for (auto &entry : metadataMap)
    entry.second->exportToOp();
}

/// Walk the module and create metadata objects for operations with arts.*
/// attributes
void ArtsMetadataManager::collectFromModule(ModuleOp module) {
  ARTS_DEBUG_HEADER(ArtsMetadataManager::collectFromModule);

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

    /// Try to import metadata based on operation type
    importFromOperation(op);

    if (metadataMap.size() > before)
      importedCount++;
  });

  ARTS_DEBUG("Found " << opsWithArtsAttrs
                      << " operations with arts.* attributes");
  ARTS_DEBUG("Successfully imported metadata for " << importedCount
                                                   << " operations");
  ARTS_DEBUG_FOOTER(ArtsMetadataManager::collectFromModule);
}

///===----------------------------------------------------------------------===///
/// JSON Import/Export
///===----------------------------------------------------------------------===///

bool ArtsMetadataManager::importFromJson(llvm::StringRef jsonStr) {
  ARTS_DEBUG_HEADER(ArtsMetadataManager::importFromJson);

  /// Parse JSON
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

bool ArtsMetadataManager::importFromJsonFile(ModuleOp module,
                                             llvm::StringRef filename) {
  ARTS_DEBUG("Importing metadata from JSON file: " << filename);
  setMetadataFile(filename);

  if (!loadJsonCache())
    return false;

  return attachMetadataFromCache(module, filename);
}

std::string ArtsMetadataManager::exportToJson() const {
  llvm::json::Object root, loops, memrefs, locations, values;

  /// Export all metadata objects to JSON
  for (const auto &entry : metadataMap) {
    const ArtsMetadata *metadata = entry.second.get();

    llvm::json::Object metadataJson;
    metadata->exportToJson(metadataJson);
    std::string key = metadata->getMetadataId().has_value()
                          ? std::to_string(metadata->getMetadataId().value())
                          : metadata->toString().str();

    StringRef metadataName = metadata->getMetadataName();
    if (metadataName == AttrNames::LoopMetadata::Name)
      loops[key] = std::move(metadataJson);
    else if (metadataName == AttrNames::MemrefMetadata::Name)
      memrefs[key] = std::move(metadataJson);
    else if (metadataName == AttrNames::ValueMetadata::Name)
      values[key] = std::move(metadataJson);
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

  /// Convert to string
  std::string jsonStr;
  llvm::raw_string_ostream os(jsonStr);
  os << llvm::json::Value(std::move(root));
  return os.str();
}

bool ArtsMetadataManager::exportToJsonFile(llvm::StringRef filename) const {
  ARTS_DEBUG("Exporting metadata to JSON file: " << filename);

  std::string jsonStr = exportToJson();

  /// Write to file
  std::error_code ec;
  llvm::raw_fd_ostream file(filename, ec);
  if (ec) {
    ARTS_ERROR("Error opening file for writing: " << filename);
    return false;
  }

  file << jsonStr;
  ARTS_DEBUG("Successfully exported " << metadataMap.size()
                                      << " metadata entries");
  return true;
}

///===----------------------------------------------------------------------===///
/// Iteration and Inspection
///===----------------------------------------------------------------------===///

SmallVector<Operation *>
ArtsMetadataManager::getOperationsWithMetadata() const {
  SmallVector<Operation *> ops;
  ops.reserve(metadataMap.size());

  for (const auto &entry : metadataMap)
    ops.push_back(entry.first);

  return ops;
}

SmallVector<Operation *> ArtsMetadataManager::getLoopOperations() const {
  SmallVector<Operation *> ops;

  for (const auto &entry : metadataMap) {
    if (entry.second->getMetadataName() == AttrNames::LoopMetadata::Name)
      ops.push_back(entry.first);
  }

  return ops;
}

SmallVector<Operation *> ArtsMetadataManager::getMemrefOperations() const {
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

void ArtsMetadataManager::printStatistics(llvm::raw_ostream &os) const {
  uint64_t loopCount = 0, memrefCount = 0;

  for (const auto &entry : metadataMap) {
    StringRef name = entry.second->getMetadataName();
    if (name == AttrNames::LoopMetadata::Name)
      ++loopCount;
    else if (name == AttrNames::MemrefMetadata::Name)
      ++memrefCount;
  }

  os << "ArtsMetadataManager Statistics:\n";
  os << "  Total metadata entries: " << metadataMap.size() << "\n";
  os << "  Loop metadata: " << loopCount << "\n";
  os << "  Memref metadata: " << memrefCount << "\n";
}

void ArtsMetadataManager::dump() const {
  ARTS_DEBUG_REGION({
    ARTS_DBGS() << "ArtsMetadataManager Dump:\n";
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

bool ArtsMetadataManager::loadJsonCache() {
  if (jsonCacheInitialized)
    return !(loopJsonCache.empty() && memrefJsonCache.empty());

  if (!metadataFilePath.empty())
    idRegistry.loadFromJson(metadataFilePath);

  if (!serializedMetadataCache.empty())
    return loadJsonCacheFromString(serializedMetadataCache);

  auto fileOrErr = llvm::MemoryBuffer::getFile(metadataFilePath);
  if (!fileOrErr) {
    std::error_code ec = fileOrErr.getError();
    if (ec != std::errc::no_such_file_or_directory)
      ARTS_WARN("Failed to open metadata file '" << metadataFilePath
                                                 << "': " << ec.message());
    else
      ARTS_DEBUG("Metadata file not found: " << metadataFilePath);
    return false;
  }

  serializedMetadataCache = (*fileOrErr)->getBuffer().str();
  return loadJsonCacheFromString(serializedMetadataCache);
}

bool ArtsMetadataManager::loadJsonCacheFromString(llvm::StringRef jsonStr) {
  if (jsonStr.empty())
    return false;

  auto json = llvm::json::parse(jsonStr);
  if (!json) {
    ARTS_WARN("Failed to parse metadata JSON: " << toString(json.takeError()));
    return false;
  }

  auto *root = json->getAsObject();
  if (!root) {
    ARTS_WARN("Metadata JSON root is not an object");
    return false;
  }

  jsonCacheInitialized = populateJsonCaches(*root);
  return jsonCacheInitialized;
}

bool ArtsMetadataManager::populateJsonCaches(const llvm::json::Object &root) {
  loopJsonCache.clear();
  memrefJsonCache.clear();

  if (const auto *loopsSection = root.getObject(AttrNames::Metadata::Loops)) {
    for (const auto &entry : *loopsSection) {
      const auto *obj = entry.second.getAsObject();
      if (!obj)
        continue;
      auto store = [&](llvm::StringRef key) {
        loopJsonCache[key.str()] = std::make_unique<llvm::json::Object>(*obj);
      };
      store(entry.first);
      if (auto idVal = obj->getInteger(AttrNames::Metadata::ArtsId)) {
        idRegistry.recordUsedId(*idVal);
        store(std::to_string(*idVal));
      }
      if (auto loc = obj->getString(AttrNames::LoopMetadata::LocationKey))
        store(*loc);
    }
  }

  if (const auto *memrefsSection =
          root.getObject(AttrNames::Metadata::Memrefs)) {
    for (const auto &entry : *memrefsSection) {
      const auto *obj = entry.second.getAsObject();
      if (!obj)
        continue;
      auto store = [&](llvm::StringRef key) {
        memrefJsonCache[key.str()] = std::make_unique<llvm::json::Object>(*obj);
      };
      store(entry.first);
      if (auto idVal = obj->getInteger(AttrNames::Metadata::ArtsId)) {
        idRegistry.recordUsedId(*idVal);
        store(std::to_string(*idVal));
      }
      if (auto allocId =
              obj->getString(AttrNames::MemrefMetadata::AllocationId))
        store(*allocId);
    }
  }

  ARTS_DEBUG("Loaded metadata cache: loops="
             << loopJsonCache.size() << ", memrefs=" << memrefJsonCache.size());
  return true;
}

bool ArtsMetadataManager::attachMetadataFromJson(
    Operation *op, llvm::StringRef key,
    llvm::StringMap<std::unique_ptr<llvm::json::Object>> &cache, bool isLoop) {
  if (!loadJsonCache())
    return false;

  if (key.empty())
    return false;

  auto it = cache.find(key);
  if (it == cache.end())
    return false;

  if (isLoop) {
    auto metadata = std::make_unique<LoopMetadata>(op);
    metadata->importFromJson(*it->second);
    initializeMetadata(metadata.get());
    metadata->exportToOp();
    metadataMap[op] = std::move(metadata);
  } else {
    auto metadata = std::make_unique<MemrefMetadata>(op);
    metadata->importFromJson(*it->second);
    initializeMetadata(metadata.get());
    metadata->exportToOp();
    metadataMap[op] = std::move(metadata);
  }

  return true;
}

bool ArtsMetadataManager::attachLoopMetadataNearLocation(
    Operation *op, const LocationMetadata &loc, unsigned lineTolerance) {
  ARTS_DEBUG("attachLoopMetadataNearLocation: op=" << op->getName()
             << ", loc=" << loc.getKey() << ", tolerance=" << lineTolerance);
  if (!op || !loc.isValid() || lineTolerance == 0) {
    ARTS_DEBUG("  → early return: op=" << (op ? "valid" : "null")
               << ", loc.isValid=" << loc.isValid()
               << ", tolerance=" << lineTolerance);
    return false;
  }

  if (!loadJsonCache())
    return false;

  llvm::DenseSet<const llvm::json::Object *> visited;
  const llvm::json::Object *bestMatch = nullptr;
  unsigned bestDistance = std::numeric_limits<unsigned>::max();
  std::string baseFile = LocationMetadata::getBasename(loc.file);
  ARTS_DEBUG("  → searching for baseFile=" << baseFile << ", line=" << loc.line);

  for (auto &entry : loopJsonCache) {
    const llvm::json::Object *obj = entry.second.get();
    if (!visited.insert(obj).second)
      continue;

    auto locStr = obj->getString(AttrNames::LoopMetadata::LocationKey);
    if (!locStr)
      continue;

    LocationMetadata entryLoc = LocationMetadata::fromKey(*locStr);
    if (!entryLoc.isValid())
      continue;

    ARTS_DEBUG("  → candidate: file=" << entryLoc.file << ", line="
               << entryLoc.line << " (vs baseFile=" << baseFile << ")");

    if (entryLoc.file != baseFile) {
      ARTS_DEBUG("     → file mismatch, skipping");
      continue;
    }

    unsigned distance = static_cast<unsigned>(
        std::abs(static_cast<int>(entryLoc.line) - static_cast<int>(loc.line)));
    ARTS_DEBUG("     → distance=" << distance << ", tolerance=" << lineTolerance);
    if (distance > lineTolerance) {
      ARTS_DEBUG("     → distance too large, skipping");
      continue;
    }

    if (bestMatch && distance >= bestDistance)
      continue;

    bestMatch = obj;
    bestDistance = distance;
    ARTS_DEBUG("     → new best match!");
  }

  if (!bestMatch)
    return false;

  auto metadata = std::make_unique<LoopMetadata>(op);
  metadata->importFromJson(*bestMatch);
  initializeMetadata(metadata.get());
  metadata->exportToOp();
  metadataMap[op] = std::move(metadata);
  ARTS_DEBUG("Attached loop metadata near location " << loc.getKey());
  return true;
}

void ArtsMetadataManager::initializeMetadata(ArtsMetadata *metadata) {
  if (!metadata)
    return;
  Operation *op = metadata->getOperation();
  assert(op && "Operation must always be available");
  ArtsId id = metadata->getMetadataId();

  /// If metadata already has an ID (e.g., from JSON), preserve it
  if (id.has_value()) {
    /// Set the arts.id attribute on the operation if it doesn't have one
    if (!op->getAttrOfType<IntegerAttr>(ArtsMetadata::IdAttrName))
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

ArtsId ArtsMetadataManager::assignOperationId(Operation *op) {
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

/// Transfer metadata from source operation to target operation.
/// Updates all LoopMetadata entries that reference the old arts_id.
/// Returns true if transfer was successful, false otherwise.
bool ArtsMetadataManager::transferMetadata(Operation *sourceOp,
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

///===----------------------------------------------------------------------===///
// Helper Methods
///===----------------------------------------------------------------------===///

bool ArtsMetadataManager::isLoopOp(Operation *op) {
  return isa<affine::AffineForOp, scf::ForOp, scf::ParallelOp, scf::ForallOp,
             omp::WsLoopOp, arts::ForOp>(op);
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
  }
}

bool ArtsMetadataManager::attachMetadataFromCache(ModuleOp module,
                                                  llvm::StringRef source) {
  if (!module)
    return false;

  ARTS_DEBUG_HEADER(attachMetadataFromCache);
  ARTS_DEBUG("Source: " << source);

  uint64_t loopsAttached = 0, memrefsAttached = 0, loopsChecked = 0;
  module.walk([&](Operation *op) {
    if (isLoopOp(op)) {
      loopsChecked++;
      ARTS_DEBUG("Found loop op: " << op->getName());
      bool hadAttr = op->hasAttr(AttrNames::LoopMetadata::Name);
      if (ensureLoopMetadata(op) && !hadAttr) {
        loopsAttached++;
        ARTS_DEBUG("  → attached metadata!");
      }
      return;
    }

    if (isMemrefAllocOp(op)) {
      bool hadAttr = op->hasAttr(AttrNames::MemrefMetadata::Name);
      if (ensureMemrefMetadata(op) && !hadAttr)
        memrefsAttached++;
    }
  });
  ARTS_DEBUG("Checked " << loopsChecked << " loop ops");

  if (loopsAttached == 0 && memrefsAttached == 0)
    return false;

  ARTS_INFO("Attached metadata from " << source << ": loops=" << loopsAttached
                                      << ", memrefs=" << memrefsAttached);
  return true;
}
