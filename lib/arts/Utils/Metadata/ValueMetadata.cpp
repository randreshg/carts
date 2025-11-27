///===----------------------------------------------------------------------===///
// ValueMetadata.cpp - Value Source Metadata Management
///===----------------------------------------------------------------------===///

#include "arts/Utils/Metadata/ValueMetadata.h"
#include "arts/ArtsDialect.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "llvm/ADT/StringSwitch.h"

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
// Utility Methods
///===----------------------------------------------------------------------===///

const char *ValueMetadata::sourceTypeToString() const {
  switch (type) {
  case SourceType::FunctionArgument:
    return "function_argument";
  case SourceType::LoopInductionVar:
    return "loop_iv";
  case SourceType::Constant:
    return "constant";
  case SourceType::AllocationSize:
    return "allocation_size";
  case SourceType::Unknown:
    return "unknown";
  }
  return "unknown";
}

ValueMetadata::SourceType
ValueMetadata::stringToSourceType(llvm::StringRef str) {
  return llvm::StringSwitch<SourceType>(str)
      .Case("function_argument", SourceType::FunctionArgument)
      .Case("loop_iv", SourceType::LoopInductionVar)
      .Case("constant", SourceType::Constant)
      .Case("allocation_size", SourceType::AllocationSize)
      .Default(SourceType::Unknown);
}

///===----------------------------------------------------------------------===///
// ArtsMetadata Interface Implementation
///===----------------------------------------------------------------------===///

bool ValueMetadata::importFromOp() {
  importIdFromOp();
  /// Get the metadata attribute
  ValueMetadataAttr attr =
      op_->getAttrOfType<ValueMetadataAttr>(getMetadataName());
  if (!attr)
    return false;
  type = stringToSourceType(attr.getSourceType().getValue());
  if (auto indexVal = getIntFromAttr(attr.getIndex()))
    index = *indexVal;
  if (attr.getVarName())
    varName = attr.getVarName().str();
  if (attr.getLocationKey())
    location = LocationMetadata::fromKey(attr.getLocationKey().getValue());

  return true;
}

/// Use base class implementation
void ValueMetadata::exportToOp() { ArtsMetadata::exportToOp(); }

void ValueMetadata::importFromJson(const llvm::json::Object &json) {
  importIdFromJson(json);
  /// Import source type
  if (auto i = json.getInteger(AttrNames::ValueMetadata::SourceType))
    type = static_cast<SourceType>(*i);

  /// Import index
  index = json.getInteger(AttrNames::ValueMetadata::Index).value_or(0);

  /// Import variable name
  if (auto varNameStr = json.getString(AttrNames::ValueMetadata::VarName))
    varName = varNameStr->str();

  /// Import location metadata
  if (auto *locObj = json.getObject("location"))
    location.importFromJson(*locObj);
}

void ValueMetadata::exportToJson(llvm::json::Object &json) const {
  exportIdToJson(json);
  json[AttrNames::ValueMetadata::SourceType.str()] = static_cast<int64_t>(type);
  json[AttrNames::ValueMetadata::Index.str()] = static_cast<int64_t>(index);
  if (!varName.empty())
    json[AttrNames::ValueMetadata::VarName.str()] = varName;

  llvm::json::Object locJson;
  location.exportToJson(locJson);
  if (!locJson.empty())
    json["location"] = std::move(locJson);
}

StringRef ValueMetadata::toString() const {
  if (!varName.empty())
    return StringRef(varName);
  return sourceTypeToString();
}

Attribute ValueMetadata::getMetadataAttr() const {
  MLIRContext *ctx = op_->getContext();
  OpBuilder builder(ctx);

  /// Build the ValueMetadataAttr
  return ValueMetadataAttr::get(
      ctx, builder.getStringAttr(sourceTypeToString()),
      builder.getI64IntegerAttr(index),
      !varName.empty() ? builder.getStringAttr(varName) : StringAttr(),
      location.isValid() ? builder.getStringAttr(location.getKey())
                         : StringAttr());
}
