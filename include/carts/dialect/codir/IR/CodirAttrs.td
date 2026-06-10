///==========================================================================///
/// File: CodirAttrs.td
/// This file defines CODIR-owned attributes.
///==========================================================================///

#ifndef CARTS_CODIR_ATTRS
#define CARTS_CODIR_ATTRS

include "carts/dialect/codir/IR/CodirDialect.td"
include "carts/IR/CommonAttrs.td"
include "mlir/IR/AttrTypeBase.td"
include "mlir/IR/EnumAttr.td"

class CartsCodir_I32Enum<string name, string description,
                         list<I32EnumAttrCase> cases>
    : I32EnumAttr<name, description, cases> {
  let cppNamespace = "::mlir::carts::codir";
  let genSpecializedAttr = 0;
}

class CartsCodir_I32EnumAttr<string mnemonic, CartsCodir_I32Enum enumInfo>
    : EnumAttr<CartsCodirDialect, enumInfo, mnemonic> {
  let assemblyFormat = "`<` $value `>`";
}

def CodirAccessMode
    : CartsCodir_I32Enum<"CodirAccessMode", "CODIR dependency access mode", [
      I32EnumAttrCase<"read", 0, "read">,
      I32EnumAttrCase<"write", 1, "write">,
      I32EnumAttrCase<"readwrite", 2, "readwrite">
    ]>;
def AccessModeAttr : CartsCodir_I32EnumAttr<"access_mode", CodirAccessMode>;

def CodirStorageViewKind
    : CartsCodir_I32Enum<"CodirStorageViewKind",
                         "CODIR dependency storage view fact", [
      I32EnumAttrCase<"host_whole", 0, "host_whole">,
      I32EnumAttrCase<"compute_block", 1, "compute_block">,
      I32EnumAttrCase<"replicated_read", 2, "replicated_read">,
      I32EnumAttrCase<"phase_redistributed", 3, "phase_redistributed">
    ]>;
def StorageViewKindAttr
    : CartsCodir_I32EnumAttr<"storage_view", CodirStorageViewKind>;

// First-class collective family for CODIR redistribution. This names the
// distribution pattern for a dependency edge, not a runtime algorithm or
// ARTS object identity.
def CodirCollectiveKind
    : CartsCodir_I32Enum<"CodirCollectiveKind",
                         "CODIR redistribution collective family", [
      I32EnumAttrCase<"none", 0, "none">,
      I32EnumAttrCase<"all_gather", 1, "all_gather">,
      I32EnumAttrCase<"all_to_all", 2, "all_to_all">,
      I32EnumAttrCase<"reduce_scatter", 3, "reduce_scatter">,
      I32EnumAttrCase<"allreduce", 4, "allreduce">,
      I32EnumAttrCase<"broadcast", 5, "broadcast">,
      I32EnumAttrCase<"halo", 6, "halo">
    ]>;
def CollectiveKindAttr
    : CartsCodir_I32EnumAttr<"collective", CodirCollectiveKind>;

def CodirReductionStrategy
    : CartsCodir_I32Enum<"CodirReductionStrategy",
                         "CODIR reduction execution strategy",
                         CartsReductionStrategyCases>;
def ReductionStrategyAttr
    : CartsCodir_I32EnumAttr<"reduction_strategy",
                             CodirReductionStrategy>;

def CodirPattern
    : CartsCodir_I32Enum<"CodirPattern",
                         "CODIR codelet dependency and schedule family", [
      I32EnumAttrCase<"uniform", 0, "uniform">,
      I32EnumAttrCase<"stencil_tiling_nd", 1, "stencil_tiling_nd">,
      I32EnumAttrCase<"cross_dim_stencil_3d", 2, "cross_dim_stencil_3d">,
      I32EnumAttrCase<"higher_order_stencil", 3, "higher_order_stencil">,
      I32EnumAttrCase<"wavefront_2d", 4, "wavefront_2d">,
      I32EnumAttrCase<"alternating_buffer_stencil", 5,
                      "alternating_buffer_stencil">,
      I32EnumAttrCase<"matmul", 6, "matmul">,
      I32EnumAttrCase<"elementwise_pipeline", 7, "elementwise_pipeline">,
      I32EnumAttrCase<"reduction", 8, "reduction">
    ]>;
def PatternAttr : CartsCodir_I32EnumAttr<"pattern", CodirPattern>;

def CodirDistributionKind
    : CartsCodir_I32Enum<"CodirDistributionKind",
                         "CODIR codelet distribution kind", [
      I32EnumAttrCase<"owner_compute", 0, "owner_compute">,
      I32EnumAttrCase<"blocked", 1, "blocked">,
      I32EnumAttrCase<"cyclic", 2, "cyclic">
    ]>;
def DistributionKindAttr
    : CartsCodir_I32EnumAttr<"distribution_kind", CodirDistributionKind>;

def CodirIterationTopology
    : CartsCodir_I32Enum<"CodirIterationTopology",
                         "CODIR logical iteration topology",
                         CartsIterationTopologyCases>;
def IterationTopologyAttr
    : CartsCodir_I32EnumAttr<"iteration_topology", CodirIterationTopology>;

def CodirRepetitionStructure
    : CartsCodir_I32Enum<"CodirRepetitionStructure",
                         "CODIR repeated-wave or timestep structure", [
      I32EnumAttrCase<"none", 0, "none">,
      I32EnumAttrCase<"pair_step", 1, "pair_step">,
      I32EnumAttrCase<"k_step", 2, "k_step">,
      I32EnumAttrCase<"full_timestep", 3, "full_timestep">
    ]>;
def RepetitionStructureAttr
    : CartsCodir_I32EnumAttr<"repetition_structure",
                             CodirRepetitionStructure>;

#endif // CARTS_CODIR_ATTRS
