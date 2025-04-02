#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#define TOLERANCE 1e-5

// Parallel task-based convolution
void convolution_omp_task(float *input, float *output, float *kernel, int width,
                          int height, int ksize, int tile_size) {
  int pad = ksize / 2;
#pragma omp parallel
#pragma omp single
  {
    for (int i = pad; i < height - pad; i += tile_size) {
      for (int j = pad; j < width - pad; j += tile_size) {
        int start_i = i - pad;
        int end_i =
            (i + tile_size < height - pad) ? i + tile_size : height - pad;
        int start_j = j - pad;
        int end_j = (j + tile_size < width - pad) ? j + tile_size : width - pad;

#pragma omp task depend(in : input[start_i * width + start_j])                 \
    depend(out : output[i * width + j])
        {
          for (int ti = i; ti < end_i; ti++) {
            for (int tj = j; tj < end_j; tj++) {
              float sum = 0.0f;
              for (int ki = 0; ki < ksize; ki++) {
                for (int kj = 0; kj < ksize; kj++) {
                  int x = ti - pad + ki;
                  int y = tj - pad + kj;
                  sum += input[x * width + y] * kernel[ki * ksize + kj];
                }
              }
              output[ti * width + tj] = sum;
            }
          }
        }
      }
    }
  }
}

// Sequential convolution for verification
void convolution_sequential(float *input, float *output, float *kernel,
                            int width, int height, int ksize) {
  int pad = ksize / 2;
  for (int i = pad; i < height - pad; i++) {
    for (int j = pad; j < width - pad; j++) {
      float sum = 0.0f;
      for (int ki = 0; ki < ksize; ki++) {
        for (int kj = 0; kj < ksize; kj++) {
          int x = i - pad + ki;
          int y = j - pad + kj;
          sum += input[x * width + y] * kernel[ki * ksize + kj];
        }
      }
      output[i * width + j] = sum;
    }
  }
}

// Verify results between parallel and sequential outputs
int verify_results(float *parallel, float *sequential, int width, int height,
                   int pad) {
  for (int i = pad; i < height - pad; i++) {
    for (int j = pad; j < width - pad; j++) {
      if (fabs(parallel[i * width + j] - sequential[i * width + j]) >
          TOLERANCE) {
        printf("Mismatch at (%d, %d): Parallel=%f, Sequential=%f\n", i, j,
               parallel[i * width + j], sequential[i * width + j]);
        return 0;
      }
    }
  }
  return 1;
}

int main() {
  int width = 8, height = 8;
  int ksize = 3; // 3x3 kernel
  int tile_size = 2;
  int pad = ksize / 2;

  // Allocate and initialize input/output
  float *input = (float *)malloc(width * height * sizeof(float));
  float *output_parallel = (float *)calloc(width * height, sizeof(float));
  float *output_sequential = (float *)calloc(width * height, sizeof(float));
  float kernel[9] = {1, 1, 1, 1, 1, 1, 1, 1, 1}; // Box blur kernel

  // Initialize input with a simple pattern (e.g., diagonal gradient)
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      input[i * width + j] = i + j;
    }
  }

  // Run parallel and sequential convolutions
  convolution_omp_task(input, output_parallel, kernel, width, height, ksize,
                       tile_size);
  convolution_sequential(input, output_sequential, kernel, width, height,
                         ksize);

  // Verify results
  if (verify_results(output_parallel, output_sequential, width, height, pad)) {
    printf("Convolution results match!\n");
  } else {
    printf("Convolution results DO NOT match!\n");
  }

  free(input);
  free(output_parallel);
  free(output_sequential);
  return 0;
}