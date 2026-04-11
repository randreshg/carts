///==========================================================================///
/// File: SdeTypes.td
/// This file declares the SDE dialect types.
///==========================================================================///

#ifndef ARTS_SDE_TYPES
#define ARTS_SDE_TYPES

include "mlir/IR/AttrTypeBase.td"
include "SdeDialect.td"

class ArtsSde_Type<string name, string typeMnemonic>
    : TypeDef<ArtsSdeDialect, name> {
  let mnemonic = typeMnemonic;
}

def CompletionType : ArtsSde_Type<"Completion", "completion"> {
  let summary = "typed SDE completion token";
  let description = [{
    `!arts_sde.completion` models SDE-level completion tokens used to express
    task completion and wait surfaces without exposing runtime-specific event
    objects in SDE IR.
  }];
}

def DepType : ArtsSde_Type<"Dep", "dep"> {
  let summary = "typed SDE dependency token";
  let description = [{
    `!arts_sde.dep` models an explicit SDE dependency handle produced by
    `arts_sde.mu_dep` and consumed by SDE task-dependence surfaces.
  }];
}

#endif // ARTS_SDE_TYPES
