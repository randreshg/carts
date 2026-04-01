///==========================================================================///
/// File: MetadataManager.cpp
///
/// Centralized metadata facade implementation.
///==========================================================================///

#include "arts/analysis/metadata/MetadataManager.h"

using namespace mlir;
using namespace mlir::arts;

MetadataManager::MetadataManager(MLIRContext *context)
    : registry(context), io(registry), attacher(registry, io) {}

LoopMetadata *MetadataManager::addLoopMetadata(Operation *op) {
  return registry.addLoopMetadata(op);
}

MemrefMetadata *MetadataManager::addMemrefMetadata(Operation *op) {
  return registry.addMemrefMetadata(op);
}

ValueMetadata *MetadataManager::addValueMetadata(Operation *op) {
  return registry.addValueMetadata(op);
}

LoopMetadata *MetadataManager::getLoopMetadata(Operation *op) {
  return registry.getLoopMetadata(op);
}

const LoopMetadata *MetadataManager::getLoopMetadata(Operation *op) const {
  return registry.getLoopMetadata(op);
}

MemrefMetadata *MetadataManager::getMemrefMetadata(Operation *op) {
  return registry.getMemrefMetadata(op);
}

const MemrefMetadata *MetadataManager::getMemrefMetadata(Operation *op) const {
  return registry.getMemrefMetadata(op);
}

MemrefMetadata *MetadataManager::getMemrefMetadataById(int64_t artsId) {
  return registry.getMemrefMetadataById(artsId);
}

const MemrefMetadata *
MetadataManager::getMemrefMetadataById(int64_t artsId) const {
  return registry.getMemrefMetadataById(artsId);
}

ValueMetadata *MetadataManager::getValueMetadata(Operation *op) {
  return registry.getValueMetadata(op);
}

const ValueMetadata *MetadataManager::getValueMetadata(Operation *op) const {
  return registry.getValueMetadata(op);
}

bool MetadataManager::hasMetadata(Operation *op) const {
  return registry.hasMetadata(op);
}

bool MetadataManager::removeMetadata(Operation *op) {
  return registry.removeMetadata(op);
}

void MetadataManager::clear() {
  registry.getMetadataMap().clear();
  io.clearCache();
  registry.getIdRegistry().reset();
}

void MetadataManager::importFromOperations(ModuleOp module) {
  registry.importFromOperations(module);
}

void MetadataManager::exportToOperations() { registry.exportToOperations(); }

void MetadataManager::collectFromModule(ModuleOp module) {
  registry.collectFromModule(module);
}

bool MetadataManager::importFromJson(llvm::StringRef jsonStr) {
  return io.importFromJson(jsonStr);
}

bool MetadataManager::importFromJsonFile(ModuleOp module,
                                         llvm::StringRef filename) {
  return attacher.importFromJsonFile(module, filename);
}

std::string MetadataManager::exportToJson() const { return io.exportToJson(); }

bool MetadataManager::exportToJsonFile(llvm::StringRef filename) const {
  return io.exportToJsonFile(filename);
}

SmallVector<Operation *> MetadataManager::getOperationsWithMetadata() const {
  return registry.getOperationsWithMetadata();
}

SmallVector<Operation *> MetadataManager::getLoopOperations() const {
  return registry.getLoopOperations();
}

SmallVector<Operation *> MetadataManager::getMemrefOperations() const {
  return registry.getMemrefOperations();
}

size_t MetadataManager::size() const { return registry.size(); }

bool MetadataManager::empty() const { return registry.empty(); }

MLIRContext *MetadataManager::getContext() const { return registry.getContext(); }

void MetadataManager::setMetadataFile(llvm::StringRef filename) {
  io.setMetadataFile(filename);
}

bool MetadataManager::ensureLoopMetadata(Operation *op) {
  return attacher.ensureLoopMetadata(op);
}

bool MetadataManager::ensureMemrefMetadata(Operation *op) {
  return attacher.ensureMemrefMetadata(op);
}

ArtsId MetadataManager::assignOperationId(Operation *op) {
  return registry.assignOperationId(op);
}

bool MetadataManager::transferMetadata(Operation *sourceOp, Operation *targetOp) {
  bool transferred = registry.transferMetadata(sourceOp, targetOp);
  if (transferred)
    stampTransferredMetadata(sourceOp, targetOp);
  return transferred;
}

bool MetadataManager::replaceMetadataForRewrite(Operation *sourceOp,
                                                Operation *targetOp) {
  if (!sourceOp || !targetOp)
    return false;
  refreshMetadata(sourceOp);
  bool replaced = registry.replaceMetadataForRewrite(sourceOp, targetOp);
  if (replaced)
    stampTransferredMetadata(sourceOp, targetOp);
  return replaced;
}

bool MetadataManager::rewriteMetadata(Operation *sourceOp, Operation *targetOp) {
  if (!sourceOp || !targetOp)
    return false;

  bool replaced = replaceMetadataForRewrite(sourceOp, targetOp);
  if (!replaced)
    copyArtsMetadataAttrs(sourceOp, targetOp);

  bool refreshed = refreshMetadata(targetOp);
  if (refreshed || replaced)
    stampTransferredMetadata(sourceOp, targetOp);
  return refreshed || replaced;
}

bool MetadataManager::rewriteLoopMetadata(Operation *sourceOp,
                                          Operation *targetOp) {
  ensureLoopMetadata(sourceOp);
  return rewriteMetadata(sourceOp, targetOp);
}

bool MetadataManager::cloneLoopMetadata(Operation *sourceOp,
                                        Operation *targetOp) {
  if (!sourceOp || !targetOp)
    return false;
  ensureLoopMetadata(sourceOp);
  if (!refreshMetadata(sourceOp) || !refreshMetadata(targetOp))
    return false;

  auto *sourceMetadata = getLoopMetadata(sourceOp);
  auto *targetMetadata = getLoopMetadata(targetOp);
  if (!sourceMetadata || !targetMetadata)
    return false;

  targetMetadata->potentiallyParallel = sourceMetadata->potentiallyParallel;
  targetMetadata->hasReductions = sourceMetadata->hasReductions;
  targetMetadata->reductionKinds = sourceMetadata->reductionKinds;
  targetMetadata->hasInterIterationDeps = sourceMetadata->hasInterIterationDeps;
  targetMetadata->memrefsWithLoopCarriedDeps =
      sourceMetadata->memrefsWithLoopCarriedDeps;
  targetMetadata->parallelClassification =
      sourceMetadata->parallelClassification;
  targetMetadata->exportToOp();
  return true;
}

bool MetadataManager::refreshMetadata(Operation *op) {
  bool refreshed = registry.refreshMetadata(op);
  if (refreshed)
    ensureMetadataProvenance(op, MetadataProvenanceKind::Exact);
  return refreshed;
}

IdRegistry &MetadataManager::getIdRegistry() { return registry.getIdRegistry(); }

const IdRegistry &MetadataManager::getIdRegistry() const {
  return registry.getIdRegistry();
}

void MetadataManager::printStatistics(llvm::raw_ostream &os) const {
  registry.printStatistics(os);
}

void MetadataManager::dump() const { registry.dump(); }

MetadataRegistry &MetadataManager::getRegistry() { return registry; }

const MetadataRegistry &MetadataManager::getRegistry() const { return registry; }

MetadataIO &MetadataManager::getIO() { return io; }

const MetadataIO &MetadataManager::getIO() const { return io; }

MetadataAttacher &MetadataManager::getAttacher() { return attacher; }

const MetadataAttacher &MetadataManager::getAttacher() const { return attacher; }

void MetadataManager::stampTransferredMetadata(Operation *sourceOp,
                                               Operation *targetOp) {
  if (!sourceOp || !targetOp)
    return;

  int64_t sourceId = getArtsId(sourceOp);
  if (sourceId <= 0) {
    if (ArtsId assigned = registry.assignOperationId(sourceOp);
        assigned.has_value())
      sourceId = assigned.value();
  }

  if (sourceId > 0)
    setMetadataOriginId(targetOp, sourceId);
  setMetadataProvenance(targetOp, MetadataProvenanceKind::Transferred);
}
