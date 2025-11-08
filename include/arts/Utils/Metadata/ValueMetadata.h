///==========================================================================///
/// File: ValueMetadata.h
///
/// This file defines the ValueMetadata class for tracking the source and
/// provenance of values in the MLIR IR. This is useful for understanding
/// where dynamic values (like array dimensions) come from.
///==========================================================================///

#ifndef ARTS_UTILS_VALUEMETADATA_H
#define ARTS_UTILS_VALUEMETADATA_H

#include "arts/Utils/Metadata/ArtsMetadata.h"
#include "arts/Utils/Metadata/LocationMetadata.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
#include <string>

namespace mlir {
namespace arts {

/// Attribute name constants for value metadata
namespace AttrNames {
namespace ValueMetadata {
using namespace llvm;
constexpr StringLiteral Name = "arts.value";
constexpr StringLiteral SourceType = "source_type";
constexpr StringLiteral Index = "index";
constexpr StringLiteral VarName = "var_name";
} // namespace ValueMetadata
} // namespace AttrNames

///===----------------------------------------------------------------------===///
/// ValueMetadata - Manages metadata for value provenance
///
/// This class tracks where values come from in the IR, which is particularly
/// useful for understanding dynamic values like array dimensions, loop bounds,
/// and stride values.
///===----------------------------------------------------------------------===///
class ValueMetadata : public ArtsMetadata {
public:
  ///===------------------------------------------------------------------===///
  /// Enums
  ///===------------------------------------------------------------------===///

  /// Source type for a value
  enum class SourceType {
    FunctionArgument,
    LoopInductionVar,
    Constant,
    AllocationSize,
    Unknown
  };

  ///===------------------------------------------------------------------===///
  /// Attributes
  ///===------------------------------------------------------------------===///
  SourceType type = SourceType::Unknown;
  unsigned index = 0;
  std::string varName;
  LocationMetadata location;

  ///===------------------------------------------------------------------===///
  /// Constructors
  ///===------------------------------------------------------------------===///
  explicit ValueMetadata(Operation *op) : ArtsMetadata(op) {}

  ///===------------------------------------------------------------------===///
  /// Interface
  ///===------------------------------------------------------------------===///
  StringRef getMetadataName() const override {
    return AttrNames::ValueMetadata::Name;
  }

  bool importFromOp() override;
  void exportToOp() override;
  void importFromJson(const llvm::json::Object &json) override;
  void exportToJson(llvm::json::Object &json) const override;
  StringRef toString() const override;

private:
  Attribute getMetadataAttr() const override;

public:
  ///===------------------------------------------------------------------===///
  /// Utility Methods
  ///===------------------------------------------------------------------===///
  const char *sourceTypeToString() const;
  static SourceType stringToSourceType(llvm::StringRef str);
  bool isValid() const { return type != SourceType::Unknown; }
};

} // namespace arts
} // namespace mlir

#endif /// ARTS_UTILS_VALUEMETADATA_H
