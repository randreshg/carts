///==========================================================================///
/// File: VerifyCodir.cpp
///
/// Verifies CODIR codelet ABI invariants after SDE-to-CODIR conversion and
/// before CODIR-to-ARTS materialization.
///==========================================================================///

#include "carts/dialect/codir/Transforms/Passes.h"
#include "carts/dialect/codir/Utils/CodeletABIUtils.h"
#include "carts/dialect/codir/Utils/CodirAttrNames.h"

#include "llvm/ADT/STLExtras.h"

namespace mlir::carts::codir {
#define GEN_PASS_DEF_VERIFYCODIR
#include "carts/dialect/codir/Transforms/Passes.h.inc"
} // namespace mlir::carts::codir

using namespace mlir;
using namespace mlir::carts;

namespace {

struct VerifyCodirPass : public codir::impl::VerifyCodirBase<VerifyCodirPass> {
  void runOnOperation() override {
    bool failed = false;
    getOperation().walk([&](codir::CodeletOp codelet) {
      ArrayAttr depModes = codelet.getDepModesAttr();
      if (!codelet.getDeps().empty() && !depModes) {
        codelet.emitOpError()
            << "expects dep_modes attribute when dependency operands are "
               "present";
        failed = true;
      }
      if (depModes) {
        if (depModes.size() != codelet.getDeps().size()) {
          codelet.emitOpError()
              << "expects dep_modes entry count (" << depModes.size()
              << ") to match dependency operand count ("
              << codelet.getDeps().size() << ")";
          failed = true;
        }
        for (auto [index, attr] : llvm::enumerate(depModes)) {
          if (isa<codir::CodirAccessModeAttr>(attr))
            continue;
          codelet.emitOpError()
              << "dep_modes entry #" << index
              << " must be a CODIR access_mode attribute, got " << attr;
          failed = true;
        }
      }

      ArrayAttr depStorageViews = codelet.getDepStorageViewsAttr();
      if (depStorageViews) {
        if (depStorageViews.size() != codelet.getDeps().size()) {
          codelet.emitOpError() << "expects dep_storage_views entry count ("
                                << depStorageViews.size()
                                << ") to match dependency operand count ("
                                << codelet.getDeps().size() << ")";
          failed = true;
        }
        for (auto [index, attr] : llvm::enumerate(depStorageViews)) {
          if (isa<codir::CodirStorageViewKindAttr>(attr))
            continue;
          codelet.emitOpError()
              << "dep_storage_views entry #" << index
              << " must be a CODIR storage_view attribute, got " << attr;
          failed = true;
        }
      }

      auto arrayLayoutContainsId = [&](int64_t expected) {
        ArrayAttr layout = codelet.getArrayLayoutAttr();
        if (!layout)
          return false;
        for (Attribute attr : layout) {
          auto entry = dyn_cast<DictionaryAttr>(attr);
          if (!entry)
            continue;
          auto arrayId = dyn_cast_or_null<IntegerAttr>(
              entry.get(codir::AttrNames::LayoutGraphKeys::ArrayId));
          if (arrayId && arrayId.getInt() == expected)
            return true;
        }
        return false;
      };

      ArrayAttr depArrayIds = codelet.getDepArrayIdsAttr();
      if (depArrayIds) {
        if (depArrayIds.size() != codelet.getDeps().size()) {
          codelet.emitOpError()
              << "expects dep_array_ids entry count (" << depArrayIds.size()
              << ") to match dependency operand count ("
              << codelet.getDeps().size() << ")";
          failed = true;
        }
        for (auto [index, attr] : llvm::enumerate(depArrayIds)) {
          auto id = dyn_cast<IntegerAttr>(attr);
          if (!id) {
            codelet.emitOpError()
                << "dep_array_ids entry #" << index
                << " must be an integer attribute, got " << attr;
            failed = true;
            continue;
          }
          if (id.getInt() < -1) {
            codelet.emitOpError() << "dep_array_ids entry #" << index
                                  << " must be -1 or a non-negative array id";
            failed = true;
            continue;
          }
          if (id.getInt() >= 0 && !arrayLayoutContainsId(id.getInt())) {
            codelet.emitOpError()
                << "dep_array_ids entry #" << index << " references arrayId "
                << id.getInt() << " not present in array_layout";
            failed = true;
          }
        }
      }

      ArrayAttr depCollectives = codelet.getDepCollectivesAttr();
      if (depCollectives) {
        if (depCollectives.size() != codelet.getDeps().size()) {
          codelet.emitOpError() << "expects dep_collectives entry count ("
                                << depCollectives.size()
                                << ") to match dependency operand count ("
                                << codelet.getDeps().size() << ")";
          failed = true;
        }
        for (auto [index, attr] : llvm::enumerate(depCollectives)) {
          if (isa<codir::CodirCollectiveKindAttr>(attr))
            continue;
          codelet.emitOpError()
              << "dep_collectives entry #" << index
              << " must be a CODIR collective attribute, got " << attr;
          failed = true;
        }
      }

      ArrayAttr depOwnerDims = codelet.getDepOwnerDimsAttr();
      if (depOwnerDims) {
        if (depOwnerDims.size() != codelet.getDeps().size()) {
          codelet.emitOpError()
              << "expects dep_owner_dims entry count (" << depOwnerDims.size()
              << ") to match dependency operand count ("
              << codelet.getDeps().size() << ")";
          failed = true;
        }
        for (auto [index, attr] : llvm::enumerate(depOwnerDims)) {
          auto dims = dyn_cast<ArrayAttr>(attr);
          if (!dims) {
            codelet.emitOpError()
                << "dep_owner_dims entry #" << index
                << " must be an array attribute, got " << attr;
            failed = true;
            continue;
          }
          for (Attribute dim : dims) {
            if (isa<IntegerAttr>(dim))
              continue;
            codelet.emitOpError()
                << "dep_owner_dims entry #" << index
                << " must contain integer attributes, got " << dim;
            failed = true;
          }
        }
      }

      ArrayAttr depResultDimMaps =
          codelet.getPartialReductionDepResultDimMapsAttr();
      if (depResultDimMaps) {
        if (depResultDimMaps.size() != codelet.getDeps().size()) {
          codelet.emitOpError()
              << "expects partial_reduction_dep_result_dim_maps entry count ("
              << depResultDimMaps.size()
              << ") to match dependency operand count ("
              << codelet.getDeps().size() << ")";
          failed = true;
        }
        for (auto [index, attr] : llvm::enumerate(depResultDimMaps)) {
          auto dims = dyn_cast<ArrayAttr>(attr);
          if (!dims) {
            codelet.emitOpError()
                << "partial_reduction_dep_result_dim_maps entry #" << index
                << " must be an array attribute, got " << attr;
            failed = true;
            continue;
          }
          for (Attribute dim : dims) {
            if (isa<IntegerAttr>(dim))
              continue;
            codelet.emitOpError()
                << "partial_reduction_dep_result_dim_maps entry #" << index
                << " must contain integer attributes, got " << dim;
            failed = true;
          }
        }
      }

      auto verifyPositiveI64Attr = [&](IntegerAttr attr, StringRef name) {
        if (!attr)
          return;
        if (attr.getInt() > 0)
          return;
        codelet.emitOpError()
            << name << " must be positive, got " << attr.getInt();
        failed = true;
      };

      verifyPositiveI64Attr(codelet.getPartialReductionSplitFactorAttr(),
                            "partial_reduction_split_factor");
      verifyPositiveI64Attr(
          codelet.getPartialReductionSplitOwnerTaskCountAttr(),
          "partial_reduction_split_owner_task_count");
      verifyPositiveI64Attr(
          codelet.getPartialReductionSplitTargetWorkerCountAttr(),
          "partial_reduction_split_target_worker_count");

      if (Attribute scoreAttr =
              codelet->getAttr(codir::AttrNames::PartitionScore)) {
        auto score = dyn_cast<DictionaryAttr>(scoreAttr);
        if (!score) {
          codelet.emitOpError() << codir::AttrNames::PartitionScore
                                << " must be a dictionary attribute";
          failed = true;
        } else {
          auto verifyPositiveScoreField = [&](StringRef key) -> bool {
            Attribute value = score.get(key);
            if (!value)
              return false;
            auto intAttr = dyn_cast<IntegerAttr>(value);
            if (intAttr && intAttr.getInt() > 0)
              return true;
            codelet.emitOpError()
                << codir::AttrNames::PartitionScore << "." << key
                << " must be a positive integer attribute";
            failed = true;
            return true;
          };
          bool hasConcurrencyField = false;
          hasConcurrencyField |= verifyPositiveScoreField(
              codir::AttrNames::PartitionScoreKeys::TargetLogicalWorkers);
          hasConcurrencyField |= verifyPositiveScoreField(
              codir::AttrNames::PartitionScoreKeys::ExposedCuCount);
          verifyPositiveScoreField(
              codir::AttrNames::PartitionScoreKeys::ChosenCuCount);
          verifyPositiveScoreField(
              codir::AttrNames::PartitionScoreKeys::MuBlockCount);
          if (!hasConcurrencyField) {
            codelet.emitOpError()
                << codir::AttrNames::PartitionScore << " must contain "
                << codir::AttrNames::PartitionScoreKeys::TargetLogicalWorkers
                << " or "
                << codir::AttrNames::PartitionScoreKeys::ExposedCuCount;
            failed = true;
          }
        }
      }

      if (codelet.getPartialReductionSplitRequiredAttr() &&
          !codelet.getPartialReductionAttr()) {
        codelet.emitOpError()
            << "partial_reduction_split_required requires partial_reduction";
        failed = true;
      }
      if (codelet.getPartialReductionSplitRequiredAttr() &&
          !codelet.getPartialReductionSplitFactorAttr()) {
        codelet.emitOpError() << "partial_reduction_split_required requires "
                                 "partial_reduction_split_factor";
        failed = true;
      }
      if (ArrayAttr splitDims = codelet.getPartialReductionSplitDimsAttr()) {
        for (Attribute dim : splitDims) {
          if (isa<IntegerAttr>(dim))
            continue;
          codelet.emitOpError()
              << "partial_reduction_split_dims must contain integer "
                 "attributes, got "
              << dim;
          failed = true;
        }
      }

      for (auto [index, dep] : llvm::enumerate(codelet.getDeps())) {
        if (codir::isCodirDependencyType(dep.getType()))
          continue;
        codelet.emitOpError()
            << "CODIR dependency #" << index << " must be a memref value, got "
            << dep.getType();
        failed = true;
      }

      for (auto [index, param] : llvm::enumerate(codelet.getParams())) {
        if (codir::isCodirScalarParamType(param.getType()))
          continue;
        codelet.emitOpError()
            << "CODIR parameter #" << index
            << " must be an integer, index, or float scalar; got "
            << param.getType();
        failed = true;
      }

      if (codelet.getBody().empty()) {
        codelet.emitOpError() << "expects body to contain a single block";
        failed = true;
        return;
      }

      Block &body = codelet.getBody().front();
      unsigned numDeps = codelet.getDeps().size();
      unsigned numParams = codelet.getParams().size();
      unsigned expectedArgs = numDeps + numParams;
      if (body.getNumArguments() != expectedArgs) {
        codelet.emitOpError()
            << "expects " << expectedArgs << " CODIR block argument(s) ("
            << numDeps << " dep + " << numParams << " param); got "
            << body.getNumArguments();
        failed = true;
      } else {
        for (auto [index, dep] : llvm::enumerate(codelet.getDeps())) {
          Type argType = body.getArgument(index).getType();
          if (argType == dep.getType())
            continue;
          codelet.emitOpError()
              << "dep block argument #" << index << " type (" << argType
              << ") does not match dep operand type (" << dep.getType() << ")";
          failed = true;
        }

        for (auto [index, param] : llvm::enumerate(codelet.getParams())) {
          unsigned argIndex = numDeps + index;
          Type argType = body.getArgument(argIndex).getType();
          if (argType == param.getType())
            continue;
          codelet.emitOpError()
              << "param block argument #" << index << " type (" << argType
              << ") does not match param operand type (" << param.getType()
              << ")";
          failed = true;
        }
      }

      auto yield = dyn_cast_or_null<codir::YieldOp>(body.getTerminator());
      if (!yield) {
        codelet.emitOpError() << "expects body to terminate with codir.yield";
        failed = true;
        return;
      }

      for (auto [index, yielded] : llvm::enumerate(yield.getResults())) {
        if (codir::isCodirScalarParamType(yielded.getType()))
          continue;
        codelet.emitOpError()
            << "CODIR yield operand #" << index
            << " must be an integer, index, or float scalar; got "
            << yielded.getType();
        failed = true;
      }

      Region &bodyRegion = codelet.getBody();
      bodyRegion.walk([&](Operation *op) {
        if (op == codelet.getOperation())
          return;

        StringRef dialectNamespace = op->getName().getDialectNamespace();
        if (dialectNamespace == "arts" || dialectNamespace == "arts_rt") {
          InFlightDiagnostic diag =
              op->emitOpError()
              << "materialized " << dialectNamespace
              << " operation is not allowed inside codir.codelet";
          diag.attachNote(codelet.getLoc())
              << "CODIR codelets must stay runtime-neutral until "
                 "codir-to-arts or later lowering";
          failed = true;
        }

        for (auto [operandIndex, operand] :
             llvm::enumerate(op->getOperands())) {
          Region *operandRegion = operand.getParentRegion();
          if (operandRegion && bodyRegion.isAncestor(operandRegion))
            continue;

          InFlightDiagnostic diag =
              op->emitOpError()
              << "operand #" << operandIndex
              << " is an implicit above-capture of a value not declared in the "
                 "codir.codelet deps or params";
          diag.attachNote(codelet.getLoc())
              << "captured by this codir.codelet boundary";
          if (Operation *defOp = operand.getDefiningOp()) {
            diag.attachNote(defOp->getLoc())
                << "captured value is defined here by '"
                << defOp->getName().getStringRef() << "'";
          } else if (auto blockArg = dyn_cast<BlockArgument>(operand)) {
            diag.attachNote(blockArg.getLoc())
                << "captured value is block argument #"
                << blockArg.getArgNumber()
                << " of an enclosing region outside the codelet";
          }
          failed = true;
        }
      });
    });

    if (failed)
      signalPassFailure();
  }
};

} // namespace

std::unique_ptr<Pass> mlir::carts::codir::createVerifyCodirPass() {
  return std::make_unique<VerifyCodirPass>();
}
