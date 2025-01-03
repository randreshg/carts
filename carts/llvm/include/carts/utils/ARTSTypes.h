//============================================================================//
//                                ARTSTypes.h                                 //
//============================================================================//

#ifndef LLVM_ARTS_TYPES_H
#define LLVM_ARTS_TYPES_H

#include "llvm/ADT/SetVector.h"
#include "llvm/IR/InstrTypes.h"
#include <sys/types.h>

namespace arts {
using namespace llvm;
using namespace std;

/// Forward declarations
class EDT;
class DataBlock;

/// Types
using EDTValue = Value;
using EDTValueSet = SetVector<EDTValue *>;
using EDTCallBase = CallBase;
using EDTSet = SetVector<EDT *>;
using EDTFunction = Function;
using EDTFunctionSet = SetVector<EDTFunction *>;
using DataBlockSet = SetVector<DataBlock *>;
using BlockSequence = SmallVector<BasicBlock *, 0>;

/// ------------------------------------------------------------------- ///
///                        ART Annotations                              ///
/// It contains the types used by the ARTS runtime library.
/// ------------------------------------------------------------------- ///
namespace annotations {
/// ARTS EDT Types
#define ARTS_MD "arts"
#define ARTS_EDT "arts.edt"
#define ARTS_EDT_TASK "arts.edt.task"
#define ARTS_EDT_MAIN "arts.edt.main"
#define ARTS_EDT_SYNC "arts.edt.sync"
#define ARTS_EDT_SYNC_DONE "arts.edt.sync.done"
#define ARTS_EDT_PARALLEL "arts.edt.parallel"
#define ARTS_EDT_PARALLEL_DONE "arts.edt.parallel.done"

/// ARTS DB Types
#define ARTS_DB "arts.db"
#define ARTS_DB_READ "arts.db.read"
#define ARTS_DB_WRITE "arts.db.write"
#define ARTS_DB_READWRITE "arts.db.readwrite"
#define ARTS_DB_PRIVATE "arts.db.private"
#define ARTS_DB_FIRSTPRIVATE "arts.db.firstprivate"
#define ARTS_DB_LASTPRIVATE "arts.db.lastprivate"
#define ARTS_DB_SHARED "arts.db.shared"
#define ARTS_DB_DEFAULT "arts.db.default"
} // end namespace annotations

/// ------------------------------------------------------------------- ///
///                            ART TYPES                                ///
/// It contains the types used by the ARTS runtime library.
/// ------------------------------------------------------------------- ///
namespace types {

/// EDT types
enum EDTTypeKind {
  TK_UNKNOWN = 0,
  TK_TASK = (1 << 0),               // "0001"
  TK_SYNC = (1 << 1) | TK_TASK,     // "0011"
  TK_MAIN = (1 << 2) | TK_TASK,     // "0101"
  TK_PARALLEL = (1 << 3) | TK_SYNC, // "1011"

};

enum class EDTType { Task, Main, Sync, Parallel, Unknown };
enum class EDTArgType { Param, Dep, Unknown };
const Twine toString(const EDTType Ty);
const Twine toString(const EDTArgType Ty);
EDTType toEDTType(const StringRef Str);
EDTArgType toEDTArgType(const StringRef Str);
/// IDs for all arts runtime library (RTL) functions.
enum class RuntimeFunction {
#define ARTS_RTL(Enum, ...) Enum,
#include "arts/codegen/ARTSKinds.def"
};

#define ARTS_RTL(Enum, ...)                                                    \
  constexpr auto Enum = arts::types::RuntimeFunction::Enum;
#include "arts/codegen/ARTSKinds.def"

} // end namespace types
} // end namespace arts
#endif // LLVM_ARTS_TYPES_H