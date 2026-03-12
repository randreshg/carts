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

#ifndef ARTS_ANALYSIS_METADATA_ARTSMETADATAMANAGER_H
#define ARTS_ANALYSIS_METADATA_ARTSMETADATAMANAGER_H

#include "arts/analysis/metadata/MetadataAttacher.h"
#include "arts/analysis/metadata/MetadataIO.h"
#include "arts/analysis/metadata/MetadataRegistry.h"
#include "arts/utils/metadata/Metadata.h"
#include "arts/utils/metadata/IdRegistry.h"
#include "arts/utils/metadata/LocationMetadata.h"
#include "arts/utils/metadata/LoopMetadata.h"
#include "arts/utils/metadata/MemrefMetadata.h"
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
  explicit MetadataManager(MLIRContext *context)
      : registry(context), io(registry), attacher(registry, io) {}

  MetadataManager(const MetadataManager &) = delete;
  MetadataManager &operator=(const MetadataManager &) = delete;
  MetadataManager(MetadataManager &&) = delete;
  MetadataManager &operator=(MetadataManager &&) = delete;

  ///===--------------------------------------------------------------------===///
  /// Metadata Collection (delegated to MetadataRegistry)
  ///===--------------------------------------------------------------------===///
  LoopMetadata *addLoopMetadata(Operation *op) {
    return registry.addLoopMetadata(op);
  }
  MemrefMetadata *addMemrefMetadata(Operation *op) {
    return registry.addMemrefMetadata(op);
  }
  ValueMetadata *addValueMetadata(Operation *op) {
    return registry.addValueMetadata(op);
  }

  template <typename MetadataT> MetadataT *addMetadata(Operation *op) {
    return registry.addMetadata<MetadataT>(op);
  }

  ///===--------------------------------------------------------------------===///
  /// Metadata Retrieval (delegated to MetadataRegistry)
  ///===--------------------------------------------------------------------===///
  LoopMetadata *getLoopMetadata(Operation *op) {
    return registry.getLoopMetadata(op);
  }
  const LoopMetadata *getLoopMetadata(Operation *op) const {
    return registry.getLoopMetadata(op);
  }
  MemrefMetadata *getMemrefMetadata(Operation *op) {
    return registry.getMemrefMetadata(op);
  }
  const MemrefMetadata *getMemrefMetadata(Operation *op) const {
    return registry.getMemrefMetadata(op);
  }
  MemrefMetadata *getMemrefMetadataById(int64_t artsId) {
    return registry.getMemrefMetadataById(artsId);
  }
  const MemrefMetadata *getMemrefMetadataById(int64_t artsId) const {
    return registry.getMemrefMetadataById(artsId);
  }
  ValueMetadata *getValueMetadata(Operation *op) {
    return registry.getValueMetadata(op);
  }
  const ValueMetadata *getValueMetadata(Operation *op) const {
    return registry.getValueMetadata(op);
  }

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
  bool hasMetadata(Operation *op) const { return registry.hasMetadata(op); }

  template <typename MetadataT> bool hasMetadata(Operation *op) const {
    return registry.hasMetadata<MetadataT>(op);
  }

  ///===--------------------------------------------------------------------===///
  /// Metadata Removal (delegated to MetadataRegistry)
  ///===--------------------------------------------------------------------===///
  bool removeMetadata(Operation *op) { return registry.removeMetadata(op); }
  void clear() {
    registry.getMetadataMap().clear();
    io.clearCache();
    registry.getIdRegistry().reset();
  }

  ///===--------------------------------------------------------------------===///
  /// Batch Operations (delegated to MetadataRegistry)
  ///===--------------------------------------------------------------------===///
  void importFromOperations(ModuleOp module) {
    registry.importFromOperations(module);
  }
  void exportToOperations() { registry.exportToOperations(); }
  void collectFromModule(ModuleOp module) {
    registry.collectFromModule(module);
  }

  ///===--------------------------------------------------------------------===///
  /// JSON Import/Export (delegated to MetadataIO and MetadataAttacher)
  ///===--------------------------------------------------------------------===///
  bool importFromJson(llvm::StringRef jsonStr) {
    return io.importFromJson(jsonStr);
  }
  bool importFromJsonFile(ModuleOp module, llvm::StringRef filename) {
    return attacher.importFromJsonFile(module, filename);
  }
  std::string exportToJson() const { return io.exportToJson(); }
  bool exportToJsonFile(llvm::StringRef filename) const {
    return io.exportToJsonFile(filename);
  }

  ///===--------------------------------------------------------------------===///
  /// Iteration and Inspection (delegated to MetadataRegistry)
  ///===--------------------------------------------------------------------===///
  SmallVector<Operation *> getOperationsWithMetadata() const {
    return registry.getOperationsWithMetadata();
  }
  SmallVector<Operation *> getLoopOperations() const {
    return registry.getLoopOperations();
  }
  SmallVector<Operation *> getMemrefOperations() const {
    return registry.getMemrefOperations();
  }
  size_t size() const { return registry.size(); }
  bool empty() const { return registry.empty(); }
  MLIRContext *getContext() const { return registry.getContext(); }

  ///===--------------------------------------------------------------------===///
  /// Metadata Helpers (delegated to MetadataAttacher and MetadataRegistry)
  ///===--------------------------------------------------------------------===///
  void setMetadataFile(llvm::StringRef filename) {
    io.setMetadataFile(filename);
  }
  bool ensureLoopMetadata(Operation *op) {
    return attacher.ensureLoopMetadata(op);
  }
  bool ensureMemrefMetadata(Operation *op) {
    return attacher.ensureMemrefMetadata(op);
  }
  ArtsId assignOperationId(Operation *op) {
    return registry.assignOperationId(op);
  }
  bool transferMetadata(Operation *sourceOp, Operation *targetOp) {
    return registry.transferMetadata(sourceOp, targetOp);
  }

  ///===--------------------------------------------------------------------===///
  /// ID Registry Access (delegated to MetadataRegistry)
  ///===--------------------------------------------------------------------===///
  IdRegistry &getIdRegistry() { return registry.getIdRegistry(); }
  const IdRegistry &getIdRegistry() const { return registry.getIdRegistry(); }

  ///===--------------------------------------------------------------------===///
  /// Debug and Utilities (delegated to MetadataRegistry)
  ///===--------------------------------------------------------------------===///
  void printStatistics(llvm::raw_ostream &os) const {
    registry.printStatistics(os);
  }
  void dump() const { registry.dump(); }

  ///===--------------------------------------------------------------------===///
  /// Component Access (for advanced usage)
  ///===--------------------------------------------------------------------===///
  MetadataRegistry &getRegistry() { return registry; }
  const MetadataRegistry &getRegistry() const { return registry; }
  MetadataIO &getIO() { return io; }
  const MetadataIO &getIO() const { return io; }
  MetadataAttacher &getAttacher() { return attacher; }
  const MetadataAttacher &getAttacher() const { return attacher; }

private:
  MetadataRegistry registry;
  MetadataIO io;
  MetadataAttacher attacher;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_METADATA_ARTSMETADATAMANAGER_H
