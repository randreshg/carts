////===----------------------------------------------------------------------===////
/// MetadataAttacher.cpp
////===----------------------------------------------------------------------===////

#include "arts/Analysis/Metadata/MetadataAttacher.h"
#include "arts/Analysis/Metadata/ArtsMetadataManager.h"
#include "arts/Analysis/Metadata/MetadataIO.h"
#include "arts/Analysis/Metadata/MetadataRegistry.h"
#include "arts/ArtsDialect.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/Metadata/LocationMetadata.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "arts/Utils/Metadata/MemrefMetadata.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringExtras.h"
#include <cmath>
#include <limits>

ARTS_DEBUG_SETUP(metadata_attacher);

using namespace mlir;
using namespace mlir::arts;

namespace {
static unsigned getLoopDepth(Operation *loopOp) {
  unsigned depth = 0;
  for (Operation *parent = loopOp ? loopOp->getParentOp() : nullptr; parent;
       parent = parent->getParentOp()) {
    if (isa<affine::AffineForOp, scf::ForOp, scf::ParallelOp, scf::ForallOp,
            omp::WsLoopOp, arts::ForOp>(parent))
      ++depth;
  }
  return depth;
}

static std::optional<int64_t> parseSignedInt(llvm::StringRef text) {
  int64_t value = 0;
  if (!llvm::to_integer(text, value))
    return std::nullopt;
  return value;
}
} // namespace

///===----------------------------------------------------------------------===///
/// Ensure Methods
///===----------------------------------------------------------------------===///

bool MetadataAttacher::ensureLoopMetadata(Operation *op) {
  if (!op)
    return false;

  ARTS_DEBUG("ensureLoopMetadata: " << op->getName());

  /// 1. Check if already in memory
  if (registry.getLoopMetadata(op)) {
    ARTS_DEBUG("  -> already in memory");
    return true;
  }

  /// 2. Check if op has metadata attribute
  if (op->getAttr(AttrNames::LoopMetadata::Name)) {
    ARTS_DEBUG("  -> has arts.loop attr, importing");
    registry.importFromOperation(op);
    return registry.getLoopMetadata(op) != nullptr;
  }

  /// 3. Try to attach from cache using arts.id or location
  LocationMetadata loc = LocationMetadata::fromLocation(op->getLoc());
  ARTS_DEBUG("  -> extracted location: " << loc.getKey()
                                         << " (file=" << loc.file
                                         << ", line=" << loc.line << ")");
  std::string key;
  if (auto idAttr = op->getAttrOfType<IntegerAttr>(ArtsMetadata::IdAttrName))
    key = std::to_string(idAttr.getInt());
  if (key.empty())
    key = loc.getKey().str();
  ARTS_DEBUG("  -> trying exact match with key: " << key);
  if (!key.empty() &&
      attachMetadataFromJson(op, key, io.getLoopJsonCache(), /*isLoop=*/true)) {
    ARTS_DEBUG("  -> exact match found!");
    return true;
  }

  ARTS_DEBUG("  -> no exact match, trying near-location match");
  if (!loc.isValid()) {
    ARTS_DEBUG("  -> location invalid, giving up");
    return false;
  }
  return attachLoopMetadataNearLocation(op, loc);
}

bool MetadataAttacher::ensureMemrefMetadata(Operation *op) {
  if (!op)
    return false;

  /// 1. Check if already in memory
  if (registry.getMemrefMetadata(op))
    return true;

  /// 2. Check if op has metadata attribute
  if (op->getAttr(AttrNames::MemrefMetadata::Name)) {
    registry.importFromOperation(op);
    return registry.getMemrefMetadata(op) != nullptr;
  }

  /// 3. Try to attach from cache using arts.id or location
  std::string key;
  if (auto idAttr = op->getAttrOfType<IntegerAttr>(ArtsMetadata::IdAttrName))
    key = std::to_string(idAttr.getInt());
  if (key.empty())
    key = LocationMetadata::fromLocation(op->getLoc()).getKey().str();
  if (key.empty())
    return false;
  return attachMetadataFromJson(op, key, io.getMemrefJsonCache(),
                                /*isLoop=*/false);
}

///===----------------------------------------------------------------------===///
/// Metadata Attachment from JSON
///===----------------------------------------------------------------------===///

bool MetadataAttacher::attachMetadataFromJson(
    Operation *op, llvm::StringRef key,
    llvm::StringMap<std::unique_ptr<llvm::json::Object>> &cache, bool isLoop) {
  if (!io.loadJsonCache())
    return false;

  if (key.empty())
    return false;

  auto &metadataMap = registry.getMetadataMap();

  const llvm::json::Object *matched = nullptr;
  if (isLoop) {
    const unsigned opLoopDepth = getLoopDepth(op);
    const auto requestedId = parseSignedInt(key);
    const auto opLoc = LocationMetadata::fromLocation(op->getLoc());
    const std::string opLocKey =
        opLoc.isValid() ? opLoc.getKey().str() : std::string();

    unsigned bestNestingDistance = std::numeric_limits<unsigned>::max();
    unsigned bestLineDistance = std::numeric_limits<unsigned>::max();
    std::string bestKey;

    for (const auto &entry : cache) {
      const llvm::json::Object *obj = entry.second.get();
      bool matches = entry.first() == key;

      if (!matches && requestedId.has_value()) {
        if (auto idVal = obj->getInteger(AttrNames::Metadata::ArtsId))
          matches = (*idVal == *requestedId);
      }
      if (!matches && !opLocKey.empty()) {
        if (auto locKey = obj->getString(AttrNames::LoopMetadata::LocationKey))
          matches = (*locKey == opLocKey);
      }

      if (!matches)
        continue;

      unsigned nestingDistance = std::numeric_limits<unsigned>::max();
      if (auto jsonNesting =
              obj->getInteger(AttrNames::LoopMetadata::NestingLevel))
        nestingDistance = static_cast<unsigned>(
            std::abs(static_cast<int64_t>(opLoopDepth) - *jsonNesting));

      unsigned lineDistance = std::numeric_limits<unsigned>::max();
      if (!opLocKey.empty()) {
        if (auto locKey =
                obj->getString(AttrNames::LoopMetadata::LocationKey)) {
          LocationMetadata entryLoc = LocationMetadata::fromKey(*locKey);
          if (entryLoc.isValid())
            lineDistance =
                static_cast<unsigned>(std::abs(static_cast<int>(entryLoc.line) -
                                               static_cast<int>(opLoc.line)));
        }
      }

      if (!matched || nestingDistance < bestNestingDistance ||
          (nestingDistance == bestNestingDistance &&
           lineDistance < bestLineDistance) ||
          (nestingDistance == bestNestingDistance &&
           lineDistance == bestLineDistance && entry.first() < bestKey)) {
        matched = obj;
        bestNestingDistance = nestingDistance;
        bestLineDistance = lineDistance;
        bestKey = entry.first().str();
      }
    }
  } else {
    auto it = cache.find(key);
    if (it == cache.end())
      return false;
    matched = it->second.get();
  }

  if (!matched)
    return false;

  if (isLoop) {
    auto metadata = std::make_unique<LoopMetadata>(op);
    metadata->importFromJson(*matched);
    registry.initializeMetadata(metadata.get());
    metadata->exportToOp();
    metadataMap[op] = std::move(metadata);
  } else {
    auto metadata = std::make_unique<MemrefMetadata>(op);
    metadata->importFromJson(*matched);
    registry.initializeMetadata(metadata.get());
    metadata->exportToOp();
    metadataMap[op] = std::move(metadata);
  }

  /// Consume this metadata object from all cache aliases so different
  /// operations don't reattach the same JSON entry.
  llvm::SmallVector<std::string, 4> keysToErase;
  for (const auto &entry : cache) {
    if (entry.second.get() == matched)
      keysToErase.push_back(entry.first().str());
  }
  for (const std::string &eraseKey : keysToErase)
    cache.erase(eraseKey);

  return true;
}

bool MetadataAttacher::attachLoopMetadataNearLocation(
    Operation *op, const LocationMetadata &loc, unsigned lineTolerance) {
  ARTS_DEBUG("attachLoopMetadataNearLocation: op="
             << op->getName() << ", loc=" << loc.getKey()
             << ", tolerance=" << lineTolerance);
  if (!op || !loc.isValid() || lineTolerance == 0) {
    ARTS_DEBUG("  -> early return: op=" << (op ? "valid" : "null")
                                        << ", loc.isValid=" << loc.isValid()
                                        << ", tolerance=" << lineTolerance);
    return false;
  }

  if (!io.loadJsonCache())
    return false;

  auto &loopJsonCache = io.getLoopJsonCache();
  auto &metadataMap = registry.getMetadataMap();

  llvm::DenseSet<const llvm::json::Object *> visited;
  const llvm::json::Object *bestMatch = nullptr;
  std::string bestMatchKey;
  unsigned bestDistance = std::numeric_limits<unsigned>::max();
  unsigned bestNestingDistance = std::numeric_limits<unsigned>::max();
  const unsigned opLoopDepth = getLoopDepth(op);
  std::string baseFile = LocationMetadata::getBasename(loc.file);
  ARTS_DEBUG("  -> searching for baseFile=" << baseFile
                                            << ", line=" << loc.line);

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

    ARTS_DEBUG("  -> candidate: file=" << entryLoc.file
                                       << ", line=" << entryLoc.line
                                       << " (vs baseFile=" << baseFile << ")");

    if (entryLoc.file != baseFile) {
      ARTS_DEBUG("     -> file mismatch, skipping");
      continue;
    }

    unsigned distance = static_cast<unsigned>(
        std::abs(static_cast<int>(entryLoc.line) - static_cast<int>(loc.line)));
    ARTS_DEBUG("     -> distance=" << distance
                                   << ", tolerance=" << lineTolerance);
    if (distance > lineTolerance) {
      ARTS_DEBUG("     -> distance too large, skipping");
      continue;
    }

    unsigned nestingDistance = std::numeric_limits<unsigned>::max();
    if (auto jsonNesting =
            obj->getInteger(AttrNames::LoopMetadata::NestingLevel))
      nestingDistance = static_cast<unsigned>(
          std::abs(static_cast<int64_t>(opLoopDepth) - *jsonNesting));

    bool better = false;
    if (!bestMatch) {
      better = true;
    } else if (distance < bestDistance) {
      better = true;
    } else if (distance == bestDistance &&
               nestingDistance < bestNestingDistance) {
      better = true;
    } else if (distance == bestDistance &&
               nestingDistance == bestNestingDistance &&
               entry.first() < bestMatchKey) {
      /// Deterministic final tie-break.
      better = true;
    }
    if (!better)
      continue;

    bestMatch = obj;
    bestMatchKey = entry.first().str();
    bestDistance = distance;
    bestNestingDistance = nestingDistance;
    ARTS_DEBUG("     -> new best match!");
  }

  if (!bestMatch)
    return false;

  auto metadata = std::make_unique<LoopMetadata>(op);
  metadata->importFromJson(*bestMatch);
  registry.initializeMetadata(metadata.get());
  metadata->exportToOp();
  metadataMap[op] = std::move(metadata);

  /// Consume matched metadata object aliases from cache.
  llvm::SmallVector<std::string, 4> keysToErase;
  for (const auto &entry : loopJsonCache) {
    if (entry.second.get() == bestMatch)
      keysToErase.push_back(entry.first().str());
  }
  for (const std::string &eraseKey : keysToErase)
    loopJsonCache.erase(eraseKey);

  ARTS_DEBUG("Attached loop metadata near location " << loc.getKey());
  return true;
}

///===----------------------------------------------------------------------===///
/// Module-Level Attachment
///===----------------------------------------------------------------------===///

bool MetadataAttacher::importFromJsonFile(ModuleOp module,
                                          llvm::StringRef filename) {
  ARTS_DEBUG("Importing metadata from JSON file: " << filename);
  io.setMetadataFile(filename);

  if (!io.loadJsonCache())
    return false;

  return attachMetadataFromCache(module, filename);
}

bool MetadataAttacher::attachMetadataFromCache(ModuleOp module,
                                               llvm::StringRef source) {
  if (!module)
    return false;

  ARTS_DEBUG_HEADER(attachMetadataFromCache);
  ARTS_DEBUG("Source: " << source);

  uint64_t loopsAttached = 0, memrefsAttached = 0, loopsChecked = 0;
  module.walk([&](Operation *op) {
    if (MetadataRegistry::isLoopOp(op)) {
      loopsChecked++;
      ARTS_DEBUG("Found loop op: " << op->getName());
      bool hadAttr = op->hasAttr(AttrNames::LoopMetadata::Name);
      if (ensureLoopMetadata(op) && !hadAttr) {
        loopsAttached++;
        ARTS_DEBUG("  -> attached metadata!");
      }
      return;
    }

    if (MetadataRegistry::isMemrefAllocOp(op)) {
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
