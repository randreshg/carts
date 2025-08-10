///==========================================================================
/// File: ArtsDebug.h
/// Centralized debug utilities for ARTS passes and components
///==========================================================================

#ifndef ARTS_UTILS_ARTSDEBUG_H
#define ARTS_UTILS_ARTSDEBUG_H

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace mlir {
namespace arts {
/// Returns the ARTS debug output stream. Intended as the single entrypoint
/// for emitting debug logs so callers don't use llvm::dbgs() directly.
static inline llvm::raw_ostream &debugStream() { return llvm::dbgs(); }

/// Common line separator used in debug headers/footers
#ifndef ARTS_LINE
#define ARTS_LINE "-----------------------------------------\n"
#endif

/// Macro to set up debug infrastructure for a pass/component
/// Usage: ARTS_DEBUG_SETUP(edt) at the top of your file
#define ARTS_DEBUG_SETUP(pass_name)                                            \
  static constexpr const char *ARTS_DEBUG_TYPE_STR = #pass_name;

/// Debug macros that work with LLVM's debug infrastructure
// #define ARTS_DEBUG(x) DEBUG_WITH_TYPE(ARTS_DEBUG_TYPE_STR, { x; })

/// ARTS debug stream accessor. Use this instead of llvm::dbgs()
#define ARTS_DBGS() debugStream()

#define ARTS_DEBUG_TYPE(x)                                                     \
  DEBUG_WITH_TYPE(ARTS_DEBUG_TYPE_STR, {                                       \
    ARTS_DBGS() << "[" << ARTS_DEBUG_TYPE_STR << "] " << x << "\n";            \
  })

#define ARTS_DEBUG_MSG(msg)                                                    \
  DEBUG_WITH_TYPE(ARTS_DEBUG_TYPE_STR, { ARTS_DBGS() << msg << "\n"; })

/// Multi-statement debug region. Wraps an entire block under LLVM_DEBUG.
/// Example:
///   ARTS_DEBUG_REGION(
///     ARTS_DBGS() << "String memrefs:\n";
///     for (auto value : stringMemRefs)
///       ARTS_DBGS() << "  " << value << "\n";
///   );
#define ARTS_DEBUG_REGION(...)                                                 \
  DEBUG_WITH_TYPE(ARTS_DEBUG_TYPE_STR, {__VA_ARGS__})

/// Debug section with automatic header/footer around a region.
/// Example:
///   ARTS_DEBUG_SECTION("StringAnalysis",
///     ARTS_DBGS() << "String memrefs:\n";
///     for (auto value : stringMemRefs)
///       ARTS_DBGS() << "  " << value << "\n";
///   );
#define ARTS_DEBUG_SECTION(title, ...)                                         \
  DEBUG_WITH_TYPE(ARTS_DEBUG_TYPE_STR, {                                       \
    ARTS_DBGS() << "\n" << ARTS_LINE << title << " STARTED\n" << ARTS_LINE;    \
    __VA_ARGS__;                                                               \
    ARTS_DBGS() << "\n" << ARTS_LINE << title << " FINISHED\n" << ARTS_LINE;   \
  })

/// Standard header/footer patterns for passes
#define ARTS_DEBUG_HEADER(pass_name)                                           \
  DEBUG_WITH_TYPE(ARTS_DEBUG_TYPE_STR, {                                       \
    auto &__os = ARTS_DBGS();                                                  \
    __os.changeColor(llvm::raw_ostream::CYAN, /*bold=*/true);                  \
    __os << "\n" << ARTS_LINE << #pass_name " STARTED\n" << ARTS_LINE;         \
    __os.resetColor();                                                         \
  })

#define ARTS_DEBUG_FOOTER(pass_name)                                           \
  DEBUG_WITH_TYPE(ARTS_DEBUG_TYPE_STR, {                                       \
    auto &__os = ARTS_DBGS();                                                  \
    __os.changeColor(llvm::raw_ostream::GREEN, /*bold=*/true);                 \
    __os << "\n" << ARTS_LINE << #pass_name " FINISHED\n" << ARTS_LINE;        \
    __os.resetColor();                                                         \
  })

/// Colored levels
#define ARTS_INFO(x)                                                           \
  DEBUG_WITH_TYPE(ARTS_DEBUG_TYPE_STR, {                                       \
    auto &__os = ARTS_DBGS();                                                  \
    __os.changeColor(llvm::raw_ostream::WHITE, /*bold=*/true);                 \
    __os << "[INFO] " << x << "\n";                                            \
    __os.resetColor();                                                         \
  })

#define ARTS_DEBUG(x)                                                          \
  DEBUG_WITH_TYPE(ARTS_DEBUG_TYPE_STR, {                                       \
    auto &__os = ARTS_DBGS();                                                  \
    __os.changeColor(llvm::raw_ostream::MAGENTA, /*bold=*/true);               \
    __os << "[DEBUG] " << x << "\n";                                           \
    __os.resetColor();                                                         \
  })

#define ARTS_WARN(x)                                                           \
  DEBUG_WITH_TYPE(ARTS_DEBUG_TYPE_STR, {                                       \
    auto &__os = ARTS_DBGS();                                                  \
    __os.changeColor(llvm::raw_ostream::YELLOW, /*bold=*/true);                \
    __os << "[WARN] " << x << "\n";                                            \
    __os.resetColor();                                                         \
  })

#define ARTS_ERROR(x)                                                          \
  DEBUG_WITH_TYPE(ARTS_DEBUG_TYPE_STR, {                                       \
    auto &__os = ARTS_DBGS();                                                  \
    __os.changeColor(llvm::raw_ostream::RED, /*bold=*/true);                   \
    __os << "[ERROR] " << x << "\n";                                           \
    __os.resetColor();                                                         \
  })
} // namespace arts
} // namespace mlir
#endif // ARTS_UTILS_ARTSDEBUG_H
