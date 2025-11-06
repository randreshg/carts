//===----------------------------------------------------------------------===//
// ArtsMetadata.h - Metadata Import/Export Utilities
//===----------------------------------------------------------------------===//
//
// This file defines utilities for importing and exporting ARTS metadata
// to/from JSON files. This centralizes metadata handling logic for reuse
// across passes and tools.
//
//===----------------------------------------------------------------------===//

#ifndef ARTS_UTILS_ARTSMETADATA_H
#define ARTS_UTILS_ARTSMETADATA_H

#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/JSON.h"
#include <optional>
#include <string>

namespace mlir {
namespace arts {

//===----------------------------------------------------------------------===//
// ArtsMetadata - Base class for all metadata
//===----------------------------------------------------------------------===//
class ArtsMetadata {
public:
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
  };

  /// Import/export metadata to/from JSON
  virtual void importFromJson(const llvm::json::Object &json) = 0;
  virtual void exportToJson(llvm::json::Object &json) const = 0;
  virtual StringRef toString() const = 0;

private:
  /// Get the metadata attribute based on the Metadata attributes
  virtual Attribute getMetadataAttr() const = 0;

protected:
  Operation *op_;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_UTILS_ARTSMETADATA_H
