///==========================================================================///
/// File: AccessStats.cpp
///
/// Implementation of shared access statistics.
///==========================================================================///
#include "arts/Utils/Metadata/AccessStats.h"
#include "arts/Utils/Metadata/MemrefMetadata.h"

using namespace mlir::arts;

void AccessStats::importFromJson(const llvm::json::Object &json) {
  if (auto val = json.getInteger(AttrNames::MemrefMetadata::ReadCount))
    readCount = *val;
  if (auto val = json.getInteger(AttrNames::MemrefMetadata::WriteCount))
    writeCount = *val;
  if (auto val = json.getInteger(AttrNames::MemrefMetadata::TotalAccesses))
    totalAccesses = *val;
  if (auto val = json.getNumber(AttrNames::MemrefMetadata::ReadWriteRatio))
    readWriteRatio = *val;
}

void AccessStats::exportToJson(llvm::json::Object &json) const {
  if (readCount)
    json[AttrNames::MemrefMetadata::ReadCount] = *readCount;
  if (writeCount)
    json[AttrNames::MemrefMetadata::WriteCount] = *writeCount;
  if (totalAccesses)
    json[AttrNames::MemrefMetadata::TotalAccesses] = *totalAccesses;
  if (readWriteRatio)
    json[AttrNames::MemrefMetadata::ReadWriteRatio] = *readWriteRatio;
}
