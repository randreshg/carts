////===----------------------------------------------------------------------===////
/// MetadataIO.cpp
////===----------------------------------------------------------------------===////

#include "arts/dialect/core/Analysis/metadata/MetadataIO.h"
#include "arts/dialect/core/Analysis/metadata/MetadataManager.h"
#include "arts/dialect/core/Analysis/metadata/MetadataRegistry.h"
#include "arts/utils/Debug.h"
#include "arts/utils/metadata/LoopMetadata.h"
#include "arts/utils/metadata/MemrefMetadata.h"
#include "arts/utils/metadata/ValueMetadata.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include <system_error>

ARTS_DEBUG_SETUP(metadata_io);

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
/// File Path Management
///===----------------------------------------------------------------------===///

void MetadataIO::setMetadataFile(llvm::StringRef filename) {
  if (filename.empty())
    return;
  metadataFilePath = filename.str();
  jsonCacheInitialized = false;
  loopJsonCache.clear();
  memrefJsonCache.clear();
  serializedMetadataCache.clear();
}

///===----------------------------------------------------------------------===///
/// JSON Import/Export
///===----------------------------------------------------------------------===///

bool MetadataIO::importFromJson(llvm::StringRef jsonStr) {
  ARTS_DEBUG_HEADER(MetadataIO::importFromJson);

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

  ARTS_DEBUG_FOOTER(MetadataIO::importFromJson);
  return true;
}

std::string MetadataIO::exportToJson() const {
  const auto &metadataMap = registry.getMetadataMap();

  llvm::json::Object root, loops, memrefs, locations, values;
  auto hasKey = [](const llvm::json::Object &obj, llvm::StringRef key) {
    for (const auto &entry : obj)
      if (entry.first.str() == key)
        return true;
    return false;
  };
  auto makeUniqueKey = [&](const llvm::json::Object &obj,
                           llvm::StringRef base) {
    std::string key = base.empty() ? "entry" : base.str();
    if (!hasKey(obj, key))
      return key;
    unsigned suffix = 1;
    while (true) {
      std::string candidate = key + "#" + std::to_string(suffix++);
      if (!hasKey(obj, candidate))
        return candidate;
    }
  };

  /// Export all metadata objects to JSON
  for (const auto &entry : metadataMap) {
    const ArtsMetadata *metadata = entry.second.get();

    llvm::json::Object metadataJson;
    metadata->exportToJson(metadataJson);
    std::string key = metadata->getMetadataId().has_value()
                          ? std::to_string(metadata->getMetadataId().value())
                          : metadata->toString().str();

    StringRef metadataName = metadata->getMetadataName();
    if (metadataName == AttrNames::LoopMetadata::Name) {
      key = makeUniqueKey(loops, key);
      loops[key] = std::move(metadataJson);
    } else if (metadataName == AttrNames::MemrefMetadata::Name) {
      key = makeUniqueKey(memrefs, key);
      memrefs[key] = std::move(metadataJson);
    } else if (metadataName == AttrNames::ValueMetadata::Name) {
      key = makeUniqueKey(values, key);
      values[key] = std::move(metadataJson);
    }
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

bool MetadataIO::exportToJsonFile(llvm::StringRef filename) const {
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
  ARTS_DEBUG("Successfully exported " << registry.size()
                                      << " metadata entries");
  return true;
}

///===----------------------------------------------------------------------===///
/// JSON Cache Management
///===----------------------------------------------------------------------===///

bool MetadataIO::loadJsonCache() {
  if (jsonCacheInitialized)
    return !(loopJsonCache.empty() && memrefJsonCache.empty());

  if (!metadataFilePath.empty())
    registry.getIdRegistry().loadFromJson(metadataFilePath);

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

bool MetadataIO::loadJsonCacheFromString(llvm::StringRef jsonStr) {
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

bool MetadataIO::populateJsonCaches(const llvm::json::Object &root) {
  loopJsonCache.clear();
  memrefJsonCache.clear();

  IdRegistry &idRegistry = registry.getIdRegistry();

  if (const auto *loopsSection = root.getObject(AttrNames::Metadata::Loops)) {
    for (const auto &entry : *loopsSection) {
      const auto *obj = entry.second.getAsObject();
      if (!obj)
        continue;
      loopJsonCache[entry.first] = std::make_unique<llvm::json::Object>(*obj);
      if (auto idVal = obj->getInteger(AttrNames::Metadata::ArtsId))
        idRegistry.recordUsedId(*idVal);
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
