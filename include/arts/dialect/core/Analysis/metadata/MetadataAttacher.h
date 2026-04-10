///==========================================================================///
/// File: MetadataAttacher.h
///
/// Attaching metadata from JSON cache to MLIR operations.
///==========================================================================///

#ifndef ARTS_DIALECT_CORE_ANALYSIS_METADATA_METADATAATTACHER_H
#define ARTS_DIALECT_CORE_ANALYSIS_METADATA_METADATAATTACHER_H

#include "arts/utils/metadata/LocationMetadata.h"
#include "arts/utils/metadata/Metadata.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/JSON.h"
#include <memory>
#include <string>

namespace mlir {
namespace arts {

class MetadataRegistry;
class MetadataIO;

///===----------------------------------------------------------------------===///
/// MetadataAttacher - Attaching metadata to operations
///
/// Handles the logic of matching JSON metadata entries to MLIR operations
/// and attaching the appropriate metadata objects.
///===----------------------------------------------------------------------===///
class MetadataAttacher {
public:
  MetadataAttacher(MetadataRegistry &registry, MetadataIO &io)
      : registry(registry), io(io) {}

  MetadataAttacher(const MetadataAttacher &) = delete;
  MetadataAttacher &operator=(const MetadataAttacher &) = delete;
  MetadataAttacher(MetadataAttacher &&) = delete;
  MetadataAttacher &operator=(MetadataAttacher &&) = delete;

  ///===------------------------------------------------------------------===///
  /// Metadata Attachment from JSON
  ///===------------------------------------------------------------------===///

  /// Attach metadata from JSON cache to an operation using a key lookup.
  bool attachMetadataFromJson(
      Operation *op, llvm::StringRef key,
      llvm::StringMap<std::unique_ptr<llvm::json::Object>> &cache, bool isLoop);

  /// Attach loop metadata by finding the nearest location match.
  bool attachLoopMetadataNearLocation(Operation *op,
                                      const LocationMetadata &loc,
                                      unsigned lineTolerance = 1);

  ///===------------------------------------------------------------------===///
  /// Module-Level Attachment
  ///===------------------------------------------------------------------===///

  /// Import metadata from a JSON file and attach to module operations.
  bool importFromJsonFile(ModuleOp module, llvm::StringRef filename);

  /// Walk module and attach cached metadata to matching operations.
  bool attachMetadataFromCache(ModuleOp module, llvm::StringRef source);

  ///===------------------------------------------------------------------===///
  /// Ensure Methods (check in-memory, then attrs, then cache)
  ///===------------------------------------------------------------------===///
  bool ensureLoopMetadata(Operation *op);
  bool ensureMemrefMetadata(Operation *op);

private:
  MetadataRegistry &registry;
  MetadataIO &io;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_DIALECT_CORE_ANALYSIS_METADATA_METADATAATTACHER_H
