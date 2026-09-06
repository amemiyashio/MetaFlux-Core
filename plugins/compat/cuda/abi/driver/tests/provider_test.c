#define _GNU_SOURCE

#include "metaflux/cuda/provider.h"

#include <linux/memfd.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MF_TEST_OBJECT_CAPACITY UINT32_C(32)
#define MF_TEST_OBJECT_DEVICE_MEMORY UINT32_C(1)
#define MF_TEST_OBJECT_HOST_MEMORY UINT32_C(2)
#define MF_TEST_OBJECT_ARTIFACT UINT32_C(3)
#define MF_TEST_OBJECT_ARGUMENT_BLOCK UINT32_C(4)
#define MF_TEST_OBJECT_MODULE UINT32_C(5)
#define MF_TEST_CONTEXT_ID UINT64_C(1)
#define MF_TEST_SUBMISSION_QUEUE_ID UINT64_C(1)
#define MF_TEST_COMPLETION_QUEUE_ID UINT64_C(2)
#define MF_TEST_QUEUE_GENERATION UINT64_C(1)
#define MF_TEST_EVENT_ID UINT64_C(1)
#define MF_TEST_EVENT_GENERATION UINT64_C(1)
#define MF_TEST_ADD_KERNEL_ID UINT64_C(1)
#define MF_TEST_DEVICE_COUNT UINT32_C(3)
#define MF_TEST_WARM_THREAD_COUNT UINT32_C(4)
#define MF_TEST_WARM_THREAD_ITERATIONS UINT32_C(4)
#define MF_CUDA_PROVIDER_TEST_COPY_ARGUMENT_SIZE                                                   \
  (sizeof(mf_argument_block_header_v1) +                                                           \
   ((size_t)MF_COPY_REGION_ARGUMENT_ENTRY_COUNT_V1 * sizeof(mf_argument_entry_v1)))

typedef struct mf_test_object {
  uint64_t id;
  uint64_t generation;
  uint64_t linked_id;
  uint64_t linked_generation;
  uint8_t* bytes;
  uint64_t byte_count;
  uint32_t kind;
  uint32_t active;
} mf_test_object;

typedef struct mf_test_add_argument_block {
  mf_argument_block_header_v1 header;
  mf_argument_entry_v1 entries[4];
} mf_test_add_argument_block;

typedef struct mf_cuda_fixture {
  int registry_fd;
  void* registry_mapping;
  uint64_t registry_size;
  mf_registry_view_id_v1 view_id;
  mf_client_ring_v1 submission;
  mf_client_ring_v1 completion;
  mf_test_object objects[MF_TEST_OBJECT_CAPACITY];
  uint64_t next_object_id;
  uint64_t event_timeline;
  pthread_mutex_t object_mutex;
  uint32_t object_mutex_initialized;
  atomic_uint stop;
  atomic_uint pause_completions;
  atomic_uint command_processed;
  atomic_uint injected_status;
  atomic_uint persistent_status;
  atomic_uint mismatch_next_request;
  atomic_uint argument_register_calls;
  atomic_uint argument_release_calls;
  atomic_uint live_contexts;
  atomic_uint context_acquire_calls;
  atomic_uint context_release_calls;
  atomic_uint reenter_control_once;
  atomic_uint reenter_control_result;
  atomic_uint reenter_read_once;
  atomic_uint reenter_read_result;
  pthread_t worker;
  uint32_t worker_started;
} mf_cuda_fixture;

typedef struct mf_cuda_context_thread_args {
  CUdevice device;
  atomic_uint* failure;
} mf_cuda_context_thread_args;

typedef struct mf_cuda_warm_thread_args {
  CUcontext context;
  CUfunction function;
  CUstream stream;
  CUevent event;
  CUdeviceptr output;
  CUdeviceptr left;
  CUdeviceptr right;
  unsigned int element_count;
  atomic_uint* ready;
  atomic_uint* go;
  atomic_uint* failure;
} mf_cuda_warm_thread_args;

typedef struct mf_cuda_ptds_thread_args {
  CUcontext context;
  CUevent event;
  CUresult set_current_result;
  CUresult query_result;
  CUresult synchronize_result;
  CUresult record_result;
} mf_cuda_ptds_thread_args;

typedef enum mf_cuda_release_kind {
  MF_CUDA_RELEASE_MEMORY,
  MF_CUDA_RELEASE_MODULE,
  MF_CUDA_RELEASE_STREAM,
  MF_CUDA_RELEASE_EVENT,
  MF_CUDA_RELEASE_STREAM_SYNCHRONIZE
} mf_cuda_release_kind;

typedef struct mf_cuda_release_thread_args {
  mf_cuda_release_kind kind;
  CUdeviceptr memory;
  CUcontext context;
  CUmodule module;
  CUstream stream;
  CUevent event;
  CUresult result;
  atomic_uint started;
  atomic_uint finished;
} mf_cuda_release_thread_args;

static void* mf_cuda_release_thread(void* context) {
  mf_cuda_release_thread_args* args = (mf_cuda_release_thread_args*)context;
  atomic_store_explicit(&args->started, UINT32_C(1), memory_order_release);
  switch (args->kind) {
  case MF_CUDA_RELEASE_MEMORY:
    args->result = cuCtxSetCurrent(args->context);
    if (args->result == CUDA_SUCCESS) {
      args->result = cuMemFree_v2(args->memory);
    }
    break;
  case MF_CUDA_RELEASE_MODULE:
    args->result = cuModuleUnload(args->module);
    break;
  case MF_CUDA_RELEASE_STREAM:
    args->result = cuCtxSetCurrent(args->context);
    if (args->result == CUDA_SUCCESS) {
      args->result = cuStreamDestroy_v2(args->stream);
    }
    break;
  case MF_CUDA_RELEASE_EVENT:
    args->result = cuEventDestroy_v2(args->event);
    break;
  case MF_CUDA_RELEASE_STREAM_SYNCHRONIZE:
    args->result = cuCtxSetCurrent(args->context);
    if (args->result == CUDA_SUCCESS) {
      args->result = cuStreamSynchronize(args->stream);
    }
    break;
  }
  atomic_store_explicit(&args->finished, UINT32_C(1), memory_order_release);
  return (void*)0;
}

static int mf_cuda_wait_finished(const atomic_uint* finished, uint64_t timeout_ns) {
  struct timespec start;
  struct timespec now;
  uint64_t start_ns = UINT64_C(0);
  uint64_t now_ns = UINT64_C(0);
  if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
    return 0;
  }
  start_ns = (uint64_t)start.tv_sec * UINT64_C(1000000000) + (uint64_t)start.tv_nsec;
  do {
    if (atomic_load_explicit(finished, memory_order_acquire) != UINT32_C(0)) {
      return 1;
    }
    (void)sched_yield();
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
      return 0;
    }
    now_ns = (uint64_t)now.tv_sec * UINT64_C(1000000000) + (uint64_t)now.tv_nsec;
  } while (now_ns - start_ns < timeout_ns);
  return 0;
}

static int mf_cuda_wait_for_provider_waiter(uint64_t timeout_ns) {
  struct timespec start;
  struct timespec now;
  uint64_t start_ns = UINT64_C(0);
  uint64_t now_ns = UINT64_C(0);
  if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
    return 0;
  }
  start_ns = (uint64_t)start.tv_sec * UINT64_C(1000000000) + (uint64_t)start.tv_nsec;
  do {
    if (mf_cuda_provider_test_active_waiter_count_v1() != UINT32_C(0)) {
      return 1;
    }
    (void)sched_yield();
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
      return 0;
    }
    now_ns = (uint64_t)now.tv_sec * UINT64_C(1000000000) + (uint64_t)now.tv_nsec;
  } while (now_ns - start_ns < timeout_ns);
  return 0;
}

static void* mf_cuda_ptds_thread(void* context) {
  mf_cuda_ptds_thread_args* args = (mf_cuda_ptds_thread_args*)context;
  args->set_current_result = cuCtxSetCurrent(args->context);
  if (args->set_current_result == CUDA_SUCCESS) {
    args->query_result = cuStreamQuery_ptsz((CUstream)0);
    args->synchronize_result = cuStreamSynchronize_ptsz((CUstream)0);
    args->record_result = cuEventRecord_ptsz(args->event, (CUstream)0);
  }
  return (void*)0;
}

static void* mf_cuda_context_thread(void* context) {
  mf_cuda_context_thread_args* args = (mf_cuda_context_thread_args*)context;
  uint32_t iteration = 0;
  for (iteration = 0; iteration < UINT32_C(32); ++iteration) {
    CUcontext created = (CUcontext)0;
    CUcontext current = (CUcontext)0;
    if (cuInit(UINT32_C(0)) != CUDA_SUCCESS ||
        cuCtxCreate_v2(&created, UINT32_C(0), args->device) != CUDA_SUCCESS ||
        cuCtxGetCurrent(&current) != CUDA_SUCCESS || current != created ||
        cuCtxDestroy_v2(created) != CUDA_SUCCESS || cuCtxGetCurrent(&current) != CUDA_SUCCESS ||
        current != (CUcontext)0) {
      atomic_store_explicit(args->failure, UINT32_C(1), memory_order_release);
      break;
    }
  }
  return (void*)0;
}

static int mf_cuda_run_context_threads(CUdevice device) {
  mf_cuda_context_thread_args args;
  pthread_t threads[4];
  atomic_uint failure;
  uint32_t created = 0;
  uint32_t index = 0;
  int result = 0;
  atomic_init(&failure, UINT32_C(0));
  args.device = device;
  args.failure = &failure;
  for (index = 0; index < (uint32_t)(sizeof(threads) / sizeof(threads[0])); ++index) {
    if (pthread_create(&threads[index], (const pthread_attr_t*)0, mf_cuda_context_thread, &args) !=
        0) {
      result = -1;
      break;
    }
    created += UINT32_C(1);
  }
  for (index = 0; index < created; ++index) {
    if (pthread_join(threads[index], (void**)0) != 0) {
      result = -1;
    }
  }
  return result == 0 && atomic_load_explicit(&failure, memory_order_acquire) == UINT32_C(0) ? 0
                                                                                            : -1;
}

static void* mf_cuda_warm_thread(void* context) {
  mf_cuda_warm_thread_args* args = (mf_cuda_warm_thread_args*)context;
  CUdeviceptr output = args->output;
  CUdeviceptr left = args->left;
  CUdeviceptr right = args->right;
  unsigned int element_count = args->element_count;
  void* parameters[4] = {&output, &left, &right, &element_count};
  uint32_t iteration = UINT32_C(0);
  if (cuCtxSetCurrent(args->context) != CUDA_SUCCESS) {
    atomic_store_explicit(args->failure, UINT32_C(1), memory_order_release);
  }
  (void)atomic_fetch_add_explicit(args->ready, UINT32_C(1), memory_order_acq_rel);
  while (atomic_load_explicit(args->go, memory_order_acquire) == UINT32_C(0)) {
    (void)sched_yield();
  }
  if (atomic_load_explicit(args->failure, memory_order_acquire) != UINT32_C(0)) {
    return (void*)0;
  }
  for (iteration = UINT32_C(0); iteration < MF_TEST_WARM_THREAD_ITERATIONS; ++iteration) {
    if (cuMemcpyDtoDAsync_v2(output, left, (size_t)element_count * sizeof(uint32_t),
                             args->stream) != CUDA_SUCCESS ||
        cuLaunchKernel(args->function, UINT32_C(1), UINT32_C(2), UINT32_C(1), UINT32_C(2),
                       UINT32_C(2), UINT32_C(1), UINT32_C(0), args->stream, parameters,
                       (void**)0) != CUDA_SUCCESS ||
        cuEventRecord(args->event, args->stream) != CUDA_SUCCESS ||
        cuStreamWaitEvent(args->stream, args->event, UINT32_C(0)) != CUDA_SUCCESS) {
      atomic_store_explicit(args->failure, UINT32_C(1), memory_order_release);
      break;
    }
  }
  return (void*)0;
}

static int mf_cuda_run_warm_path_threads(CUcontext context, CUfunction function, CUdeviceptr output,
                                         CUdeviceptr left, CUdeviceptr right,
                                         unsigned int element_count) {
  mf_cuda_warm_thread_args args[MF_TEST_WARM_THREAD_COUNT];
  mf_cuda_provider_test_path_counters_v1 before = {0};
  mf_cuda_provider_test_path_counters_v1 after = {0};
  pthread_t threads[MF_TEST_WARM_THREAD_COUNT];
  atomic_uint ready;
  atomic_uint go;
  atomic_uint failure;
  uint32_t created = UINT32_C(0);
  uint32_t index = UINT32_C(0);
  int result = 0;
  (void)memset(args, 0, sizeof(args));
  (void)memset(threads, 0, sizeof(threads));
  atomic_init(&ready, UINT32_C(0));
  atomic_init(&go, UINT32_C(0));
  atomic_init(&failure, UINT32_C(0));
  for (index = UINT32_C(0); index < MF_TEST_WARM_THREAD_COUNT; ++index) {
    args[index].context = context;
    args[index].function = function;
    args[index].output = output;
    args[index].left = left;
    args[index].right = right;
    args[index].element_count = element_count;
    args[index].ready = &ready;
    args[index].go = &go;
    args[index].failure = &failure;
    if (cuStreamCreate(&args[index].stream, CU_STREAM_NON_BLOCKING) != CUDA_SUCCESS ||
        cuEventCreate(&args[index].event, CU_EVENT_DISABLE_TIMING) != CUDA_SUCCESS ||
        pthread_create(&threads[index], (const pthread_attr_t*)0, mf_cuda_warm_thread,
                       &args[index]) != 0) {
      result = -1;
      break;
    }
    created += UINT32_C(1);
  }
  while (result == 0 &&
         atomic_load_explicit(&ready, memory_order_acquire) != MF_TEST_WARM_THREAD_COUNT) {
    (void)sched_yield();
  }
  mf_cuda_provider_test_get_path_counters_v1(&before);
  atomic_store_explicit(&go, UINT32_C(1), memory_order_release);
  for (index = UINT32_C(0); index < created; ++index) {
    if (pthread_join(threads[index], (void**)0) != 0) {
      result = -1;
    }
  }
  mf_cuda_provider_test_get_path_counters_v1(&after);
  if (created != MF_TEST_WARM_THREAD_COUNT ||
      atomic_load_explicit(&failure, memory_order_acquire) != UINT32_C(0) ||
      after.dispatch_lock_acquisitions != before.dispatch_lock_acquisitions ||
      after.global_lock_acquisitions != before.global_lock_acquisitions ||
      after.heap_allocation_attempts != before.heap_allocation_attempts ||
      after.queue_gate_acquisitions - before.queue_gate_acquisitions !=
          (uint64_t)MF_TEST_WARM_THREAD_COUNT * (uint64_t)MF_TEST_WARM_THREAD_ITERATIONS *
              UINT64_C(4)) {
    result = -1;
  }
  for (index = UINT32_C(0); index < MF_TEST_WARM_THREAD_COUNT; ++index) {
    if (args[index].stream != (CUstream)0 &&
        cuStreamSynchronize(args[index].stream) != CUDA_SUCCESS) {
      result = -1;
    }
    if (args[index].event != (CUevent)0 && cuEventDestroy_v2(args[index].event) != CUDA_SUCCESS) {
      result = -1;
    }
    if (args[index].stream != (CUstream)0 &&
        cuStreamDestroy_v2(args[index].stream) != CUDA_SUCCESS) {
      result = -1;
    }
  }
  return result;
}

static int mf_cuda_audit_warm_aliases(CUfunction function, CUstream stream, CUevent event,
                                      CUdeviceptr output, CUdeviceptr left, CUdeviceptr right,
                                      const uint32_t host_values[8], unsigned int element_count) {
  uint32_t host_readback[8] = {0};
  CUdeviceptr parameter_output = output;
  CUdeviceptr parameter_left = left;
  CUdeviceptr parameter_right = right;
  unsigned int parameter_count = element_count;
  void* parameters[4] = {&parameter_output, &parameter_left, &parameter_right, &parameter_count};
  mf_cuda_provider_test_path_counters_v1 before = {0};
  mf_cuda_provider_test_path_counters_v1 after = {0};
  mf_cuda_provider_test_get_path_counters_v1(&before);
  if (cuMemcpyHtoD_v2(left, host_values, (size_t)element_count * sizeof(uint32_t)) !=
          CUDA_SUCCESS ||
      cuMemcpyDtoH_v2(host_readback, left, (size_t)element_count * sizeof(uint32_t)) !=
          CUDA_SUCCESS ||
      cuMemcpyDtoD_v2(output, left, (size_t)element_count * sizeof(uint32_t)) != CUDA_SUCCESS ||
      cuMemcpyHtoD_v2_ptds(left, host_values, (size_t)element_count * sizeof(uint32_t)) !=
          CUDA_SUCCESS ||
      cuMemcpyDtoH_v2_ptds(host_readback, left, (size_t)element_count * sizeof(uint32_t)) !=
          CUDA_SUCCESS ||
      cuMemcpyDtoD_v2_ptds(output, left, (size_t)element_count * sizeof(uint32_t)) !=
          CUDA_SUCCESS ||
      cuMemcpyHtoDAsync_v2(left, host_values, (size_t)element_count * sizeof(uint32_t), stream) !=
          CUDA_SUCCESS ||
      cuMemcpyDtoHAsync_v2(host_readback, left, (size_t)element_count * sizeof(uint32_t), stream) !=
          CUDA_SUCCESS ||
      cuMemcpyDtoDAsync_v2(output, left, (size_t)element_count * sizeof(uint32_t), stream) !=
          CUDA_SUCCESS ||
      cuMemcpyHtoDAsync_v2_ptsz(left, host_values, (size_t)element_count * sizeof(uint32_t),
                                stream) != CUDA_SUCCESS ||
      cuMemcpyDtoHAsync_v2_ptsz(host_readback, left, (size_t)element_count * sizeof(uint32_t),
                                stream) != CUDA_SUCCESS ||
      cuMemcpyDtoDAsync_v2_ptsz(output, left, (size_t)element_count * sizeof(uint32_t), stream) !=
          CUDA_SUCCESS ||
      cuLaunchKernel(function, UINT32_C(1), UINT32_C(2), UINT32_C(1), UINT32_C(2), UINT32_C(2),
                     UINT32_C(1), UINT32_C(0), stream, parameters, (void**)0) != CUDA_SUCCESS ||
      cuLaunchKernel_ptsz(function, UINT32_C(1), UINT32_C(2), UINT32_C(1), UINT32_C(2), UINT32_C(2),
                          UINT32_C(1), UINT32_C(0), stream, parameters,
                          (void**)0) != CUDA_SUCCESS ||
      cuEventRecord(event, stream) != CUDA_SUCCESS ||
      cuEventRecord_ptsz(event, stream) != CUDA_SUCCESS ||
      cuStreamWaitEvent(stream, event, UINT32_C(0)) != CUDA_SUCCESS ||
      cuStreamWaitEvent_ptsz(stream, event, UINT32_C(0)) != CUDA_SUCCESS) {
    return -1;
  }
  mf_cuda_provider_test_get_path_counters_v1(&after);
  if (after.dispatch_lock_acquisitions != before.dispatch_lock_acquisitions ||
      after.global_lock_acquisitions != before.global_lock_acquisitions ||
      after.heap_allocation_attempts != before.heap_allocation_attempts ||
      after.queue_gate_acquisitions - before.queue_gate_acquisitions != UINT64_C(18) ||
      cuStreamSynchronize(stream) != CUDA_SUCCESS ||
      memcmp(host_readback, host_values, (size_t)element_count * sizeof(uint32_t)) != 0) {
    return -1;
  }
  return 0;
}

static mf_test_object* mf_test_find(mf_cuda_fixture* fixture, uint64_t id) {
  uint32_t index = 0;
  for (index = 0; index < MF_TEST_OBJECT_CAPACITY; ++index) {
    if (fixture->objects[index].active != UINT32_C(0) && fixture->objects[index].id == id) {
      return &fixture->objects[index];
    }
  }
  return (mf_test_object*)0;
}

static uint32_t mf_test_active_object_count(mf_cuda_fixture* fixture) {
  uint32_t active = 0;
  uint32_t index = 0;
  (void)pthread_mutex_lock(&fixture->object_mutex);
  for (index = 0; index < MF_TEST_OBJECT_CAPACITY; ++index) {
    active += fixture->objects[index].active != UINT32_C(0) ? UINT32_C(1) : UINT32_C(0);
  }
  (void)pthread_mutex_unlock(&fixture->object_mutex);
  return active;
}

static uint32_t mf_test_active_kind_count(mf_cuda_fixture* fixture, uint32_t kind) {
  uint32_t active = UINT32_C(0);
  uint32_t index = UINT32_C(0);
  (void)pthread_mutex_lock(&fixture->object_mutex);
  for (index = UINT32_C(0); index < MF_TEST_OBJECT_CAPACITY; ++index) {
    active += fixture->objects[index].active != UINT32_C(0) && fixture->objects[index].kind == kind
                  ? UINT32_C(1)
                  : UINT32_C(0);
  }
  (void)pthread_mutex_unlock(&fixture->object_mutex);
  return active;
}

static mf_shared_status_v1 mf_test_resolve(mf_cuda_fixture* fixture, uint64_t id,
                                           uint64_t generation, uint32_t kind,
                                           mf_test_object** output) {
  mf_test_object* object = mf_test_find(fixture, id);
  if (object == (mf_test_object*)0 || object->generation != generation || object->kind != kind) {
    return MF_SHARED_STALE_HANDLE;
  }
  *output = object;
  return MF_SHARED_SUCCESS;
}

static int mf_test_memory_kind(uint32_t kind) {
  return kind == MF_TEST_OBJECT_DEVICE_MEMORY || kind == MF_TEST_OBJECT_HOST_MEMORY;
}

static mf_shared_status_v1 mf_test_resolve_memory(mf_cuda_fixture* fixture, uint64_t id,
                                                  uint64_t generation, mf_test_object** output) {
  mf_test_object* object = mf_test_find(fixture, id);
  if (object == (mf_test_object*)0 || object->generation != generation ||
      !mf_test_memory_kind(object->kind)) {
    return MF_SHARED_STALE_HANDLE;
  }
  *output = object;
  return MF_SHARED_SUCCESS;
}

static mf_shared_status_v1 mf_test_add_object(mf_cuda_fixture* fixture, uint32_t kind,
                                              const uint8_t* bytes, uint64_t byte_count,
                                              uint64_t linked_id, uint64_t linked_generation,
                                              uint64_t* out_id, uint64_t* out_generation) {
  uint32_t index = 0;
  mf_test_object* object = (mf_test_object*)0;
  if (byte_count > (uint64_t)SIZE_MAX) {
    return MF_SHARED_RESOURCE_EXHAUSTED;
  }
  for (index = 0; index < MF_TEST_OBJECT_CAPACITY; ++index) {
    if (fixture->objects[index].active == UINT32_C(0)) {
      object = &fixture->objects[index];
      break;
    }
  }
  if (object == (mf_test_object*)0) {
    return MF_SHARED_RESOURCE_EXHAUSTED;
  }
  object->bytes = byte_count == UINT64_C(0) ? (uint8_t*)0 : (uint8_t*)malloc((size_t)byte_count);
  if (byte_count != UINT64_C(0) && object->bytes == (uint8_t*)0) {
    return MF_SHARED_RESOURCE_EXHAUSTED;
  }
  if (byte_count != UINT64_C(0)) {
    if (bytes == (const uint8_t*)0) {
      (void)memset(object->bytes, 0, (size_t)byte_count);
    } else {
      (void)memcpy(object->bytes, bytes, (size_t)byte_count);
    }
  }
  fixture->next_object_id += UINT64_C(1);
  object->id = fixture->next_object_id;
  object->generation += UINT64_C(1);
  if (object->generation == UINT64_C(0)) {
    object->generation = UINT64_C(1);
  }
  object->linked_id = linked_id;
  object->linked_generation = linked_generation;
  object->byte_count = byte_count;
  object->kind = kind;
  object->active = UINT32_C(1);
  *out_id = object->id;
  *out_generation = object->generation;
  return MF_SHARED_SUCCESS;
}

static mf_shared_status_v1 mf_test_release(mf_cuda_fixture* fixture, uint64_t id,
                                           uint64_t generation, uint32_t kind) {
  mf_test_object* object = (mf_test_object*)0;
  mf_shared_status_v1 status = mf_test_resolve(fixture, id, generation, kind, &object);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  free(object->bytes);
  object->bytes = (uint8_t*)0;
  object->byte_count = UINT64_C(0);
  object->active = UINT32_C(0);
  return MF_SHARED_SUCCESS;
}

static uint32_t mf_test_control_status(mf_shared_status_v1 status) {
  switch (status) {
  case MF_SHARED_SUCCESS:
    return MF_CLIENT_CONTROL_OK;
  case MF_SHARED_STALE_HANDLE:
    return MF_CLIENT_CONTROL_STALE_GENERATION;
  case MF_SHARED_INVALID_ARGUMENT:
    return MF_CLIENT_CONTROL_INVALID_ARGUMENT;
  case MF_SHARED_RESOURCE_EXHAUSTED:
    return MF_CLIENT_CONTROL_RESOURCE_EXHAUSTED;
  case MF_SHARED_NOT_SUPPORTED:
    return MF_CLIENT_CONTROL_UNSUPPORTED;
  default:
    return MF_CLIENT_CONTROL_INTERNAL_ERROR;
  }
}

static mf_shared_status_v1 mf_test_control(void* context,
                                           const mf_client_control_request_v1* request,
                                           const uint8_t* payload, uint64_t payload_size,
                                           mf_client_control_response_v1* response) {
  mf_cuda_fixture* fixture = (mf_cuda_fixture*)context;
  uint16_t opcode = 0;
  uint16_t flags = 0;
  uint64_t object_id = 0;
  uint64_t argument = 0;
  uint64_t response_id = 0;
  uint64_t response_generation = 0;
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;
  if (fixture == (mf_cuda_fixture*)0 || response == (mf_client_control_response_v1*)0 ||
      mf_client_control_request_validate_v1(request) != MF_CLIENT_CONTROL_OK) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  if (mf_client_load_le64_v1(request->bytes + 32) != fixture->view_id.daemon_incarnation ||
      mf_client_load_le64_v1(request->bytes + 40) != fixture->view_id.view_serial) {
    status = MF_SHARED_STALE_HANDLE;
  }
  opcode = mf_client_load_le16_v1(request->bytes + 12);
  flags = mf_client_load_le16_v1(request->bytes + 14);
  object_id = mf_client_load_le64_v1(request->bytes + 48);
  argument = mf_client_load_le64_v1(request->bytes + 56);
  response_id = object_id;
  response_generation = argument;
  if (status == MF_SHARED_SUCCESS) {
    if (atomic_exchange_explicit(&fixture->reenter_control_once, UINT32_C(0),
                                 memory_order_acq_rel) != UINT32_C(0)) {
      int count = -1;
      const CUresult reenter_result = cuDeviceGetCount(&count);
      atomic_store_explicit(&fixture->reenter_control_result,
                            (uint32_t)(reenter_result == CUDA_SUCCESS && count == 2
                                           ? CUDA_SUCCESS
                                           : CUDA_ERROR_UNKNOWN),
                            memory_order_release);
    }
    (void)pthread_mutex_lock(&fixture->object_mutex);
    switch (opcode) {
    case MF_CLIENT_CONTROL_HOST_MEMORY_REGISTER_V1:
      if ((flags & MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD) == UINT16_C(0) ||
          (flags & (MF_CLIENT_CONTROL_FLAG_READ | MF_CLIENT_CONTROL_FLAG_WRITE)) == UINT16_C(0) ||
          object_id != MF_TEST_CONTEXT_ID || argument == UINT64_C(0) ||
          payload == (const uint8_t*)0 || payload_size != argument) {
        status = MF_SHARED_INVALID_ARGUMENT;
      } else {
        status = mf_test_add_object(fixture, MF_TEST_OBJECT_HOST_MEMORY, payload, payload_size,
                                    UINT64_C(0), UINT64_C(0), &response_id, &response_generation);
      }
      break;
    case MF_CLIENT_CONTROL_HOST_MEMORY_RELEASE_V1:
      status = flags == UINT16_C(0) && payload_size == UINT64_C(0)
                   ? mf_test_release(fixture, object_id, argument, MF_TEST_OBJECT_HOST_MEMORY)
                   : MF_SHARED_INVALID_ARGUMENT;
      break;
    case MF_CLIENT_CONTROL_ARTIFACT_REGISTER_V1:
      if (flags != (MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_PTX) ||
          object_id != MF_TEST_CONTEXT_ID || argument == UINT64_C(0) ||
          payload == (const uint8_t*)0 || payload_size != argument) {
        status = MF_SHARED_INVALID_ARGUMENT;
      } else {
        status = mf_test_add_object(fixture, MF_TEST_OBJECT_ARTIFACT, payload, payload_size,
                                    UINT64_C(0), UINT64_C(0), &response_id, &response_generation);
      }
      break;
    case MF_CLIENT_CONTROL_ARTIFACT_RELEASE_V1:
      status = flags == UINT16_C(0) && payload_size == UINT64_C(0)
                   ? mf_test_release(fixture, object_id, argument, MF_TEST_OBJECT_ARTIFACT)
                   : MF_SHARED_INVALID_ARGUMENT;
      break;
    case MF_CLIENT_CONTROL_ARGUMENT_BLOCK_REGISTER_V1:
      (void)atomic_fetch_add_explicit(&fixture->argument_register_calls, UINT32_C(1),
                                      memory_order_relaxed);
      if (flags != MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD || object_id != MF_TEST_CONTEXT_ID ||
          argument == UINT64_C(0) || payload == (const uint8_t*)0 || payload_size != argument ||
          mf_client_argument_block_validate_v1(payload, payload_size) != MF_SHARED_SUCCESS) {
        status = MF_SHARED_INVALID_ARGUMENT;
      } else {
        status = mf_test_add_object(fixture, MF_TEST_OBJECT_ARGUMENT_BLOCK, payload, payload_size,
                                    UINT64_C(0), UINT64_C(0), &response_id, &response_generation);
      }
      break;
    case MF_CLIENT_CONTROL_ARGUMENT_BLOCK_RELEASE_V1:
      (void)atomic_fetch_add_explicit(&fixture->argument_release_calls, UINT32_C(1),
                                      memory_order_relaxed);
      status = flags == UINT16_C(0) && payload_size == UINT64_C(0)
                   ? mf_test_release(fixture, object_id, argument, MF_TEST_OBJECT_ARGUMENT_BLOCK)
                   : MF_SHARED_INVALID_ARGUMENT;
      break;
    case MF_CLIENT_CONTROL_CONTEXT_ACQUIRE_V1:
      if (flags != UINT16_C(0) || object_id != MF_TEST_CONTEXT_ID || argument != UINT64_C(1) ||
          payload_size != UINT64_C(0)) {
        status = MF_SHARED_INVALID_ARGUMENT;
      } else if (atomic_load_explicit(&fixture->live_contexts, memory_order_relaxed) ==
                 UINT32_MAX) {
        status = MF_SHARED_RESOURCE_EXHAUSTED;
      } else {
        (void)atomic_fetch_add_explicit(&fixture->live_contexts, UINT32_C(1), memory_order_relaxed);
        (void)atomic_fetch_add_explicit(&fixture->context_acquire_calls, UINT32_C(1),
                                        memory_order_relaxed);
      }
      break;
    case MF_CLIENT_CONTROL_CONTEXT_RELEASE_V1:
      if (flags != UINT16_C(0) || object_id != MF_TEST_CONTEXT_ID || argument != UINT64_C(1) ||
          payload_size != UINT64_C(0) ||
          atomic_load_explicit(&fixture->live_contexts, memory_order_relaxed) == UINT32_C(0)) {
        status = MF_SHARED_INVALID_ARGUMENT;
      } else {
        (void)atomic_fetch_sub_explicit(&fixture->live_contexts, UINT32_C(1), memory_order_relaxed);
        (void)atomic_fetch_add_explicit(&fixture->context_release_calls, UINT32_C(1),
                                        memory_order_relaxed);
      }
      break;
    default:
      status = MF_SHARED_NOT_SUPPORTED;
      break;
    }
    (void)pthread_mutex_unlock(&fixture->object_mutex);
  }
  mf_client_control_response_init_v1(
      response, mf_test_control_status(status), UINT32_C(0),
      mf_client_load_le64_v1(request->bytes + 24), fixture->view_id.daemon_incarnation,
      fixture->view_id.view_serial, status == MF_SHARED_SUCCESS ? response_id : UINT64_C(0),
      status == MF_SHARED_SUCCESS ? response_generation : UINT64_C(0));
  return MF_SHARED_SUCCESS;
}

static mf_shared_status_v1 mf_test_read_object(const void* context, uint64_t object_id,
                                               uint64_t object_generation, uint64_t offset,
                                               uint8_t* bytes, uint64_t byte_count) {
  mf_cuda_fixture* fixture = (mf_cuda_fixture*)context;
  mf_test_object* object = (mf_test_object*)0;
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;
  if (fixture == (mf_cuda_fixture*)0 || (byte_count != UINT64_C(0) && bytes == (uint8_t*)0)) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  if (atomic_exchange_explicit(&fixture->reenter_read_once, UINT32_C(0), memory_order_acq_rel) !=
      UINT32_C(0)) {
    int count = -1;
    const CUresult reenter_result = cuDeviceGetCount(&count);
    atomic_store_explicit(&fixture->reenter_read_result,
                          (uint32_t)(reenter_result == CUDA_SUCCESS && count == 2
                                         ? CUDA_SUCCESS
                                         : CUDA_ERROR_UNKNOWN),
                          memory_order_release);
  }
  (void)pthread_mutex_lock(&fixture->object_mutex);
  status = mf_test_resolve_memory(fixture, object_id, object_generation, &object);
  if (status == MF_SHARED_SUCCESS &&
      (offset > object->byte_count || byte_count > object->byte_count - offset)) {
    status = MF_SHARED_INVALID_ARGUMENT;
  }
  if (status == MF_SHARED_SUCCESS && byte_count != UINT64_C(0)) {
    (void)memcpy(bytes, object->bytes + (size_t)offset, (size_t)byte_count);
  }
  (void)pthread_mutex_unlock(&fixture->object_mutex);
  return status;
}

static int mf_test_registry_create(mf_cuda_fixture* fixture) {
  uint64_t offset = sizeof(mf_shared_registry_header_v1);
  mf_shared_registry_header_v1* header = (mf_shared_registry_header_v1*)0;
  mf_view_admission_control_v1* view_admission = (mf_view_admission_control_v1*)0;
  mf_registry_view_control_v1* view_control = (mf_registry_view_control_v1*)0;
  mf_virtual_device_identity_v1* identity = (mf_virtual_device_identity_v1*)0;
  mf_device_admission_control_v1* device_admission = (mf_device_admission_control_v1*)0;
  mf_virtual_device_lifecycle_fence_v1* fence = (mf_virtual_device_lifecycle_fence_v1*)0;
  mf_telemetry_control_v1* telemetry_control = (mf_telemetry_control_v1*)0;
  mf_virtual_device_telemetry_v1* bank0 = (mf_virtual_device_telemetry_v1*)0;
  mf_virtual_device_telemetry_v1* bank1 = (mf_virtual_device_telemetry_v1*)0;
  uint32_t index = 0;
  fixture->registry_size =
      sizeof(mf_shared_registry_header_v1) + sizeof(mf_view_admission_control_v1) +
      sizeof(mf_registry_view_control_v1) +
      ((uint64_t)MF_TEST_DEVICE_COUNT * sizeof(mf_virtual_device_identity_v1)) +
      ((uint64_t)MF_TEST_DEVICE_COUNT * sizeof(mf_device_admission_control_v1)) +
      ((uint64_t)MF_TEST_DEVICE_COUNT * sizeof(mf_virtual_device_lifecycle_fence_v1)) +
      sizeof(mf_telemetry_control_v1) +
      (UINT64_C(2) * (uint64_t)MF_TEST_DEVICE_COUNT * sizeof(mf_virtual_device_telemetry_v1));
  fixture->registry_fd = memfd_create("metaflux-cuda-test-registry", MFD_CLOEXEC);
  if (fixture->registry_fd < 0 ||
      ftruncate(fixture->registry_fd, (off_t)fixture->registry_size) != 0) {
    return -1;
  }
  fixture->registry_mapping = mmap((void*)0, (size_t)fixture->registry_size, PROT_READ | PROT_WRITE,
                                   MAP_SHARED, fixture->registry_fd, 0);
  if (fixture->registry_mapping == MAP_FAILED) {
    fixture->registry_mapping = (void*)0;
    return -1;
  }
  (void)memset(fixture->registry_mapping, 0, (size_t)fixture->registry_size);
  header = (mf_shared_registry_header_v1*)fixture->registry_mapping;
  header->magic = MF_SHARED_REGISTRY_MAGIC;
  header->abi_version = MF_SHARED_DEVICE_ABI_VERSION_1;
  header->header_size = (uint32_t)sizeof(*header);
  header->total_size = fixture->registry_size;
  header->registry_view_id = fixture->view_id;
  header->process_view_revision = UINT64_C(1);
  header->device_count = MF_TEST_DEVICE_COUNT;
  header->telemetry_row_count = MF_TEST_DEVICE_COUNT;
  header->view_admission_offset = offset;
  view_admission = (mf_view_admission_control_v1*)((uint8_t*)fixture->registry_mapping + offset);
  offset += sizeof(*view_admission);
  header->view_control_offset = offset;
  view_control = (mf_registry_view_control_v1*)((uint8_t*)fixture->registry_mapping + offset);
  offset += sizeof(*view_control);
  header->identities_offset = offset;
  identity = (mf_virtual_device_identity_v1*)((uint8_t*)fixture->registry_mapping + offset);
  offset += (uint64_t)MF_TEST_DEVICE_COUNT * sizeof(*identity);
  header->device_admission_offset = offset;
  device_admission =
      (mf_device_admission_control_v1*)((uint8_t*)fixture->registry_mapping + offset);
  offset += (uint64_t)MF_TEST_DEVICE_COUNT * sizeof(*device_admission);
  header->lifecycle_fences_offset = offset;
  fence = (mf_virtual_device_lifecycle_fence_v1*)((uint8_t*)fixture->registry_mapping + offset);
  offset += (uint64_t)MF_TEST_DEVICE_COUNT * sizeof(*fence);
  header->telemetry_control_offset = offset;
  telemetry_control = (mf_telemetry_control_v1*)((uint8_t*)fixture->registry_mapping + offset);
  offset += sizeof(*telemetry_control);
  header->telemetry_bank0_offset = offset;
  bank0 = (mf_virtual_device_telemetry_v1*)((uint8_t*)fixture->registry_mapping + offset);
  offset += (uint64_t)MF_TEST_DEVICE_COUNT * sizeof(*bank0);
  header->telemetry_bank1_offset = offset;
  bank1 = (mf_virtual_device_telemetry_v1*)((uint8_t*)fixture->registry_mapping + offset);

  view_admission->registry_view_id = fixture->view_id;
  view_admission->state_generation = mf_view_admission_pack_v1(UINT64_C(1), MF_VIEW_ADMISSION_OPEN);
  view_control->registry_view_id = fixture->view_id;
  view_control->process_view_revision = UINT64_C(1);
  view_control->gate_state = MF_VIEW_GATE_OPEN;
  telemetry_control->snapshot_sequence = UINT64_C(1);
  telemetry_control->active_bank_state =
      mf_telemetry_bank_state_pack_v1(UINT32_C(0), MF_TELEMETRY_STATE_READY);
  telemetry_control->row_count = MF_TEST_DEVICE_COUNT;
  for (index = UINT32_C(0); index < MF_TEST_DEVICE_COUNT; ++index) {
    uint32_t uuid_index = UINT32_C(0);
    identity[index].identity_record_id = (uint64_t)index + UINT64_C(1);
    for (uuid_index = UINT32_C(0); uuid_index < UINT32_C(16); ++uuid_index) {
      identity[index].gpu_uuid[uuid_index] =
          (uint8_t)(index * UINT32_C(16) + uuid_index + UINT32_C(1));
    }
    (void)snprintf((char*)identity[index].display_name, sizeof(identity[index].display_name),
                   "MetaFlux CUDA Test %u", index);
    identity[index].committed_generation = UINT64_C(1);
    identity[index].virtual_compute_capability = UINT32_C(80);
    identity[index].pci_domain = UINT32_C(0);
    identity[index].pci_bus = UINT32_C(3) + index;
    identity[index].pci_device = UINT32_C(4);
    identity[index].pci_function = UINT32_C(0);
    device_admission[index].identity_record_id = (uint64_t)index + UINT64_C(1);
    device_admission[index].state_generation_tag =
        mf_device_admission_pack_v1(UINT32_C(1), MF_DEVICE_ADMISSION_OPEN, UINT32_C(1));
    fence[index].identity_record_id = (uint64_t)index + UINT64_C(1);
    fence[index].lifecycle_sequence = UINT64_C(1);
    fence[index].epoch = UINT64_C(1);
    fence[index].effective_quota_bytes = UINT64_C(1073741824);
    fence[index].device_state = MF_DEVICE_STATE_ONLINE;
    bank0[index].identity_record_id = (uint64_t)index + UINT64_C(1);
    bank0[index].observed_lifecycle_sequence = UINT64_C(1);
    bank0[index].memory_capacity_bytes = UINT64_C(1073741824);
    bank0[index].sample_time_ns = UINT64_C(100000000);
    bank1[index] = bank0[index];
  }
  return offset + ((uint64_t)MF_TEST_DEVICE_COUNT * sizeof(*bank1)) == fixture->registry_size ? 0
                                                                                              : -1;
}

static void mf_test_registry_set_process_view_revision(mf_cuda_fixture* fixture,
                                                        uint64_t revision) {
  mf_shared_registry_header_v1* header = (mf_shared_registry_header_v1*)fixture->registry_mapping;
  mf_registry_view_control_v1* view_control =
      (mf_registry_view_control_v1*)((uint8_t*)fixture->registry_mapping +
                                     header->view_control_offset);
  header->process_view_revision = revision;
  view_control->process_view_revision = revision;
}

static mf_shared_status_v1 mf_test_launch(mf_cuda_fixture* fixture,
                                          const mf_ring_descriptor_v1* command) {
  mf_test_object* module = (mf_test_object*)0;
  mf_test_object* argument_block = (mf_test_object*)0;
  mf_test_object* destination = (mf_test_object*)0;
  mf_test_object* left = (mf_test_object*)0;
  mf_test_object* right = (mf_test_object*)0;
  mf_test_add_argument_block arguments;
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;
  uint64_t byte_count = 0;
  uint64_t logical_x_threads = 0;
  uint64_t logical_y_threads = 0;
  uint64_t logical_threads = 0;
  uint64_t index = 0;
  status = mf_test_resolve(fixture, command->target_id, command->arguments[0],
                           MF_TEST_OBJECT_MODULE, &module);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  if (command->arguments[1] != MF_TEST_ADD_KERNEL_ID) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  status = mf_test_resolve(fixture, command->arguments[2], command->arguments[3],
                           MF_TEST_OBJECT_ARGUMENT_BLOCK, &argument_block);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  if (argument_block->byte_count != sizeof(arguments)) {
    return MF_SHARED_MALFORMED;
  }
  (void)module;
  (void)memcpy(&arguments, argument_block->bytes, sizeof(arguments));
  status = mf_client_argument_block_validate_v1((const uint8_t*)&arguments, sizeof(arguments));
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  if ((arguments.header.flags & MF_ARGUMENT_BLOCK_FLAG_LAUNCH_DIMENSIONS_XY_V1) == UINT32_C(0) ||
      arguments.entries[3].value > UINT64_MAX / sizeof(uint32_t) ||
      arguments.header.reserved[MF_ARGUMENT_BLOCK_LAUNCH_GRID_X_INDEX_V1] == UINT64_C(0) ||
      arguments.header.reserved[MF_ARGUMENT_BLOCK_LAUNCH_GRID_Y_INDEX_V1] == UINT64_C(0) ||
      arguments.header.reserved[MF_ARGUMENT_BLOCK_LAUNCH_BLOCK_X_INDEX_V1] == UINT64_C(0) ||
      arguments.header.reserved[MF_ARGUMENT_BLOCK_LAUNCH_BLOCK_Y_INDEX_V1] == UINT64_C(0) ||
      arguments.header.reserved[MF_ARGUMENT_BLOCK_LAUNCH_GRID_X_INDEX_V1] >
          UINT64_MAX / arguments.header.reserved[MF_ARGUMENT_BLOCK_LAUNCH_BLOCK_X_INDEX_V1] ||
      arguments.header.reserved[MF_ARGUMENT_BLOCK_LAUNCH_GRID_Y_INDEX_V1] >
          UINT64_MAX / arguments.header.reserved[MF_ARGUMENT_BLOCK_LAUNCH_BLOCK_Y_INDEX_V1]) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  byte_count = arguments.entries[3].value * sizeof(uint32_t);
  logical_x_threads = arguments.header.reserved[MF_ARGUMENT_BLOCK_LAUNCH_GRID_X_INDEX_V1] *
                      arguments.header.reserved[MF_ARGUMENT_BLOCK_LAUNCH_BLOCK_X_INDEX_V1];
  logical_y_threads = arguments.header.reserved[MF_ARGUMENT_BLOCK_LAUNCH_GRID_Y_INDEX_V1] *
                      arguments.header.reserved[MF_ARGUMENT_BLOCK_LAUNCH_BLOCK_Y_INDEX_V1];
  if (logical_x_threads > UINT64_MAX / logical_y_threads) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  logical_threads = logical_x_threads * logical_y_threads;
  if (mf_test_resolve(fixture, arguments.entries[0].object_id,
                      arguments.entries[0].object_generation, MF_TEST_OBJECT_DEVICE_MEMORY,
                      &destination) != MF_SHARED_SUCCESS ||
      mf_test_resolve(fixture, arguments.entries[1].object_id,
                      arguments.entries[1].object_generation, MF_TEST_OBJECT_DEVICE_MEMORY,
                      &left) != MF_SHARED_SUCCESS ||
      mf_test_resolve(fixture, arguments.entries[2].object_id,
                      arguments.entries[2].object_generation, MF_TEST_OBJECT_DEVICE_MEMORY,
                      &right) != MF_SHARED_SUCCESS) {
    return MF_SHARED_STALE_HANDLE;
  }
  if (arguments.entries[0].value > destination->byte_count ||
      arguments.entries[1].value > left->byte_count ||
      arguments.entries[2].value > right->byte_count ||
      byte_count > destination->byte_count - arguments.entries[0].value ||
      byte_count > left->byte_count - arguments.entries[1].value ||
      byte_count > right->byte_count - arguments.entries[2].value) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  for (index = 0; index < arguments.entries[3].value && index < logical_threads; ++index) {
    uint32_t left_value = 0;
    uint32_t right_value = 0;
    uint32_t sum = 0;
    const size_t offset = (size_t)(index * sizeof(uint32_t));
    (void)memcpy(&left_value, left->bytes + arguments.entries[1].value + offset,
                 sizeof(left_value));
    (void)memcpy(&right_value, right->bytes + arguments.entries[2].value + offset,
                 sizeof(right_value));
    sum = left_value + right_value;
    (void)memcpy(destination->bytes + arguments.entries[0].value + offset, &sum, sizeof(sum));
  }
  return MF_SHARED_SUCCESS;
}

static mf_shared_status_v1 mf_test_process_command(mf_cuda_fixture* fixture,
                                                   const mf_ring_descriptor_v1* command,
                                                   uint64_t* result_id, uint64_t* result_generation,
                                                   uint64_t* timeline) {
  mf_test_object* object = (mf_test_object*)0;
  mf_test_object* source = (mf_test_object*)0;
  mf_test_object* destination = (mf_test_object*)0;
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;
  *result_id = command->target_id;
  *result_generation = UINT64_C(1);
  *timeline = UINT64_C(0);
  switch (command->opcode) {
  case MF_RING_OPCODE_MEMORY_ALLOC:
    if (command->target_id != MF_TEST_CONTEXT_ID || command->arguments[0] == UINT64_C(0) ||
        command->arguments[1] == UINT64_C(0)) {
      return MF_SHARED_INVALID_ARGUMENT;
    }
    return mf_test_add_object(fixture, MF_TEST_OBJECT_DEVICE_MEMORY, (const uint8_t*)0,
                              command->arguments[0], UINT64_C(0), UINT64_C(0), result_id,
                              result_generation);
  case MF_RING_OPCODE_MEMORY_FREE:
    *result_generation = command->arguments[0];
    return mf_test_release(fixture, command->target_id, command->arguments[0],
                           MF_TEST_OBJECT_DEVICE_MEMORY);
  case MF_RING_OPCODE_MODULE_LOAD:
    status = mf_test_resolve(fixture, command->target_id, command->arguments[0],
                             MF_TEST_OBJECT_ARTIFACT, &object);
    if (status != MF_SHARED_SUCCESS) {
      return status;
    }
    return mf_test_add_object(fixture, MF_TEST_OBJECT_MODULE, (const uint8_t*)0, UINT64_C(0),
                              object->id, object->generation, result_id, result_generation);
  case MF_RING_OPCODE_MODULE_UNLOAD:
    *result_generation = command->arguments[0];
    return mf_test_release(fixture, command->target_id, command->arguments[0],
                           MF_TEST_OBJECT_MODULE);
  case MF_RING_OPCODE_COPY: {
    mf_test_add_argument_block copy_arguments;
    uint64_t destination_offset = UINT64_C(0);
    uint64_t source_offset = UINT64_C(0);
    uint64_t byte_count = UINT64_C(0);
    *result_generation = command->arguments[0];
    if (command->flags == MF_RING_COPY_FLAG_REGION_ARGUMENT_BLOCK_V1) {
      if (command->arguments[1] != UINT64_C(0) || command->arguments[2] != UINT64_C(0) ||
          command->arguments[3] != UINT64_C(0) ||
          mf_test_resolve(fixture, command->target_id, command->arguments[0],
                          MF_TEST_OBJECT_ARGUMENT_BLOCK, &object) != MF_SHARED_SUCCESS ||
          object->byte_count != (uint64_t)MF_CUDA_PROVIDER_TEST_COPY_ARGUMENT_SIZE) {
        return MF_SHARED_MALFORMED;
      }
      (void)memset(&copy_arguments, 0, sizeof(copy_arguments));
      (void)memcpy(&copy_arguments, object->bytes, MF_CUDA_PROVIDER_TEST_COPY_ARGUMENT_SIZE);
      if (mf_client_copy_region_argument_block_validate_v1(
              (const uint8_t*)&copy_arguments, MF_CUDA_PROVIDER_TEST_COPY_ARGUMENT_SIZE) !=
          MF_SHARED_SUCCESS) {
        return MF_SHARED_MALFORMED;
      }
      if (mf_test_resolve_memory(
              fixture, copy_arguments.entries[MF_COPY_REGION_DESTINATION_INDEX_V1].object_id,
              copy_arguments.entries[MF_COPY_REGION_DESTINATION_INDEX_V1].object_generation,
              &destination) != MF_SHARED_SUCCESS ||
          mf_test_resolve_memory(
              fixture, copy_arguments.entries[MF_COPY_REGION_SOURCE_INDEX_V1].object_id,
              copy_arguments.entries[MF_COPY_REGION_SOURCE_INDEX_V1].object_generation,
              &source) != MF_SHARED_SUCCESS) {
        return MF_SHARED_STALE_HANDLE;
      }
      destination_offset = copy_arguments.entries[MF_COPY_REGION_DESTINATION_INDEX_V1].value;
      source_offset = copy_arguments.entries[MF_COPY_REGION_SOURCE_INDEX_V1].value;
      byte_count = copy_arguments.entries[MF_COPY_REGION_BYTE_COUNT_INDEX_V1].value;
    } else if (command->flags == UINT32_C(0)) {
      if (mf_test_resolve_memory(fixture, command->target_id, command->arguments[0],
                                 &destination) != MF_SHARED_SUCCESS ||
          mf_test_resolve_memory(fixture, command->arguments[1], command->arguments[2], &source) !=
              MF_SHARED_SUCCESS) {
        return MF_SHARED_STALE_HANDLE;
      }
      byte_count = command->arguments[3];
    } else {
      return MF_SHARED_MALFORMED;
    }
    if (destination == (mf_test_object*)0 || source == (mf_test_object*)0) {
      return MF_SHARED_STALE_HANDLE;
    }
    if (byte_count == UINT64_C(0) || destination_offset > destination->byte_count ||
        source_offset > source->byte_count ||
        byte_count > destination->byte_count - destination_offset ||
        byte_count > source->byte_count - source_offset) {
      return MF_SHARED_INVALID_ARGUMENT;
    }
    (void)memmove(destination->bytes + (size_t)destination_offset,
                  source->bytes + (size_t)source_offset, (size_t)byte_count);
    return MF_SHARED_SUCCESS;
  }
  case MF_RING_OPCODE_LAUNCH:
    *result_generation = command->arguments[0];
    return mf_test_launch(fixture, command);
  case MF_RING_OPCODE_EVENT_RECORD:
    *result_generation = command->arguments[0];
    if (command->target_id != MF_TEST_EVENT_ID ||
        command->arguments[0] != MF_TEST_EVENT_GENERATION || command->arguments[1] == UINT64_C(0)) {
      return MF_SHARED_STALE_HANDLE;
    }
    fixture->event_timeline = command->arguments[1];
    *timeline = fixture->event_timeline;
    return MF_SHARED_SUCCESS;
  case MF_RING_OPCODE_EVENT_WAIT:
    *result_generation = command->arguments[0];
    if (command->target_id != MF_TEST_EVENT_ID ||
        command->arguments[0] != MF_TEST_EVENT_GENERATION) {
      return MF_SHARED_STALE_HANDLE;
    }
    if (fixture->event_timeline < command->arguments[1]) {
      return MF_SHARED_WOULD_BLOCK;
    }
    *timeline = fixture->event_timeline;
    return MF_SHARED_SUCCESS;
  case MF_RING_OPCODE_QUEUE_SYNCHRONIZE:
    if (command->target_id != MF_TEST_SUBMISSION_QUEUE_ID) {
      return MF_SHARED_STALE_HANDLE;
    }
    *result_generation = MF_TEST_QUEUE_GENERATION;
    return MF_SHARED_SUCCESS;
  default:
    return MF_SHARED_NOT_SUPPORTED;
  }
}

static void* mf_test_worker(void* context) {
  mf_cuda_fixture* fixture = (mf_cuda_fixture*)context;
  while (atomic_load_explicit(&fixture->stop, memory_order_acquire) == UINT32_C(0)) {
    mf_ring_descriptor_v1 command;
    mf_ring_descriptor_v1 completion;
    mf_shared_status_v1 status = mf_client_ring_try_consume_v1(&fixture->submission, &command);
    uint64_t result_id = UINT64_C(0);
    uint64_t result_generation = UINT64_C(0);
    uint64_t timeline = UINT64_C(0);
    if (status == MF_SHARED_WOULD_BLOCK) {
      (void)mf_client_ring_wait_readable_v1(&fixture->submission, UINT64_C(10000000));
      continue;
    }
    if (status != MF_SHARED_SUCCESS) {
      continue;
    }
    (void)pthread_mutex_lock(&fixture->object_mutex);
    status = mf_test_process_command(fixture, &command, &result_id, &result_generation, &timeline);
    (void)pthread_mutex_unlock(&fixture->object_mutex);
    atomic_fetch_add_explicit(&fixture->command_processed, UINT32_C(1), memory_order_release);
    while (atomic_load_explicit(&fixture->pause_completions, memory_order_acquire) != UINT32_C(0) &&
           atomic_load_explicit(&fixture->stop, memory_order_acquire) == UINT32_C(0)) {
      (void)sched_yield();
    }
    {
      const uint32_t injected =
          atomic_exchange_explicit(&fixture->injected_status, UINT32_MAX, memory_order_acq_rel);
      if (injected != UINT32_MAX) {
        status = (mf_shared_status_v1)injected;
      } else {
        const uint32_t persistent =
            atomic_load_explicit(&fixture->persistent_status, memory_order_acquire);
        if (persistent != UINT32_MAX) {
          status = (mf_shared_status_v1)persistent;
        }
      }
    }
    (void)memset(&completion, 0, sizeof(completion));
    completion.opcode = MF_RING_OPCODE_COMPLETION;
    completion.flags = command.flags;
    completion.request_id = command.request_id;
    if (atomic_exchange_explicit(&fixture->mismatch_next_request, UINT32_C(0),
                                 memory_order_acq_rel) != UINT32_C(0)) {
      completion.request_id += UINT64_C(1);
    }
    completion.target_id = result_id;
    completion.arguments[0] = (uint64_t)(uint32_t)status;
    completion.arguments[1] = result_generation;
    completion.arguments[2] = timeline;
    for (;;) {
      const mf_shared_status_v1 submit_status =
          mf_client_ring_try_submit_v1(&fixture->completion, &completion);
      if (submit_status == MF_SHARED_SUCCESS) {
        break;
      }
      if (submit_status != MF_SHARED_WOULD_BLOCK) {
        break;
      }
      (void)mf_client_ring_wait_writable_v1(&fixture->completion, UINT64_C(10000000));
    }
  }
  return (void*)0;
}

static int mf_test_fixture_create(mf_cuda_fixture* fixture) {
  (void)memset(fixture, 0, sizeof(*fixture));
  fixture->registry_fd = -1;
  fixture->submission.owned_fd = -1;
  fixture->completion.owned_fd = -1;
  fixture->view_id.daemon_incarnation = UINT64_C(31);
  fixture->view_id.view_serial = UINT64_C(37);
  fixture->next_object_id = UINT64_C(100);
  if (pthread_mutex_init(&fixture->object_mutex, (const pthread_mutexattr_t*)0) != 0) {
    return -1;
  }
  fixture->object_mutex_initialized = UINT32_C(1);
  atomic_init(&fixture->stop, UINT32_C(0));
  atomic_init(&fixture->pause_completions, UINT32_C(0));
  atomic_init(&fixture->command_processed, UINT32_C(0));
  atomic_init(&fixture->injected_status, UINT32_MAX);
  atomic_init(&fixture->persistent_status, UINT32_MAX);
  atomic_init(&fixture->mismatch_next_request, UINT32_C(0));
  atomic_init(&fixture->argument_register_calls, UINT32_C(0));
  atomic_init(&fixture->argument_release_calls, UINT32_C(0));
  atomic_init(&fixture->live_contexts, UINT32_C(0));
  atomic_init(&fixture->context_acquire_calls, UINT32_C(0));
  atomic_init(&fixture->context_release_calls, UINT32_C(0));
  atomic_init(&fixture->reenter_control_once, UINT32_C(0));
  atomic_init(&fixture->reenter_control_result, (uint32_t)CUDA_ERROR_UNKNOWN);
  atomic_init(&fixture->reenter_read_once, UINT32_C(0));
  atomic_init(&fixture->reenter_read_result, (uint32_t)CUDA_ERROR_UNKNOWN);
  if (mf_test_registry_create(fixture) != 0 ||
      mf_client_ring_create_v1(UINT32_C(256), fixture->view_id, MF_TEST_SUBMISSION_QUEUE_ID,
                               MF_TEST_QUEUE_GENERATION,
                               &fixture->submission) != MF_SHARED_SUCCESS ||
      mf_client_ring_create_v1(UINT32_C(256), fixture->view_id, MF_TEST_COMPLETION_QUEUE_ID,
                               MF_TEST_QUEUE_GENERATION,
                               &fixture->completion) != MF_SHARED_SUCCESS ||
      pthread_create(&fixture->worker, (const pthread_attr_t*)0, mf_test_worker, fixture) != 0) {
    return -1;
  }
  fixture->worker_started = UINT32_C(1);
  return 0;
}

static void mf_test_fixture_destroy(mf_cuda_fixture* fixture) {
  uint32_t index = 0;
  atomic_store_explicit(&fixture->stop, UINT32_C(1), memory_order_release);
  if (fixture->worker_started != UINT32_C(0)) {
    (void)pthread_join(fixture->worker, (void**)0);
  }
  for (index = 0; index < MF_TEST_OBJECT_CAPACITY; ++index) {
    free(fixture->objects[index].bytes);
  }
  mf_client_ring_close_v1(&fixture->completion);
  mf_client_ring_close_v1(&fixture->submission);
  if (fixture->registry_mapping != (void*)0) {
    (void)munmap(fixture->registry_mapping, (size_t)fixture->registry_size);
  }
  if (fixture->registry_fd >= 0) {
    (void)close(fixture->registry_fd);
  }
  if (fixture->object_mutex_initialized != UINT32_C(0)) {
    (void)pthread_mutex_destroy(&fixture->object_mutex);
    fixture->object_mutex_initialized = UINT32_C(0);
  }
}

#define MF_TEST_REQUIRE(condition, error_code)                                                     \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      (void)fprintf(stderr, "cuda-provider-test: requirement %d failed at line %d: %s\n",          \
                    (error_code), __LINE__, #condition);                                           \
      result = (error_code);                                                                       \
      goto cleanup;                                                                                \
    }                                                                                              \
  } while (0)

static int mf_cuda_run_direct_registration_tests(mf_cuda_provider_test_transport_v1* transport) {
  static const struct {
    uint32_t local_unavailable;
    mf_shared_status_v1 transport_status;
    uint32_t control_status;
  } fallback_cases[] = {
      {UINT32_C(1), MF_SHARED_SUCCESS, MF_CLIENT_CONTROL_INTERNAL_ERROR},
      {UINT32_C(0), MF_SHARED_SUCCESS, MF_CLIENT_CONTROL_UNSUPPORTED},
      {UINT32_C(0), MF_SHARED_SUCCESS, MF_CLIENT_CONTROL_INVALID_ARGUMENT},
  };
  static const struct {
    mf_shared_status_v1 transport_status;
    uint32_t control_status;
    CUresult expected;
  } fatal_cases[] = {
      {MF_SHARED_MALFORMED, MF_CLIENT_CONTROL_INTERNAL_ERROR, CUDA_ERROR_INVALID_VALUE},
      {MF_SHARED_STALE_HANDLE, MF_CLIENT_CONTROL_INTERNAL_ERROR, CUDA_ERROR_INVALID_HANDLE},
      {MF_SHARED_SYSTEM_ERROR, MF_CLIENT_CONTROL_INTERNAL_ERROR, CUDA_ERROR_SYSTEM_NOT_READY},
      {MF_SHARED_SUCCESS, MF_CLIENT_CONTROL_STALE_GENERATION, CUDA_ERROR_INVALID_HANDLE},
      {MF_SHARED_SUCCESS, MF_CLIENT_CONTROL_INTERNAL_ERROR, CUDA_ERROR_UNKNOWN},
  };
  const uint32_t input[] = {UINT32_C(11), UINT32_C(23), UINT32_C(47), UINT32_C(89)};
  uint32_t output[sizeof(input) / sizeof(input[0])];
  size_t index = 0;
  int result = 0;

  if (setenv("METAFLUX_MODE", "managed", 1) != 0 ||
      mf_cuda_provider_test_set_direct_registration_v1(UINT32_C(2), MF_SHARED_SUCCESS,
                                                       MF_CLIENT_CONTROL_OK) != -1) {
    return 1;
  }
  for (index = 0; index < sizeof(fallback_cases) / sizeof(fallback_cases[0]); ++index) {
    CUcontext context = (CUcontext)0;
    CUdeviceptr memory = (CUdeviceptr)0;
    CUdevice device = 0;
    mf_cuda_provider_test_reset_v1();
    transport->negotiated_capabilities = MF_CLIENT_CAP_DIRECT_HOST_COPY_V1;
    (void)memset(output, 0, sizeof(output));
    if (mf_cuda_provider_test_install_transport_v1(transport) != 0 ||
        mf_cuda_provider_test_set_direct_registration_v1(
            fallback_cases[index].local_unavailable, fallback_cases[index].transport_status,
            fallback_cases[index].control_status) != 0 ||
        cuInit(UINT32_C(0)) != CUDA_SUCCESS || cuDeviceGet(&device, 0) != CUDA_SUCCESS ||
        cuCtxCreate_v2(&context, UINT32_C(0), device) != CUDA_SUCCESS ||
        cuMemAlloc_v2(&memory, sizeof(input)) != CUDA_SUCCESS ||
        cuMemcpyHtoD_v2(memory, input, sizeof(input)) != CUDA_SUCCESS ||
        cuMemcpyDtoH_v2(output, memory, sizeof(output)) != CUDA_SUCCESS ||
        memcmp(input, output, sizeof(input)) != 0 || cuMemFree_v2(memory) != CUDA_SUCCESS ||
        cuCtxDestroy_v2(context) != CUDA_SUCCESS) {
      result = 10 + (int)index;
      break;
    }
  }
  for (index = 0; result == 0 && index < sizeof(fatal_cases) / sizeof(fatal_cases[0]); ++index) {
    mf_cuda_provider_test_reset_v1();
    transport->negotiated_capabilities = MF_CLIENT_CAP_DIRECT_HOST_COPY_V1;
    if (mf_cuda_provider_test_install_transport_v1(transport) != 0 ||
        mf_cuda_provider_test_set_direct_registration_v1(UINT32_C(0),
                                                         fatal_cases[index].transport_status,
                                                         fatal_cases[index].control_status) != 0 ||
        cuInit(UINT32_C(0)) != fatal_cases[index].expected ||
        mf_cuda_provider_test_managed_is_pristine_v1() != UINT32_C(1)) {
      result = 20 + (int)index;
      break;
    }
  }
  mf_cuda_provider_test_reset_v1();
  transport->negotiated_capabilities = UINT64_C(0);
  (void)unsetenv("METAFLUX_MODE");
  if (result != 0) {
    (void)fprintf(stderr, "direct-registration scenario %d failed\n", result);
  }
  return result;
}

int main(void) {
  static const char ptx[] = ".version 9.0\n.target sm_70\n.visible .entry add_u32() { ret; }\n";
  const uint32_t left_values[] = {1, 2, 3, 4, 5, 6, 7, 8};
  const uint32_t right_values[] = {8, 7, 6, 5, 4, 3, 2, 1};
  const uint32_t zero_values[8] = {0};
  const uint8_t interior_bytes[] = {0xa1U, 0xb2U, 0xc3U, 0xd4U, 0xe5U, 0xf6U, 0x17U};
  uint32_t asynchronous_source[8];
  uint32_t output_values[8] = {0};
  uint32_t copied_values[8] = {0};
  uint8_t interior_readback[sizeof(interior_bytes)] = {0};
  uint8_t interior_full_readback[sizeof(output_values)] = {0};
  unsigned int element_count = UINT32_C(8);
  unsigned int interior_element_count = UINT32_C(7);
  mf_cuda_fixture fixture;
  mf_cuda_release_thread_args release_args;
  mf_cuda_ptds_thread_args ptds_args;
  mf_cuda_provider_test_transport_v1 transport;
  pthread_t release_thread;
  pthread_t ptds_thread;
  CUctxCreateParams create_parameters;
  CUdevice device = -1;
  CUdevice current_device = -1;
  CUdevice reordered_device = -1;
  CUcontext context = (CUcontext)0;
  CUcontext primary_context = (CUcontext)0;
  CUcontext retained_primary_context = (CUcontext)0;
  CUcontext overflow_primary_context = (CUcontext)0;
  CUcontext foreign_context = (CUcontext)0;
  CUcontext transient_context = (CUcontext)0;
  CUcontext version_four_context = (CUcontext)0;
  CUcontext popped_context = (CUcontext)0;
  CUmodule module = (CUmodule)0;
  CUfunction function = (CUfunction)0;
  CUfunction missing_function = (CUfunction)0;
  CUstream stream = (CUstream)0;
  CUstream transient_stream = (CUstream)0;
  CUevent event = (CUevent)0;
  CUevent ptds_event = (CUevent)0;
  CUevent transient_event = (CUevent)0;
  CUdeviceptr device_left = (CUdeviceptr)0;
  CUdeviceptr device_right = (CUdeviceptr)0;
  CUdeviceptr device_output = (CUdeviceptr)0;
  CUdeviceptr device_copy = (CUdeviceptr)0;
  CUdeviceptr primary_memory = (CUdeviceptr)0;
  CUdeviceptr foreign_memory = (CUdeviceptr)0;
  CUdeviceptr interior_output = (CUdeviceptr)0;
  CUdeviceptr interior_left = (CUdeviceptr)0;
  CUdeviceptr interior_right = (CUdeviceptr)0;
  void* parameters[4];
  void* resolved = (void*)0;
  void* legacy_resolved = (void*)0;
  void* versioned_resolved = (void*)0;
  CUresult (*resolved_stream_query)(CUstream) = (CUresult (*)(CUstream))0;
  CUresult (*resolved_stream_synchronize)(CUstream) = (CUresult (*)(CUstream))0;
  CUresult (*resolved_event_record)(CUevent, CUstream) = (CUresult (*)(CUevent, CUstream))0;
  CUdriverProcAddressQueryResult query_status = CU_GET_PROC_ADDRESS_SYMBOL_NOT_FOUND;
  CUuuid uuid;
  char name[64];
  char pci_bus_id[32];
  int count = 0;
  int driver_version = 0;
  int major = 0;
  int minor = 0;
  float elapsed_ms = 0.0F;
  int result = 0;
  uint32_t fixture_created = UINT32_C(0);
  uint32_t pending_capacity = UINT32_C(0);
  uint32_t async_index = UINT32_C(0);
  uint32_t argument_register_calls = UINT32_C(0);
  uint32_t copy_argument_register_calls = UINT32_C(0);
  uint32_t primary_acquire_calls = UINT32_C(0);
  uint32_t primary_release_calls = UINT32_C(0);
  uint32_t synchronization_thread_created = UINT32_C(0);
  uint32_t waiter_observed = UINT32_C(0);
  uint32_t synchronization_was_blocked = UINT32_C(0);
  int concurrent_device_count = -1;
  CUresult concurrent_count_result = CUDA_ERROR_UNKNOWN;
  int synchronization_join_result = -1;

  MF_TEST_REQUIRE(setenv("METAFLUX_SOCKET", "/tmp/metaflux-provider-test-no-daemon.sock", 1) == 0 &&
                      unsetenv("CUDA_VISIBLE_DEVICES") == 0,
                  83);
  mf_cuda_provider_test_reset_v1();
  MF_TEST_REQUIRE(cuInit(UINT32_C(0)) == CUDA_ERROR_SYSTEM_NOT_READY, 1);
  MF_TEST_REQUIRE(mf_test_fixture_create(&fixture) == 0, 2);
  fixture_created = UINT32_C(1);
  MF_TEST_REQUIRE(setenv("CUDA_VISIBLE_DEVICES", "2,0,-1,1", 1) == 0, 84);
  (void)memset(&transport, 0, sizeof(transport));
  transport.registry_fd = fixture.registry_fd;
  transport.submission_fd = mf_client_ring_borrow_fd_v1(&fixture.submission);
  transport.completion_fd = mf_client_ring_borrow_fd_v1(&fixture.completion);
  transport.registry_view_id = fixture.view_id;
  transport.submission_queue_id = MF_TEST_SUBMISSION_QUEUE_ID;
  transport.submission_queue_generation = MF_TEST_QUEUE_GENERATION;
  transport.completion_queue_id = MF_TEST_COMPLETION_QUEUE_ID;
  transport.completion_queue_generation = MF_TEST_QUEUE_GENERATION;
  transport.runtime_context_id = MF_TEST_CONTEXT_ID;
  transport.runtime_event_id = MF_TEST_EVENT_ID;
  transport.runtime_event_generation = MF_TEST_EVENT_GENERATION;
  transport.runtime_add_kernel_id = MF_TEST_ADD_KERNEL_ID;
  transport.negotiated_capabilities = UINT64_C(0);
  transport.control_context = &fixture;
  transport.control = mf_test_control;
  transport.read_object = mf_test_read_object;
  MF_TEST_REQUIRE(mf_cuda_run_direct_registration_tests(&transport) == 0, 158);
  copy_argument_register_calls =
      atomic_load_explicit(&fixture.argument_register_calls, memory_order_relaxed);
  MF_TEST_REQUIRE(mf_cuda_provider_test_install_transport_v1(&transport) == 0 &&
                      cuInit(UINT32_C(0)) == CUDA_SUCCESS &&
                      mf_cuda_provider_test_process_view_revision_v1() == UINT64_C(1) &&
                      cuDeviceGet(&device, 0) == CUDA_SUCCESS &&
                      cuCtxCreate_v2(&context, UINT32_C(0), device) == CUDA_SUCCESS &&
                      cuMemAlloc_v2(&device_left, sizeof(left_values)) == CUDA_SUCCESS,
                  156);
  MF_TEST_REQUIRE(cuMemcpyHtoD_v2(device_left, left_values, sizeof(left_values)) == CUDA_SUCCESS &&
                      cuMemcpyHtoD_v2(device_left + UINT64_C(1), interior_bytes,
                                      sizeof(interior_bytes)) == CUDA_ERROR_NOT_SUPPORTED &&
                      atomic_load_explicit(&fixture.argument_register_calls,
                                           memory_order_relaxed) == copy_argument_register_calls &&
                      cuMemFree_v2(device_left) == CUDA_SUCCESS &&
                      cuCtxDestroy_v2(context) == CUDA_SUCCESS,
                  157);
  mf_cuda_provider_test_reset_v1();
  context = (CUcontext)0;
  device_left = (CUdeviceptr)0;
  transport.negotiated_capabilities = MF_CLIENT_CAP_COPY_REGION_V1;
  MF_TEST_REQUIRE(mf_cuda_provider_test_install_transport_v1(&transport) == 0, 3);
  MF_TEST_REQUIRE(cuInit(UINT32_C(0)) == CUDA_SUCCESS, 4);
  MF_TEST_REQUIRE(cuDriverGetVersion(&driver_version) == CUDA_SUCCESS && driver_version == 12000 &&
                      cuDeviceGetCount(&count) == CUDA_SUCCESS && count == 2 &&
                      cuDeviceGet(&device, 0) == CUDA_SUCCESS &&
                      cuDeviceGet(&reordered_device, 1) == CUDA_SUCCESS && reordered_device == 1 &&
                      cuDeviceGetName(name, (int)sizeof(name), device) == CUDA_SUCCESS &&
                      strcmp(name, "MetaFlux CUDA Test 2") == 0 &&
                      cuDeviceGetPCIBusId(pci_bus_id, (int)sizeof(pci_bus_id), device) ==
                          CUDA_SUCCESS &&
                      strcmp(pci_bus_id, "0000:05:04.0") == 0 &&
                      cuDeviceGetUuid_v2(&uuid, device) == CUDA_SUCCESS &&
                      cuDeviceComputeCapability(&major, &minor, device) == CUDA_SUCCESS &&
                      major == 8 && minor == 0 && setenv("CUDA_VISIBLE_DEVICES", "1", 1) == 0 &&
                      cuDeviceGetCount(&count) == CUDA_SUCCESS && count == 2,
                  5);
  mf_test_registry_set_process_view_revision(&fixture, UINT64_C(2));
  MF_TEST_REQUIRE(cuDeviceGetCount(&count) == CUDA_SUCCESS && count == 2 &&
                      mf_cuda_provider_test_process_view_revision_v1() == UINT64_C(1),
                  160);
  MF_TEST_REQUIRE(cuGetProcAddress_v2("cuLaunchKernel", &resolved, MF_CUDA_DRIVER_API_VERSION,
                                      CU_GET_PROC_ADDRESS_PER_THREAD_DEFAULT_STREAM,
                                      &query_status) == CUDA_SUCCESS &&
                      resolved != (void*)0 && query_status == CU_GET_PROC_ADDRESS_SUCCESS,
                  6);
  resolved = (void*)1;
  query_status = CU_GET_PROC_ADDRESS_SUCCESS;
  MF_TEST_REQUIRE(cuGetProcAddress_v2("cuMissing", &resolved, MF_CUDA_DRIVER_API_VERSION,
                                      CU_GET_PROC_ADDRESS_DEFAULT, &query_status) == CUDA_SUCCESS &&
                      resolved == (void*)0 && query_status == CU_GET_PROC_ADDRESS_SYMBOL_NOT_FOUND,
                  7);
  MF_TEST_REQUIRE(cuGetProcAddress_v2("cuInit", &resolved, 999, CU_GET_PROC_ADDRESS_DEFAULT,
                                      &query_status) == CUDA_SUCCESS &&
                      query_status == CU_GET_PROC_ADDRESS_VERSION_NOT_SUFFICIENT,
                  8);
  MF_TEST_REQUIRE(
      cuGetProcAddress_v2("cuMemAlloc", &legacy_resolved, 3010, CU_GET_PROC_ADDRESS_DEFAULT,
                          &query_status) == CUDA_SUCCESS &&
          legacy_resolved != (void*)0 && query_status == CU_GET_PROC_ADDRESS_SUCCESS &&
          cuGetProcAddress_v2("cuMemAlloc", &versioned_resolved, 3020, CU_GET_PROC_ADDRESS_DEFAULT,
                              &query_status) == CUDA_SUCCESS &&
          versioned_resolved != (void*)0 && versioned_resolved != legacy_resolved &&
          cuGetProcAddress_v2("cuMemAlloc", &resolved, 13000, CU_GET_PROC_ADDRESS_DEFAULT,
                              (CUdriverProcAddressQueryResult*)0) == CUDA_SUCCESS &&
          resolved == versioned_resolved &&
          cuGetProcAddress_v2("cuInit", &resolved, MF_CUDA_DRIVER_API_VERSION,
                              CU_GET_PROC_ADDRESS_LEGACY_STREAM |
                                  CU_GET_PROC_ADDRESS_PER_THREAD_DEFAULT_STREAM,
                              &query_status) == CUDA_ERROR_INVALID_VALUE,
      81);
  MF_TEST_REQUIRE(cuGetProcAddress_v2("cuStreamQuery", &resolved, MF_CUDA_DRIVER_API_VERSION,
                                      CU_GET_PROC_ADDRESS_PER_THREAD_DEFAULT_STREAM,
                                      &query_status) == CUDA_SUCCESS &&
                      query_status == CU_GET_PROC_ADDRESS_SUCCESS,
                  132);
  (void)memcpy(&resolved_stream_query, &resolved, sizeof(resolved_stream_query));
  MF_TEST_REQUIRE(resolved_stream_query == cuStreamQuery_ptsz, 133);
  MF_TEST_REQUIRE(cuGetProcAddress_v2("cuStreamSynchronize", &resolved, MF_CUDA_DRIVER_API_VERSION,
                                      CU_GET_PROC_ADDRESS_PER_THREAD_DEFAULT_STREAM,
                                      &query_status) == CUDA_SUCCESS &&
                      query_status == CU_GET_PROC_ADDRESS_SUCCESS,
                  134);
  (void)memcpy(&resolved_stream_synchronize, &resolved, sizeof(resolved_stream_synchronize));
  MF_TEST_REQUIRE(resolved_stream_synchronize == cuStreamSynchronize_ptsz, 135);
  MF_TEST_REQUIRE(cuGetProcAddress_v2("cuEventRecord", &resolved, MF_CUDA_DRIVER_API_VERSION,
                                      CU_GET_PROC_ADDRESS_PER_THREAD_DEFAULT_STREAM,
                                      &query_status) == CUDA_SUCCESS &&
                      query_status == CU_GET_PROC_ADDRESS_SUCCESS,
                  136);
  (void)memcpy(&resolved_event_record, &resolved, sizeof(resolved_event_record));
  MF_TEST_REQUIRE(resolved_event_record == cuEventRecord_ptsz, 137);
  MF_TEST_REQUIRE(
      mf_cuda_run_context_threads(device) == 0 &&
          atomic_load_explicit(&fixture.live_contexts, memory_order_relaxed) == UINT32_C(0) &&
          atomic_load_explicit(&fixture.context_acquire_calls, memory_order_relaxed) ==
              atomic_load_explicit(&fixture.context_release_calls, memory_order_relaxed),
      82);
  (void)memset(&create_parameters, 0, sizeof(create_parameters));
  MF_TEST_REQUIRE(
      cuCtxCreate_v4(&version_four_context, (CUctxCreateParams*)0, UINT32_C(0), device) ==
              CUDA_SUCCESS &&
          cuCtxDestroy_v2(version_four_context) == CUDA_SUCCESS &&
          cuCtxCreate_v4(&version_four_context, &create_parameters, UINT32_C(0), device) ==
              CUDA_SUCCESS &&
          cuCtxDestroy_v2(version_four_context) == CUDA_SUCCESS &&
          atomic_load_explicit(&fixture.live_contexts, memory_order_relaxed) == UINT32_C(0) &&
          atomic_load_explicit(&fixture.context_acquire_calls, memory_order_relaxed) ==
              atomic_load_explicit(&fixture.context_release_calls, memory_order_relaxed),
      87);
  create_parameters.numExecAffinityParams = 1;
  MF_TEST_REQUIRE(cuCtxCreate_v4(&version_four_context, &create_parameters, UINT32_C(0), device) ==
                      CUDA_ERROR_INVALID_VALUE,
                  88);
  create_parameters.execAffinityParams = (CUexecAffinityParam*)(uintptr_t)UINT64_C(1);
  MF_TEST_REQUIRE(cuCtxCreate_v4(&version_four_context, &create_parameters, UINT32_C(0), device) ==
                          CUDA_ERROR_NOT_SUPPORTED &&
                      cuCtxCreate_v4(&version_four_context, (CUctxCreateParams*)0, UINT32_C(0x100),
                                     device) == CUDA_ERROR_INVALID_VALUE,
                  89);
  primary_acquire_calls =
      atomic_load_explicit(&fixture.context_acquire_calls, memory_order_relaxed);
  primary_release_calls =
      atomic_load_explicit(&fixture.context_release_calls, memory_order_relaxed);
  MF_TEST_REQUIRE(
      cuDevicePrimaryCtxRetain(&primary_context, device) == CUDA_SUCCESS &&
          primary_context != (CUcontext)0 &&
          cuDevicePrimaryCtxRetain(&retained_primary_context, device) == CUDA_SUCCESS &&
          retained_primary_context == primary_context &&
          atomic_load_explicit(&fixture.live_contexts, memory_order_relaxed) == UINT32_C(1) &&
          atomic_load_explicit(&fixture.context_acquire_calls, memory_order_relaxed) ==
              primary_acquire_calls + UINT32_C(1) &&
          cuCtxSetCurrent(primary_context) == CUDA_SUCCESS &&
          cuMemAlloc_v2(&primary_memory, sizeof(left_values)) == CUDA_SUCCESS &&
          cuDevicePrimaryCtxReset_v2(device) == CUDA_SUCCESS &&
          atomic_load_explicit(&fixture.live_contexts, memory_order_relaxed) == UINT32_C(1) &&
          atomic_load_explicit(&fixture.context_release_calls, memory_order_relaxed) ==
              primary_release_calls &&
          cuCtxGetCurrent(&popped_context) == CUDA_SUCCESS && popped_context == primary_context &&
          cuMemFree_v2(primary_memory) == CUDA_ERROR_INVALID_VALUE &&
          mf_cuda_provider_test_set_primary_refcount_v1(device, UINT32_MAX) == 0 &&
          cuDevicePrimaryCtxRetain(&overflow_primary_context, device) == CUDA_ERROR_OUT_OF_MEMORY &&
          overflow_primary_context == (CUcontext)0 &&
          mf_cuda_provider_test_set_primary_refcount_v1(device, UINT32_C(2)) == 0 &&
          cuDevicePrimaryCtxRelease_v2(device) == CUDA_SUCCESS &&
          atomic_load_explicit(&fixture.live_contexts, memory_order_relaxed) == UINT32_C(1) &&
          atomic_load_explicit(&fixture.context_release_calls, memory_order_relaxed) ==
              primary_release_calls &&
          cuDevicePrimaryCtxRelease_v2(device) == CUDA_SUCCESS &&
          atomic_load_explicit(&fixture.live_contexts, memory_order_relaxed) == UINT32_C(0) &&
          atomic_load_explicit(&fixture.context_release_calls, memory_order_relaxed) ==
              primary_release_calls + UINT32_C(1) &&
          cuCtxGetCurrent(&popped_context) == CUDA_SUCCESS && popped_context == (CUcontext)0,
      147);
  primary_context = (CUcontext)0;
  retained_primary_context = (CUcontext)0;
  primary_memory = (CUdeviceptr)0;
  atomic_store_explicit(&fixture.reenter_control_once, UINT32_C(1), memory_order_release);
  MF_TEST_REQUIRE(cuCtxCreate_v2(&context, UINT32_C(0), device) == CUDA_SUCCESS &&
                      atomic_load_explicit(&fixture.reenter_control_result, memory_order_acquire) ==
                          (uint32_t)CUDA_SUCCESS &&
                      cuCtxGetDevice(&current_device) == CUDA_SUCCESS && current_device == device &&
                      cuCtxPopCurrent_v2(&popped_context) == CUDA_SUCCESS &&
                      popped_context == context && cuCtxPushCurrent_v2(context) == CUDA_SUCCESS &&
                      cuMemAlloc_v2(&device_left, sizeof(left_values)) == CUDA_SUCCESS &&
                      cuMemAlloc_v2(&device_right, sizeof(right_values)) == CUDA_SUCCESS &&
                      cuMemAlloc_v2(&device_output, sizeof(output_values)) == CUDA_SUCCESS &&
                      cuMemAlloc_v2(&device_copy, sizeof(copied_values)) == CUDA_SUCCESS,
                  9);
  atomic_store_explicit(&fixture.reenter_read_once, UINT32_C(1), memory_order_release);
  MF_TEST_REQUIRE(
      cuMemcpyHtoD_v2(device_left, left_values, sizeof(left_values)) == CUDA_SUCCESS &&
          cuMemcpyHtoD_v2(device_right, right_values, sizeof(right_values)) == CUDA_SUCCESS &&
          cuMemcpyDtoD_v2(device_copy, device_left, sizeof(left_values)) == CUDA_SUCCESS &&
          cuMemcpyDtoH_v2(copied_values, device_copy, sizeof(copied_values)) == CUDA_SUCCESS &&
          atomic_load_explicit(&fixture.reenter_read_result, memory_order_acquire) ==
              (uint32_t)CUDA_SUCCESS &&
          memcmp(copied_values, left_values, sizeof(left_values)) == 0,
      10);
  MF_TEST_REQUIRE(
      cuMemcpyHtoD_v2(device_copy, zero_values, sizeof(zero_values)) == CUDA_SUCCESS &&
          cuMemcpyHtoD_v2(device_output, zero_values, sizeof(zero_values)) == CUDA_SUCCESS &&
          cuMemcpyHtoD_v2(device_copy + UINT64_C(1), interior_bytes, sizeof(interior_bytes)) ==
              CUDA_SUCCESS &&
          cuMemcpyDtoH_v2(interior_readback, device_copy + UINT64_C(1),
                          sizeof(interior_readback)) == CUDA_SUCCESS &&
          memcmp(interior_readback, interior_bytes, sizeof(interior_bytes)) == 0 &&
          cuMemcpyDtoD_v2(device_output + UINT64_C(3), device_copy + UINT64_C(2),
                          sizeof(interior_bytes) - (size_t)2) == CUDA_SUCCESS,
      154);
  copy_argument_register_calls =
      atomic_load_explicit(&fixture.argument_register_calls, memory_order_relaxed);
  MF_TEST_REQUIRE(
      cuMemcpyDtoD_v2(device_output + UINT64_C(3), device_copy + UINT64_C(2),
                      sizeof(interior_bytes) - (size_t)2) == CUDA_SUCCESS &&
          atomic_load_explicit(&fixture.argument_register_calls, memory_order_relaxed) ==
              copy_argument_register_calls &&
          cuMemcpyDtoH_v2(interior_full_readback, device_output, sizeof(interior_full_readback)) ==
              CUDA_SUCCESS &&
          interior_full_readback[0] == UINT8_C(0) && interior_full_readback[1] == UINT8_C(0) &&
          interior_full_readback[2] == UINT8_C(0) &&
          memcmp(interior_full_readback + (size_t)3, interior_bytes + (size_t)1,
                 sizeof(interior_bytes) - (size_t)2) == 0 &&
          interior_full_readback[8] == UINT8_C(0) &&
          cuMemcpyHtoD_v2(device_copy + sizeof(copied_values) - (size_t)1, interior_bytes,
                          (size_t)2) == CUDA_ERROR_INVALID_VALUE &&
          cuMemcpyDtoH_v2(interior_readback, device_copy + sizeof(copied_values) - (size_t)1,
                          (size_t)2) == CUDA_ERROR_INVALID_VALUE &&
          cuMemcpyDtoD_v2(device_output, device_copy + sizeof(copied_values) - (size_t)1,
                          (size_t)2) == CUDA_ERROR_INVALID_VALUE,
      155);
  MF_TEST_REQUIRE(cuMemcpyDtoD_v2(device_output + sizeof(uint32_t), device_left,
                                  sizeof(left_values)) == CUDA_ERROR_INVALID_VALUE &&
                      cuMemFree_v2(device_left + UINT64_C(1)) == CUDA_ERROR_INVALID_VALUE,
                  11);
  /* Kernel-name resolution is permissive since the daemon-side module
     surface expanded: unknown names resolve to function tokens and fail at
     launch with the backend's kernel diagnostics. */
  MF_TEST_REQUIRE(cuModuleLoadData(&module, ptx) == CUDA_SUCCESS &&
                      cuModuleGetFunction(&missing_function, module, "shadow_add_u32") ==
                          CUDA_SUCCESS &&
                      missing_function != (CUfunction)0 &&
                      cuModuleGetFunction(&missing_function, module, "add_u32_suffix") ==
                          CUDA_SUCCESS &&
                      missing_function != (CUfunction)0 &&
                      cuModuleGetFunction(&function, module, "add_u32") == CUDA_SUCCESS &&
                      cuStreamCreate(&stream, CU_STREAM_NON_BLOCKING) == CUDA_SUCCESS &&
                      cuEventCreate(&event, CU_EVENT_DISABLE_TIMING) == CUDA_SUCCESS,
                  12);
  parameters[0] = &device_output;
  parameters[1] = &device_left;
  parameters[2] = &device_right;
  parameters[3] = &element_count;
  MF_TEST_REQUIRE(cuCtxCreate_v2(&foreign_context, UINT32_C(0), device) == CUDA_SUCCESS &&
                      cuMemAlloc_v2(&foreign_memory, sizeof(left_values)) == CUDA_SUCCESS &&
                      cuCtxSetCurrent(context) == CUDA_SUCCESS,
                  148);
  parameters[1] = &foreign_memory;
  MF_TEST_REQUIRE(cuLaunchKernel(function, UINT32_C(1), UINT32_C(1), UINT32_C(1), element_count,
                                 UINT32_C(1), UINT32_C(1), UINT32_C(0), stream, parameters,
                                 (void**)0) == CUDA_ERROR_INVALID_CONTEXT &&
                      cuMemFree_v2(foreign_memory) == CUDA_ERROR_INVALID_CONTEXT &&
                      cuMemcpyHtoD_v2(foreign_memory, left_values, sizeof(left_values)) ==
                          CUDA_ERROR_INVALID_CONTEXT &&
                      cuMemcpyDtoH_v2(copied_values, foreign_memory, sizeof(copied_values)) ==
                          CUDA_ERROR_INVALID_CONTEXT &&
                      cuMemcpyDtoD_v2(foreign_memory, device_left, sizeof(left_values)) ==
                          CUDA_ERROR_INVALID_CONTEXT &&
                      cuMemcpyDtoD_v2(device_output, foreign_memory, sizeof(left_values)) ==
                          CUDA_ERROR_INVALID_CONTEXT &&
                      mf_cuda_provider_test_pending_count_v1() == UINT32_C(0) &&
                      cuCtxSetCurrent(foreign_context) == CUDA_SUCCESS &&
                      cuMemFree_v2(foreign_memory) == CUDA_SUCCESS &&
                      cuCtxDestroy_v2(foreign_context) == CUDA_SUCCESS &&
                      cuCtxGetCurrent(&popped_context) == CUDA_SUCCESS && popped_context == context,
                  149);
  parameters[1] = &device_left;
  foreign_context = (CUcontext)0;
  foreign_memory = (CUdeviceptr)0;
  (void)memcpy(asynchronous_source, left_values, sizeof(asynchronous_source));
  atomic_store_explicit(&fixture.pause_completions, UINT32_C(1), memory_order_release);
  MF_TEST_REQUIRE(
      cuMemcpyHtoDAsync_v2(device_left, asynchronous_source, sizeof(asynchronous_source), stream) ==
              CUDA_SUCCESS &&
          (memset(asynchronous_source, 0, sizeof(asynchronous_source)) != (void*)0) &&
          cuLaunchKernel(function, UINT32_C(1), UINT32_C(2), UINT32_C(1), UINT32_C(2), UINT32_C(2),
                         UINT32_C(1), UINT32_C(0), stream, parameters, (void**)0) == CUDA_SUCCESS &&
          cuEventRecord(event, stream) == CUDA_SUCCESS &&
          cuStreamWaitEvent(stream, event, UINT32_C(0)) == CUDA_SUCCESS &&
          cuMemcpyDtoHAsync_v2(output_values, device_output, sizeof(output_values), stream) ==
              CUDA_SUCCESS &&
          mf_cuda_provider_test_pending_count_v1() == UINT32_C(5) &&
          mf_test_active_kind_count(&fixture, MF_TEST_OBJECT_ARGUMENT_BLOCK) == UINT32_C(2) &&
          mf_test_active_kind_count(&fixture, MF_TEST_OBJECT_HOST_MEMORY) == UINT32_C(2) &&
          cuEventQuery(event) == CUDA_ERROR_NOT_READY &&
          memcmp(output_values, (const uint32_t[8]){0}, sizeof(output_values)) == 0,
      13);
  atomic_store_explicit(&fixture.pause_completions, UINT32_C(0), memory_order_release);
  MF_TEST_REQUIRE(
      cuEventSynchronize(event) == CUDA_SUCCESS &&
          mf_test_active_kind_count(&fixture, MF_TEST_OBJECT_ARGUMENT_BLOCK) == UINT32_C(2) &&
          cuStreamSynchronize(stream) == CUDA_SUCCESS &&
          mf_cuda_provider_test_pending_count_v1() == UINT32_C(0) &&
          mf_test_active_kind_count(&fixture, MF_TEST_OBJECT_HOST_MEMORY) == UINT32_C(0) &&
          cuEventElapsedTime(&elapsed_ms, event, event) == CUDA_ERROR_NOT_SUPPORTED &&
          cuEventQuery(event) == CUDA_SUCCESS,
      91);
  argument_register_calls =
      atomic_load_explicit(&fixture.argument_register_calls, memory_order_relaxed);
  MF_TEST_REQUIRE(
      argument_register_calls == copy_argument_register_calls + UINT32_C(1) &&
          cuLaunchKernel(function, UINT32_C(1), UINT32_C(2), UINT32_C(1), UINT32_C(2), UINT32_C(2),
                         UINT32_C(1), UINT32_C(0), stream, parameters, (void**)0) == CUDA_SUCCESS &&
          cuStreamSynchronize(stream) == CUDA_SUCCESS &&
          atomic_load_explicit(&fixture.argument_register_calls, memory_order_relaxed) ==
              argument_register_calls &&
          mf_test_active_kind_count(&fixture, MF_TEST_OBJECT_ARGUMENT_BLOCK) == UINT32_C(2),
      115);
  MF_TEST_REQUIRE(mf_cuda_audit_warm_aliases(function, stream, event, device_output, device_left,
                                             device_right, left_values, element_count) == 0,
                  158);
  MF_TEST_REQUIRE(mf_cuda_run_warm_path_threads(context, function, device_output, device_left,
                                                device_right, element_count) == 0,
                  159);
  for (count = 0; count < (int)element_count; ++count) {
    MF_TEST_REQUIRE(output_values[count] == left_values[count] + right_values[count], 14);
  }
  atomic_store_explicit(&fixture.pause_completions, UINT32_C(1), memory_order_release);
  (void)memset(&release_args, 0, sizeof(release_args));
  release_args.kind = MF_CUDA_RELEASE_STREAM_SYNCHRONIZE;
  release_args.context = context;
  release_args.stream = stream;
  release_args.result = CUDA_ERROR_UNKNOWN;
  atomic_init(&release_args.started, UINT32_C(0));
  atomic_init(&release_args.finished, UINT32_C(0));
  if (cuMemcpyDtoDAsync_v2(device_output, device_left, sizeof(left_values), stream) ==
          CUDA_SUCCESS &&
      pthread_create(&release_thread, (const pthread_attr_t*)0, mf_cuda_release_thread,
                     &release_args) == 0) {
    synchronization_thread_created = UINT32_C(1);
    waiter_observed = (uint32_t)mf_cuda_wait_for_provider_waiter(UINT64_C(100000000));
    if (waiter_observed != UINT32_C(0)) {
      concurrent_count_result = cuDeviceGetCount(&concurrent_device_count);
      synchronization_was_blocked =
          atomic_load_explicit(&release_args.finished, memory_order_acquire) == UINT32_C(0)
              ? UINT32_C(1)
              : UINT32_C(0);
    }
  }
  atomic_store_explicit(&fixture.pause_completions, UINT32_C(0), memory_order_release);
  if (synchronization_thread_created != UINT32_C(0)) {
    synchronization_join_result = pthread_join(release_thread, (void**)0);
  }
  MF_TEST_REQUIRE(synchronization_thread_created != UINT32_C(0) && waiter_observed != UINT32_C(0) &&
                      concurrent_count_result == CUDA_SUCCESS && concurrent_device_count == 2 &&
                      synchronization_was_blocked != UINT32_C(0) &&
                      synchronization_join_result == 0 && release_args.result == CUDA_SUCCESS &&
                      mf_cuda_provider_test_active_waiter_count_v1() == UINT32_C(0),
                  146);
  MF_TEST_REQUIRE(cuMemcpyHtoD_v2(device_output, zero_values, sizeof(zero_values)) ==
                          CUDA_SUCCESS &&
                      cuLaunchKernel(function, UINT32_C(1), UINT32_C(1), UINT32_C(2), UINT32_C(1),
                                     UINT32_C(1), UINT32_C(1), UINT32_C(0), stream, parameters,
                                     (void**)0) == CUDA_ERROR_NOT_SUPPORTED &&
                      cuLaunchKernel(function, UINT32_C(1), UINT32_C(1), UINT32_C(1), UINT32_C(1),
                                     UINT32_C(1), UINT32_C(1), UINT32_C(1), stream, parameters,
                                     (void**)0) == CUDA_ERROR_NOT_SUPPORTED,
                  142);
  atomic_store_explicit(&fixture.pause_completions, UINT32_C(1), memory_order_release);
  MF_TEST_REQUIRE(cuLaunchKernel_ptsz(function, UINT32_C(1), UINT32_C(1), UINT32_C(1), UINT32_C(1),
                                      UINT32_C(1), UINT32_C(1), UINT32_C(0), (CUstream)0,
                                      parameters, (void**)0) == CUDA_SUCCESS &&
                      cuStreamQuery((CUstream)0) == CUDA_SUCCESS &&
                      cuStreamQuery_ptsz((CUstream)0) == CUDA_ERROR_NOT_READY,
                  143);
  atomic_store_explicit(&fixture.pause_completions, UINT32_C(0), memory_order_release);
  MF_TEST_REQUIRE(
      cuStreamSynchronize_ptsz((CUstream)0) == CUDA_SUCCESS &&
          cuMemcpyDtoH_v2(output_values, device_output, sizeof(output_values)) == CUDA_SUCCESS &&
          atomic_load_explicit(&fixture.argument_register_calls, memory_order_relaxed) ==
              argument_register_calls + 1U,
      144);
  argument_register_calls += UINT32_C(1);
  for (count = 0; count < (int)element_count; ++count) {
    const uint32_t expected = count == 0 ? left_values[count] + right_values[count] : UINT32_C(0);
    MF_TEST_REQUIRE(output_values[count] == expected, 145);
  }
  atomic_store_explicit(&fixture.injected_status, (uint32_t)MF_SHARED_DEVICE_LOST,
                        memory_order_release);
  MF_TEST_REQUIRE(cuMemcpyDtoDAsync_v2(device_copy, device_left, sizeof(left_values), stream) ==
                          CUDA_SUCCESS &&
                      cuStreamSynchronize(stream) == CUDA_ERROR_CONTEXT_IS_DESTROYED &&
                      cuStreamSynchronize(stream) == CUDA_SUCCESS,
                  92);

  pending_capacity = mf_cuda_provider_test_pending_capacity_v1();
  MF_TEST_REQUIRE(pending_capacity == UINT32_C(128), 93);
  atomic_store_explicit(&fixture.pause_completions, UINT32_C(1), memory_order_release);
  for (async_index = UINT32_C(0); async_index < pending_capacity; ++async_index) {
    if (cuMemcpyDtoDAsync_v2(device_copy, device_left, sizeof(left_values), stream) !=
        CUDA_SUCCESS) {
      break;
    }
  }
  MF_TEST_REQUIRE(async_index == pending_capacity &&
                      mf_cuda_provider_test_pending_count_v1() == pending_capacity &&
                      cuMemcpyDtoDAsync_v2(device_copy, device_left, sizeof(left_values), stream) ==
                          CUDA_ERROR_OUT_OF_MEMORY,
                  94);
  atomic_store_explicit(&fixture.pause_completions, UINT32_C(0), memory_order_release);
  MF_TEST_REQUIRE(cuStreamSynchronize(stream) == CUDA_SUCCESS &&
                      mf_cuda_provider_test_pending_count_v1() == UINT32_C(0),
                  95);

  MF_TEST_REQUIRE(mf_cuda_provider_test_set_next_request_v1(UINT64_MAX) == 0 &&
                      cuMemcpyDtoDAsync_v2(device_copy, device_left, sizeof(left_values), stream) ==
                          CUDA_ERROR_OUT_OF_MEMORY &&
                      mf_cuda_provider_test_pending_count_v1() == UINT32_C(0) &&
                      mf_cuda_provider_test_set_next_request_v1(UINT64_C(1000000)) == 0,
                  96);

  for (async_index = UINT32_C(0); async_index <= pending_capacity; ++async_index) {
    transient_stream = (CUstream)0;
    MF_TEST_REQUIRE(cuStreamCreate(&transient_stream, CU_STREAM_DEFAULT) == CUDA_SUCCESS, 108);
    atomic_store_explicit(&fixture.injected_status, (uint32_t)MF_SHARED_DEVICE_LOST,
                          memory_order_release);
    MF_TEST_REQUIRE(cuMemcpyDtoDAsync_v2(device_copy, device_left, sizeof(left_values),
                                         transient_stream) == CUDA_SUCCESS &&
                        cuStreamDestroy_v2(transient_stream) == CUDA_SUCCESS &&
                        cuCtxSynchronize() == CUDA_ERROR_CONTEXT_IS_DESTROYED &&
                        mf_cuda_provider_test_pending_count_v1() == UINT32_C(0) &&
                        mf_cuda_provider_test_async_error_count_v1() == UINT32_C(0),
                    109);
  }
  for (async_index = UINT32_C(0); async_index <= pending_capacity; ++async_index) {
    transient_event = (CUevent)0;
    MF_TEST_REQUIRE(cuEventCreate(&transient_event, CU_EVENT_DEFAULT) == CUDA_SUCCESS, 110);
    atomic_store_explicit(&fixture.injected_status, (uint32_t)MF_SHARED_DEVICE_LOST,
                          memory_order_release);
    MF_TEST_REQUIRE(cuEventRecord(transient_event, stream) == CUDA_SUCCESS &&
                        cuEventDestroy_v2(transient_event) == CUDA_SUCCESS &&
                        cuCtxSynchronize() == CUDA_ERROR_CONTEXT_IS_DESTROYED &&
                        mf_cuda_provider_test_pending_count_v1() == UINT32_C(0) &&
                        mf_cuda_provider_test_async_error_count_v1() == UINT32_C(0),
                    111);
  }
  for (async_index = UINT32_C(0); async_index <= pending_capacity; ++async_index) {
    transient_context = (CUcontext)0;
    transient_event = (CUevent)0;
    MF_TEST_REQUIRE(cuCtxCreate_v2(&transient_context, UINT32_C(0), device) == CUDA_SUCCESS &&
                        cuEventCreate(&transient_event, CU_EVENT_DEFAULT) == CUDA_SUCCESS,
                    112);
    atomic_store_explicit(&fixture.injected_status, (uint32_t)MF_SHARED_DEVICE_LOST,
                          memory_order_release);
    MF_TEST_REQUIRE(cuEventRecord(transient_event, (CUstream)0) == CUDA_SUCCESS &&
                        cuCtxDestroy_v2(transient_context) == CUDA_ERROR_CONTEXT_IS_DESTROYED &&
                        mf_cuda_provider_test_pending_count_v1() == UINT32_C(0) &&
                        mf_cuda_provider_test_async_error_count_v1() == UINT32_C(0),
                    113);
  }
  MF_TEST_REQUIRE(cuCtxGetDevice(&current_device) == CUDA_SUCCESS && current_device == device &&
                      cuStreamSynchronize(stream) == CUDA_SUCCESS,
                  114);

  MF_TEST_REQUIRE(cuEventCreate(&ptds_event, CU_EVENT_DEFAULT) == CUDA_SUCCESS, 138);
  atomic_store_explicit(&fixture.pause_completions, UINT32_C(1), memory_order_release);
  atomic_store_explicit(&fixture.injected_status, (uint32_t)MF_SHARED_DEVICE_LOST,
                        memory_order_release);
  MF_TEST_REQUIRE(cuMemcpyDtoDAsync_v2_ptsz(device_output, device_left, sizeof(left_values),
                                            (CUstream)0) == CUDA_SUCCESS &&
                      cuStreamQuery_ptsz((CUstream)0) == CUDA_ERROR_NOT_READY,
                  139);
  (void)memset(&ptds_args, 0, sizeof(ptds_args));
  ptds_args.context = context;
  ptds_args.event = ptds_event;
  ptds_args.set_current_result = CUDA_ERROR_UNKNOWN;
  ptds_args.query_result = CUDA_ERROR_UNKNOWN;
  ptds_args.synchronize_result = CUDA_ERROR_UNKNOWN;
  ptds_args.record_result = CUDA_ERROR_UNKNOWN;
  MF_TEST_REQUIRE(
      pthread_create(&ptds_thread, (const pthread_attr_t*)0, mf_cuda_ptds_thread, &ptds_args) ==
              0 &&
          pthread_join(ptds_thread, (void**)0) == 0 &&
          ptds_args.set_current_result == CUDA_SUCCESS && ptds_args.query_result == CUDA_SUCCESS &&
          ptds_args.synchronize_result == CUDA_SUCCESS && ptds_args.record_result == CUDA_SUCCESS,
      140);
  atomic_store_explicit(&fixture.pause_completions, UINT32_C(0), memory_order_release);
  MF_TEST_REQUIRE(cuEventSynchronize(ptds_event) == CUDA_SUCCESS &&
                      cuStreamSynchronize_ptsz((CUstream)0) == CUDA_ERROR_CONTEXT_IS_DESTROYED &&
                      cuStreamSynchronize_ptsz((CUstream)0) == CUDA_SUCCESS &&
                      cuEventDestroy_v2(ptds_event) == CUDA_SUCCESS,
                  141);
  ptds_event = (CUevent)0;

  transient_stream = (CUstream)0;
  MF_TEST_REQUIRE(cuStreamCreate(&transient_stream, CU_STREAM_DEFAULT) == CUDA_SUCCESS, 118);
  atomic_store_explicit(&fixture.pause_completions, UINT32_C(1), memory_order_release);
  MF_TEST_REQUIRE(cuMemcpyDtoDAsync_v2(device_output, device_left, sizeof(left_values),
                                       transient_stream) == CUDA_SUCCESS,
                  119);
  (void)memset(&release_args, 0, sizeof(release_args));
  release_args.kind = MF_CUDA_RELEASE_STREAM;
  release_args.context = context;
  release_args.stream = transient_stream;
  release_args.result = CUDA_ERROR_UNKNOWN;
  atomic_init(&release_args.started, UINT32_C(0));
  atomic_init(&release_args.finished, UINT32_C(0));
  MF_TEST_REQUIRE(pthread_create(&release_thread, (const pthread_attr_t*)0, mf_cuda_release_thread,
                                 &release_args) == 0,
                  120);
  while (atomic_load_explicit(&release_args.started, memory_order_acquire) == UINT32_C(0)) {
    (void)sched_yield();
  }
  async_index = (uint32_t)mf_cuda_wait_finished(&release_args.finished, UINT64_C(100000000));
  atomic_store_explicit(&fixture.pause_completions, UINT32_C(0), memory_order_release);
  MF_TEST_REQUIRE(pthread_join(release_thread, (void**)0) == 0 && async_index == UINT32_C(1) &&
                      release_args.result == CUDA_SUCCESS,
                  121);
  MF_TEST_REQUIRE(cuCtxSynchronize() == CUDA_SUCCESS &&
                      mf_cuda_provider_test_pending_count_v1() == UINT32_C(0),
                  130);
  transient_stream = (CUstream)0;

  transient_event = (CUevent)0;
  MF_TEST_REQUIRE(cuEventCreate(&transient_event, CU_EVENT_DEFAULT) == CUDA_SUCCESS, 122);
  atomic_store_explicit(&fixture.pause_completions, UINT32_C(1), memory_order_release);
  MF_TEST_REQUIRE(cuEventRecord(transient_event, stream) == CUDA_SUCCESS, 123);
  (void)memset(&release_args, 0, sizeof(release_args));
  release_args.kind = MF_CUDA_RELEASE_EVENT;
  release_args.event = transient_event;
  release_args.result = CUDA_ERROR_UNKNOWN;
  atomic_init(&release_args.started, UINT32_C(0));
  atomic_init(&release_args.finished, UINT32_C(0));
  MF_TEST_REQUIRE(pthread_create(&release_thread, (const pthread_attr_t*)0, mf_cuda_release_thread,
                                 &release_args) == 0,
                  124);
  while (atomic_load_explicit(&release_args.started, memory_order_acquire) == UINT32_C(0)) {
    (void)sched_yield();
  }
  async_index = (uint32_t)mf_cuda_wait_finished(&release_args.finished, UINT64_C(100000000));
  atomic_store_explicit(&fixture.pause_completions, UINT32_C(0), memory_order_release);
  MF_TEST_REQUIRE(pthread_join(release_thread, (void**)0) == 0 && async_index == UINT32_C(1) &&
                      release_args.result == CUDA_SUCCESS,
                  125);
  MF_TEST_REQUIRE(cuCtxSynchronize() == CUDA_SUCCESS &&
                      mf_cuda_provider_test_pending_count_v1() == UINT32_C(0),
                  131);
  transient_event = (CUevent)0;

  MF_TEST_REQUIRE(cuEventCreate(&transient_event, CU_EVENT_DEFAULT) == CUDA_SUCCESS, 126);
  atomic_store_explicit(&fixture.persistent_status, (uint32_t)MF_SHARED_DEVICE_LOST,
                        memory_order_release);
  for (async_index = UINT32_C(0); async_index <= pending_capacity; ++async_index) {
    MF_TEST_REQUIRE(cuMemcpyDtoDAsync_v2(device_output, device_left, sizeof(left_values), stream) ==
                        CUDA_SUCCESS,
                    127);
    while (mf_cuda_provider_test_pending_count_v1() != UINT32_C(0)) {
      MF_TEST_REQUIRE(cuEventQuery(transient_event) == CUDA_SUCCESS, 128);
      (void)sched_yield();
    }
  }
  atomic_store_explicit(&fixture.persistent_status, UINT32_MAX, memory_order_release);
  MF_TEST_REQUIRE(mf_cuda_provider_test_async_error_count_v1() == pending_capacity &&
                      cuStreamDestroy_v2(stream) == CUDA_ERROR_CONTEXT_IS_DESTROYED &&
                      mf_cuda_provider_test_async_error_count_v1() == UINT32_C(0) &&
                      cuEventDestroy_v2(transient_event) == CUDA_SUCCESS &&
                      cuStreamCreate(&stream, CU_STREAM_DEFAULT) == CUDA_SUCCESS &&
                      cuMemcpyDtoDAsync_v2(device_output, device_left, sizeof(left_values),
                                           stream) == CUDA_SUCCESS &&
                      cuStreamSynchronize(stream) == CUDA_SUCCESS,
                  129);
  transient_event = (CUevent)0;

  atomic_store_explicit(&fixture.pause_completions, UINT32_C(1), memory_order_release);
  MF_TEST_REQUIRE(cuMemcpyDtoDAsync_v2(device_copy, device_left, sizeof(left_values), stream) ==
                      CUDA_SUCCESS,
                  97);
  (void)memset(&release_args, 0, sizeof(release_args));
  release_args.kind = MF_CUDA_RELEASE_MEMORY;
  release_args.memory = device_copy;
  release_args.context = context;
  release_args.result = CUDA_ERROR_UNKNOWN;
  atomic_init(&release_args.started, UINT32_C(0));
  atomic_init(&release_args.finished, UINT32_C(0));
  MF_TEST_REQUIRE(pthread_create(&release_thread, (const pthread_attr_t*)0, mf_cuda_release_thread,
                                 &release_args) == 0,
                  98);
  while (atomic_load_explicit(&release_args.started, memory_order_acquire) == UINT32_C(0)) {
    (void)sched_yield();
  }
  async_index = atomic_load_explicit(&release_args.finished, memory_order_acquire);
  atomic_store_explicit(&fixture.pause_completions, UINT32_C(0), memory_order_release);
  MF_TEST_REQUIRE(pthread_join(release_thread, (void**)0) == 0 && async_index == UINT32_C(0) &&
                      release_args.result == CUDA_SUCCESS &&
                      mf_cuda_provider_test_pending_count_v1() == UINT32_C(0),
                  99);
  device_copy = (CUdeviceptr)0;

  interior_output = device_output + sizeof(uint32_t);
  interior_left = device_left + sizeof(uint32_t);
  interior_right = device_right + sizeof(uint32_t);
  parameters[0] = &interior_output;
  parameters[1] = &interior_left;
  parameters[2] = &interior_right;
  parameters[3] = &interior_element_count;
  MF_TEST_REQUIRE(
      cuMemcpyHtoD_v2(device_output, zero_values, sizeof(zero_values)) == CUDA_SUCCESS &&
          cuLaunchKernel(function, UINT32_C(1), UINT32_C(1), UINT32_C(1), interior_element_count,
                         UINT32_C(1), UINT32_C(1), UINT32_C(0), stream, parameters,
                         (void**)0) == CUDA_SUCCESS &&
          cuStreamSynchronize(stream) == CUDA_SUCCESS &&
          cuMemcpyDtoH_v2(output_values, device_output, sizeof(output_values)) == CUDA_SUCCESS &&
          output_values[0] == UINT32_C(0),
      150);
  for (count = 1; count < (int)element_count; ++count) {
    MF_TEST_REQUIRE(output_values[count] == left_values[count] + right_values[count], 151);
  }
  interior_output = device_output + UINT64_C(1);
  MF_TEST_REQUIRE(cuLaunchKernel(function, UINT32_C(1), UINT32_C(1), UINT32_C(1),
                                 interior_element_count, UINT32_C(1), UINT32_C(1), UINT32_C(0),
                                 stream, parameters, (void**)0) == CUDA_ERROR_INVALID_VALUE,
                  152);
  interior_output = device_output + sizeof(uint32_t);
  parameters[3] = &element_count;
  MF_TEST_REQUIRE(cuLaunchKernel(function, UINT32_C(1), UINT32_C(1), UINT32_C(1), element_count,
                                 UINT32_C(1), UINT32_C(1), UINT32_C(0), stream, parameters,
                                 (void**)0) == CUDA_ERROR_INVALID_VALUE,
                  153);
  parameters[0] = &device_output;
  parameters[1] = &device_left;
  parameters[2] = &device_right;
  parameters[3] = &element_count;
  argument_register_calls =
      atomic_load_explicit(&fixture.argument_register_calls, memory_order_relaxed);

  atomic_store_explicit(&fixture.pause_completions, UINT32_C(1), memory_order_release);
  MF_TEST_REQUIRE(cuLaunchKernel(function, UINT32_C(1), UINT32_C(2), UINT32_C(1), UINT32_C(2),
                                 UINT32_C(2), UINT32_C(1), UINT32_C(0), stream, parameters,
                                 (void**)0) == CUDA_SUCCESS,
                  100);
  (void)memset(&release_args, 0, sizeof(release_args));
  release_args.kind = MF_CUDA_RELEASE_MODULE;
  release_args.module = module;
  release_args.result = CUDA_ERROR_UNKNOWN;
  atomic_init(&release_args.started, UINT32_C(0));
  atomic_init(&release_args.finished, UINT32_C(0));
  MF_TEST_REQUIRE(pthread_create(&release_thread, (const pthread_attr_t*)0, mf_cuda_release_thread,
                                 &release_args) == 0,
                  101);
  while (atomic_load_explicit(&release_args.started, memory_order_acquire) == UINT32_C(0)) {
    (void)sched_yield();
  }
  async_index = atomic_load_explicit(&release_args.finished, memory_order_acquire);
  atomic_store_explicit(&fixture.pause_completions, UINT32_C(0), memory_order_release);
  MF_TEST_REQUIRE(
      pthread_join(release_thread, (void**)0) == 0 && async_index == UINT32_C(0) &&
          release_args.result == CUDA_SUCCESS &&
          mf_test_active_kind_count(&fixture, MF_TEST_OBJECT_ARGUMENT_BLOCK) == UINT32_C(0) &&
          atomic_load_explicit(&fixture.argument_register_calls, memory_order_relaxed) ==
              argument_register_calls &&
          atomic_load_explicit(&fixture.argument_release_calls, memory_order_relaxed) ==
              argument_register_calls,
      102);
  module = (CUmodule)0;
  function = (CUfunction)0;
  MF_TEST_REQUIRE(
      cuStreamDestroy_v2(stream) == CUDA_SUCCESS &&
          cuStreamQuery(stream) == CUDA_ERROR_INVALID_HANDLE &&
          cuEventDestroy_v2(event) == CUDA_SUCCESS && cuMemFree_v2(device_output) == CUDA_SUCCESS &&
          cuMemFree_v2(device_right) == CUDA_SUCCESS && cuMemFree_v2(device_left) == CUDA_SUCCESS &&
          cuCtxDestroy_v2(context) == CUDA_SUCCESS &&
          atomic_load_explicit(&fixture.live_contexts, memory_order_relaxed) == UINT32_C(0) &&
          atomic_load_explicit(&fixture.context_acquire_calls, memory_order_relaxed) ==
              atomic_load_explicit(&fixture.context_release_calls, memory_order_relaxed),
      15);
  context = (CUcontext)0;
  module = (CUmodule)0;
  function = (CUfunction)0;
  stream = (CUstream)0;
  event = (CUevent)0;
  device_left = (CUdeviceptr)0;
  device_right = (CUdeviceptr)0;
  device_output = (CUdeviceptr)0;
  MF_TEST_REQUIRE(cuCtxCreate_v2(&context, UINT32_C(0), device) == CUDA_SUCCESS &&
                      cuMemAlloc_v2(&device_left, sizeof(left_values)) == CUDA_SUCCESS &&
                      cuMemAlloc_v2(&device_right, sizeof(right_values)) == CUDA_SUCCESS &&
                      cuMemAlloc_v2(&device_output, sizeof(output_values)) == CUDA_SUCCESS &&
                      cuModuleLoadData(&module, ptx) == CUDA_SUCCESS &&
                      cuModuleGetFunction(&function, module, "add_u32") == CUDA_SUCCESS &&
                      cuStreamCreate(&stream, CU_STREAM_DEFAULT) == CUDA_SUCCESS &&
                      cuEventCreate(&event, CU_EVENT_DEFAULT) == CUDA_SUCCESS &&
                      cuLaunchKernel(function, UINT32_C(1), UINT32_C(1), UINT32_C(1), element_count,
                                     UINT32_C(1), UINT32_C(1), UINT32_C(0), stream, parameters,
                                     (void**)0) == CUDA_SUCCESS &&
                      cuStreamSynchronize(stream) == CUDA_SUCCESS,
                  16);
  MF_TEST_REQUIRE(cuCtxDestroy_v2(context) == CUDA_SUCCESS, 138);
  MF_TEST_REQUIRE(cuModuleUnload(module) == CUDA_ERROR_INVALID_HANDLE, 139);
  MF_TEST_REQUIRE(cuMemFree_v2(device_left) == CUDA_ERROR_INVALID_CONTEXT, 140);
  MF_TEST_REQUIRE(cuEventQuery(event) == CUDA_ERROR_INVALID_HANDLE, 141);
  MF_TEST_REQUIRE(atomic_load_explicit(&fixture.argument_release_calls, memory_order_relaxed) ==
                      argument_register_calls + UINT32_C(1),
                  142);
  MF_TEST_REQUIRE(mf_test_active_object_count(&fixture) == UINT32_C(0), 143);

  mf_cuda_provider_test_reset_v1();
  context = (CUcontext)0;
  module = (CUmodule)0;
  function = (CUfunction)0;
  stream = (CUstream)0;
  device_left = (CUdeviceptr)0;
  device_right = (CUdeviceptr)0;
  device_output = (CUdeviceptr)0;
  MF_TEST_REQUIRE(
          setenv("CUDA_VISIBLE_DEVICES", "0", 1) == 0 &&
          mf_cuda_provider_test_install_transport_v1(&transport) == 0 &&
          cuInit(UINT32_C(0)) == CUDA_SUCCESS &&
          mf_cuda_provider_test_process_view_revision_v1() == UINT64_C(2) &&
          cuDeviceGet(&device, 0) == CUDA_SUCCESS &&
          cuCtxCreate_v2(&context, UINT32_C(0), device) == CUDA_SUCCESS &&
          cuMemAlloc_v2(&device_left, sizeof(left_values)) == CUDA_SUCCESS &&
          cuMemAlloc_v2(&device_right, sizeof(right_values)) == CUDA_SUCCESS &&
          cuMemAlloc_v2(&device_output, sizeof(output_values)) == CUDA_SUCCESS &&
          cuModuleLoadData(&module, ptx) == CUDA_SUCCESS &&
          cuModuleGetFunction(&function, module, "add_u32") == CUDA_SUCCESS &&
          cuStreamCreate(&stream, CU_STREAM_DEFAULT) == CUDA_SUCCESS &&
          cuLaunchKernel(function, UINT32_C(1), UINT32_C(1), UINT32_C(1), element_count,
                         UINT32_C(1), UINT32_C(1), UINT32_C(0), stream, parameters,
                         (void**)0) == CUDA_SUCCESS &&
          cuStreamSynchronize(stream) == CUDA_SUCCESS &&
          atomic_load_explicit(&fixture.argument_register_calls, memory_order_relaxed) ==
              argument_register_calls + UINT32_C(2) &&
          mf_test_active_kind_count(&fixture, MF_TEST_OBJECT_ARGUMENT_BLOCK) == UINT32_C(1),
      116);
  mf_cuda_provider_test_reset_v1();
  MF_TEST_REQUIRE(atomic_load_explicit(&fixture.argument_release_calls, memory_order_relaxed) ==
                          argument_register_calls + UINT32_C(2) &&
                      mf_test_active_kind_count(&fixture, MF_TEST_OBJECT_ARGUMENT_BLOCK) ==
                          UINT32_C(0),
                  117);
  context = (CUcontext)0;
  module = (CUmodule)0;
  function = (CUfunction)0;
  stream = (CUstream)0;
  device_left = (CUdeviceptr)0;
  device_right = (CUdeviceptr)0;
  device_output = (CUdeviceptr)0;

  mf_cuda_provider_test_reset_v1();
  MF_TEST_REQUIRE(setenv("CUDA_VISIBLE_DEVICES", "", 1) == 0 &&
                      mf_cuda_provider_test_install_transport_v1(&transport) == 0 &&
                      cuInit(UINT32_C(0)) == CUDA_SUCCESS &&
                      cuDeviceGetCount(&count) == CUDA_SUCCESS && count == 0 &&
                      cuDeviceGet(&device, 0) == CUDA_ERROR_INVALID_DEVICE,
                  85);
  mf_cuda_provider_test_reset_v1();
  MF_TEST_REQUIRE(setenv("CUDA_VISIBLE_DEVICES", "GPU-11121314", 1) == 0 &&
                      mf_cuda_provider_test_install_transport_v1(&transport) == 0 &&
                      cuInit(UINT32_C(0)) == CUDA_SUCCESS &&
                      cuDeviceGetCount(&count) == CUDA_SUCCESS && count == 1 &&
                      cuDeviceGet(&device, 0) == CUDA_SUCCESS &&
                      cuDeviceGetName(name, (int)sizeof(name), device) == CUDA_SUCCESS &&
                      strcmp(name, "MetaFlux CUDA Test 1") == 0,
                  86);
  mf_cuda_provider_test_reset_v1();
  MF_TEST_REQUIRE(setenv("CUDA_VISIBLE_DEVICES", "9999999999999999999999999999999999999999", 1) ==
                          0 &&
                      mf_cuda_provider_test_install_transport_v1(&transport) == 0 &&
                      cuInit(UINT32_C(0)) == CUDA_SUCCESS &&
                      cuDeviceGetCount(&count) == CUDA_SUCCESS && count == 0,
                  90);

  mf_cuda_provider_test_reset_v1();
  MF_TEST_REQUIRE(setenv("CUDA_VISIBLE_DEVICES", "0", 1) == 0 &&
                      mf_cuda_provider_test_install_transport_v1(&transport) == 0 &&
                      cuInit(UINT32_C(0)) == CUDA_SUCCESS &&
                      cuDeviceGet(&device, 0) == CUDA_SUCCESS &&
                      cuCtxCreate_v2(&context, UINT32_C(0), device) == CUDA_SUCCESS &&
                      cuMemAlloc_v2(&device_left, sizeof(left_values)) == CUDA_SUCCESS &&
                      cuMemAlloc_v2(&device_copy, sizeof(left_values)) == CUDA_SUCCESS &&
                      cuStreamCreate(&stream, CU_STREAM_DEFAULT) == CUDA_SUCCESS,
                  103);
  atomic_store_explicit(&fixture.mismatch_next_request, UINT32_C(1), memory_order_release);
  MF_TEST_REQUIRE(cuMemcpyDtoDAsync_v2(device_copy, device_left, sizeof(left_values), stream) ==
                          CUDA_SUCCESS &&
                      cuStreamSynchronize(stream) == CUDA_ERROR_UNKNOWN &&
                      mf_cuda_provider_test_pending_count_v1() == UINT32_C(0),
                  104);

  mf_cuda_provider_test_reset_v1();
  context = (CUcontext)0;
  stream = (CUstream)0;
  device_left = (CUdeviceptr)0;
  device_copy = (CUdeviceptr)0;
  MF_TEST_REQUIRE(mf_cuda_provider_test_install_transport_v1(&transport) == 0 &&
                      cuInit(UINT32_C(0)) == CUDA_SUCCESS &&
                      cuDeviceGet(&device, 0) == CUDA_SUCCESS &&
                      cuCtxCreate_v2(&context, UINT32_C(0), device) == CUDA_SUCCESS &&
                      cuMemAlloc_v2(&device_left, sizeof(left_values)) == CUDA_SUCCESS &&
                      cuMemAlloc_v2(&device_copy, sizeof(left_values)) == CUDA_SUCCESS &&
                      cuStreamCreate(&stream, CU_STREAM_DEFAULT) == CUDA_SUCCESS,
                  105);
  atomic_store_explicit(&fixture.pause_completions, UINT32_C(1), memory_order_release);
  MF_TEST_REQUIRE(cuMemcpyDtoDAsync_v2(device_copy, device_left, sizeof(left_values), stream) ==
                          CUDA_SUCCESS &&
                      mf_cuda_provider_test_pending_count_v1() == UINT32_C(1),
                  106);
  mf_cuda_provider_test_reset_v1();
  MF_TEST_REQUIRE(mf_cuda_provider_test_pending_count_v1() == UINT32_C(0), 107);
  atomic_store_explicit(&fixture.pause_completions, UINT32_C(0), memory_order_release);
  context = (CUcontext)0;
  stream = (CUstream)0;
  device_left = (CUdeviceptr)0;
  device_copy = (CUdeviceptr)0;

cleanup:
  mf_cuda_provider_test_reset_v1();
  (void)unsetenv("CUDA_VISIBLE_DEVICES");
  (void)unsetenv("METAFLUX_SOCKET");
  if (fixture_created != UINT32_C(0)) {
    mf_test_fixture_destroy(&fixture);
  }
  return result;
}
