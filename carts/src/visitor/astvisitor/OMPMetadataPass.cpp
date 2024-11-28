/*
    Author: Knud Andersen
    Date: 07/29/24
    ======================================================================================================
    Description: LLVM Pass that uploads gathered AST/IR metadata to an output IR
   file using the LLVM API.|
    ======================================================================================================

*/

#include "OMPMetadata.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
// #include "llvm/Support/raw_ostream.h"
#include <stdio.h>
// #include <string>

using namespace llvm;

typedef std::pair<int, std::pair<int, int>> META_ID;
META_ID InitMETAID(int a, int b, int c) { return {a, {b, c}}; }
// Map of instruction metadata we have added, so we can keep pointers to it and
// add to function metadata. Key: META_ID = <directive_line, variable_line,
// variable_column> Value: <Variable, Metadata>

// Note that we keep track of the variable to extract its clause when adding
// metadata
//  and we keep track of the metadata we have added as well so we can append to
//  the function
std::map<META_ID, std::pair<OMPVariable *, MDNode *>> meta_map;
int globvar = 0;

namespace {
struct OMP_PASS : public FunctionPass {
  static char ID;
  OMP_PASS() : FunctionPass(ID) {}
  bool runOnFunction(Function &F) override {
    std::vector<OMPDirective *> metadata =
        GetMetadata(GLOBAL_AST_PATH, GLOBAL_IR_PATH);
    if (!(globvar++)) {
      // OMPPrintMetadata(metadata);
      for (auto *directive : metadata) {
        for (auto *variable : directive->variables) {
          meta_map.insert(
              {{directive->line, {variable->line, variable->column}},
               {variable, NULL}});
        }
      }
    }
    /* int function_line = -1;
    Metadata* function_meta = F.getMetadata("dbg");
    DISubprogram* Loc = NULL;
    if (function_meta && (Loc = dyn_cast<DISubprogram>(function_meta))) {
      function_line = Loc->getLine();
    }  */
    // STEP 1:
    //         a) check if there there llvm.dbg.declare statements that allocate
    //         variables used in OpenMP clauses (according to AST/IR
    //         information) b) if there is such a matching variable, record it
    //         in a map (to add to function metadata later), and set its own
    //         instruction metadata
    for (auto &BB : F) {
      for (auto &I : BB) {
        CallInst *call;
        Metadata *meta;
        DILocation *Loc;
        if ((call = dyn_cast<CallInst>(&I))) {
          if (call->getCalledFunction()->getName() == "llvm.dbg.declare") {
            if ((meta = I.getMetadata("dbg"))) {
              if ((Loc = dyn_cast<DILocation>(meta))) {
                int found_line = Loc->getLine();
                int found_col = Loc->getColumn();
                for (auto *directive : metadata) {
                  int this_directive_line = directive->line;
                  META_ID this_id =
                      InitMETAID(this_directive_line, found_line, found_col);
                  if (meta_map.count(this_id) &&
                      meta_map[this_id].second == NULL) {
                    // We found a matching entry!
                    LLVMContext &icon = I.getContext();
                    MDNode *clause_md = MDNode::get(
                        icon,
                        MDString::get(icon, meta_map[this_id].first->clause));
                    // Record it in the map:
                    meta_map[this_id].second = clause_md;
                    // Set its metadata:
                    MDNode *old_md = I.getMetadata("carts_instruction");
                    // See if there is other metadata here:
                    MDNode *final_md = clause_md;
                    if (old_md != NULL) {
                      // There is a previous entry, i.e. this variable is used
                      // in multiple OpenMP clauses. There will now be subnodes,
                      // and if a function uses this variable as a clause, it
                      // will point to the appropriate subnode, not the node
                      // itself.
                      final_md = MDNode::get(icon, {old_md, clause_md});
                    }
                    I.setMetadata("carts_instruction", final_md);
                  }
                }
              }
            }
          }
        }
      }
    }
    // Step 2:
    //       a) If we are in an openMP call (and not a debug), then we want to
    //       add its function metadata b) Loop through all the AST/IR
    //       directives, at the matching line number add every single meta_map
    //       entry with metadata that we found in STEP 1
    if (F.getName().contains(".omp") && !F.getName().contains("debug")) {
      Metadata *meta = F.getMetadata("dbg");
      if (meta) {
        DISubprogram *loc = dyn_cast<DISubprogram>(meta);
        if (loc) {
          int found_dir_line = loc->getLine();
          LLVMContext &f_context = F.getContext();
          for (auto *directive : metadata) {
            if ((int)directive->line == found_dir_line) {
              MDNode *name_md = MDNode::get(
                  f_context,
                  MDString::get(f_context, directive->directive_name));
              std::vector<Metadata *> var_md_nodes;
              for (auto *variable : directive->variables) {
                META_ID this_id = InitMETAID(found_dir_line, variable->line,
                                             variable->column);
                if (meta_map.count(this_id)) {
                  var_md_nodes.push_back(meta_map[this_id].second);
                }
              }
              ArrayRef<Metadata *> var_md_array(var_md_nodes);
              MDNode *var_md = MDNode::get(f_context, var_md_array);
              MDNode *all_dir_md = MDNode::get(f_context, {name_md, var_md});
              F.setMetadata("carts_function", all_dir_md);
            }
          }
        }
      }
    }
    OMPDestroyMetadata(metadata);
    // Step 3: Strip "dbg" info
    return false; // We haven't modified the program, so return false.
  }
};
} // namespace

char OMP_PASS::ID = 0;

// Register the pass so `opt` can call it with `opt -hello-world`.
static RegisterPass<OMP_PASS> X("OMP_PASS", "OpenMP Metadata Pass",
                                false /* only looks at CFG */,
                                false /* analysis Pass */);
