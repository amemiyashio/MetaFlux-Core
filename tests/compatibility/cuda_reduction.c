#include <cuda.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MF_REDUCTION_ELEMENT_COUNT UINT32_C(256)
#define MF_REDUCTION_BLOCK_THREADS UINT32_C(128)

_Static_assert(MF_REDUCTION_ELEMENT_COUNT / 2U == MF_REDUCTION_BLOCK_THREADS,
               "every block thread must reduce exactly two input elements");

static const char mf_reduction_ptx[] = ".version 9.0\n"
                                       ".target sm_70\n"
                                       ".address_size 64\n"
                                       ".visible .entry block_reduce_u32(\n"
                                       "  .param .u64 output,\n"
                                       "  .param .u64 input,\n"
                                       "  .param .u64 auxiliary,\n"
                                       "  .param .u32 one\n"
                                       ")\n"
                                       "{\n"
                                       "  .shared .align 4 .u32 scratch[256];\n"
                                       "  .reg .pred %p<8>;\n"
                                       "  .reg .b32 %r<64>;\n"
                                       "  .reg .b64 %rd<8>;\n"
                                       "  ld.param.u64 %rd0, [output];\n"
                                       "  ld.param.u64 %rd1, [input];\n"
                                       "  ld.param.u32 %r0, [one];\n"
                                       "  add.u32 %r1, %r0, %r0;\n"
                                       "  add.u32 %r2, %r1, %r1;\n"
                                       "  add.u32 %r3, %r2, %r2;\n"
                                       "  add.u32 %r4, %r3, %r3;\n"
                                       "  add.u32 %r5, %r4, %r4;\n"
                                       "  add.u32 %r6, %r5, %r5;\n"
                                       "  sub.u32 %r7, %r0, %r0;\n"
                                       "  mov.u32 %r8, %tid.x;\n"
                                       "  add.u32 %r9, %r6, %r6;\n"
                                       "  add.u32 %r10, %r9, %r9;\n"
                                       "  mul.lo.u32 %r11, %r8, %r1;\n"
                                       "  add.u32 %r12, %r11, %r0;\n"
                                       "  mul.wide.u32 %rd2, %r11, 4;\n"
                                       "  add.u64 %rd3, %rd1, %rd2;\n"
                                       "  mul.wide.u32 %rd4, %r12, 4;\n"
                                       "  add.u64 %rd5, %rd1, %rd4;\n"
                                       "  ld.global.u32 %r13, [%rd3];\n"
                                       "  ld.global.u32 %r14, [%rd5];\n"
                                       "  add.u32 %r15, %r13, %r14;\n"
                                       "  mov.u32 %r16, scratch;\n"
                                       "  mul.lo.u32 %r17, %r8, %r2;\n"
                                       "  add.u32 %r18, %r16, %r17;\n"
                                       "  st.shared.u32 [%r18], %r15;\n"
                                       "  bar.sync 0;\n"
                                       "  setp.ge.u32 %p0, %r8, %r6;\n"
                                       "  add.u32 %r19, %r18, %r10;\n"
                                       "  ld.shared.u32 %r20, [%r18];\n"
                                       "  ld.shared.u32 %r21, [%r19];\n"
                                       "  add.u32 %r22, %r20, %r21;\n"
                                       "  @!%p0 st.shared.u32 [%r18], %r22;\n"
                                       "  bar.sync 0;\n"
                                       "  setp.ge.u32 %p1, %r8, %r5;\n"
                                       "  add.u32 %r23, %r18, %r9;\n"
                                       "  ld.shared.u32 %r24, [%r18];\n"
                                       "  ld.shared.u32 %r25, [%r23];\n"
                                       "  add.u32 %r26, %r24, %r25;\n"
                                       "  @!%p1 st.shared.u32 [%r18], %r26;\n"
                                       "  bar.sync 0;\n"
                                       "  setp.ge.u32 %p2, %r8, %r4;\n"
                                       "  add.u32 %r27, %r18, %r6;\n"
                                       "  ld.shared.u32 %r28, [%r18];\n"
                                       "  ld.shared.u32 %r29, [%r27];\n"
                                       "  add.u32 %r30, %r28, %r29;\n"
                                       "  @!%p2 st.shared.u32 [%r18], %r30;\n"
                                       "  bar.sync 0;\n"
                                       "  setp.ge.u32 %p3, %r8, %r3;\n"
                                       "  add.u32 %r31, %r18, %r5;\n"
                                       "  ld.shared.u32 %r32, [%r18];\n"
                                       "  ld.shared.u32 %r33, [%r31];\n"
                                       "  add.u32 %r34, %r32, %r33;\n"
                                       "  @!%p3 st.shared.u32 [%r18], %r34;\n"
                                       "  bar.sync 0;\n"
                                       "  setp.ge.u32 %p4, %r8, %r2;\n"
                                       "  add.u32 %r35, %r18, %r4;\n"
                                       "  ld.shared.u32 %r36, [%r18];\n"
                                       "  ld.shared.u32 %r37, [%r35];\n"
                                       "  add.u32 %r38, %r36, %r37;\n"
                                       "  @!%p4 st.shared.u32 [%r18], %r38;\n"
                                       "  bar.sync 0;\n"
                                       "  setp.ge.u32 %p5, %r8, %r1;\n"
                                       "  add.u32 %r39, %r18, %r3;\n"
                                       "  ld.shared.u32 %r40, [%r18];\n"
                                       "  ld.shared.u32 %r41, [%r39];\n"
                                       "  add.u32 %r42, %r40, %r41;\n"
                                       "  @!%p5 st.shared.u32 [%r18], %r42;\n"
                                       "  bar.sync 0;\n"
                                       "  setp.ge.u32 %p6, %r8, %r0;\n"
                                       "  add.u32 %r43, %r18, %r2;\n"
                                       "  ld.shared.u32 %r44, [%r18];\n"
                                       "  ld.shared.u32 %r45, [%r43];\n"
                                       "  add.u32 %r46, %r44, %r45;\n"
                                       "  @!%p6 st.shared.u32 [%r18], %r46;\n"
                                       "  bar.sync 0;\n"
                                       "  setp.eq.u32 %p7, %r8, %r7;\n"
                                       "  ld.shared.u32 %r47, [%r18];\n"
                                       "  @%p7 st.global.u32 [%rd0], %r47;\n"
                                       "  ret;\n"
                                       "}\n";

static int mf_report_cuda_failure(CUresult result, const char* expression, int line) {
  const char* name = "unknown";
  const char* description = "unknown";
  (void)cuGetErrorName(result, &name);
  (void)cuGetErrorString(result, &description);
  (void)fprintf(stderr, "cuda-reduction: %s failed at line %d: %s (%s)\n", expression, line, name,
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
  uint32_t input[MF_REDUCTION_ELEMENT_COUNT];
  uint32_t auxiliary[MF_REDUCTION_ELEMENT_COUNT];
  uint32_t output[MF_REDUCTION_ELEMENT_COUNT];
  uint32_t expected = 0;
  uint32_t one = 1;
  CUdeviceptr input_device = (CUdeviceptr)0;
  CUdeviceptr auxiliary_device = (CUdeviceptr)0;
  CUdeviceptr output_device = (CUdeviceptr)0;
  CUcontext context = (CUcontext)0;
  CUstream stream = (CUstream)0;
  CUevent event = (CUevent)0;
  CUmodule module = (CUmodule)0;
  CUfunction function = (CUfunction)0;
  CUdevice device = 0;
  CUresult result = CUDA_SUCCESS;
  unsigned int index = 0;
  int device_count = 0;
  int exit_code = 0;
  char device_name[256];
  void* arguments[] = {&output_device, &input_device, &auxiliary_device, &one};

  (void)memset(input, 0, sizeof(input));
  (void)memset(auxiliary, 0, sizeof(auxiliary));
  (void)memset(output, 0, sizeof(output));
  (void)memset(device_name, 0, sizeof(device_name));
  for (index = 0; index < MF_REDUCTION_ELEMENT_COUNT; ++index) {
    input[index] = index + UINT32_C(1);
    expected += input[index];
  }

  MF_CUDA_CALL(cuInit(0));
  MF_CUDA_CALL(cuDeviceGetCount(&device_count));
  if (device_count < 1) {
    (void)fprintf(stderr, "cuda-reduction: no CUDA Driver device was published\n");
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
  MF_CUDA_CALL(cuModuleLoadData(&module, mf_reduction_ptx));
  MF_CUDA_CALL(cuModuleGetFunction(&function, module, "block_reduce_u32"));
  MF_CUDA_CALL(cuMemAlloc(&input_device, sizeof(input)));
  MF_CUDA_CALL(cuMemAlloc(&auxiliary_device, sizeof(auxiliary)));
  MF_CUDA_CALL(cuMemAlloc(&output_device, sizeof(output)));
  MF_CUDA_CALL(cuMemcpyHtoDAsync(input_device, input, sizeof(input), stream));
  MF_CUDA_CALL(cuLaunchKernel(function, 1, 1, 1, MF_REDUCTION_BLOCK_THREADS, 1, 1, 0, stream,
                              arguments, (void**)0));
  MF_CUDA_CALL(cuEventCreate(&event, CU_EVENT_DEFAULT));
  MF_CUDA_CALL(cuEventRecord(event, stream));
  MF_CUDA_CALL(cuEventSynchronize(event));
  MF_CUDA_CALL(cuMemcpyDtoHAsync(output, output_device, sizeof(output), stream));
  MF_CUDA_CALL(cuStreamSynchronize(stream));

  if (output[0] != expected) {
    (void)fprintf(stderr, "cuda-reduction: mismatch: observed=%u expected=%u\n", output[0],
                  expected);
    exit_code = 1;
    goto cleanup;
  }

  (void)printf("cuda-reduction: PASS device=%s elements=%u block-threads=%u sum=%g\n", device_name,
               MF_REDUCTION_ELEMENT_COUNT, MF_REDUCTION_BLOCK_THREADS, (double)output[0]);

cleanup:
  if (event != (CUevent)0 && cuEventDestroy(event) != CUDA_SUCCESS) {
    exit_code = 1;
  }
  if (output_device != (CUdeviceptr)0 && cuMemFree(output_device) != CUDA_SUCCESS) {
    exit_code = 1;
  }
  if (auxiliary_device != (CUdeviceptr)0 && cuMemFree(auxiliary_device) != CUDA_SUCCESS) {
    exit_code = 1;
  }
  if (input_device != (CUdeviceptr)0 && cuMemFree(input_device) != CUDA_SUCCESS) {
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
