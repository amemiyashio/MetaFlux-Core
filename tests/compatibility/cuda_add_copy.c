#include <cuda.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MF_ACCEPTANCE_ELEMENT_COUNT UINT32_C(256)

static const char mf_acceptance_ptx[] = ".version 9.0\n"
                                        ".target sm_70\n"
                                        ".address_size 64\n"
                                        ".visible .entry add_u32(\n"
                                        "  .param .u64 destination,\n"
                                        "  .param .u64 left,\n"
                                        "  .param .u64 right,\n"
                                        "  .param .u32 count\n"
                                        ")\n"
                                        "{\n"
                                        "  .reg .pred %p;\n"
                                        "  .reg .b32 %r<10>;\n"
                                        "  .reg .b64 %rd<10>;\n"
                                        "  ld.param.u64 %rd0, [destination];\n"
                                        "  ld.param.u64 %rd1, [left];\n"
                                        "  ld.param.u64 %rd2, [right];\n"
                                        "  ld.param.u32 %r0, [count];\n"
                                        "  mov.u32 %r1, %tid.x;\n"
                                        "  mov.u32 %r2, %ctaid.x;\n"
                                        "  mov.u32 %r3, %ntid.x;\n"
                                        "  mad.lo.u32 %r4, %r2, %r3, %r1;\n"
                                        "  setp.ge.u32 %p, %r4, %r0;\n"
                                        "  @%p bra done;\n"
                                        "  mul.wide.u32 %rd3, %r4, 4;\n"
                                        "  add.u64 %rd4, %rd0, %rd3;\n"
                                        "  add.u64 %rd5, %rd1, %rd3;\n"
                                        "  add.u64 %rd6, %rd2, %rd3;\n"
                                        "  ld.global.u32 %r5, [%rd5];\n"
                                        "  ld.global.u32 %r6, [%rd6];\n"
                                        "  add.u32 %r7, %r5, %r6;\n"
                                        "  st.global.u32 [%rd4], %r7;\n"
                                        "done:\n"
                                        "  ret;\n"
                                        "}\n";

static int mf_report_cuda_failure(CUresult result, const char* expression, int line) {
  const char* name = "unknown";
  const char* description = "unknown";
  (void)cuGetErrorName(result, &name);
  (void)cuGetErrorString(result, &description);
  (void)fprintf(stderr, "cuda-add-copy: %s failed at line %d: %s (%s)\n", expression, line, name,
                description);
  return 1;
}

#define MF_CUDA_CALL(expression)                                                                   \
  do {                                                                                             \
    result = (expression);                                                                         \
    if (result != CUDA_SUCCESS) {                                                                  \
      exit_code = mf_report_cuda_failure(result, #expression, __LINE__);                           \
      goto cleanup;                                                                                \
    }                                                                                              \
  } while (0)

int main(void) {
  uint32_t left[MF_ACCEPTANCE_ELEMENT_COUNT];
  uint32_t right[MF_ACCEPTANCE_ELEMENT_COUNT];
  uint32_t output[MF_ACCEPTANCE_ELEMENT_COUNT];
  CUdeviceptr left_device = (CUdeviceptr)0;
  CUdeviceptr right_device = (CUdeviceptr)0;
  CUdeviceptr output_device = (CUdeviceptr)0;
  CUcontext context = (CUcontext)0;
  CUstream stream = (CUstream)0;
  CUevent event = (CUevent)0;
  CUmodule module = (CUmodule)0;
  CUfunction function = (CUfunction)0;
  CUdevice device = 0;
  CUresult result = CUDA_SUCCESS;
  unsigned int index = 0;
  unsigned int count = MF_ACCEPTANCE_ELEMENT_COUNT;
  int device_count = 0;
  int exit_code = 0;
  char device_name[256];
  void* arguments[] = {&output_device, &left_device, &right_device, &count};

  (void)memset(output, 0, sizeof(output));
  (void)memset(device_name, 0, sizeof(device_name));
  for (index = 0; index < MF_ACCEPTANCE_ELEMENT_COUNT; ++index) {
    left[index] = index * UINT32_C(3) + UINT32_C(1);
    right[index] = UINT32_C(1000) - index;
  }

  MF_CUDA_CALL(cuInit(0));
  MF_CUDA_CALL(cuDeviceGetCount(&device_count));
  if (device_count < 1) {
    (void)fprintf(stderr, "cuda-add-copy: no CUDA Driver device was published\n");
    return 1;
  }
  MF_CUDA_CALL(cuDeviceGet(&device, 0));
  MF_CUDA_CALL(cuDeviceGetName(device_name, (int)sizeof(device_name), device));
#if CUDA_VERSION >= 13000
  MF_CUDA_CALL(cuCtxCreate(&context, (CUctxCreateParams*)0, 0, device));
#else
  MF_CUDA_CALL(cuCtxCreate(&context, 0, device));
#endif
  MF_CUDA_CALL(cuStreamCreate(&stream, CU_STREAM_DEFAULT));
  MF_CUDA_CALL(cuModuleLoadData(&module, mf_acceptance_ptx));
  MF_CUDA_CALL(cuModuleGetFunction(&function, module, "add_u32"));
  MF_CUDA_CALL(cuMemAlloc(&left_device, sizeof(left)));
  MF_CUDA_CALL(cuMemAlloc(&right_device, sizeof(right)));
  MF_CUDA_CALL(cuMemAlloc(&output_device, sizeof(output)));
  MF_CUDA_CALL(cuMemcpyHtoDAsync(left_device, left, sizeof(left), stream));
  MF_CUDA_CALL(cuMemcpyHtoDAsync(right_device, right, sizeof(right), stream));
  MF_CUDA_CALL(cuLaunchKernel(function, 1, 1, 1, MF_ACCEPTANCE_ELEMENT_COUNT, 1, 1, 0, stream,
                              arguments, (void**)0));
  MF_CUDA_CALL(cuEventCreate(&event, CU_EVENT_DEFAULT));
  MF_CUDA_CALL(cuEventRecord(event, stream));
  MF_CUDA_CALL(cuEventSynchronize(event));
  MF_CUDA_CALL(cuMemcpyDtoHAsync(output, output_device, sizeof(output), stream));
  MF_CUDA_CALL(cuStreamSynchronize(stream));

  for (index = 0; index < MF_ACCEPTANCE_ELEMENT_COUNT; ++index) {
    const uint32_t expected = left[index] + right[index];
    if (output[index] != expected) {
      (void)fprintf(stderr, "cuda-add-copy: mismatch at element %u: observed=%u expected=%u\n",
                    index, output[index], expected);
      exit_code = 1;
      goto cleanup;
    }
  }

  (void)memset(output, 0, sizeof(output));
  MF_CUDA_CALL(cuMemcpyHtoDAsync(output_device, output, sizeof(output), stream));
  MF_CUDA_CALL(cuLaunchKernel(function, 1, 1, 1, 1, 1, 1, 0, stream, arguments, (void**)0));
  MF_CUDA_CALL(cuMemcpyDtoHAsync(output, output_device, sizeof(output), stream));
  MF_CUDA_CALL(cuStreamSynchronize(stream));
  for (index = 0; index < MF_ACCEPTANCE_ELEMENT_COUNT; ++index) {
    const uint32_t expected = index == 0 ? left[index] + right[index] : UINT32_C(0);
    if (output[index] != expected) {
      (void)fprintf(stderr,
                    "cuda-add-copy: launch-dimension mismatch at element %u: observed=%u "
                    "expected=%u\n",
                    index, output[index], expected);
      exit_code = 1;
      goto cleanup;
    }
  }
  (void)printf("cuda-add-copy: PASS device=%s elements=%u launch-threads=1\n", device_name,
               MF_ACCEPTANCE_ELEMENT_COUNT);

cleanup:
  if (event != (CUevent)0 && cuEventDestroy(event) != CUDA_SUCCESS) {
    exit_code = 1;
  }
  if (output_device != (CUdeviceptr)0 && cuMemFree(output_device) != CUDA_SUCCESS) {
    exit_code = 1;
  }
  if (right_device != (CUdeviceptr)0 && cuMemFree(right_device) != CUDA_SUCCESS) {
    exit_code = 1;
  }
  if (left_device != (CUdeviceptr)0 && cuMemFree(left_device) != CUDA_SUCCESS) {
    exit_code = 1;
  }
  if (module != (CUmodule)0 && cuModuleUnload(module) != CUDA_SUCCESS) {
    exit_code = 1;
  }
  if (stream != (CUstream)0 && cuStreamDestroy(stream) != CUDA_SUCCESS) {
    exit_code = 1;
  }
  if (context != (CUcontext)0 && cuCtxDestroy(context) != CUDA_SUCCESS) {
    exit_code = 1;
  }
  return exit_code;
}
