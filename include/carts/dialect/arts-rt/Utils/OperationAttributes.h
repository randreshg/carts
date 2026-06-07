#ifndef CARTS_DIALECT_ARTS_RT_UTILS_OPERATIONATTRIBUTES_H
#define CARTS_DIALECT_ARTS_RT_UTILS_OPERATIONATTRIBUTES_H

#include "carts/dialect/arts-rt/Utils/ArtsRtAttrNames.h"
#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "mlir/IR/Operation.h"

namespace mlir {
namespace carts::arts_rt {

inline void copyCoreExecutionHintAttrsToRtFunction(arts::EdtOp source,
                                                   Operation *dest) {
  if (!source || !dest)
    return;
  if (auto attr = source.getInterleaveCountAttr())
    dest->setAttr(AttrNames::Rt::InterleaveCount, attr);
}

} // namespace carts::arts_rt
} // namespace mlir

#endif // CARTS_DIALECT_ARTS_RT_UTILS_OPERATIONATTRIBUTES_H
