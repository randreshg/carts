#include "carts/Ops.h"

#include "carts/Dialect.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/Utils/Utils.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/IR/IntegerSet.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/Support/Debug.h"

#include <set>

#define GET_OP_CLASSES
#include "carts/ArtsOps.cpp.inc"
using namespace mlir;
using namespace mlir::arts;

namespace mlir::arts::utils {
bool collectEffects(Operation *op,
                    SmallVectorImpl<MemoryEffects::EffectInstance> &effects,
                    bool ignoreBarriers) {
  /// Skip over barriers to avoid infinite recursion (those barriers would ask
  /// this barrier again).
  // if (ignoreBarriers && isa<BarrierOp>(op))
  //   return true;

  /// Ignore CacheLoads as they are already guaranteed to not have side effects
  /// in the context of a parallel op, these only exist while we are in the
  /// CPUifyPass
  // if (isa<CacheLoad>(op))
  //   return true;

  /// Collect effect instances the operation. Note that the implementation of
  /// getEffects erases all effect instances that have the type other than the
  /// template parameter so we collect them first in a local buffer and then
  /// copy.
  if (auto iface = dyn_cast<MemoryEffectOpInterface>(op)) {
    SmallVector<MemoryEffects::EffectInstance> localEffects;
    iface.getEffects(localEffects);
    llvm::append_range(effects, localEffects);
    return true;
  }

  if (op->hasTrait<OpTrait::HasRecursiveMemoryEffects>()) {
    for (auto &region : op->getRegions()) {
      for (auto &block : region) {
        for (auto &innerOp : block)
          if (!collectEffects(&innerOp, effects, ignoreBarriers))
            return false;
      }
    }
    return true;
  }

  if (auto cop = dyn_cast<LLVM::CallOp>(op)) {
    if (auto callee = cop.getCallee()) {
      StringRef calleeName = *callee;
      if (calleeName == "scanf" || calleeName == "__isoc99_scanf" ||
          calleeName == "fscanf" || calleeName == "__isoc99_fscanf") {
        // Global read
        effects.emplace_back(MemoryEffects::Effect::get<MemoryEffects::Read>());

        for (auto argp : llvm::enumerate(cop.getArgOperands())) {
          auto arg = argp.value();
          auto idx = argp.index();
          if (calleeName == "scanf" || calleeName == "__isoc99_scanf") {
            if (idx == 0)
              effects.emplace_back(::mlir::MemoryEffects::Read::get(), arg,
                                   ::mlir::SideEffects::DefaultResource::get());
            else
              effects.emplace_back(::mlir::MemoryEffects::Write::get(), arg,
                                   ::mlir::SideEffects::DefaultResource::get());
          } else {
            if (idx == 0 || idx == 1)
              effects.emplace_back(::mlir::MemoryEffects::Read::get(), arg,
                                   ::mlir::SideEffects::DefaultResource::get());
            if (idx == 0 || idx > 1)
              effects.emplace_back(::mlir::MemoryEffects::Write::get(), arg,
                                   ::mlir::SideEffects::DefaultResource::get());
          }
        }
        return true;
      }
      if (calleeName == "printf") {
        // Global read
        effects.emplace_back(
            MemoryEffects::Effect::get<MemoryEffects::Write>());
        for (auto arg : cop.getArgOperands()) {
          effects.emplace_back(::mlir::MemoryEffects::Read::get(), arg,
                               ::mlir::SideEffects::DefaultResource::get());
        }
        return true;
      }
      if (calleeName == "free") {
        for (auto arg : cop.getArgOperands()) {
          effects.emplace_back(::mlir::MemoryEffects::Free::get(), arg,
                               ::mlir::SideEffects::DefaultResource::get());
        }
        return true;
      }
      if (calleeName == "strlen") {
        for (auto arg : cop.getArgOperands()) {
          effects.emplace_back(::mlir::MemoryEffects::Read::get(), arg,
                               ::mlir::SideEffects::DefaultResource::get());
        }
        return true;
      }
    }
  }

  /// We need to be conservative here in case the op doesn't have the interface
  /// and assume it can have any possible effect.
  effects.emplace_back(MemoryEffects::Effect::get<MemoryEffects::Read>());
  effects.emplace_back(MemoryEffects::Effect::get<MemoryEffects::Write>());
  effects.emplace_back(MemoryEffects::Effect::get<MemoryEffects::Allocate>());
  effects.emplace_back(MemoryEffects::Effect::get<MemoryEffects::Free>());
  return false;
}

bool getEffectsBefore(Operation *op,
                      SmallVectorImpl<MemoryEffects::EffectInstance> &effects,
                      bool stopAtBarrier) {
  if (op != &op->getBlock()->front())
    for (Operation *it = op->getPrevNode(); it != nullptr;
         it = it->getPrevNode()) {
      // if (isa<BarrierOp>(it)) {
      //   if (stopAtBarrier)
      //     return true;
      //   else
      //     continue;
      // }
      if (!collectEffects(it, effects, /* ignoreBarriers */ true))
        return false;
    }

  bool conservative = false;

  if (isa<scf::ParallelOp, affine::AffineParallelOp>(op->getParentOp()))
    return true;

  // As we didn't hit another barrier, we must check the predecessors of this
  // operation.
  if (!getEffectsBefore(op->getParentOp(), effects, stopAtBarrier))
    return false;

  // If the parent operation is not guaranteed to execute its (single-block)
  // region once, walk the block.
  if (!isa<scf::IfOp, affine::AffineIfOp, memref::AllocaScopeOp>(
          op->getParentOp()))
    op->getParentOp()->walk([&](Operation *in) {
      if (conservative)
        return WalkResult::interrupt();
      if (!collectEffects(in, effects, /* ignoreBarriers */ true)) {
        conservative = true;
        return WalkResult::interrupt();
      }
      return WalkResult::advance();
    });

  return !conservative;
}

bool getEffectsAfter(Operation *op,
                     SmallVectorImpl<MemoryEffects::EffectInstance> &effects,
                     bool stopAtBarrier) {
  if (op != &op->getBlock()->back()) {
    for (Operation *it = op->getNextNode(); it != nullptr;
         it = it->getNextNode()) {
      // if (isa<BarrierOp>(it)) {
      //   if (stopAtBarrier)
      //     return true;
      //   continue;
      // }
      if (!collectEffects(it, effects, /* ignoreBarriers */ true))
        return false;
    }
  }

  bool conservative = false;

  if (isa<scf::ParallelOp, affine::AffineParallelOp>(op->getParentOp()))
    return true;

  /// As we didn't hit another barrier, we must check the successors of this
  /// operation.
  if (!getEffectsAfter(op->getParentOp(), effects, stopAtBarrier))
    return false;

  /// If the parent operation is not guaranteed to execute its (single-block)
  /// region once, walk the block.
  if (!isa<scf::IfOp, affine::AffineIfOp, memref::AllocaScopeOp>(
          op->getParentOp())) {
    op->getParentOp()->walk([&](Operation *in) {
      if (conservative)
        return WalkResult::interrupt();
      if (!collectEffects(in, effects, /* ignoreBarriers */ true)) {
        conservative = true;
        return WalkResult::interrupt();
      }
      return WalkResult::advance();
    });
  }

  return !conservative;
}

bool isReadOnly(Operation *op) {
  /// If the op has recursive memory effects, check all nested ops.
  if (op->hasTrait<OpTrait::HasRecursiveMemoryEffects>()) {
    for (Region &region : op->getRegions()) {
      for (auto &block : region) {
        for (auto &nestedOp : block)
          if (!isReadOnly(&nestedOp))
            return false;
      }
    }
    return true;
  }

  /// If the op has memory effects, try to characterize them to see if the op
  /// is trivially dead here.
  if (auto effectInterface = dyn_cast<MemoryEffectOpInterface>(op)) {
    /// Check to see if this op either has no effects, or only allocates/reads
    /// memory.
    SmallVector<MemoryEffects::EffectInstance, 1> effects;
    effectInterface.getEffects(effects);
    return llvm::all_of(effects, [](const MemoryEffects::EffectInstance &it) {
      return isa<MemoryEffects::Read>(it.getEffect());
    });
  }
  return false;
}

bool isReadNone(Operation *op) {
  if (op->hasTrait<OpTrait::HasRecursiveMemoryEffects>()) {
    for (Region &region : op->getRegions()) {
      for (auto &block : region) {
        for (auto &nestedOp : block)
          if (!isReadNone(&nestedOp))
            return false;
      }
    }
    return true;
  }

  if (auto effectInterface = dyn_cast<MemoryEffectOpInterface>(op)) {
    SmallVector<MemoryEffects::EffectInstance, 1> effects;
    effectInterface.getEffects(effects);
    return llvm::none_of(effects, [](const MemoryEffects::EffectInstance &it) {
      return isa<MemoryEffects::Read>(it.getEffect()) ||
             isa<MemoryEffects::Write>(it.getEffect());
    });
  }
  return false;
}

const std::set<std::string> &getNonCapturingFunctions() {
  static std::set<std::string> NonCapturingFunctions = {
      "free",           "printf",       "fprintf",       "scanf",
      "fscanf",         "gettimeofday", "clock_gettime", "getenv",
      "strrchr",        "strlen",       "sprintf",       "sscanf",
      "mkdir",          "fwrite",       "fread",         "memcpy",
      "cudaMemcpy",     "memset",       "cudaMemset",    "__isoc99_scanf",
      "__isoc99_fscanf"};
  return NonCapturingFunctions;
}

bool isCaptured(Value v, Operation *potentialUser, bool *seenuse) {
  SmallVector<Value> todo = {v};
  while (!todo.empty()) {
    Value v = todo.pop_back_val();
    for (auto u : v.getUsers()) {
      if (seenuse && u == potentialUser)
        *seenuse = true;

      // Skip over load operations
      // if (isa<memref::LoadOp, LLVM::LoadOp, affine::AffineLoadOp,
      //         polygeist::CacheLoad>(u))
      //   continue;

      // Check store operations
      if (auto s = dyn_cast<memref::StoreOp>(u)) {
        if (s.getValue() == v)
          return true;
        continue;
      }
      if (auto s = dyn_cast<affine::AffineStoreOp>(u)) {
        if (s.getValue() == v)
          return true;
        continue;
      }
      if (auto s = dyn_cast<LLVM::StoreOp>(u)) {
        if (s.getValue() == v)
          return true;
        continue;
      }

      // Handle pointer manipulation operations
      if (auto sub = dyn_cast<LLVM::GEPOp>(u)) {
        todo.push_back(sub);
        continue;
      }
      if (auto sub = dyn_cast<LLVM::BitcastOp>(u)) {
        todo.push_back(sub);
        continue;
      }
      if (auto sub = dyn_cast<LLVM::AddrSpaceCastOp>(u)) {
        todo.push_back(sub);
        continue;
      }
      if (auto sub = dyn_cast<memref::CastOp>(u)) {
        todo.push_back(sub);
        continue;
      }

      // Skip over specific operations
      if (isa<func::ReturnOp, LLVM::MemsetOp, LLVM::MemcpyOp, LLVM::MemmoveOp,
              memref::DeallocOp>(u))
        continue;

      // if (auto sub = dyn_cast<polygeist::SubIndexOp>(u)) {
      //   todo.push_back(sub);
      // }
      // if (auto sub = dyn_cast<polygeist::Memref2PointerOp>(u)) {
      //   todo.push_back(sub);
      // }
      // if (auto sub = dyn_cast<polygeist::Pointer2MemrefOp>(u)) {
      //   todo.push_back(sub);
      // }

      // Handle function calls
      if (auto cop = dyn_cast<LLVM::CallOp>(u)) {
        if (auto callee = cop.getCallee()) {
          if (getNonCapturingFunctions().count(callee->str()))
            continue;
        }
      }
      if (auto cop = dyn_cast<func::CallOp>(u)) {
        if (getNonCapturingFunctions().count(cop.getCallee().str()))
          continue;
      }

      // If none of the above conditions are met, the value is captured
      return true;
    }
  }

  return false;
}

Value getBase(Value v) {
  while (true) {
    // if (auto s = v.getDefiningOp<SubIndexOp>()) {
    //   v = s.getSource();
    //   continue;
    // }
    // if (auto s = v.getDefiningOp<Memref2PointerOp>()) {
    //   v = s.getSource();
    //   continue;
    // }
    // if (auto s = v.getDefiningOp<Pointer2MemrefOp>()) {
    //   v = s.getSource();
    //   continue;
    // }
    if (auto s = v.getDefiningOp<LLVM::GEPOp>()) {
      v = s.getBase();
      continue;
    }
    if (auto s = v.getDefiningOp<LLVM::BitcastOp>()) {
      v = s.getArg();
      continue;
    }
    if (auto s = v.getDefiningOp<LLVM::AddrSpaceCastOp>()) {
      v = s.getArg();
      continue;
    }
    if (auto s = v.getDefiningOp<memref::CastOp>()) {
      v = s.getSource();
      continue;
    }
    break;
  }
  return v;
}

bool isStackAlloca(Value v) {
  return v.getDefiningOp<memref::AllocaOp>() ||
         v.getDefiningOp<memref::AllocOp>() ||
         v.getDefiningOp<LLVM::AllocaOp>();
}

static bool mayAlias(Value v, Value v2) {
  v = getBase(v);
  v2 = getBase(v2);
  if (v == v2)
    return true;

  if (auto glob = v.getDefiningOp<memref::GetGlobalOp>()) {
    if (auto Aglob = v2.getDefiningOp<memref::GetGlobalOp>()) {
      return glob.getName() == Aglob.getName();
    }
  }

  if (auto glob = v.getDefiningOp<LLVM::AddressOfOp>()) {
    if (auto Aglob = v2.getDefiningOp<LLVM::AddressOfOp>()) {
      return glob.getGlobalName() == Aglob.getGlobalName();
    }
  }

  bool isAlloca[2] = {isStackAlloca(v), isStackAlloca(v2)};
  bool isGlobal[2] = {v.getDefiningOp<memref::GetGlobalOp>() ||
                          v.getDefiningOp<LLVM::AddressOfOp>(),
                      v2.getDefiningOp<memref::GetGlobalOp>() ||
                          v2.getDefiningOp<LLVM::AddressOfOp>()};

  // Non-equivalent allocas/global's cannot conflict with each other
  if ((isAlloca[0] || isGlobal[0]) && (isAlloca[1] || isGlobal[1]))
    return false;

  bool isArg[2] = {v.isa<BlockArgument>() &&
                       isa<FunctionOpInterface>(
                           v.cast<BlockArgument>().getOwner()->getParentOp()),
                   v2.isa<BlockArgument>() &&
                       isa<FunctionOpInterface>(
                           v2.cast<BlockArgument>().getOwner()->getParentOp())};

  // Stack allocations cannot have been passed as an argument.
  if ((isAlloca[0] && isArg[1]) || (isAlloca[1] && isArg[0]))
    return false;

  // Non captured base allocas cannot conflict with another base value.
  if ((isAlloca[0] && !isCaptured(v)) || (isAlloca[1] && !isCaptured(v2)))
    return false;

  return true;
}

bool mayAlias(MemoryEffects::EffectInstance a,
              MemoryEffects::EffectInstance b) {
  if (a.getResource() != b.getResource())
    return false;
  if (Value v2 = b.getValue())
    return mayAlias(a, v2);
  if (Value v = a.getValue())
    return mayAlias(b, v);
  return true;
}

bool mayAlias(MemoryEffects::EffectInstance a, Value v2) {
  if (Value v = a.getValue())
    return mayAlias(v, v2);
  return true;
}

} // namespace mlir::arts::utils

/// Operation builders.
// //
// // 1) Example: Implementation of a build function for EdtOp
// //
// void arts::buildEdtOp(OpBuilder &builder, OperationState &state,
//                       ArrayAttr parameters, ArrayAttr dependencies) {
//   // You could store these arrays as attributes:
//   state.addAttribute("parameters", parameters);
//   state.addAttribute("dependencies", dependencies);

//   // The op has one region for the body:
//   Region *bodyRegion = state.addRegion();
//   // Typically, you might create a block in that region if you want an
//   immediate body
//   // but often you leave it to the user or a pattern to add the block later.
// }

// //
// // 2) ARTS_EdtOp parse/print
// //    If you declared "def ARTS_EdtOp : ARTS_Op<"edt">" in TableGen
// //    and want to override the default parse/print, do it here.
// //

// // Custom Parser for EdtOp
// ParseResult ARTS_EdtOp::parse(OpAsmParser &parser, OperationState &result) {
//   // Syntax we desire:
//   //   arts.edt parameters(...stuff...) dependencies(...stuff...) {
//   //       <body region>
//   //   }

//   // 1) parse literal "parameters("
//   if (parser.parseKeyword("parameters"))
//     return failure();
//   if (parser.parseLParen())
//     return failure();

//   // 2) parse an optional list of anything you want to store in "parameters"
//   //    e.g. parse array of symbols, or parse a custom form.
//   //    For simplicity, let's parse an ArrayAttr of strings
//   SmallVector<Attribute, 4> paramList;
//   // You might do a loop or parseCommaSeparatedList. For example:
//   if (parser.parseOptionalRParen()) {
//     // we have something
//     // parse a list of string-literals or symbol references
//     while (true) {
//       // parse a string attribute
//       // e.g. parse a string-literal
//       // (Below is a simplistic approach; real code might do more checks)
//       Attribute strAttr;
//       if (parser.parseAttribute(strAttr))
//         return failure();
//       paramList.push_back(strAttr);
//       // check if we are done or see a comma
//       if (succeeded(parser.parseOptionalComma()))
//         continue; // parse next
//       break;
//     }
//     // finally parse ')'
//     if (parser.parseRParen())
//       return failure();
//   } else {
//     // means we had an immediate ')', so no parameters
//   }
//   auto paramAttr = parser.getBuilder().getArrayAttr(paramList);
//   result.addAttribute("parameters", paramAttr);

//   // 3) parse literal "dependencies("
//   if (parser.parseKeyword("dependencies"))
//     return failure();
//   if (parser.parseLParen())
//     return failure();
//   // parse array of dictionary attrs or something
//   SmallVector<Attribute, 4> depsList;
//   // let's say we parse dictionary attributes separated by commas
//   if (failed(parser.parseOptionalRParen())) {
//     // parse at least one
//     while (true) {
//       DictionaryAttr dictAttr;
//       if (parser.parseAttribute(dictAttr))
//         return failure();
//       depsList.push_back(dictAttr);
//       if (succeeded(parser.parseOptionalComma()))
//         continue;
//       break;
//     }
//     if (parser.parseRParen())
//       return failure();
//   }
//   auto depsAttr = parser.getBuilder().getArrayAttr(depsList);
//   result.addAttribute("dependencies", depsAttr);

//   // 4) parse region body
//   Region *body = result.addRegion();
//   if (parser.parseRegion(*body))
//     return failure();

//   // Let the user optionally add a block in the body, or we can do it
//   ourselves: ARTS_EdtOp::ensureTerminator(*body, parser.getBuilder(),
//   result.location);

//   return success();
// }

// // Custom Printer for EdtOp
// void ARTS_EdtOp::print(OpAsmPrinter &printer) {
//   // We want to print:
//   //   arts.edt parameters(...) dependencies(...) <region>
//   printer << "arts.edt parameters(";

//   // Print the parameters array
//   auto paramArray = (*this)->getAttrOfType<ArrayAttr>("parameters");
//   if (paramArray && !paramArray.empty()) {
//     // print them comma-separated
//     bool first = true;
//     for (auto attr : paramArray) {
//       if (!first) printer << ", ";
//       printer.printAttribute(attr);
//       first = false;
//     }
//   }
//   printer << ") dependencies(";

//   // Print the dependencies array
//   auto depsArray = (*this)->getAttrOfType<ArrayAttr>("dependencies");
//   if (depsArray && !depsArray.empty()) {
//     bool first = true;
//     for (auto dict : depsArray) {
//       if (!first) printer << ", ";
//       printer.printAttribute(dict);
//       first = false;
//     }
//   }
//   printer << ") ";

//   // Print the region
//   printer.printRegion(getBodyRegion(), /*printEntryBlockArgs=*/false,
//                       /*printBlockTerminators=*/false);
// }

// //
// // 3) For other ops like ARTS_EpochOp, ARTS_SingleOp, ARTS_ParallelOp,
// //    ARTS_DataBlockCreateOp, we might just rely on the default
// //    assemblyFormat from TableGen, or do similarly minimal parse/print if
// needed.
// //

// // Example: ARTS_DataBlockCreateOp parse/print
// ParseResult ARTS_DataBlockCreateOp::parse(OpAsmParser &parser, OperationState
// &result) {
//   // Suppose we do:   datablock.create size(<int>) align(<int>) : <type>
//   // For brevity, skip thorough checks:
//   if (parser.parseKeyword("size"))
//     return failure();
//   if (parser.parseLParen())
//     return failure();
//   IntegerAttr sizeAttr;
//   if (parser.parseAttribute(sizeAttr))
//     return failure();
//   if (parser.parseRParen())
//     return failure();

//   if (parser.parseKeyword("align"))
//     return failure();
//   if (parser.parseLParen())
//     return failure();
//   IntegerAttr alignAttr;
//   if (parser.parseAttribute(alignAttr))
//     return failure();
//   if (parser.parseRParen())
//     return failure();

//   Type resultType;
//   if (parser.parseColonType(resultType))
//     return failure();

//   // store them
//   result.addAttribute("size", sizeAttr);
//   result.addAttribute("align", alignAttr);
//   result.addTypes(resultType);
//   return success();
// }

// void ARTS_DataBlockCreateOp::print(OpAsmPrinter &printer) {
//   printer << "arts.datablock.create size(" << getSize() << ") align(" <<
//   getAlign() << ") : "
//           << getResult().getType();
// }

// //
// // 4) Optionally you can define verifiers, e.g. verify() for EdtOp
// //
// LogicalResult ARTS_EdtOp::verify() {
//   // For example, check that the 'dependencies' attr is an array of
//   dictionaries auto deps = (*this)->getAttrOfType<ArrayAttr>("dependencies");
//   if (!deps)
//     return emitOpError() << "missing 'dependencies' attribute";
//   for (auto d : deps) {
//     if (!d.isa<DictionaryAttr>())
//       return emitOpError() << "expected dictionary for dependencies, got " <<
//       d;
//     // etc...
//   }
//   return success();
// }

// //
// // 5) That’s it! The rest of the ops can remain with default parse/print
// //    if your TableGen assemblyFormat is enough, or define them similarly
// here.
// //