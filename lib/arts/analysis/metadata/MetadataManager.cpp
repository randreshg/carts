///==========================================================================///
/// MetadataManager.cpp
///
/// This file is intentionally minimal. MetadataManager is now a facade
/// whose methods are defined inline in the header, delegating to:
///   - MetadataRegistry (storage, retrieval, lifecycle)
///   - MetadataIO (JSON import/export, file I/O, cache)
///   - MetadataAttacher (attaching metadata from JSON cache to operations)
///==========================================================================///

#include "arts/analysis/metadata/MetadataManager.h"

/// All MetadataManager methods are inline delegates defined in the header.
/// This translation unit ensures the header is compiled and any implicit
/// template instantiations are emitted.
