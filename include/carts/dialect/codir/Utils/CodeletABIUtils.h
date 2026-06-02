#ifndef CARTS_DIALECT_CODIR_UTILS_CODELETABIUTILS_H
#define CARTS_DIALECT_CODIR_UTILS_CODELETABIUTILS_H

#include "carts/dialect/codir/IR/CodirDialect.h"
#include "mlir/IR/BuiltinTypes.h"
#include <optional>

namespace mlir::carts::codir {

bool isCodirDependencyType(Type type);
bool isCodirScalarParamType(Type type);

/// True for regionless ops that produce at least one memref result and are
/// therefore candidates for memref-forwarding analysis (cast, subview,
/// reinterpret_cast, polygeist::SubIndexOp, etc.).
bool isMemrefForwardingOp(Operation *op);

/// Return the declared access mode for the given dependency index of a
/// codelet, or nullopt when the codelet has no dep-modes attribute or the
/// index entry is not a CodirAccessModeAttr.
std::optional<CodirAccessMode> getDepAccessMode(CodeletOp codelet,
                                                unsigned depIndex);

/// Return the declared storage view for the given dependency index of a
/// codelet, or nullopt when the codelet has no dep-storage-views attribute or
/// the index entry is not a CodirStorageViewKindAttr.
std::optional<CodirStorageViewKind> getDepStorageViewKind(CodeletOp codelet,
                                                          unsigned depIndex);

/// Return the SDE arrayId joined to the given dependency, or nullopt when the
/// dependency has no committed SDE layout fact. `dep_array_ids = -1` is the
/// explicit no-layout sentinel.
std::optional<int64_t> getDepArrayId(CodeletOp codelet, unsigned depIndex);

/// Return the `array_layout` dictionary joined to the given dependency by
/// `dep_array_ids`, or null when the join is absent or malformed. CODIR must
/// use this helper instead of indexing per-array layout facts by dep slot.
DictionaryAttr getArrayLayoutEntryForDep(CodeletOp codelet, unsigned depIndex);

/// True when a stencil codelet's per-iteration write footprint fits inside its
/// owner-dim tile slice. Storage planning and collective selection share this
/// gate so `halo` cannot be named for a dep whose block storage was rejected.
bool stencilWriteFitsInTile(CodeletOp codelet);

/// --- First-class CODIR collective selection -------------------------------
/// These predicates are the EXACT gate bodies that ConvertCodirToArts has
/// historically used to decide the all-gather and cross-owner reduce
/// realizations (ArtsMaterializationUtils.h). Hoisted here so StoragePlanning
/// can stamp the first-class `dep_collectives` carrier from the same bodies.

/// True when |producer|'s |depIndex| coarse intermediate buffer is read by a
/// sibling `replicated_read` contraction consumer. This is the all-gather
/// signature.
bool coarseBridgeTargetHasReplicatedReadConsumer(CodeletOp producer,
                                                 unsigned depIndex);

/// True when |consumer| reads a dependency as a cross-owner transpose
/// reduction: a rank-2 matrix dep mapped
/// `[-1, ownerDim]` (leading row dim reduces, a trailing dim carries the
/// result-owner mapping).
bool codeletIsCrossOwnerTransposeReduce(CodeletOp consumer);

/// True when |producer|'s |depIndex| coarse buffer is read by a cross-owner
/// transpose reduction consumer.
bool coarseBridgeTargetHasCrossOwnerReduceConsumer(CodeletOp producer,
                                                   unsigned depIndex);

/// Pure name-free selection of the first-class collective family for
/// |codelet|'s |depIndex|, from the gate predicate bodies above. Returns the
/// kind that reproduces today's gate decision:
///   all_gather     iff coarseBridgeTargetHasReplicatedReadConsumer fires;
///   reduce_scatter iff the cross-owner transpose-reduce gate fires
///                  (codeletIsCrossOwnerTransposeReduce on this codelet, or its
///                   coarse target is read by one);
///   halo           iff a block-storage stencil dependency reads shifted owner
///                  indices and therefore needs neighbor exchange;
///   none           otherwise.
CodirCollectiveKind chooseCollective(CodeletOp codelet, unsigned depIndex);

} // namespace mlir::carts::codir

#endif // CARTS_DIALECT_CODIR_UTILS_CODELETABIUTILS_H
