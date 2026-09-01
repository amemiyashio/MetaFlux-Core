#define _POSIX_C_SOURCE 200809L

#include "benchmark_common.h"

#include <cuda.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MF_BENCHMARK_ADD_ELEMENT_COUNT UINT32_C(256)

static const char mf_benchmark_add_ptx[] = ".version 9.0\n"
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
  (void)fprintf(stderr, "milestone-0.1.0.0-cuda-benchmark: %s failed at line %d: %s (%s)\n", expression, line,
                name, description);
  return 1;
}

#define MF_CUDA_CALL(expression)                                                                   \
  do {                                                                                             \
    cuda_result = (expression);                                                                    \
    if (cuda_result != CUDA_SUCCESS) {                                                             \
      exit_code = mf_report_cuda_failure(cuda_result, #expression, __LINE__);                      \
      goto cleanup;                                                                                \
    }                                                                                              \
  } while (0)

int main(int argc, char** argv) {
  uint32_t left[MF_BENCHMARK_ADD_ELEMENT_COUNT];
  uint32_t right[MF_BENCHMARK_ADD_ELEMENT_COUNT];
  uint32_t output[MF_BENCHMARK_ADD_ELEMENT_COUNT];
  uint8_t* copy_source_host = (uint8_t*)0;
  uint8_t* copy_destination_host = (uint8_t*)0;
  uint64_t* launch_submit_samples = (uint64_t*)0;
  uint64_t* launch_complete_samples = (uint64_t*)0;
  uint64_t* copy_submit_samples = (uint64_t*)0;
  uint64_t* copy_complete_samples = (uint64_t*)0;
  uint64_t* h2d_submit_samples = (uint64_t*)0;
  uint64_t* h2d_complete_samples = (uint64_t*)0;
  uint64_t* d2h_submit_samples = (uint64_t*)0;
  uint64_t* d2h_complete_samples = (uint64_t*)0;
  CUdeviceptr left_device = (CUdeviceptr)0;
  CUdeviceptr right_device = (CUdeviceptr)0;
  CUdeviceptr output_device = (CUdeviceptr)0;
  CUdeviceptr copy_source_device = (CUdeviceptr)0;
  CUdeviceptr copy_destination_device = (CUdeviceptr)0;
  CUcontext context = (CUcontext)0;
  CUstream stream = (CUstream)0;
  CUmodule module = (CUmodule)0;
  CUfunction function = (CUfunction)0;
  CUdevice device = 0;
  CUresult cuda_result = CUDA_SUCCESS;
  uint64_t copy_bytes_value = 0;
  size_t copy_bytes = 0;
  uint64_t start_ns = 0;
  uint64_t submit_ns = 0;
  uint64_t complete_ns = 0;
  uint32_t warmup_count = 0;
  uint32_t sample_count = 0;
  uint32_t index = 0;
  unsigned int element_count = MF_BENCHMARK_ADD_ELEMENT_COUNT;
  int device_count = 0;
  int exit_code = 1;
  char device_name[256];
  void* arguments[] = {&output_device, &left_device, &right_device, &element_count};

  if (argc != 4 || mf_benchmark_parse_u32(argv[1], &warmup_count) != 0 ||
      mf_benchmark_parse_u32(argv[2], &sample_count) != 0 ||
      mf_benchmark_parse_u64(argv[3], &copy_bytes_value) != 0 ||
      copy_bytes_value > (uint64_t)SIZE_MAX) {
    (void)fprintf(stderr, "usage: %s WARMUP_COUNT SAMPLE_COUNT COPY_BYTES\n", argv[0]);
    return 2;
  }
  copy_bytes = (size_t)copy_bytes_value;
  copy_source_host = (uint8_t*)malloc(copy_bytes);
  copy_destination_host = (uint8_t*)malloc(copy_bytes);
  launch_submit_samples = (uint64_t*)calloc((size_t)sample_count, sizeof(uint64_t));
  launch_complete_samples = (uint64_t*)calloc((size_t)sample_count, sizeof(uint64_t));
  copy_submit_samples = (uint64_t*)calloc((size_t)sample_count, sizeof(uint64_t));
  copy_complete_samples = (uint64_t*)calloc((size_t)sample_count, sizeof(uint64_t));
  h2d_submit_samples = (uint64_t*)calloc((size_t)sample_count, sizeof(uint64_t));
  h2d_complete_samples = (uint64_t*)calloc((size_t)sample_count, sizeof(uint64_t));
  d2h_submit_samples = (uint64_t*)calloc((size_t)sample_count, sizeof(uint64_t));
  d2h_complete_samples = (uint64_t*)calloc((size_t)sample_count, sizeof(uint64_t));
  if (copy_source_host == (uint8_t*)0 || copy_destination_host == (uint8_t*)0 ||
      launch_submit_samples == (uint64_t*)0 || launch_complete_samples == (uint64_t*)0 ||
      copy_submit_samples == (uint64_t*)0 || copy_complete_samples == (uint64_t*)0 ||
      h2d_submit_samples == (uint64_t*)0 || h2d_complete_samples == (uint64_t*)0 ||
      d2h_submit_samples == (uint64_t*)0 || d2h_complete_samples == (uint64_t*)0) {
    goto cleanup;
  }

  (void)memset(output, 0, sizeof(output));
  (void)memset(device_name, 0, sizeof(device_name));
  for (index = 0; index < MF_BENCHMARK_ADD_ELEMENT_COUNT; ++index) {
    left[index] = index * UINT32_C(3) + UINT32_C(1);
    right[index] = UINT32_C(1000) - index;
  }
  for (copy_bytes_value = UINT64_C(0); copy_bytes_value < (uint64_t)copy_bytes;
       ++copy_bytes_value) {
    copy_source_host[(size_t)copy_bytes_value] = (uint8_t)(copy_bytes_value % UINT64_C(251));
  }
  (void)memset(copy_destination_host, 0, copy_bytes);

  MF_CUDA_CALL(cuInit(0));
  MF_CUDA_CALL(cuDeviceGetCount(&device_count));
  if (device_count < 1) {
    (void)fprintf(stderr, "milestone-0.1.0.0-cuda-benchmark: no managed CUDA device was published\n");
    goto cleanup;
  }
  MF_CUDA_CALL(cuDeviceGet(&device, 0));
  MF_CUDA_CALL(cuDeviceGetName(device_name, (int)sizeof(device_name), device));
#if CUDA_VERSION >= 13000
  MF_CUDA_CALL(cuCtxCreate(&context, (CUctxCreateParams*)0, 0, device));
#else
  MF_CUDA_CALL(cuCtxCreate(&context, 0, device));
#endif
  MF_CUDA_CALL(cuStreamCreate(&stream, CU_STREAM_NON_BLOCKING));
  MF_CUDA_CALL(cuModuleLoadData(&module, mf_benchmark_add_ptx));
  MF_CUDA_CALL(cuModuleGetFunction(&function, module, "add_u32"));
  MF_CUDA_CALL(cuMemAlloc(&left_device, sizeof(left)));
  MF_CUDA_CALL(cuMemAlloc(&right_device, sizeof(right)));
  MF_CUDA_CALL(cuMemAlloc(&output_device, sizeof(output)));
  MF_CUDA_CALL(cuMemAlloc(&copy_source_device, copy_bytes));
  MF_CUDA_CALL(cuMemAlloc(&copy_destination_device, copy_bytes));
  MF_CUDA_CALL(cuMemcpyHtoDAsync(left_device, left, sizeof(left), stream));
  MF_CUDA_CALL(cuMemcpyHtoDAsync(right_device, right, sizeof(right), stream));
  MF_CUDA_CALL(cuMemcpyHtoDAsync(copy_source_device, copy_source_host, copy_bytes, stream));
  MF_CUDA_CALL(cuStreamSynchronize(stream));

  for (index = 0; index < warmup_count; ++index) {
    MF_CUDA_CALL(cuMemcpyHtoDAsync(copy_source_device, copy_source_host, copy_bytes, stream));
    MF_CUDA_CALL(cuStreamSynchronize(stream));
    MF_CUDA_CALL(cuLaunchKernel(function, 1, 1, 1, MF_BENCHMARK_ADD_ELEMENT_COUNT, 1, 1, 0, stream,
                                arguments, (void**)0));
    MF_CUDA_CALL(cuStreamSynchronize(stream));
    MF_CUDA_CALL(
        cuMemcpyDtoDAsync(copy_destination_device, copy_source_device, copy_bytes, stream));
    MF_CUDA_CALL(cuStreamSynchronize(stream));
    MF_CUDA_CALL(cuMemcpyDtoHAsync(copy_destination_host, copy_source_device, copy_bytes, stream));
    MF_CUDA_CALL(cuStreamSynchronize(stream));
  }

  for (index = 0; index < sample_count; ++index) {
    if (mf_benchmark_now_ns(&start_ns) != 0) {
      goto cleanup;
    }
    MF_CUDA_CALL(cuMemcpyHtoDAsync(copy_source_device, copy_source_host, copy_bytes, stream));
    if (mf_benchmark_now_ns(&submit_ns) != 0) {
      goto cleanup;
    }
    MF_CUDA_CALL(cuStreamSynchronize(stream));
    if (mf_benchmark_now_ns(&complete_ns) != 0 || submit_ns < start_ns || complete_ns < start_ns) {
      goto cleanup;
    }
    h2d_submit_samples[index] = submit_ns - start_ns;
    h2d_complete_samples[index] = complete_ns - start_ns;

    if (mf_benchmark_now_ns(&start_ns) != 0) {
      goto cleanup;
    }
    MF_CUDA_CALL(cuMemcpyDtoHAsync(copy_destination_host, copy_source_device, copy_bytes, stream));
    if (mf_benchmark_now_ns(&submit_ns) != 0) {
      goto cleanup;
    }
    MF_CUDA_CALL(cuStreamSynchronize(stream));
    if (mf_benchmark_now_ns(&complete_ns) != 0 || submit_ns < start_ns || complete_ns < start_ns) {
      goto cleanup;
    }
    d2h_submit_samples[index] = submit_ns - start_ns;
    d2h_complete_samples[index] = complete_ns - start_ns;

    if (mf_benchmark_now_ns(&start_ns) != 0) {
      goto cleanup;
    }
    MF_CUDA_CALL(cuLaunchKernel(function, 1, 1, 1, MF_BENCHMARK_ADD_ELEMENT_COUNT, 1, 1, 0, stream,
                                arguments, (void**)0));
    if (mf_benchmark_now_ns(&submit_ns) != 0) {
      goto cleanup;
    }
    MF_CUDA_CALL(cuStreamSynchronize(stream));
    if (mf_benchmark_now_ns(&complete_ns) != 0 || submit_ns < start_ns || complete_ns < start_ns) {
      goto cleanup;
    }
    launch_submit_samples[index] = submit_ns - start_ns;
    launch_complete_samples[index] = complete_ns - start_ns;

    if (mf_benchmark_now_ns(&start_ns) != 0) {
      goto cleanup;
    }
    MF_CUDA_CALL(
        cuMemcpyDtoDAsync(copy_destination_device, copy_source_device, copy_bytes, stream));
    if (mf_benchmark_now_ns(&submit_ns) != 0) {
      goto cleanup;
    }
    MF_CUDA_CALL(cuStreamSynchronize(stream));
    if (mf_benchmark_now_ns(&complete_ns) != 0 || submit_ns < start_ns || complete_ns < start_ns) {
      goto cleanup;
    }
    copy_submit_samples[index] = submit_ns - start_ns;
    copy_complete_samples[index] = complete_ns - start_ns;
  }

  MF_CUDA_CALL(cuMemcpyDtoHAsync(output, output_device, sizeof(output), stream));
  MF_CUDA_CALL(
      cuMemcpyDtoHAsync(copy_destination_host, copy_destination_device, copy_bytes, stream));
  MF_CUDA_CALL(cuStreamSynchronize(stream));
  for (index = 0; index < MF_BENCHMARK_ADD_ELEMENT_COUNT; ++index) {
    if (output[index] != left[index] + right[index]) {
      (void)fprintf(stderr, "milestone-0.1.0.0-cuda-benchmark: Add mismatch at element %" PRIu32 "\n", index);
      goto cleanup;
    }
  }
  if (memcmp(copy_source_host, copy_destination_host, copy_bytes) != 0) {
    (void)fprintf(stderr, "milestone-0.1.0.0-cuda-benchmark: managed copy mismatch\n");
    goto cleanup;
  }

  mf_benchmark_emit_metadata_text("workload", "managed_cuda_add_copy");
  mf_benchmark_emit_metadata_text("device_name", device_name);
  mf_benchmark_emit_metadata_u64("warmup_count", warmup_count);
  mf_benchmark_emit_metadata_u64("sample_count", sample_count);
  mf_benchmark_emit_metadata_u64("add_elements", MF_BENCHMARK_ADD_ELEMENT_COUNT);
  mf_benchmark_emit_metadata_u64("copy_bytes", (uint64_t)copy_bytes);
  mf_benchmark_emit_metadata_u64("direct_copy_bytes", (uint64_t)copy_bytes);
  mf_benchmark_emit_metadata_u64("correctness", UINT64_C(1));
  mf_benchmark_emit_metadata_u64("direct_copy_correctness", UINT64_C(1));
  mf_benchmark_emit_metadata_text("launch_submit_boundary", "cuLaunchKernel_api_return");
  mf_benchmark_emit_metadata_text("direct_h2d_submit_boundary", "cuMemcpyHtoDAsync_api_return");
  mf_benchmark_emit_metadata_text("direct_d2h_submit_boundary", "cuMemcpyDtoHAsync_api_return");
  mf_benchmark_emit_metadata_text("d2d_submit_boundary", "cuMemcpyDtoDAsync_api_return");
  mf_benchmark_emit_metadata_text("completion_boundary", "cuStreamSynchronize_return");
  for (index = 0; index < sample_count; ++index) {
    mf_benchmark_emit_sample("cuda_warm_launch_submit_ns", index, launch_submit_samples[index],
                             "ns");
    mf_benchmark_emit_sample("cuda_warm_launch_complete_ns", index, launch_complete_samples[index],
                             "ns");
    mf_benchmark_emit_sample("cuda_direct_h2d_submit_ns", index, h2d_submit_samples[index], "ns");
    mf_benchmark_emit_sample("cuda_direct_h2d_complete_ns", index, h2d_complete_samples[index],
                             "ns");
    mf_benchmark_emit_sample("cuda_direct_d2h_submit_ns", index, d2h_submit_samples[index], "ns");
    mf_benchmark_emit_sample("cuda_direct_d2h_complete_ns", index, d2h_complete_samples[index],
                             "ns");
    mf_benchmark_emit_sample("cuda_warm_copy_submit_ns", index, copy_submit_samples[index], "ns");
    mf_benchmark_emit_sample("cuda_warm_copy_complete_ns", index, copy_complete_samples[index],
                             "ns");
  }
  exit_code = 0;

cleanup:
  if (copy_destination_device != (CUdeviceptr)0 &&
      cuMemFree(copy_destination_device) != CUDA_SUCCESS) {
    exit_code = 1;
  }
  if (copy_source_device != (CUdeviceptr)0 && cuMemFree(copy_source_device) != CUDA_SUCCESS) {
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
  free(copy_complete_samples);
  free(copy_submit_samples);
  free(d2h_complete_samples);
  free(d2h_submit_samples);
  free(h2d_complete_samples);
  free(h2d_submit_samples);
  free(launch_complete_samples);
  free(launch_submit_samples);
  free(copy_destination_host);
  free(copy_source_host);
  return exit_code;
}
