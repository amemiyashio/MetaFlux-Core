#include <cuda.h>

#include <stdint.h>
#include <stdio.h>

#define MF_HOLDER_ALLOCATION_BYTES (UINT64_C(4) * UINT64_C(1024) * UINT64_C(1024))

static int report_failure(CUresult result, const char* expression) {
  const char* name = "unknown";
  const char* description = "unknown";
  (void)cuGetErrorName(result, &name);
  (void)cuGetErrorString(result, &description);
  (void)fprintf(stderr, "cuda-process-holder: %s failed: %s (%s)\n", expression, name, description);
  return 1;
}

#define CUDA_CALL(expression)                                                                      \
  do {                                                                                             \
    result = (expression);                                                                         \
    if (result != CUDA_SUCCESS) {                                                                  \
      exit_code = report_failure(result, #expression);                                             \
      goto cleanup;                                                                                \
    }                                                                                              \
  } while (0)

int main(void) {
  CUcontext context = (CUcontext)0;
  CUdeviceptr allocation = (CUdeviceptr)0;
  CUdevice device = 0;
  CUresult result = CUDA_SUCCESS;
  int exit_code = 0;

  CUDA_CALL(cuInit(0));
  CUDA_CALL(cuDeviceGet(&device, 0));
#if CUDA_VERSION >= 13000
  CUDA_CALL(cuCtxCreate(&context, (CUctxCreateParams*)0, 0, device));
#else
  CUDA_CALL(cuCtxCreate(&context, 0, device));
#endif
  CUDA_CALL(cuMemAlloc(&allocation, (size_t)MF_HOLDER_ALLOCATION_BYTES));
  (void)printf("READY %llu\n", (unsigned long long)MF_HOLDER_ALLOCATION_BYTES);
  if (fflush(stdout) != 0 || fgetc(stdin) == EOF) {
    exit_code = 1;
  }

cleanup:
  if (allocation != (CUdeviceptr)0 && cuMemFree(allocation) != CUDA_SUCCESS) {
    exit_code = 1;
  }
  if (context != (CUcontext)0 && cuCtxDestroy(context) != CUDA_SUCCESS) {
    exit_code = 1;
  }
  return exit_code;
}
