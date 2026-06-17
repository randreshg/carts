///==========================================================================///
/// File: SdeToArtsBoundaryTypes.h
/// Shared SDE→ARTS boundary value types.
///==========================================================================///

#ifndef CARTS_DIALECT_ARTS_TRANSFORMS_BOUNDARY_SDETOARTSBOUNDARYTYPES_H
#define CARTS_DIALECT_ARTS_TRANSFORMS_BOUNDARY_SDETOARTSBOUNDARYTYPES_H

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/MovementLoweringUtils.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "llvm/ADT/SmallVector.h"
#include <optional>

namespace mlir {
namespace carts::arts::boundary {

struct CommittedPhysicalLayout {
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> blockShape;
};

struct DepOwnerAccessSlot {
  std::optional<unsigned> loopDim;
  int64_t coordinateBlockSize = 0;
  std::optional<int64_t> fixedBlock;
  bool fullWindow = false;
  int64_t minElementOffset = 0;
  int64_t maxElementOffset = 0;
};

inline bool operator==(const DepOwnerAccessSlot &lhs,
                       const DepOwnerAccessSlot &rhs) {
  return lhs.loopDim == rhs.loopDim &&
         lhs.coordinateBlockSize == rhs.coordinateBlockSize &&
         lhs.fixedBlock == rhs.fixedBlock && lhs.fullWindow == rhs.fullWindow &&
         lhs.minElementOffset == rhs.minElementOffset &&
         lhs.maxElementOffset == rhs.maxElementOffset;
}

struct DirectDepSpec {
  arts::DbAllocOp alloc;
  ArtsMode mode = ArtsMode::uninitialized;
  IntegerAttr arrayId;
  unsigned ownerDimCount = 0;
  SmallVector<int64_t, 4> blockLo;
  SmallVector<int64_t, 4> blockHi;
  SmallVector<int64_t, 4> validExtents;
  std::optional<SmallVector<int64_t, 4>> arrayOwnerDims;
  SmallVector<DepOwnerAccessSlot, 4> accessSlots;
  ArrayAttr haloShape;
  std::optional<ReduceScatterRedistFacts> reduceScatter;
};

enum class Halo2DFace { Center, Top, Bottom, Left, Right };

struct CompactHaloColumnSpec {
  arts::DbAllocOp leftColumnDb;
  arts::DbAllocOp rightColumnDb;
  Value rowExtent;
  Value colExtent;
  bool cleanPayloadShape = false;
};

struct CompactHaloNdSideSpec {
  SmallVector<int64_t, 4> sourceOffsets;
  arts::DbAllocOp payloadDb;
};

struct CompactHaloNdSpec {
  unsigned ownerDimCount = 0;
  SmallVector<unsigned, 4> ownerPayloadDims;
  SmallVector<Value, 4> elementExtents;
  SmallVector<CompactHaloNdSideSpec, 8> sides;
};

struct Halo2DTaskWork {
  unsigned centerTaskDepIndex = 0;
  unsigned topTaskDepIndex = 0;
  unsigned bottomTaskDepIndex = 0;
  unsigned leftTaskDepIndex = 0;
  unsigned rightTaskDepIndex = 0;
  SmallVector<int64_t, 4> centerGroupBlockCounts;
  Value rowExtent;
  Value colExtent;
};

struct HaloNdTaskWork {
  unsigned ownerDimCount = 0;
  unsigned centerTaskDepIndex = 0;
  SmallVector<unsigned, 4> ownerLoopDims;
  SmallVector<unsigned, 4> ownerPayloadDims;
  SmallVector<int64_t, 4> centerGroupBlockCounts;
  SmallVector<Value, 4> elementExtents;
  SmallVector<SmallVector<int64_t, 4>, 8> sideSourceOffsets;
  SmallVector<unsigned, 8> sideTaskDepIndices;
};

struct HaloLoadRewrite {
  unsigned haloWorkIndex = 0;
  Halo2DFace face = Halo2DFace::Center;
};

struct HaloNdLoadRewrite {
  unsigned haloWorkIndex = 0;
};

struct WriterGroupingSpec {
  SmallVector<int64_t, 4> dbSizes;
  SmallVector<int64_t, 4> groupSlotByDbDim;
  SmallVector<int64_t, 4> globalBlockSizeByDbDim;
  SmallVector<int64_t, 4> coordinateBlockSizeByDbDim;
  DbOwnerRouteFacts ownerFacts;
};

struct CoarseSuDependency {
  arts::DbAllocOp alloc;
  ArtsMode mode = ArtsMode::uninitialized;
};

struct DirectCuDepSpec {
  arts::DbAllocOp alloc;
  ArtsMode mode = ArtsMode::uninitialized;
  unsigned ownerDimCount = 0;
  SmallVector<int64_t, 4> blockLo;
  SmallVector<int64_t, 4> blockHi;
  SmallVector<int64_t, 4> validExtents;
  ArrayAttr haloShape;
  Value acquiredPtr;
};

struct CuResultSpec {
  arts::DbAllocOp alloc;
  Value writePtr;
  Value replacement;
};

struct AccessWindowFacts {
  SmallVector<int64_t, 4> blockLo;
  SmallVector<int64_t, 4> blockHi;
  SmallVector<int64_t, 4> validExtents;
};

struct MappedLoopIv {
  Value iv;
  std::optional<unsigned> loopDim;
  std::optional<int64_t> lowerBound;
  std::optional<int64_t> upperBound;
  std::optional<int64_t> step;
};

struct TaskDepSpec {
  sde::SdeMuDepOp dep;
  arts::DbAllocOp alloc;
  ArtsMode mode = ArtsMode::uninitialized;
  SmallVector<Value, 4> offsets;
  SmallVector<Value, 4> sizes;
};

} // namespace carts::arts::boundary
} // namespace mlir

#endif
