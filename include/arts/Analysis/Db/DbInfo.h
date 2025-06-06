#ifndef CARTS_ANALYSIS_DB_INFO_H
#define CARTS_ANALYSIS_DB_INFO_H

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Types.h"
#include "mlir/Support/LLVM.h"
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>

namespace mlir {
namespace arts {

class DbAnalysis;
class DbAccessNode;
class DbAllocNode;

enum class AccessType { Unknown, Read, Write, ReadWrite };
std::string toString(AccessType type);

/// Address space type
struct AddressSpace {
  enum Type { Unknown, Stack, Heap, Global, Constant } type = Unknown;
  bool operator==(const AddressSpace &other) const {
    return type == other.type;
  }
  bool operator!=(const AddressSpace &other) const {
    return type != other.type;
  }
};

struct MemoryLayout {
  int64_t offset = 0;
  int64_t size = -1;
  int64_t stride = 1;
  bool isStatic = false;
  bool overlaps(const MemoryLayout &other) const {
    if (!isStatic || !other.isStatic || size <= 0 || other.size <= 0) {
      return true;
    }
    int64_t thisEnd = offset + size;
    int64_t otherEnd = other.offset + other.size;
    return !(thisEnd <= other.offset || otherEnd <= offset);
  }
};

/// Abstract base class for Database Information Nodes
class DbInfo {
public:
  DbInfo(Operation *op, bool isAlloc, DbAnalysis *analysis)
      : op(op), isAllocFlag(isAlloc), analysis(analysis) {
    this->id = nextGlobalId++;
    this->hierId = "";
  }
  virtual ~DbInfo() = default;

  /// Virtual methods
  virtual void analyze() = 0;
  virtual void collectUses() = 0;
  virtual void print(llvm::raw_ostream &os) const = 0;

  /// Accessors
  unsigned getId() const { return id; }
  const std::string& getHierId() const { return hierId; }
  void setHierId(const std::string& newId) { hierId = newId; }
  Operation *getOp() const { return op; }
  Type getElementType() const { return elementType; }
  uint64_t getElementTypeSize() const {
    return elementType.getIntOrFloatBitWidth();
  }
  Value getResult() const { return op->getResult(0); }
  Type getResultType() const { return op->getResultTypes()[0]; }
  Value getPtr() const { return ptr; }
  DbInfo *getParent() const { return parent.value_or(nullptr); }
  DbAllocNode *getParentAlloc() { return dyn_cast<DbAllocNode>(getParent()); }
  DbAccessNode *getParentAccess() {
    return dyn_cast<DbAccessNode>(getParent());
  }
  bool isAlloc() const { return isAllocFlag; }
  SmallVector<Value> &getOffsets() { return offsets; }
  SmallVector<Value> &getSizes() { return sizes; }
  SmallVector<Value> &getStrides() { return strides; }
  AccessType getAccessType() const { return accessType; }
  AddressSpace getAddressSpace() const { return addressSpace; }
  MemoryLayout getLayout() const { return layout; }
  bool isWriter() {
    return accessType == AccessType::Write ||
           accessType == AccessType::ReadWrite;
  }
  bool isReader() {
    return accessType == AccessType::Read ||
           accessType == AccessType::ReadWrite;
  }
  bool isOnlyReader() { return isReader() && !isWriter(); }
  bool isOnlyWriter() { return isWriter() && !isReader(); }

protected:
  unsigned id;
  std::string hierId;
  Operation *op;
  bool isAllocFlag;
  Value ptr;
  Type elementType;
  SmallVector<Value> sizes;
  SmallVector<Value> strides;
  SmallVector<Value> offsets;
  SmallVector<memref::LoadOp> loads;
  SmallVector<memref::StoreOp> stores;
  std::optional<DbInfo *> parent;

  /// Memory region information
  AccessType accessType = AccessType::Unknown;
  AddressSpace addressSpace;
  MemoryLayout layout;

  /// Analysis instance
  DbAnalysis *analysis;

private:
  static unsigned nextGlobalId;

  /// Helper methods
  void setMemoryLayout();
  void setAccessType();
  // void setAddressSpace();
};

} // namespace arts
} // namespace mlir
#endif // CARTS_ANALYSIS_DB_INFO_H