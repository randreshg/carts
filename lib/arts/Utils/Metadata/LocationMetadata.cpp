///===----------------------------------------------------------------------===///
// LocationMetadata.cpp - Source Location Metadata Implementation
///===----------------------------------------------------------------------===///

#include "arts/Utils/Metadata/LocationMetadata.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/Location.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
// Interface Implementation
///===----------------------------------------------------------------------===///

void LocationMetadata::updateKey() {
  if (line == 0) {
    key = "-";
    return;
  }

  std::string keyStr;
  llvm::raw_string_ostream os(keyStr);
  os << getLocationBasename(file) << ":" << line << ":" << column;
  key = os.str();
}

void LocationMetadata::importFromJson(const llvm::json::Object &json) {
  if (auto fileStr = json.getString("file"))
    file = fileStr->str();
  line = json.getInteger("line").value_or(0);
  column = json.getInteger("column").value_or(0);
  updateKey();
}

void LocationMetadata::exportToJson(llvm::json::Object &json) const {
  if (!file.empty())
    json["file"] = file;
  if (line > 0)
    json["line"] = static_cast<int64_t>(line);
  if (column > 0)
    json["column"] = static_cast<int64_t>(column);
  if (!key.empty())
    json["key"] = key;
}

///===----------------------------------------------------------------------===///
// Utility Methods
///===----------------------------------------------------------------------===///

/// Extract basename from a file path for compact location representation
std::string LocationMetadata::getLocationBasename(llvm::StringRef path) {
  size_t lastSlash = path.rfind('/');
  if (lastSlash != llvm::StringRef::npos)
    return path.substr(lastSlash + 1).str();
  return path.str();
}

/// Convert a location to a compact string key (basename:line:col)
/// Extracts C source locations from various MLIR location types
std::string LocationMetadata::getCompactLocationKey(Location loc) {
  std::string key;
  llvm::raw_string_ostream os(key);

  /// Try to extract FileLineColLoc directly
  if (auto fileLoc = loc.dyn_cast<FileLineColLoc>()) {
    llvm::StringRef filename = fileLoc.getFilename();
    os << getLocationBasename(filename) << ":" << fileLoc.getLine() << ":"
       << fileLoc.getColumn();
  }
  /// Handle FusedLoc - may contain C source location
  else if (auto fusedLoc = loc.dyn_cast<FusedLoc>()) {
    /// Try to find a FileLineColLoc in the fused locations
    for (Location subLoc : fusedLoc.getLocations()) {
      if (auto fileLoc = subLoc.dyn_cast<FileLineColLoc>()) {
        llvm::StringRef filename = fileLoc.getFilename();
        /// Prefer .c, .cpp, .h files over MLIR files
        if (filename.ends_with(".c") || filename.ends_with(".cpp") ||
            filename.ends_with(".h") || filename.ends_with(".hpp")) {
          os << getLocationBasename(filename) << ":" << fileLoc.getLine() << ":"
             << fileLoc.getColumn();
          return os.str();
        }
      }
    }
    /// If no C source file found, recurse on first location
    if (!fusedLoc.getLocations().empty())
      return getCompactLocationKey(fusedLoc.getLocations()[0]);
  }
  /// Handle CallSiteLoc - may wrap C source location
  else if (auto callLoc = loc.dyn_cast<CallSiteLoc>()) {
    /// Prefer callee location (the actual source) over caller
    return getCompactLocationKey(callLoc.getCallee());
  }
  /// Handle NameLoc
  else if (auto nameLoc = loc.dyn_cast<NameLoc>()) {
    /// Check if the child location has more info
    Location childLoc = nameLoc.getChildLoc();
    if (!childLoc.isa<UnknownLoc>()) {
      std::string childKey = getCompactLocationKey(childLoc);
      if (childKey != "unknown:0:0")
        return childKey;
    }
    os << nameLoc.getName().str();
  }
  /// Fallback to printing the location
  else {
    loc.print(os);
    os.flush();
    /// If the printed location is empty or just "-", mark as unknown
    if (key.empty() || key == "-")
      return "unknown:0:0";
  }

  return os.str();
}

////===----------------------------------------------------------------------===////
/// Factory Methods
////===----------------------------------------------------------------------===////

LocationMetadata LocationMetadata::fromMLIRLocation(Location loc) {
  LocationMetadata metadata;

  if (auto fileLoc = loc.dyn_cast<FileLineColLoc>()) {
    metadata.file = fileLoc.getFilename().str();
    metadata.line = fileLoc.getLine();
    metadata.column = fileLoc.getColumn();
  } else if (auto fusedLoc = loc.dyn_cast<FusedLoc>()) {
    /// Try to find a FileLineColLoc in the fused locations
    for (Location subLoc : fusedLoc.getLocations()) {
      if (auto fileLoc = subLoc.dyn_cast<FileLineColLoc>()) {
        llvm::StringRef filename = fileLoc.getFilename();
        /// Prefer .c, .cpp, .h files over MLIR files
        if (filename.ends_with(".c") || filename.ends_with(".cpp") ||
            filename.ends_with(".h") || filename.ends_with(".hpp")) {
          metadata.file = filename.str();
          metadata.line = fileLoc.getLine();
          metadata.column = fileLoc.getColumn();
          break;
        }
      }
    }
  } else if (auto callLoc = loc.dyn_cast<CallSiteLoc>()) {
    return fromMLIRLocation(callLoc.getCallee());
  }

  metadata.updateKey();
  return metadata;
}

LocationMetadata LocationMetadata::fromKey(llvm::StringRef keyStr) {
  LocationMetadata metadata;
  metadata.key = keyStr.str();

  /// Try to parse the key format "file:line:col"
  llvm::SmallVector<llvm::StringRef, 3> parts;
  keyStr.split(parts, ':');

  if (parts.size() >= 3) {
    metadata.file = parts[0].str();
    parts[1].getAsInteger(10, metadata.line);
    parts[2].getAsInteger(10, metadata.column);
  }

  return metadata;
}
