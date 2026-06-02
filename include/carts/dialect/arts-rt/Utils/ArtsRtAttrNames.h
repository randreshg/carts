#ifndef CARTS_DIALECT_ARTS_RT_UTILS_ARTSRTATTRNAMES_H
#define CARTS_DIALECT_ARTS_RT_UTILS_ARTSRTATTRNAMES_H

#include "llvm/ADT/StringRef.h"

namespace mlir::carts::arts_rt::AttrNames {

/// RT-facing loop execution hints copied onto outlined EDT functions. These
/// live on func/LLVM function ops, so they cannot be ODS accessors on
/// arts_rt ops; in-dialect producers use generated accessors and copy the
/// final consumed values onto the outlined func through this shared name set.
namespace Rt {

inline constexpr llvm::StringLiteral InterleaveCount =
    "arts.rt.interleave_count";

} // namespace Rt

} // namespace mlir::carts::arts_rt::AttrNames

#endif /// CARTS_DIALECT_ARTS_RT_UTILS_ARTSRTATTRNAMES_H
