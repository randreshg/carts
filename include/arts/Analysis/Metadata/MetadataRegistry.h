///==========================================================================///
/// File: MetadataRegistry.h
///
/// Registry for storing, retrieving, and managing metadata objects
/// associated with MLIR operations.
///==========================================================================///

#ifndef ARTS_ANALYSIS_METADATA_METADATAREGISTRY_H
#define ARTS_ANALYSIS_METADATA_METADATAREGISTRY_H

#include "arts/Utils/Metadata/ArtsMetadata.h"
#include "arts/Utils/Metadata/IdRegistry.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "arts/Utils/Metadata/MemrefMetadata.h"
#include "arts/Utils/Metadata/ValueMetadata.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include <memory>

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// MetadataRegistry - Storage and retrieval of metadata objects
///
/// Manages the lifecycle of metadata objects and provides methods for
/// adding, retrieving, removing, and iterating over metadata entries.
///===----------------------------------------------------------------------===///
class MetadataRegistry {
public:
  explicit MetadataRegistry(MLIRContext *context) : context(context) {
    assert(context && "Context must not be null");
  }

  MetadataRegistry(const MetadataRegistry &) = delete;
  MetadataRegistry &operator=(const MetadataRegistry &) = delete;
  MetadataRegistry(MetadataRegistry &&) = default;
  MetadataRegistry &operator=(MetadataRegistry &&) = default;

  ///===------------------------------------------------------------------===///
  /// Metadata Addition
  ///===------------------------------------------------------------------===///
  LoopMetadata *addLoopMetadata(Operation *op);
  MemrefMetadata *addMemrefMetadata(Operation *op);
  ValueMetadata *addValueMetadata(Operation *op);

  template <typename MetadataT> MetadataT *addMetadata(Operation *op);

  ///===------------------------------------------------------------------===///
  /// Metadata Retrieval
  ///===------------------------------------------------------------------===///
  LoopMetadata *getLoopMetadata(Operation *op);
  const LoopMetadata *getLoopMetadata(Operation *op) const;
  MemrefMetadata *getMemrefMetadata(Operation *op);
  const MemrefMetadata *getMemrefMetadata(Operation *op) const;
  MemrefMetadata *getMemrefMetadataById(int64_t artsId);
  const MemrefMetadata *getMemrefMetadataById(int64_t artsId) const;
  ValueMetadata *getValueMetadata(Operation *op);
  const ValueMetadata *getValueMetadata(Operation *op) const;

  template <typename MetadataT> MetadataT *getMetadata(Operation *op) {
    static_assert(std::is_base_of<ArtsMetadata, MetadataT>::value,
                  "MetadataT must derive from ArtsMetadata");

    auto it = metadataMap.find(op);
    if (it == metadataMap.end())
      return nullptr;

    return static_cast<MetadataT *>(it->second.get());
  }

  template <typename MetadataT>
  const MetadataT *getMetadata(Operation *op) const {
    static_assert(std::is_base_of<ArtsMetadata, MetadataT>::value,
                  "MetadataT must derive from ArtsMetadata");

    auto it = metadataMap.find(op);
    if (it == metadataMap.end())
      return nullptr;

    return static_cast<const MetadataT *>(it->second.get());
  }

  ///===------------------------------------------------------------------===///
  /// Metadata Existence Checks
  ///===------------------------------------------------------------------===///
  bool hasMetadata(Operation *op) const { return metadataMap.count(op) > 0; }

  template <typename MetadataT> bool hasMetadata(Operation *op) const {
    return getMetadata<MetadataT>(op) != nullptr;
  }

  ///===------------------------------------------------------------------===///
  /// Metadata Removal
  ///===------------------------------------------------------------------===///
  bool removeMetadata(Operation *op) { return metadataMap.erase(op) > 0; }

  ///===------------------------------------------------------------------===///
  /// Batch Operations
  ///===------------------------------------------------------------------===///
  void importFromOperations(ModuleOp module);
  void exportToOperations();
  void collectFromModule(ModuleOp module);

  ///===------------------------------------------------------------------===///
  /// Iteration and Inspection
  ///===------------------------------------------------------------------===///
  SmallVector<Operation *> getOperationsWithMetadata() const;
  SmallVector<Operation *> getLoopOperations() const;
  SmallVector<Operation *> getMemrefOperations() const;
  size_t size() const { return metadataMap.size(); }
  bool empty() const { return metadataMap.empty(); }

  ///===------------------------------------------------------------------===///
  /// Metadata Helpers
  ///===------------------------------------------------------------------===///
  void initializeMetadata(ArtsMetadata *metadata);
  ArtsId assignOperationId(Operation *op);
  bool transferMetadata(Operation *sourceOp, Operation *targetOp);

  ///===------------------------------------------------------------------===///
  /// Operation Classification
  ///===------------------------------------------------------------------===///
  static bool isLoopOp(Operation *op);
  static bool isMemrefAllocOp(Operation *op);

  ///===------------------------------------------------------------------===///
  /// Debug and Utilities
  ///===------------------------------------------------------------------===///
  void printStatistics(llvm::raw_ostream &os) const;
  void dump() const;

  ///===------------------------------------------------------------------===///
  /// Accessors
  ///===------------------------------------------------------------------===///
  MLIRContext *getContext() const { return context; }
  IdRegistry &getIdRegistry() { return idRegistry; }
  const IdRegistry &getIdRegistry() const { return idRegistry; }

  /// Direct access to the metadata map (for MetadataIO and MetadataAttacher)
  llvm::DenseMap<Operation *, std::unique_ptr<ArtsMetadata>> &
  getMetadataMap() {
    return metadataMap;
  }
  const llvm::DenseMap<Operation *, std::unique_ptr<ArtsMetadata>> &
  getMetadataMap() const {
    return metadataMap;
  }

  /// Import metadata from a single operation's existing attributes.
  void importFromOperation(Operation *op);

private:
  MLIRContext *context;
  llvm::DenseMap<Operation *, std::unique_ptr<ArtsMetadata>> metadataMap;
  IdRegistry idRegistry;
};

template <typename MetadataT>
MetadataT *MetadataRegistry::addMetadata(Operation *op) {
  static_assert(std::is_base_of<ArtsMetadata, MetadataT>::value,
                "MetadataT must derive from ArtsMetadata");
  auto metadata = std::make_unique<MetadataT>(op);
  MetadataT *ptr = metadata.get();
  initializeMetadata(ptr);
  metadataMap[op] = std::move(metadata);
  return ptr;
}

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_METADATA_METADATAREGISTRY_H
