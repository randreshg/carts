///==========================================================================///
/// File: Metadata.h
///
/// This file defines utilities for importing and exporting ARTS metadata
/// to/from JSON files. This centralizes metadata handling logic for reuse
/// across passes and tools.
///==========================================================================///

#ifndef ARTS_UTILS_ARTSMETADATA_H
#define ARTS_UTILS_ARTSMETADATA_H

#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/JSON.h"
#include <cstdint>
#include <optional>
#include <string>

namespace mlir {
namespace arts {

using ArtsId = std::optional<int64_t>;

///===----------------------------------------------------------------------===///
/// ArtsMetadata - Base class for all metadata
///===----------------------------------------------------------------------===///
class ArtsMetadata {
public:
  static constexpr llvm::StringLiteral IdAttrName = "arts.id";

  ArtsMetadata(Operation *op) : op_(op) {
    assert(op_ && "Operation must not be null");
  }
  virtual ~ArtsMetadata() = default;

  /// Get the name of the metadata attribute
  virtual StringRef getMetadataName() const = 0;

  /// Import and export metadata to/from the operation
  virtual bool importFromOp() = 0;
  virtual void exportToOp() {
    Attribute attr = getMetadataAttr();
    if (attr)
      op_->setAttr(getMetadataName(), attr);
    exportIdToOp();
  };

  /// Import/export metadata to/from JSON
  virtual void importFromJson(const llvm::json::Object &json) = 0;
  virtual void exportToJson(llvm::json::Object &json) const = 0;
  virtual StringRef toString() const = 0;

  Operation *getOperation() const { return op_; }
  void setOperation(Operation *op) {
    assert(op && "Operation must not be null");
    op_ = op;
  }
  ArtsId getMetadataId() const { return metadataId_; }
  void setMetadataId(ArtsId id) { metadataId_ = id; }

protected:
  void importIdFromOp() {
    if (!op_)
      return;
    if (auto attr = op_->getAttrOfType<IntegerAttr>(IdAttrName))
      metadataId_ = attr.getInt();
  }

  void exportIdToOp() const {
    if (!metadataId_.has_value() || !op_)
      return;
    auto *ctx = op_->getContext();
    auto i64Ty = IntegerType::get(ctx, 64);
    op_->setAttr(IdAttrName, IntegerAttr::get(i64Ty, metadataId_.value()));
  }

  void importIdFromJson(const llvm::json::Object &json,
                        llvm::StringRef key = "arts_id") {
    if (auto value = json.getInteger(key))
      metadataId_ = *value;
  }

  void exportIdToJson(llvm::json::Object &json,
                      llvm::StringRef key = "arts_id") const {
    if (metadataId_.has_value())
      json[key] = metadataId_.value();
  }

private:
  /// Get the metadata attribute based on the Metadata attributes
  virtual Attribute getMetadataAttr() const = 0;

protected:
  Operation *op_;
  ArtsId metadataId_;
};

///===----------------------------------------------------------------------===///
/// Metadata Attribute Utilities
///===----------------------------------------------------------------------===///

/// Extract boolean value from BoolAttr, returning false if attribute is null.
inline bool getBoolFromAttr(BoolAttr attr) {
  return attr ? attr.getValue() : false;
}

/// Extract integer value from IntegerAttr, returning nullopt if attribute is
/// null.
inline std::optional<int64_t> getIntFromAttr(IntegerAttr attr) {
  if (!attr)
    return std::nullopt;
  return attr.getInt();
}

} // namespace arts
} // namespace mlir

#endif /// ARTS_UTILS_ARTSMETADATA_H
