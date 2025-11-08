///==========================================================================///
/// ArtsMetadataManager.h - Centralized Metadata Management
///
/// Centralized interface for managing metadata collection, attachment, and
/// recovery across MLIR operations.
///==========================================================================///

#ifndef ARTS_ANALYSIS_METADATA_ARTSMETADATAMANAGER_H
#define ARTS_ANALYSIS_METADATA_ARTSMETADATAMANAGER_H

#include "arts/Utils/Metadata/ArtsMetadata.h"
#include "arts/Utils/Metadata/LocationMetadata.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "arts/Utils/Metadata/MemrefMetadata.h"
#include "arts/Utils/Metadata/ValueMetadata.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/JSON.h"
#include <memory>
#include <optional>

namespace mlir {
namespace arts {

/// Attribute name constants for metadata
namespace AttrNames {
namespace Metadata {
using namespace llvm;
constexpr StringLiteral Version = "version";
constexpr StringLiteral Loops = "loops";
constexpr StringLiteral Memrefs = "memrefs";
constexpr StringLiteral Locations = "locations";
constexpr StringLiteral Values = "values";
constexpr StringLiteral Allocations = "allocations";
} // namespace Metadata
} // namespace AttrNames

///===--------------------------------------------------------------------===///
/// ArtsMetadataManager - Metadata management
///
/// This class manages the lifecycle of metadata objects and provides a clean
/// interface for metadata collection, attachment, and recovery. It maintains
/// a mapping from operations to their metadata and supports batch operations.
///
///===--------------------------------------------------------------------===///
class ArtsMetadataManager {
public:
  explicit ArtsMetadataManager(MLIRContext *context) : context_(context) {
    assert(context && "Context must not be null");
  }

  ArtsMetadataManager(const ArtsMetadataManager &) = delete;
  ArtsMetadataManager &operator=(const ArtsMetadataManager &) = delete;
  ArtsMetadataManager(ArtsMetadataManager &&) = default;
  ArtsMetadataManager &operator=(ArtsMetadataManager &&) = default;

  //===------------------------------------------------------------------===//
  // Metadata Collection
  //===------------------------------------------------------------------===//
  LoopMetadata *addLoopMetadata(Operation *op);
  MemrefMetadata *addMemrefMetadata(Operation *op);
  ValueMetadata *addValueMetadata(Operation *op);

  template <typename MetadataT> MetadataT *addMetadata(Operation *op) {
    static_assert(std::is_base_of<ArtsMetadata, MetadataT>::value,
                  "MetadataT must derive from ArtsMetadata");

    auto metadata = std::make_unique<MetadataT>(op);
    MetadataT *ptr = metadata.get();
    metadataMap_[op] = std::move(metadata);
    return ptr;
  }

  //===------------------------------------------------------------------===//
  // Metadata Retrieval
  //===------------------------------------------------------------------===//
  LoopMetadata *getLoopMetadata(Operation *op);
  const LoopMetadata *getLoopMetadata(Operation *op) const;
  MemrefMetadata *getMemrefMetadata(Operation *op);
  const MemrefMetadata *getMemrefMetadata(Operation *op) const;
  ValueMetadata *getValueMetadata(Operation *op);
  const ValueMetadata *getValueMetadata(Operation *op) const;

  /// Generic template method to get any metadata type
  /// Usage: auto *metadata = manager.getMetadata<LoopMetadata>(op)
  template <typename MetadataT> MetadataT *getMetadata(Operation *op) {
    static_assert(std::is_base_of<ArtsMetadata, MetadataT>::value,
                  "MetadataT must derive from ArtsMetadata");

    auto it = metadataMap_.find(op);
    if (it == metadataMap_.end())
      return nullptr;

    return static_cast<MetadataT *>(it->second.get());
  }

  /// Generic template method to get any metadata type
  template <typename MetadataT>
  const MetadataT *getMetadata(Operation *op) const {
    static_assert(std::is_base_of<ArtsMetadata, MetadataT>::value,
                  "MetadataT must derive from ArtsMetadata");

    auto it = metadataMap_.find(op);
    if (it == metadataMap_.end())
      return nullptr;

    return static_cast<const MetadataT *>(it->second.get());
  }

  //===------------------------------------------------------------------===//
  // Metadata Existence Checks
  //===------------------------------------------------------------------===//
  bool hasMetadata(Operation *op) const { return metadataMap_.count(op) > 0; }

  template <typename MetadataT> bool hasMetadata(Operation *op) const {
    return getMetadata<MetadataT>(op) != nullptr;
  }

  //===------------------------------------------------------------------===//
  // Metadata Removal
  //===------------------------------------------------------------------===//
  bool removeMetadata(Operation *op) { return metadataMap_.erase(op) > 0; }
  void clear() { metadataMap_.clear(); }

  //===------------------------------------------------------------------===//
  // Batch Operations
  //===------------------------------------------------------------------===//
  void importFromOperations(ModuleOp module);
  void exportToOperations();
  void collectFromModule(ModuleOp module);

  //===------------------------------------------------------------------===//
  // JSON Import/Export
  //===------------------------------------------------------------------===//
  bool importFromJson(llvm::StringRef jsonStr);
  bool importFromJsonFile(llvm::StringRef filename);
  std::string exportToJson() const;
  bool exportToJsonFile(llvm::StringRef filename) const;

  //===------------------------------------------------------------------===//
  // Iteration and Inspection
  //===------------------------------------------------------------------===//
  SmallVector<Operation *> getOperationsWithMetadata() const;
  SmallVector<Operation *> getLoopOperations() const;
  SmallVector<Operation *> getMemrefOperations() const;
  size_t size() const { return metadataMap_.size(); }
  bool empty() const { return metadataMap_.empty(); }
  MLIRContext *getContext() const { return context_; }

  //===------------------------------------------------------------------===//
  // Debug and Utilities
  //===------------------------------------------------------------------===//
  void printStatistics(llvm::raw_ostream &os) const;
  void dump() const;

private:
  MLIRContext *context_;
  llvm::DenseMap<Operation *, std::unique_ptr<ArtsMetadata>> metadataMap_;

  //===------------------------------------------------------------------===//
  // Helper Methods
  //===------------------------------------------------------------------===//
  static bool isLoopOp(Operation *op);
  static bool isMemrefAllocOp(Operation *op);
  void importFromOperation(Operation *op);
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_METADATA_ARTSMETADATAMANAGER_H

