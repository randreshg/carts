#include "arts.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Manual ARTS reference for the ideal Jacobi2D lowering.
 *
 * Design:
 * - Row-strip DB families are rotated every pair step.
 * - One CPS step per t += 2 pair.
 * - One outer epoch per pair, with both A->B and B->A strip waves created
 *   immediately and ordered by DB frontier/dataflow instead of a second epoch.
 * - Exact one-row halo deps via arts_add_dependence_at().
 * - Advance EDT destroys the previous pair's input/intermediate GUID families
 *   after the outer epoch completes, then launches the next pair with a fresh
 *   state DB.
 * - Row-specialized worker kernel: choose halo sources once per row, never
 *   perform owned/left/right buffer selection per element access.
 *
 * This is the runtime-first target pattern CARTS should eventually emit for
 * Jacobi2D, instead of the current generic stencil worker shape.
 */

#ifndef N
#define N 256
#endif

#ifndef TSTEPS
#define TSTEPS 20
#endif

#ifndef VERIFY_REFERENCE
#define VERIFY_REFERENCE 1
#endif

#ifndef MAX_STRIPS
#define MAX_STRIPS 256
#endif

typedef struct {
  uint32_t n;
  uint32_t tsteps;
  uint32_t num_strips;
  uint32_t iter;
  double expected_checksum;
  arts_guid_t finish_guid;
  uint32_t strip_starts[MAX_STRIPS];
  uint32_t strip_rows[MAX_STRIPS];
  arts_guid_t current_guids[MAX_STRIPS];
  arts_guid_t cleanup_input_guids[MAX_STRIPS];
  arts_guid_t cleanup_temp_guids[MAX_STRIPS];
} state_t;

static float init_value(uint32_t i, uint32_t j) {
  return (float)((i + j) % 256) * 0.001f;
}

static void fill_initial_strip(float *dst, uint32_t start_row, uint32_t rows,
                               uint32_t n, int zero_fill) {
  for (uint32_t local_i = 0; local_i < rows; ++local_i) {
    uint32_t global_i = start_row + local_i;
    for (uint32_t j = 0; j < n; ++j) {
      dst[local_i * n + j] = zero_fill ? 0.0f : init_value(global_i, j);
    }
  }
}

static void compute_reference_pair(float *input, float *output, uint32_t n) {
  for (uint32_t i = 0; i < n; ++i) {
    output[i * n + 0] = input[i * n + 0];
    output[i * n + (n - 1)] = input[i * n + (n - 1)];
  }
  for (uint32_t j = 0; j < n; ++j) {
    output[0 * n + j] = input[0 * n + j];
    output[(n - 1) * n + j] = input[(n - 1) * n + j];
  }

  for (uint32_t i = 1; i + 1 < n; ++i) {
    for (uint32_t j = 1; j + 1 < n; ++j) {
      output[i * n + j] =
          0.2f *
          (input[i * n + j] + input[(i - 1) * n + j] + input[(i + 1) * n + j] +
           input[i * n + (j - 1)] + input[i * n + (j + 1)]);
    }
  }
}

static double compute_reference_checksum(uint32_t n, uint32_t tsteps) {
  float *a = (float *)malloc((size_t)n * (size_t)n * sizeof(float));
  float *b = (float *)malloc((size_t)n * (size_t)n * sizeof(float));
  if (!a || !b) {
    free(a);
    free(b);
    return -1.0;
  }

  for (uint32_t i = 0; i < n; ++i) {
    for (uint32_t j = 0; j < n; ++j) {
      a[i * n + j] = init_value(i, j);
      b[i * n + j] = 0.0f;
    }
  }

  for (uint32_t t = 0; t < tsteps; t += 2) {
    compute_reference_pair(a, b, n);
    compute_reference_pair(b, a, n);
  }

  double checksum = 0.0;
  for (uint32_t i = 0; i < n; ++i)
    checksum += a[i * n + i];

  free(a);
  free(b);
  return checksum;
}

static void fail_run(const char *msg) {
  arts_printf("jacobi2d_pair_strip FAIL: %s\n", msg);
  abort();
}

static void pair_iter_edt(uint32_t paramc, const uint64_t *paramv,
                          uint32_t depc, arts_edt_dep_t depv[]);
static void advance_edt(uint32_t paramc, const uint64_t *paramv, uint32_t depc,
                        arts_edt_dep_t depv[]);

static void destroy_guid_family(const arts_guid_t *guids, uint32_t count) {
  for (uint32_t i = 0; i < count; ++i)
    if (guids[i] != NULL_GUID)
      arts_db_destroy(guids[i]);
}

static inline void preserve_row(const float *src, float *dst, uint32_t n) {
  memcpy(dst, src, (size_t)n * sizeof(float));
}

static inline void jacobi_row_core(const float *restrict up,
                                   const float *restrict row,
                                   const float *restrict down,
                                   float *restrict out, uint32_t n) {
  out[0] = row[0];
#pragma GCC ivdep
  for (uint32_t j = 1; j + 1 < n; ++j) {
    out[j] = 0.2f * (row[j] + up[j] + down[j] + row[j - 1] + row[j + 1]);
  }
  out[n - 1] = row[n - 1];
}

static void jacobi_strip_kernel(const float *center, const float *top_halo,
                                const float *bottom_halo, float *out,
                                uint32_t start_row, uint32_t rows, uint32_t n) {
  if (rows == 0)
    return;

  if (rows == 1) {
    uint32_t global_i = start_row;
    if (global_i == 0 || global_i + 1 == n) {
      preserve_row(center, out, n);
      return;
    }
    if (!top_halo || !bottom_halo)
      fail_run("single-row interior strip is missing halo rows");
    jacobi_row_core(top_halo, center, bottom_halo, out, n);
    return;
  }

  uint32_t first_row = 0;
  uint32_t last_row_exclusive = rows;

  if (start_row == 0) {
    preserve_row(center, out, n);
    first_row = 1;
  }
  if (start_row + rows == n) {
    preserve_row(center + (size_t)(rows - 1) * n, out + (size_t)(rows - 1) * n,
                 n);
    last_row_exclusive = rows - 1;
  }
  if (first_row >= last_row_exclusive)
    return;

  if (first_row == 0) {
    if (!top_halo)
      fail_run("top strip row is missing top halo");
    jacobi_row_core(top_halo, center, center + n, out, n);
    first_row = 1;
  }
  if (first_row >= last_row_exclusive)
    return;

  uint32_t last_row_with_bottom_halo = last_row_exclusive;
  if (last_row_exclusive == rows && start_row + rows < n)
    last_row_with_bottom_halo = rows - 1;

  for (uint32_t local_i = first_row; local_i < last_row_with_bottom_halo;
       ++local_i) {
    const float *row = center + (size_t)local_i * n;
    float *dst = out + (size_t)local_i * n;
    jacobi_row_core(row - n, row, row + n, dst, n);
  }

  if (last_row_with_bottom_halo < last_row_exclusive) {
    const float *row = center + (size_t)(rows - 1) * n;
    float *dst = out + (size_t)(rows - 1) * n;
    if (!bottom_halo)
      fail_run("bottom strip row is missing bottom halo");
    jacobi_row_core(row - n, row, bottom_halo, dst, n);
  }
}

static void jacobi_strip_edt(uint32_t paramc, const uint64_t *paramv,
                             uint32_t depc, arts_edt_dep_t depv[]) {
  (void)paramc;
  (void)depc;

  uint32_t start_row = (uint32_t)paramv[0];
  uint32_t rows = (uint32_t)paramv[1];
  uint32_t n = (uint32_t)paramv[2];

  float *center = (float *)depv[0].ptr;
  float *top_halo = (float *)depv[1].ptr;
  float *bottom_halo = (float *)depv[2].ptr;
  float *out = (float *)depv[3].ptr;

  if (!center || !out)
    fail_run("strip EDT missing center or output DB");
  if (depv[0].mode != DB_MODE_RO || depv[3].mode != DB_MODE_EW)
    fail_run("strip EDT got unexpected center/output access modes");
  if (depv[1].ptr && depv[1].mode != DB_MODE_PTR)
    fail_run("top halo must arrive as DB_MODE_PTR");
  if (depv[2].ptr && depv[2].mode != DB_MODE_PTR)
    fail_run("bottom halo must arrive as DB_MODE_PTR");

  jacobi_strip_kernel(center, top_halo, bottom_halo, out, start_row, rows, n);
}

static void launch_strip_wave(arts_guid_t epoch_guid, arts_guid_t *input_guids,
                              arts_guid_t *output_guids, const state_t *state) {
  const uint64_t halo_bytes = (uint64_t)state->n * sizeof(float);

  for (uint32_t strip = 0; strip < state->num_strips; ++strip) {
    uint64_t params[3];
    params[0] = state->strip_starts[strip];
    params[1] = state->strip_rows[strip];
    params[2] = state->n;

    arts_guid_t edt = arts_edt_create_with_epoch(
        jacobi_strip_edt, 3, params, 4, epoch_guid,
        &(arts_hint_t){.route = ARTS_HINT_CURRENT_NODE});

    arts_add_dependence(input_guids[strip], edt, 0, DB_MODE_RO);

    if (strip > 0) {
      uint64_t prev_rows = state->strip_rows[strip - 1];
      uint64_t top_offset =
          (prev_rows - 1) * (uint64_t)state->n * sizeof(float);
      arts_add_dependence_at(input_guids[strip - 1], edt, 1, DB_MODE_RO,
                             top_offset, halo_bytes);
    } else {
      arts_add_dependence(NULL_GUID, edt, 1, DB_MODE_RO);
    }

    if (strip + 1 < state->num_strips) {
      arts_add_dependence_at(input_guids[strip + 1], edt, 2, DB_MODE_RO, 0,
                             halo_bytes);
    } else {
      arts_add_dependence(NULL_GUID, edt, 2, DB_MODE_RO);
    }

    arts_add_dependence(output_guids[strip], edt, 3, DB_MODE_EW);
  }
}

static void finish_edt(uint32_t paramc, const uint64_t *paramv, uint32_t depc,
                       arts_edt_dep_t depv[]) {
  (void)paramc;
  (void)paramv;
  (void)depc;

  const state_t *state = (const state_t *)depv[0].ptr;
  if (!state)
    fail_run("finish EDT missing final state DB");

  double checksum = 0.0;
  for (uint32_t strip = 0; strip < state->num_strips; ++strip) {
    const float *data = (const float *)depv[strip + 1].ptr;
    uint32_t start_row = state->strip_starts[strip];
    uint32_t rows = state->strip_rows[strip];
    if (!data)
      fail_run("finish EDT missing final strip DB");
    for (uint32_t local_i = 0; local_i < rows; ++local_i) {
      uint32_t global_i = start_row + local_i;
      checksum += data[(size_t)local_i * state->n + global_i];
    }
  }

  arts_printf("jacobi2d_pair_strip: checksum=%0.12e\n", checksum);
  if (VERIFY_REFERENCE) {
    double diff = fabs(checksum - state->expected_checksum);
    arts_printf("jacobi2d_pair_strip: expected=%0.12e diff=%0.12e\n",
                state->expected_checksum, diff);
    if (diff > 1.0e-3)
      fail_run("reference checksum mismatch");
  }

  arts_shutdown();
}

static void pair_iter_edt(uint32_t paramc, const uint64_t *paramv,
                          uint32_t depc, arts_edt_dep_t depv[]) {
  (void)paramc;
  (void)paramv;
  (void)depc;

  const state_t *state = (const state_t *)depv[0].ptr;
  if (!state)
    fail_run("pair_iter EDT missing state DB");

  void *next_state_ptr = NULL;
  arts_guid_t next_state_guid =
      arts_db_create(&next_state_ptr, sizeof(state_t), ARTS_DB_DEFAULT, NULL);
  state_t *next_state = (state_t *)next_state_ptr;
  memset(next_state, 0, sizeof(*next_state));
  *next_state = *state;
  next_state->iter = state->iter + 2;

  for (uint32_t strip = 0; strip < state->num_strips; ++strip) {
    uint32_t rows = state->strip_rows[strip];
    size_t bytes = (size_t)rows * (size_t)state->n * sizeof(float);

    void *temp_ptr = NULL;
    arts_guid_t temp_guid =
        arts_db_create(&temp_ptr, (uint64_t)bytes, ARTS_DB_DEFAULT, NULL);
    memset(temp_ptr, 0, bytes);

    void *next_ptr = NULL;
    arts_guid_t next_guid =
        arts_db_create(&next_ptr, (uint64_t)bytes, ARTS_DB_DEFAULT, NULL);
    memset(next_ptr, 0, bytes);

    next_state->current_guids[strip] = next_guid;
    next_state->cleanup_input_guids[strip] = state->current_guids[strip];
    next_state->cleanup_temp_guids[strip] = temp_guid;
  }

  arts_guid_t advance_guid = arts_edt_create(advance_edt, 0, NULL, 2, NULL);
  arts_add_dependence(next_state_guid, advance_guid, 1, DB_MODE_RO);

  arts_guid_t outer_epoch = arts_initialize_and_start_epoch(advance_guid, 0);

  launch_strip_wave(outer_epoch, (arts_guid_t *)state->current_guids,
                    (arts_guid_t *)next_state->cleanup_temp_guids, state);
  launch_strip_wave(outer_epoch, (arts_guid_t *)next_state->cleanup_temp_guids,
                    (arts_guid_t *)next_state->current_guids, state);
}

static void advance_edt(uint32_t paramc, const uint64_t *paramv, uint32_t depc,
                        arts_edt_dep_t depv[]) {
  (void)paramc;
  (void)paramv;
  (void)depc;
  (void)depv[0];

  const state_t *state = (const state_t *)depv[1].ptr;
  if (!state)
    fail_run("advance EDT missing next-state DB");

  destroy_guid_family(state->cleanup_input_guids, state->num_strips);
  destroy_guid_family(state->cleanup_temp_guids, state->num_strips);

  if (state->iter < state->tsteps) {
    arts_guid_t next_iter = arts_edt_create(pair_iter_edt, 0, NULL, 1, NULL);
    arts_add_dependence(depv[1].guid, next_iter, 0, DB_MODE_RO);
    return;
  }

  arts_add_dependence(depv[1].guid, state->finish_guid, 0, DB_MODE_RO);
  for (uint32_t strip = 0; strip < state->num_strips; ++strip) {
    arts_add_dependence(state->current_guids[strip], state->finish_guid,
                        strip + 1, DB_MODE_RO);
  }
}

void main_edt(uint32_t paramc, const uint64_t *paramv, uint32_t depc,
              arts_edt_dep_t depv[]) {
  (void)paramc;
  (void)paramv;
  (void)depc;
  (void)depv;

  if ((TSTEPS % 2) != 0)
    fail_run("TSTEPS must be even for paired strip pipeline");

  unsigned int workers = arts_get_total_workers();
  uint32_t num_strips = (uint32_t)workers;
  if (num_strips == 0)
    num_strips = 1;
  if (num_strips > MAX_STRIPS)
    num_strips = MAX_STRIPS;
  if (num_strips > N)
    num_strips = N;

  void *state_ptr = NULL;
  arts_guid_t state_guid =
      arts_db_create(&state_ptr, sizeof(state_t), ARTS_DB_DEFAULT, NULL);
  state_t *state = (state_t *)state_ptr;
  memset(state, 0, sizeof(*state));
  state->n = N;
  state->tsteps = TSTEPS;
  state->num_strips = num_strips;
  state->iter = 0;

  uint32_t base_rows = N / num_strips;
  uint32_t extra_rows = N % num_strips;
  uint32_t row_cursor = 0;

  for (uint32_t strip = 0; strip < num_strips; ++strip) {
    uint32_t rows = base_rows + (strip < extra_rows ? 1u : 0u);
    state->strip_starts[strip] = row_cursor;
    state->strip_rows[strip] = rows;

    size_t bytes = (size_t)rows * (size_t)N * sizeof(float);

    void *a_ptr = NULL;
    arts_guid_t a_guid =
        arts_db_create(&a_ptr, (uint64_t)bytes, ARTS_DB_DEFAULT, NULL);
    fill_initial_strip((float *)a_ptr, row_cursor, rows, N, 0);
    state->current_guids[strip] = a_guid;
    row_cursor += rows;
  }

  state->expected_checksum =
      VERIFY_REFERENCE ? compute_reference_checksum(N, TSTEPS) : 0.0;
  state->finish_guid =
      arts_edt_create(finish_edt, 0, NULL, num_strips + 1, NULL);

  arts_printf("jacobi2d_pair_strip: N=%u TSTEPS=%u strips=%u workers=%u\n", N,
              TSTEPS, num_strips, workers);

  arts_guid_t start_iter = arts_edt_create(pair_iter_edt, 0, NULL, 1, NULL);
  arts_add_dependence(state_guid, start_iter, 0, DB_MODE_RO);
}

int main(int argc, char **argv) {
  arts_rt(argc, argv);
  return 0;
}
