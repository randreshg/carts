#include "arts.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Runtime-first manual reference for an async Seidel-style iteration chain.
 *
 * Design contract:
 * - `iter_edt` is the launcher for one logical iteration.
 * - `inner_cont_edt` runs after all workers in that iteration finish.
 * - `advance_edt` is the only cross-iteration continuation.
 * - Mutable iteration state flows through a scratch/state DB in depv.
 * - The program never calls arts_wait_on_handle().
 *
 * This is the shape EpochOpt should target for epoch-only wavefront loops that
 * do not need a self-recursive CPS continuation EDT.
 */

#define NUM_BLOCKS 3
#define BLOCK_WIDTH 4
#define MAX_ITERS 4

typedef struct {
  uint32_t iter;
  uint32_t max_iters;
  arts_guid_t finish_guid;
  arts_guid_t block_guids[NUM_BLOCKS];
} seidel_state_t;

static void fail_example(const char *msg) {
  arts_printf("seidel_wavefront_async FAIL: %s\n", msg);
  abort();
}

static void finish_edt(uint32_t paramc, const uint64_t *paramv, uint32_t depc,
                       arts_edt_dep_t depv[]);
static void iter_edt(uint32_t paramc, const uint64_t *paramv, uint32_t depc,
                     arts_edt_dep_t depv[]);
static void inner_cont_edt(uint32_t paramc, const uint64_t *paramv,
                           uint32_t depc, arts_edt_dep_t depv[]);
static void advance_edt(uint32_t paramc, const uint64_t *paramv, uint32_t depc,
                        arts_edt_dep_t depv[]);

static void seidel_block_worker(uint32_t paramc, const uint64_t *paramv,
                                uint32_t depc, arts_edt_dep_t depv[]) {
  (void)paramc;
  (void)depc;

  uint32_t block = (uint32_t)paramv[0];
  int *out = (int *)depv[0].ptr;
  int *left = (int *)depv[1].ptr;
  int *center = (int *)depv[2].ptr;
  int *right = (int *)depv[3].ptr;

  if (!out || !center)
    fail_example("worker missing center/output payload");
  if (depv[0].mode != DB_MODE_EW || depv[2].mode != DB_MODE_RO)
    fail_example("worker got unexpected center/output modes");
  if (left && depv[1].mode != DB_MODE_PTR)
    fail_example("left halo must be DB_MODE_PTR");
  if (right && depv[3].mode != DB_MODE_PTR)
    fail_example("right halo must be DB_MODE_PTR");

  for (uint32_t i = 0; i < BLOCK_WIDTH; ++i) {
    int left_val = left ? left[i] : center[i];
    int right_val = right ? right[i] : center[i];
    out[i] = center[i] + left_val + right_val + (int)block;
  }
}

static void inner_cont_edt(uint32_t paramc, const uint64_t *paramv,
                           uint32_t depc, arts_edt_dep_t depv[]) {
  (void)paramc;
  (void)paramv;

  if (depc != NUM_BLOCKS + 2)
    fail_example("inner continuation dep count mismatch");

  seidel_state_t *next_state = (seidel_state_t *)depv[NUM_BLOCKS].ptr;
  if (!next_state)
    fail_example("inner continuation missing next_state payload");
  if (depv[NUM_BLOCKS].mode != DB_MODE_EW)
    fail_example("next_state must arrive EW");

  for (uint32_t block = 0; block < NUM_BLOCKS; ++block) {
    if (depv[block].mode != DB_MODE_RO)
      fail_example("inner continuation must read produced blocks as RO");
    next_state->block_guids[block] = depv[block].guid;
  }
}

static void advance_edt(uint32_t paramc, const uint64_t *paramv, uint32_t depc,
                        arts_edt_dep_t depv[]) {
  (void)paramc;
  (void)depc;
  (void)depv[0];

  uint32_t iter = (uint32_t)paramv[0];
  seidel_state_t *state = (seidel_state_t *)depv[1].ptr;
  if (!state)
    fail_example("advance missing state payload");

  if (iter + 1 < state->max_iters) {
    uint64_t next_param = (uint64_t)(iter + 1);
    arts_guid_t next_iter = arts_edt_create(iter_edt, 1, &next_param, 1, NULL);
    arts_add_dependence(depv[1].guid, next_iter, 0, DB_MODE_RO);
    return;
  }

  arts_add_dependence(depv[1].guid, state->finish_guid, 0, DB_MODE_RO);
  for (uint32_t block = 0; block < NUM_BLOCKS; ++block)
    arts_add_dependence(state->block_guids[block], state->finish_guid,
                        block + 1, DB_MODE_RO);
}

static void iter_edt(uint32_t paramc, const uint64_t *paramv, uint32_t depc,
                     arts_edt_dep_t depv[]) {
  (void)paramc;
  (void)depc;

  uint32_t iter = (uint32_t)paramv[0];
  const seidel_state_t *state = (const seidel_state_t *)depv[0].ptr;
  if (!state)
    fail_example("iter EDT missing state payload");

  void *next_state_ptr = NULL;
  arts_guid_t next_state_guid = arts_db_create(
      &next_state_ptr, sizeof(seidel_state_t), ARTS_DB_DEFAULT, NULL);
  seidel_state_t *next_state = (seidel_state_t *)next_state_ptr;
  memset(next_state, 0, sizeof(*next_state));
  next_state->iter = iter + 1;
  next_state->max_iters = state->max_iters;
  next_state->finish_guid = state->finish_guid;

  arts_guid_t next_blocks[NUM_BLOCKS];
  for (uint32_t block = 0; block < NUM_BLOCKS; ++block) {
    void *ptr = NULL;
    next_blocks[block] =
        arts_db_create(&ptr, BLOCK_WIDTH * sizeof(int), ARTS_DB_DEFAULT, NULL);
    memset(ptr, 0, BLOCK_WIDTH * sizeof(int));
  }

  uint64_t advance_param = iter;
  arts_guid_t advance_guid =
      arts_edt_create(advance_edt, 1, &advance_param, 2, NULL);
  arts_guid_t outer_epoch = arts_initialize_and_start_epoch(advance_guid, 0);

  arts_guid_t inner_cont = arts_edt_create_with_epoch(
      inner_cont_edt, 0, NULL, NUM_BLOCKS + 2, outer_epoch, NULL);
  arts_guid_t inner_epoch =
      arts_initialize_and_start_epoch(inner_cont, NUM_BLOCKS + 1);

  for (uint32_t block = 0; block < NUM_BLOCKS; ++block) {
    uint64_t worker_param = block;
    arts_guid_t worker = arts_edt_create_with_epoch(
        seidel_block_worker, 1, &worker_param, 4, inner_epoch, NULL);

    arts_add_dependence(next_blocks[block], worker, 0, DB_MODE_EW);
    if (block > 0) {
      arts_add_dependence_at(state->block_guids[block - 1], worker, 1,
                             DB_MODE_RO, 0, BLOCK_WIDTH * sizeof(int));
    } else {
      arts_add_dependence(NULL_GUID, worker, 1, DB_MODE_RO);
    }
    arts_add_dependence(state->block_guids[block], worker, 2, DB_MODE_RO);
    if (block + 1 < NUM_BLOCKS) {
      arts_add_dependence_at(state->block_guids[block + 1], worker, 3,
                             DB_MODE_RO, 0, BLOCK_WIDTH * sizeof(int));
    } else {
      arts_add_dependence(NULL_GUID, worker, 3, DB_MODE_RO);
    }
  }

  for (uint32_t block = 0; block < NUM_BLOCKS; ++block)
    arts_add_dependence(next_blocks[block], inner_cont, block, DB_MODE_RO);

  arts_add_dependence(next_state_guid, inner_cont, NUM_BLOCKS, DB_MODE_EW);
  arts_add_dependence(next_state_guid, advance_guid, 1, DB_MODE_RO);
}

static void finish_edt(uint32_t paramc, const uint64_t *paramv, uint32_t depc,
                       arts_edt_dep_t depv[]) {
  (void)paramc;
  (void)paramv;

  if (depc != NUM_BLOCKS + 1)
    fail_example("finish dep count mismatch");

  seidel_state_t *state = (seidel_state_t *)depv[0].ptr;
  if (!state)
    fail_example("finish missing final state");

  arts_printf("seidel_wavefront_async completed %u iterations\n", state->iter);
  for (uint32_t block = 0; block < NUM_BLOCKS; ++block) {
    int *data = (int *)depv[block + 1].ptr;
    if (!data)
      fail_example("finish missing final block payload");
    arts_printf("  block %u: [%d, %d, %d, %d]\n", block, data[0], data[1],
                data[2], data[3]);
  }
  arts_shutdown();
}

void main_edt(uint32_t paramc, const uint64_t *paramv, uint32_t depc,
              arts_edt_dep_t depv[]) {
  (void)paramc;
  (void)paramv;
  (void)depc;
  (void)depv;

  arts_guid_t finish_guid =
      arts_edt_create(finish_edt, 0, NULL, NUM_BLOCKS + 1, NULL);

  void *state_ptr = NULL;
  arts_guid_t state_guid =
      arts_db_create(&state_ptr, sizeof(seidel_state_t), ARTS_DB_DEFAULT, NULL);
  seidel_state_t *state = (seidel_state_t *)state_ptr;
  memset(state, 0, sizeof(*state));
  state->iter = 0;
  state->max_iters = MAX_ITERS;
  state->finish_guid = finish_guid;

  for (uint32_t block = 0; block < NUM_BLOCKS; ++block) {
    void *ptr = NULL;
    arts_guid_t block_guid =
        arts_db_create(&ptr, BLOCK_WIDTH * sizeof(int), ARTS_DB_DEFAULT, NULL);
    int *data = (int *)ptr;
    for (uint32_t i = 0; i < BLOCK_WIDTH; ++i)
      data[i] = (int)(10 * (block + 1) + i);
    state->block_guids[block] = block_guid;
  }

  uint64_t start_param = 0;
  arts_guid_t start = arts_edt_create(iter_edt, 1, &start_param, 1, NULL);
  arts_add_dependence(state_guid, start, 0, DB_MODE_RO);
}

int main(int argc, char **argv) {
  arts_rt(argc, argv);
  return 0;
}
