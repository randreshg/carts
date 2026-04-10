///==========================================================================///
/// File: MetadataIO.h
///
/// JSON import/export and file I/O for metadata, with cache management.
///==========================================================================///

#ifndef ARTS_DIALECT_CORE_ANALYSIS_METADATA_METADATAIO_H
#define ARTS_DIALECT_CORE_ANALYSIS_METADATA_METADATAIO_H

#include "arts/utils/metadata/IdRegistry.h"
#include "arts/utils/metadata/Metadata.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/JSON.h"
#include <memory>
#include <string>

namespace mlir {
namespace arts {

class MetadataRegistry;

///===----------------------------------------------------------------------===///
/// MetadataIO - JSON import/export and file I/O for metadata
///
/// Handles parsing, serializing, and caching metadata in JSON format.
/// Manages the JSON cache used for attaching metadata to operations.
///===----------------------------------------------------------------------===///
class MetadataIO {
public:
  explicit MetadataIO(MetadataRegistry &registry) : registry(registry) {}

  MetadataIO(const MetadataIO &) = delete;
  MetadataIO &operator=(const MetadataIO &) = delete;
  MetadataIO(MetadataIO &&) = delete;
  MetadataIO &operator=(MetadataIO &&) = delete;

  ///===------------------------------------------------------------------===///
  /// JSON Import/Export
  ///===------------------------------------------------------------------===///
  bool importFromJson(llvm::StringRef jsonStr);
  std::string exportToJson() const;
  bool exportToJsonFile(llvm::StringRef filename) const;

  ///===------------------------------------------------------------------===///
  /// File Path Management
  ///===------------------------------------------------------------------===///
  void setMetadataFile(llvm::StringRef filename);
  const std::string &getMetadataFilePath() const { return metadataFilePath; }

  ///===------------------------------------------------------------------===///
  /// JSON Cache Management
  ///===------------------------------------------------------------------===///
  bool loadJsonCache();
  bool loadJsonCacheFromString(llvm::StringRef jsonStr);
  bool populateJsonCaches(const llvm::json::Object &root);

  /// Clear all cached data
  void clearCache() {
    loopJsonCache.clear();
    memrefJsonCache.clear();
    jsonCacheInitialized = false;
    serializedMetadataCache.clear();
  }

  ///===------------------------------------------------------------------===///
  /// Cache Accessors (for MetadataAttacher)
  ///===------------------------------------------------------------------===///
  llvm::StringMap<std::unique_ptr<llvm::json::Object>> &getLoopJsonCache() {
    return loopJsonCache;
  }
  llvm::StringMap<std::unique_ptr<llvm::json::Object>> &getMemrefJsonCache() {
    return memrefJsonCache;
  }

private:
  MetadataRegistry &registry;

  /// JSON cache state
  bool jsonCacheInitialized = false;
  std::string metadataFilePath = ".carts-metadata.json";
  llvm::StringMap<std::unique_ptr<llvm::json::Object>> loopJsonCache;
  llvm::StringMap<std::unique_ptr<llvm::json::Object>> memrefJsonCache;
  std::string serializedMetadataCache;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_DIALECT_CORE_ANALYSIS_METADATA_METADATAIO_H
