///==========================================================================///
/// File: MetadataManager.h
///
/// Centralized interface for managing metadata collection, attachment, and
/// recovery across MLIR operations.
///
/// This class acts as a facade, delegating to three internal components:
///   - MetadataRegistry: storage, retrieval, and lifecycle of metadata objects
///   - MetadataIO: JSON import/export, file I/O, and cache management
///   - MetadataAttacher: attaching metadata from JSON cache to operations
///==========================================================================///

#ifndef ARTS_DIALECT_CORE_ANALYSIS_METADATA_METADATAMANAGER_H
#define ARTS_DIALECT_CORE_ANALYSIS_METADATA_METADATAMANAGER_H

#include "arts/dialect/core/Analysis/metadata/MetadataAttacher.h"
#include "arts/dialect/core/Analysis/metadata/MetadataIO.h"
#include "arts/dialect/core/Analysis/metadata/MetadataRegistry.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/metadata/IdRegistry.h"
#include "arts/utils/metadata/LocationMetadata.h"
#include "arts/utils/metadata/LoopMetadata.h"
#include "arts/utils/metadata/MemrefMetadata.h"
#include "arts/utils/metadata/Metadata.h"
#include "arts/utils/metadata/ValueMetadata.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/JSON.h"
#include <memory>
#include <optional>
#include <string>

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
constexpr StringLiteral SerializedBlob = "arts.metadata.serialized";

/// Shared JSON keys for nested location objects
constexpr StringLiteral ArtsId = "arts_id";
constexpr StringLiteral File = "file";
constexpr StringLiteral Line = "line";
constexpr StringLiteral Column = "column";
} // namespace Metadata
} // namespace AttrNames

///===--------------------------------------------------------------------===///
/// MetadataManager - Facade for metadata management
///
/// This class preserves the original public API and delegates internally
/// to MetadataRegistry, MetadataIO, and MetadataAttacher.
///===--------------------------------------------------------------------===///
class MetadataManager {
public:
  explicit MetadataManager(MLIRContext *context);

  MetadataManager(const MetadataManager &) = delete;
  MetadataManager &operator=(const MetadataManager &) = delete;
  MetadataManager(MetadataManager &&) = delete;
  MetadataManager &operator=(MetadataManager &&) = delete;

  ///===--------------------------------------------------------------------===///
  /// Metadata Collection (delegated to MetadataRegistry)
  ///===--------------------------------------------------------------------===///
  LoopMetadata *addLoopMetadata(Operation *op);
  MemrefMetadata *addMemrefMetadata(Operation *op);
  ValueMetadata *addValueMetadata(Operation *op);

  template <typename MetadataT> MetadataT *addMetadata(Operation *op) {
    return registry.addMetadata<MetadataT>(op);
  }

  ///===--------------------------------------------------------------------===///
  /// Metadata Retrieval (delegated to MetadataRegistry)
  ///===--------------------------------------------------------------------===///
  LoopMetadata *getLoopMetadata(Operation *op);
  const LoopMetadata *getLoopMetadata(Operation *op) const;
  MemrefMetadata *getMemrefMetadata(Operation *op);
  const MemrefMetadata *getMemrefMetadata(Operation *op) const;
  MemrefMetadata *getMemrefMetadataById(int64_t artsId);
  const MemrefMetadata *getMemrefMetadataById(int64_t artsId) const;
  ValueMetadata *getValueMetadata(Operation *op);
  const ValueMetadata *getValueMetadata(Operation *op) const;

  template <typename MetadataT> MetadataT *getMetadata(Operation *op) {
    return registry.getMetadata<MetadataT>(op);
  }

  template <typename MetadataT>
  const MetadataT *getMetadata(Operation *op) const {
    return registry.getMetadata<MetadataT>(op);
  }

  ///===--------------------------------------------------------------------===///
  /// Metadata Existence Checks (delegated to MetadataRegistry)
  ///===--------------------------------------------------------------------===///
  bool hasMetadata(Operation *op) const;

  template <typename MetadataT> bool hasMetadata(Operation *op) const {
    return registry.hasMetadata<MetadataT>(op);
  }

  ///===--------------------------------------------------------------------===///
  /// Metadata Removal (delegated to MetadataRegistry)
  ///===--------------------------------------------------------------------===///
  bool removeMetadata(Operation *op);
  void clear();

  ///===--------------------------------------------------------------------===///
  /// Batch Operations (delegated to MetadataRegistry)
  ///===--------------------------------------------------------------------===///
  void importFromOperations(ModuleOp module);
  void exportToOperations();
  void collectFromModule(ModuleOp module);

  ///===--------------------------------------------------------------------===///
  /// JSON Import/Export (delegated to MetadataIO and MetadataAttacher)
  ///===--------------------------------------------------------------------===///
  bool importFromJson(llvm::StringRef jsonStr);
  bool importFromJsonFile(ModuleOp module, llvm::StringRef filename);
  std::string exportToJson() const;
  bool exportToJsonFile(llvm::StringRef filename) const;

  ///===--------------------------------------------------------------------===///
  /// Iteration and Inspection (delegated to MetadataRegistry)
  ///===--------------------------------------------------------------------===///
  SmallVector<Operation *> getOperationsWithMetadata() const;
  SmallVector<Operation *> getLoopOperations() const;
  SmallVector<Operation *> getMemrefOperations() const;
  size_t size() const;
  bool empty() const;
  MLIRContext *getContext() const;

  ///===--------------------------------------------------------------------===///
  /// Metadata Helpers (delegated to MetadataAttacher and MetadataRegistry)
  ///===--------------------------------------------------------------------===///
  void setMetadataFile(llvm::StringRef filename);
  bool ensureLoopMetadata(Operation *op);
  bool ensureMemrefMetadata(Operation *op);
  ArtsId assignOperationId(Operation *op);
  bool transferMetadata(Operation *sourceOp, Operation *targetOp);
  bool replaceMetadataForRewrite(Operation *sourceOp, Operation *targetOp);
  bool rewriteMetadata(Operation *sourceOp, Operation *targetOp);
  bool rewriteLoopMetadata(Operation *sourceOp, Operation *targetOp);
  template <typename AdjustFn>
  bool rewriteLoopMetadata(Operation *sourceOp, Operation *targetOp,
                           AdjustFn &&adjustFn) {
    if (!rewriteLoopMetadata(sourceOp, targetOp))
      return false;
    auto *loopMetadata = getLoopMetadata(targetOp);
    if (!loopMetadata)
      return false;
    adjustFn(*loopMetadata);
    loopMetadata->exportToOp();
    return true;
  }
  bool cloneLoopMetadata(Operation *sourceOp, Operation *targetOp);
  template <typename AdjustFn>
  bool cloneLoopMetadata(Operation *sourceOp, Operation *targetOp,
                         AdjustFn &&adjustFn) {
    if (!cloneLoopMetadata(sourceOp, targetOp))
      return false;
    auto *loopMetadata = getLoopMetadata(targetOp);
    if (!loopMetadata)
      return false;
    adjustFn(*loopMetadata);
    loopMetadata->exportToOp();
    return true;
  }
  bool refreshMetadata(Operation *op);

  ///===--------------------------------------------------------------------===///
  /// ID Registry Access (delegated to MetadataRegistry)
  ///===--------------------------------------------------------------------===///
  IdRegistry &getIdRegistry();
  const IdRegistry &getIdRegistry() const;

  ///===--------------------------------------------------------------------===///
  /// Debug and Utilities (delegated to MetadataRegistry)
  ///===--------------------------------------------------------------------===///
  void printStatistics(llvm::raw_ostream &os) const;
  void dump() const;

  ///===--------------------------------------------------------------------===///
  /// Component Access (for advanced usage)
  ///===--------------------------------------------------------------------===///
  MetadataRegistry &getRegistry();
  const MetadataRegistry &getRegistry() const;
  MetadataIO &getIO();
  const MetadataIO &getIO() const;
  MetadataAttacher &getAttacher();
  const MetadataAttacher &getAttacher() const;

private:
  void stampTransferredMetadata(Operation *sourceOp, Operation *targetOp);

  MetadataRegistry registry;
  MetadataIO io;
  MetadataAttacher attacher;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_DIALECT_CORE_ANALYSIS_METADATA_METADATAMANAGER_H
