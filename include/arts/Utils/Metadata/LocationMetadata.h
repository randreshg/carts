///==========================================================================///
/// File: LocationMetadata.h
///
/// This file defines LocationMetadata for tracking and matching source code
/// locations. This is a lightweight value type for representing source
/// locations.
///==========================================================================///

#ifndef ARTS_UTILS_LOCATIONMETADATA_H
#define ARTS_UTILS_LOCATIONMETADATA_H

#include "mlir/IR/Location.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/JSON.h"
#include <string>

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// LocationMetadata
///===----------------------------------------------------------------------===///
class LocationMetadata {
public:
  /// Attributes
  std::string file;
  unsigned line = 0, column = 0;
  std::string key;
  bool isStatic = false;

  /// Constructor
  LocationMetadata() = default;

  /// Construct from file, line, column
  LocationMetadata(llvm::StringRef file, unsigned line, unsigned column)
      : file(file.str()), line(line), column(column) {
    updateKey();
  }

  //===-------------------------------------------------------------===//
  // Interface
  //===-------------------------------------------------------------===//

  /// Import/export metadata to/from JSON
  void importFromJson(const llvm::json::Object &json);
  void exportToJson(llvm::json::Object &json) const;

  /// Convert to string representation
  llvm::StringRef toString() const {
    static const llvm::StringRef emptyKey("-");
    return key.empty() ? emptyKey : key;
  }

  /// Helper methods
  static LocationMetadata fromMLIRLocation(Location loc);

  static LocationMetadata fromKey(llvm::StringRef key);

  static std::string getLocationBasename(llvm::StringRef path);

  static std::string getCompactLocationKey(Location loc);

  /// Check if location is valid
  bool isValid() const { return line > 0; }

  /// Compare locations for equality
  bool operator==(const LocationMetadata &other) const {
    return key == other.key;
  }

  bool operator<(const LocationMetadata &other) const {
    return key < other.key;
  }

private:
  /// Update the location key based on file, line, column
  void updateKey();
};

} // namespace arts
} // namespace mlir

#endif // ARTS_UTILS_LOCATIONMETADATA_H
