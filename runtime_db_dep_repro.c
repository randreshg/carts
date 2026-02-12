#include "arts/arts.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static artsGuid_t gShutdown = NULL_GUID;
static artsGuid_t gDb = NULL_GUID;
static bool gSkipCreate = false;

static void shutdownEdt(uint32_t paramc, uint64_t *paramv, uint32_t depc,
                        artsEdtDep_t depv[]) {
  (void)paramc;
  (void)paramv;
  (void)depc;
  (void)depv;
  printf("[shutdown] node=%u worker=%u\n", artsGetCurrentNode(),
         artsGetCurrentWorker());
  artsShutdown();
}

static void consumeEdt(uint32_t paramc, uint64_t *paramv, uint32_t depc,
                       artsEdtDep_t depv[]) {
  (void)paramc;
  (void)paramv;
  if (depc > 0 && depv[0].ptr) {
    unsigned int *v = (unsigned int *)depv[0].ptr;
    printf("[consume] node=%u worker=%u value=%u guid=%lu\n",
           artsGetCurrentNode(), artsGetCurrentWorker(), *v, depv[0].guid);
  } else {
    printf("[consume] node=%u worker=%u null dep ptr\n", artsGetCurrentNode(),
           artsGetCurrentWorker());
  }
  artsSignalEdtValue(gShutdown, 0, 0);
}

void initPerNode(unsigned int nodeId, int argc, char **argv) {
  (void)argc;
  (void)argv;
  const char *env = getenv("SKIP_CREATE");
  gSkipCreate = (env && env[0] == '1');

  gDb = artsReserveGuidRoute(ARTS_DB_READ, 1);
  gShutdown = artsReserveGuidRoute(ARTS_EDT, 0);
  printf("[initPerNode] node=%u skip=%d dbGuid=%lu owner=%u totalNodes=%u\n",
         nodeId, gSkipCreate ? 1 : 0, gDb, artsGuidGetRank(gDb),
         artsGetTotalNodes());
}

void initPerWorker(unsigned int nodeId, unsigned int workerId, int argc,
                   char **argv) {
  (void)argc;
  (void)argv;
  if (workerId != 0)
    return;

  if (artsIsGuidLocal(gDb) && !gSkipCreate) {
    unsigned int *ptr =
        (unsigned int *)artsDbCreateWithGuid(gDb, sizeof(unsigned int));
    *ptr = 1234 + nodeId;
    printf("[initPerWorker] node=%u created db guid=%lu value=%u\n", nodeId,
           gDb, *ptr);
  } else if (artsIsGuidLocal(gDb) && gSkipCreate) {
    printf("[initPerWorker] node=%u SKIP local create guid=%lu\n", nodeId, gDb);
  }

  if (nodeId == 0) {
    artsEdtCreateWithGuid(shutdownEdt, gShutdown, 0, NULL,
                          artsGetTotalNodes() * artsGetTotalWorkers());
    artsGuid_t edt = artsEdtCreate(consumeEdt, 0, 0, NULL, 1);
    printf("[initPerWorker] node0 signaling edt=%lu with db guid=%lu\n", edt,
           gDb);
    artsSignalEdt(edt, 0, gDb);
  }
}

int main(int argc, char **argv) { return artsRT(argc, argv); }
