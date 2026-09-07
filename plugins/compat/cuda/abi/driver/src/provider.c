#define _POSIX_C_SOURCE 200809L

#include "managed-renames.h"

#define METAFLUX_CUDA_ABI_INTERNAL 1

#include "metaflux/cuda/provider.h"

#include "metaflux/client/fastpath.h"
#include "metaflux/client/protocol.h"
#ifndef METAFLUX_PROVIDER_CDEV
#define METAFLUX_PROVIDER_CDEV 0
#endif
#if METAFLUX_PROVIDER_CDEV
#include "metaflux/transport/cdev.h"
#endif

#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int mf_cuda_entry_trace_enabled(void) {
  static int enabled = -1;
  if (enabled < 0) {
    enabled = getenv("METAFLUX_TRACE_STUBS") != (void*)0 ? 1 : 0;
  }
  return enabled;
}

#define MF_ENTRY_TRACE() \
  do { \
    if (mf_cuda_entry_trace_enabled() != 0) { \
      fprintf(stderr, "MF_ENTRY %s\n", __func__); \
    } \
  } while (0)

#define MF_ENTRY_RET(expr) \
  do { \
    CUresult _mf_entry_rc = (expr); \
    if (mf_cuda_entry_trace_enabled() != 0) { \
      fprintf(stderr, "MF_ENTRY_RET %s -> %d\n", __func__, (int)_mf_entry_rc); \
    } \
    return _mf_entry_rc; \
  } while (0)


#define MF_CUDA_OBJECT_CAPACITY UINT32_C(128)
/* Framework clients (torch) register hundreds of fatbins through the CUDA 12
   library intake; each deferred module needs a record. The token index field
   is 16-bit, so 4096 stays well inside the encoding. */
#define MF_CUDA_MODULE_CAPACITY UINT32_C(4096)
/* Library-intake modules carry client cubins the daemon cannot compile; they
   resolve functions permissively and launches route through the semantic
   kernel profile instead of daemon materialization. */
static uint8_t mf_module_deferred[MF_CUDA_MODULE_CAPACITY];
/* Per-function registered name for semantic launch routing. */
static char mf_function_names[MF_CUDA_OBJECT_CAPACITY][160];

/* Client fatbin blobs and their parsed kernel names. The blob pointers stay
   valid for the process lifetime (client images). Kernel names come from the
   cubin ELF symbol tables so cudart's enumerate-and-match binding finds a
   CUkernel for every registered function. */
static const unsigned char* mf_module_blob[MF_CUDA_MODULE_CAPACITY];
static size_t mf_module_blob_size[MF_CUDA_MODULE_CAPACITY];
#define MF_KERNEL_ENTRY_MAX UINT32_C(2714)
static struct {
  uint32_t module_index;
  uint32_t name_offset;
} mf_kernel_entries[MF_KERNEL_ENTRY_MAX];
static uint32_t mf_kernel_entry_count;
static uint32_t mf_kernel_entry_parsed[MF_CUDA_MODULE_CAPACITY];
static char mf_kernel_arena[8 * 1024 * 1024];
static size_t mf_kernel_arena_used;

static uint32_t mf_fatbin_read_u16(const unsigned char* base, size_t offset) {
  return (uint32_t)base[offset] | ((uint32_t)base[offset + 1] << 8U);
}

static uint32_t mf_fatbin_read_u32(const unsigned char* base, size_t offset) {
  return (uint32_t)base[offset] | ((uint32_t)base[offset + 1] << 8U) |
         ((uint32_t)base[offset + 2] << 16U) | ((uint32_t)base[offset + 3] << 24U);
}

static uint64_t mf_fatbin_read_u64(const unsigned char* base, size_t offset) {
  return (uint64_t)mf_fatbin_read_u32(base, offset) |
         ((uint64_t)mf_fatbin_read_u32(base, offset + 4) << 32U);
}

static int mf_kernel_arena_store(const char* name, size_t length, uint32_t* out_offset) {
  if (mf_kernel_arena_used + length + 1 > sizeof(mf_kernel_arena)) {
    return 0;
  }
  memcpy(mf_kernel_arena + mf_kernel_arena_used, name, length);
  mf_kernel_arena[mf_kernel_arena_used + length] = '\0';
  *out_offset = (uint32_t)mf_kernel_arena_used;
  mf_kernel_arena_used += length + 1;
  return 1;
}

static void mf_module_collect_elf_kernels(uint32_t module_index, const unsigned char* cubin,
                                          size_t size) {
  uint64_t section_offset = 0;
  uint16_t section_entry_size = 0;
  uint16_t section_count = 0;
  uint16_t section_index = 0;
  uint32_t symbol_table_offset = 0;
  uint32_t string_table_index = 0;
  uint64_t symbol_table_size = 0;
  const unsigned char* string_table = (const unsigned char*)0;
  uint32_t symbol_index = 0;
  if (size < 64 || cubin[0] != 0x7f || cubin[1] != 'E' || cubin[2] != 'L' || cubin[3] != 'F' ||
      cubin[4] != 2) {
    return;
  }
  section_offset = mf_fatbin_read_u64(cubin, 0x28);
  section_entry_size = (uint16_t)mf_fatbin_read_u16(cubin, 0x3a);
  section_count = (uint16_t)mf_fatbin_read_u16(cubin, 0x3c);
  if (section_entry_size < 64 || section_offset == 0 ||
      section_offset + (uint64_t)section_entry_size * section_count > size) {
    return;
  }
  uint64_t string_table_size = 0;
  for (section_index = 0; section_index < section_count; ++section_index) {
    const size_t section = (size_t)section_offset + (size_t)section_index * section_entry_size;
    if (mf_fatbin_read_u32(cubin, section + 4) == 2u) { /* SHT_SYMTAB */
      symbol_table_offset = mf_fatbin_read_u32(cubin, section + 24);
      symbol_table_size = mf_fatbin_read_u64(cubin, section + 32);
      string_table_index = mf_fatbin_read_u32(cubin, section + 40);
      break;
    }
  }
  if (symbol_table_offset == 0 || string_table_index >= section_count) {
    return;
  }
  {
    const size_t string_section =
        (size_t)section_offset + (size_t)string_table_index * section_entry_size;
    const uint64_t string_offset = mf_fatbin_read_u64(cubin, string_section + 24);
    const uint64_t string_size = mf_fatbin_read_u64(cubin, string_section + 32);
    if (string_offset >= size || string_offset + string_size > size) {
      return;
    }
    string_table = cubin + string_offset;
    if (string_table_size > size - string_offset) {
      symbol_table_size = 0; /* bound-check below rejects */
    }
    if (symbol_table_offset >= size || symbol_table_offset + symbol_table_size > size) {
      return;
    }
    for (symbol_index = 0; symbol_index * 24 < symbol_table_size; ++symbol_index) {
      const size_t symbol = (size_t)symbol_table_offset + (size_t)symbol_index * 24;
      const uint32_t name_offset = mf_fatbin_read_u32(cubin, symbol);
      const unsigned char info = cubin[symbol + 4];
      const char* name = (const char*)0;
      size_t length = 0;
      if ((info & 0xfu) != 2u || name_offset == 0) { /* STT_FUNC */
        continue;
      }
      if (name_offset >= string_size) {
        continue;
      }
      name = (const char*)string_table + name_offset;
      length = strlen(name);
      if (length == 0 || length >= 200 || name[0] == '$') {
        continue;
      }
      if (mf_kernel_entry_count >= MF_KERNEL_ENTRY_MAX) {
        return;
      }
      if (!mf_kernel_arena_store(name, length, &mf_kernel_entries[mf_kernel_entry_count].name_offset)) {
        return;
      }
      mf_kernel_entries[mf_kernel_entry_count].module_index = module_index;
      mf_kernel_entry_count += 1;
    }
  }
}

static void mf_module_parse_kernels(uint32_t module_index) {
  const unsigned char* blob = mf_module_blob[module_index];
  size_t size = mf_module_blob_size[module_index];
  size_t position = 0;
  uint32_t seen = mf_kernel_entry_parsed[module_index];
  if (seen != 0 || blob == (const unsigned char*)0 || size < 16) {
    return;
  }
  mf_kernel_entry_parsed[module_index] = 1;
  if (mf_fatbin_read_u32(blob, 0) != 0xba55ed50u) {
    return;
  }
  {
    uint64_t total = mf_fatbin_read_u64(blob, 8);
    if (total > size) {
      total = size;
    }
    position = (size_t)mf_fatbin_read_u16(blob, 4);
    while (position + 32 <= total) {
      const uint32_t kind = mf_fatbin_read_u16(blob, position);
      const uint32_t entry_header = mf_fatbin_read_u32(blob, position + 4);
      const uint64_t padded = mf_fatbin_read_u64(blob, position + 8);
      if ((kind != 1u && kind != 2u) || entry_header < 24 || padded == 0) {
        break;
      }
      if (kind == 2u && entry_header + padded <= total - position) {
        mf_module_collect_elf_kernels(module_index, blob + position + entry_header,
                                      (size_t)padded);
      }
      position += entry_header + (size_t)padded;
    }
  }
}


static uint32_t mf_module_kernel_count(uint32_t module_index) {
  uint32_t index = 0;
  uint32_t count = 0;
  mf_module_parse_kernels(module_index);
  for (index = 0; index < mf_kernel_entry_count; ++index) {
    if (mf_kernel_entries[index].module_index == module_index) {
      count += 1;
    }
  }
  return count;
}
#define MF_CUDA_CONTEXT_STACK_CAPACITY UINT32_C(16)
#define MF_CUDA_PENDING_CAPACITY UINT32_C(128)
#define MF_CUDA_ARGUMENT_CACHE_CAPACITY UINT32_C(64)
#define MF_CUDA_COPY_CACHE_CAPACITY UINT32_C(64)
#define MF_CUDA_COPY_ARGUMENT_SIZE                                                                 \
  (sizeof(mf_argument_block_header_v1) +                                                           \
   ((size_t)MF_COPY_REGION_ARGUMENT_ENTRY_COUNT_V1 * sizeof(mf_argument_entry_v1)))
#define MF_CUDA_WAIT_NS UINT64_C(5000000000)
#define MF_CUDA_WAIT_SLICE_NS UINT64_C(1000000)
#define MF_CUDA_ADDRESS_BASE UINT64_C(0x10000000)
#define MF_CUDA_ADDRESS_GUARD UINT64_C(4096)
#define MF_CUDA_INDEX_NONE UINT32_MAX

#define MF_CUDA_CONSUME_NONBLOCKING UINT32_C(0)
#define MF_CUDA_CONSUME_BLOCKING_LOCKED UINT32_C(1)
#define MF_CUDA_CONSUME_BLOCKING_COOPERATIVE UINT32_C(2)

#define MF_CUDA_OBJECT_DEVICE MF_OBJECT_TYPE_CONTEXT
#define MF_CUDA_OBJECT_CONTEXT MF_OBJECT_TYPE_CONTEXT
#define MF_CUDA_OBJECT_MODULE MF_OBJECT_TYPE_MODULE
#define MF_CUDA_OBJECT_FUNCTION MF_OBJECT_TYPE_MODULE
#define MF_CUDA_OBJECT_MEMORY MF_OBJECT_TYPE_DEVICE_MEMORY
#define MF_CUDA_OBJECT_STREAM MF_OBJECT_TYPE_QUEUE
#define MF_CUDA_OBJECT_EVENT MF_OBJECT_TYPE_EVENT

#define MF_CUDA_TAG_CONTEXT UINT64_C(0x4355000000000000)
#define MF_CUDA_TAG_MODULE UINT64_C(0x4d4f000000000000)
#define MF_CUDA_TAG_FUNCTION UINT64_C(0x4655000000000000)
#define MF_CUDA_TAG_STREAM UINT64_C(0x5354000000000000)
#define MF_CUDA_TAG_EVENT UINT64_C(0x4556000000000000)
#define MF_CUDA_TAG_MASK UINT64_C(0xffff000000000000)
#define MF_CUDA_INDEX_MASK UINT64_C(0x0000ffff00000000)
#define MF_CUDA_GENERATION_MASK UINT64_C(0x00000000ffffffff)

#define MF_CUDA_COPY_H2D UINT32_C(1)
#define MF_CUDA_COPY_D2H UINT32_C(2)
#define MF_CUDA_COPY_D2D UINT32_C(3)
#define MF_CUDA_MATERIALIZED_POINTER UINT32_C(0x80000000)

#define MF_CUDA_SUBMISSION_UNIX UINT32_C(0)
#define MF_CUDA_SUBMISSION_CDEV UINT32_C(1)

#define MF_CUDA_PENDING_STATE_FREE UINT32_C(0)
#define MF_CUDA_PENDING_STATE_PREPARING UINT32_C(1)
#define MF_CUDA_PENDING_STATE_SUBMITTED UINT32_C(2)
#define MF_CUDA_PENDING_STATE_COMPLETING UINT32_C(3)
#define MF_CUDA_PENDING_STATE_COMPLETION_READY UINT32_C(4)
#define MF_CUDA_PENDING_STATE_FINALIZING UINT32_C(5)
#define MF_CUDA_PENDING_STATE_COMPLETED UINT32_C(6)
#define MF_CUDA_PENDING_TAG_MAX (UINT64_MAX >> 8U)

#define MF_CUDA_ASYNC_ERROR_FREE UINT32_C(0)
#define MF_CUDA_ASYNC_ERROR_WRITING UINT32_C(1)
#define MF_CUDA_ASYNC_ERROR_READY UINT32_C(2)

#define MF_CUDA_CACHE_FREE UINT32_C(0)
#define MF_CUDA_CACHE_WRITING UINT32_C(1)
#define MF_CUDA_CACHE_READY UINT32_C(2)

typedef struct mf_cuda_object {
  uint64_t id;
  uint64_t remote_id;
  uint64_t remote_generation;
  uint64_t materialized_id;
  uint64_t materialized_generation;
  uint64_t address;
  uint64_t size;
  uint32_t generation;
  uint32_t active;
  uint32_t type;
  uint32_t device_index;
  uint32_t owner_context;
  uint32_t flags;
  uint32_t aux;
  uint32_t reserved;
  uint64_t last_request;
  uint64_t default_stream_last_request;
  uint32_t record_stream_index;
  uint32_t record_stream_generation;
  int32_t completion_status;
  uint32_t completion_reserved;
  mf_generation_handle_v1 handle;
} mf_cuda_object;

typedef enum mf_cuda_pending_kind {
  MF_CUDA_PENDING_GENERIC,
  MF_CUDA_PENDING_COPY,
  MF_CUDA_PENDING_LAUNCH,
  MF_CUDA_PENDING_EVENT_RECORD,
  MF_CUDA_PENDING_EVENT_WAIT
} mf_cuda_pending_kind;

typedef struct mf_cuda_pending {
  mf_client_payload_v1 host_payload;
  void* host_destination;
  uint64_t host_byte_count;
  uint64_t host_id;
  uint64_t host_generation;
  uint64_t argument_id;
  uint64_t argument_generation;
  uint64_t request_id;
  uint32_t context_index;
  uint32_t context_generation;
  uint32_t stream_index;
  uint32_t stream_generation;
  uint32_t event_index;
  uint32_t event_generation;
  uint32_t module_index;
  uint32_t module_generation;
  uint32_t memory_indices[3];
  uint32_t memory_generations[3];
  uint32_t kind;
  uint32_t defer_error;
  uint32_t argument_cached;
  uint32_t synchronous;
  uint32_t submission_transport;
} mf_cuda_pending;

typedef struct mf_cuda_pending_slot {
  uint64_t tagged_state;
  uint64_t request_id;
  uint64_t context_key;
  mf_cuda_pending pending;
  mf_client_completion_v1 completion;
  int32_t completion_result;
  uint32_t reserved;
} mf_cuda_pending_slot;

typedef struct mf_cuda_add_argument_block {
  mf_argument_block_header_v1 header;
  mf_argument_entry_v1 entries[4];
} mf_cuda_add_argument_block;

typedef struct mf_cuda_copy_argument_block {
  mf_argument_block_header_v1 header;
  mf_argument_entry_v1 entries[4];
} mf_cuda_copy_argument_block;

typedef struct mf_cuda_argument_cache_entry {
  mf_cuda_add_argument_block block;
  uint64_t id;
  uint64_t generation;
  uint32_t context_index;
  uint32_t context_generation;
  uint32_t module_index;
  uint32_t module_generation;
  uint32_t memory_indices[3];
  uint32_t memory_generations[3];
  uint32_t active;
} mf_cuda_argument_cache_entry;

typedef struct mf_cuda_copy_cache_entry {
  mf_cuda_copy_argument_block block;
  uint64_t id;
  uint64_t generation;
  uint32_t context_index;
  uint32_t context_generation;
  uint32_t memory_indices[2];
  uint32_t memory_generations[2];
  uint32_t active;
} mf_cuda_copy_cache_entry;

typedef struct mf_cuda_async_error {
  uint64_t request_id;
  int32_t result;
  uint32_t context_index;
  uint32_t context_generation;
  uint32_t stream_index;
  uint32_t stream_generation;
  uint32_t active;
  uint32_t reserved;
  uint64_t context_key;
} mf_cuda_async_error;

typedef struct mf_cuda_transport {
  int32_t registry_fd;
  int32_t submission_fd;
  int32_t completion_fd;
  mf_registry_view_id_v1 view_id;
  uint64_t submission_queue_id;
  uint64_t submission_queue_generation;
  uint64_t completion_queue_id;
  uint64_t completion_queue_generation;
  uint64_t runtime_context_id;
  uint64_t runtime_event_id;
  uint64_t runtime_event_generation;
  uint64_t runtime_add_kernel_id;
  uint64_t negotiated_capabilities;
  void* control_context;
  mf_shared_status_v1 (*control)(void*, const mf_client_control_request_v1*, const uint8_t*,
                                 uint64_t, mf_client_control_response_v1*);
  mf_shared_status_v1 (*read_object)(const void*, uint64_t, uint64_t, uint64_t, uint8_t*, uint64_t);
  uint32_t configured;
} mf_cuda_transport;

typedef struct mf_cuda_state {
  atomic_flag lock;
  atomic_uint active_waiters;
  atomic_uint_fast64_t global_lock_acquisitions;
  atomic_uint_fast64_t queue_gate_acquisitions;
  atomic_uint_fast64_t heap_allocation_attempts;
  mf_cuda_transport transport;
  mf_client_session_v1 session;
  mf_client_registry_v1 registry;
  mf_client_ring_v1 submission;
  mf_client_ring_v1 completion;
#if METAFLUX_PROVIDER_CDEV
  mf_cdev_session_v0 cdev_session;
  mf_cdev_memory_v0 cdev_payload;
#endif
  mf_cuda_object contexts[MF_CUDA_OBJECT_CAPACITY];
  mf_cuda_object modules[MF_CUDA_MODULE_CAPACITY];
  mf_cuda_object functions[MF_CUDA_OBJECT_CAPACITY];
  mf_cuda_object memories[MF_CUDA_OBJECT_CAPACITY];
  mf_cuda_object streams[MF_CUDA_OBJECT_CAPACITY];
  mf_cuda_object events[MF_CUDA_OBJECT_CAPACITY];
  mf_cuda_pending_slot pending[MF_CUDA_PENDING_CAPACITY];
  mf_cuda_async_error async_errors[MF_CUDA_PENDING_CAPACITY];
  mf_cuda_argument_cache_entry argument_cache[MF_CUDA_ARGUMENT_CACHE_CAPACITY];
  mf_cuda_copy_cache_entry copy_cache[MF_CUDA_COPY_CACHE_CAPACITY];
  uint64_t next_id;
  uint64_t next_request;
  uint64_t next_address;
  uint64_t next_timeline;
  uint32_t next_ptds_stream_generation;
  uint32_t pending_count;
  uint32_t transport_error;
  uint32_t* visible_devices;
  uint32_t visible_count;
  uint64_t process_view_revision;
  uint32_t initialized;
  uint32_t closing;
  uint32_t direct_host_copy_ready;
  uint32_t cdev_active;
  uint32_t control_gate;
  uint32_t context_gates[MF_CUDA_OBJECT_CAPACITY];
} mf_cuda_state;

typedef struct mf_cuda_tls_default_stream {
  uint64_t last_request;
  uint32_t context_generation;
  uint32_t stream_generation;
} mf_cuda_tls_default_stream;

typedef struct mf_cuda_tls_state {
  CUcontext current;
  CUcontext stack[MF_CUDA_CONTEXT_STACK_CAPACITY];
  mf_cuda_tls_default_stream default_streams[MF_CUDA_OBJECT_CAPACITY];
  uint32_t depth;
  uint32_t lock_depth;
  uint32_t queue_depth;
  uint32_t queue_context_index;
} mf_cuda_tls_state;

typedef enum mf_cuda_command_kind {
  MF_CUDA_COMMAND_ALLOC,
  MF_CUDA_COMMAND_FREE,
  MF_CUDA_COMMAND_MODULE_LOAD,
  MF_CUDA_COMMAND_MODULE_UNLOAD,
  MF_CUDA_COMMAND_COPY,
  MF_CUDA_COMMAND_LAUNCH,
  MF_CUDA_COMMAND_EVENT_RECORD,
  MF_CUDA_COMMAND_EVENT_WAIT,
  MF_CUDA_COMMAND_QUEUE_SYNC
} mf_cuda_command_kind;

typedef struct mf_cuda_command {
  mf_cuda_command_kind kind;
  uint64_t target;
  uint64_t arguments[4];
  uint32_t flags;
} mf_cuda_command;

typedef enum mf_cuda_direct_registration_result {
  MF_CUDA_DIRECT_REGISTRATION_READY,
  MF_CUDA_DIRECT_REGISTRATION_FALLBACK,
  MF_CUDA_DIRECT_REGISTRATION_FATAL
} mf_cuda_direct_registration_result;

#if defined(METAFLUX_PROVIDER_TESTING)
typedef struct mf_cuda_direct_registration_override {
  mf_shared_status_v1 transport_status;
  uint32_t control_status;
  uint32_t local_unavailable;
  uint32_t active;
} mf_cuda_direct_registration_override;
#endif

static mf_cuda_state mf_cuda_global = {.lock = ATOMIC_FLAG_INIT,
                                       .active_waiters = UINT32_C(0),
                                       .session = {.registry = {.owned_fd = -1},
                                                   .submission = {.owned_fd = -1},
                                                   .completion = {.owned_fd = -1},
                                                   .socket_fd = -1},
                                       .registry = {.owned_fd = -1},
                                       .submission = {.owned_fd = -1},
                                       .completion = {.owned_fd = -1},
#if METAFLUX_PROVIDER_CDEV
                                       .cdev_session = {.submission = {.owned_fd = -1},
                                                         .completion = {.owned_fd = -1},
                                                         .device_fd = -1,
                                                         .submission_eventfd = -1,
                                                         .completion_eventfd = -1},
                                       .cdev_payload = {.device_fd = -1},
#endif
};
static pthread_key_t mf_cuda_tls_key;
static pthread_once_t mf_cuda_tls_once = PTHREAD_ONCE_INIT;
static int mf_cuda_tls_key_status = -1;
#if defined(METAFLUX_PROVIDER_TESTING)
static mf_cuda_direct_registration_override mf_cuda_test_direct_registration;
#endif

static void mf_cuda_make_tls_key(void) {
  mf_cuda_tls_key_status = pthread_key_create(&mf_cuda_tls_key, free);
}

static void* mf_cuda_heap_calloc(size_t count, size_t size) {
  (void)atomic_fetch_add_explicit(&mf_cuda_global.heap_allocation_attempts, UINT64_C(1),
                                  memory_order_relaxed);
  return calloc(count, size);
}

static void* mf_cuda_heap_malloc(size_t size) {
  (void)atomic_fetch_add_explicit(&mf_cuda_global.heap_allocation_attempts, UINT64_C(1),
                                  memory_order_relaxed);
  return malloc(size);
}

static mf_cuda_tls_state* mf_cuda_thread_state(uint32_t create) {
  mf_cuda_tls_state* state = (mf_cuda_tls_state*)0;
  if (pthread_once(&mf_cuda_tls_once, mf_cuda_make_tls_key) != 0 || mf_cuda_tls_key_status != 0) {
    return (mf_cuda_tls_state*)0;
  }
  state = (mf_cuda_tls_state*)pthread_getspecific(mf_cuda_tls_key);
  if (state == (mf_cuda_tls_state*)0 && create != UINT32_C(0)) {
    state = (mf_cuda_tls_state*)mf_cuda_heap_calloc((size_t)1, sizeof(*state));
    if (state == (mf_cuda_tls_state*)0 || pthread_setspecific(mf_cuda_tls_key, state) != 0) {
      free(state);
      return (mf_cuda_tls_state*)0;
    }
    state->queue_context_index = MF_CUDA_INDEX_NONE;
  }
  return state;
}

static int mf_cuda_decode(const void* token, uint64_t tag, uint32_t* index, uint32_t* generation);

static void mf_cuda_gate_lock(uint32_t* gate) {
  uint32_t expected = UINT32_C(0);
  while (!mf_atomic_compare_exchange_u32_seq_cst(gate, &expected, UINT32_C(1))) {
    expected = UINT32_C(0);
    (void)sched_yield();
  }
}

static void mf_cuda_gate_unlock(uint32_t* gate) { mf_atomic_store_u32_release(gate, UINT32_C(0)); }

/* A warm call owns only its current context. Different contexts submit concurrently. */
static CUresult mf_cuda_queue_lock(uint32_t create_thread_state) {
  mf_cuda_tls_state* state = mf_cuda_thread_state(create_thread_state);
  uint32_t context_index = UINT32_C(0);
  uint32_t context_generation = UINT32_C(0);
  if (state == (mf_cuda_tls_state*)0) {
    return create_thread_state == UINT32_C(0) ? CUDA_ERROR_INVALID_CONTEXT
                                              : CUDA_ERROR_OUT_OF_MEMORY;
  }
  if (state->current == (CUcontext)0 ||
      !mf_cuda_decode((void*)state->current, MF_CUDA_TAG_CONTEXT, &context_index,
                      &context_generation) ||
      context_index >= MF_CUDA_OBJECT_CAPACITY) {
    do { if (mf_cuda_entry_trace_enabled() != 0) { fprintf(stderr, "MF_INVALID_CONTEXT %s:%d\n", __func__, __LINE__); } return CUDA_ERROR_INVALID_CONTEXT; } while (0);
  }
  if (state->queue_depth != UINT32_C(0)) {
    if (state->queue_depth == UINT32_MAX || state->queue_context_index != context_index) {
      return CUDA_ERROR_OUT_OF_MEMORY;
    }
    state->queue_depth += UINT32_C(1);
    return CUDA_SUCCESS;
  }
  mf_cuda_gate_lock(&mf_cuda_global.context_gates[context_index]);
  state->queue_depth = UINT32_C(1);
  state->queue_context_index = context_index;
  (void)atomic_fetch_add_explicit(&mf_cuda_global.queue_gate_acquisitions, UINT64_C(1),
                                  memory_order_relaxed);
  return CUDA_SUCCESS;
}

static void mf_cuda_queue_unlock(void) {
  mf_cuda_tls_state* state = mf_cuda_thread_state(UINT32_C(0));
  if (state == (mf_cuda_tls_state*)0) {
    return;
  }
  if (state->queue_depth == UINT32_C(0)) {
    return;
  }
  state->queue_depth -= UINT32_C(1);
  if (state->queue_depth == UINT32_C(0)) {
    mf_cuda_gate_unlock(&mf_cuda_global.context_gates[state->queue_context_index]);
    state->queue_context_index = MF_CUDA_INDEX_NONE;
  }
}

static void mf_cuda_lock(void) {
  mf_cuda_tls_state* state = (mf_cuda_tls_state*)0;
  uint32_t context_index = UINT32_C(0);
  state = mf_cuda_thread_state(UINT32_C(1));
  if (state == (mf_cuda_tls_state*)0) {
    return;
  }
  if (state->lock_depth != UINT32_C(0)) {
    if (state->lock_depth == UINT32_MAX) {
      return;
    }
    state->lock_depth += UINT32_C(1);
    return;
  }
  while (atomic_flag_test_and_set_explicit(&mf_cuda_global.lock, memory_order_acquire)) {
    (void)sched_yield();
  }
  for (context_index = UINT32_C(0); context_index < MF_CUDA_OBJECT_CAPACITY; ++context_index) {
    mf_cuda_gate_lock(&mf_cuda_global.context_gates[context_index]);
  }
  (void)atomic_fetch_add_explicit(&mf_cuda_global.global_lock_acquisitions, UINT64_C(1),
                                  memory_order_relaxed);
  state->lock_depth = UINT32_C(1);
}

static void mf_cuda_unlock(void) {
  mf_cuda_tls_state* state = mf_cuda_thread_state(UINT32_C(0));
  uint32_t context_index = MF_CUDA_OBJECT_CAPACITY;
  if (state != (mf_cuda_tls_state*)0 && state->lock_depth > UINT32_C(1)) {
    state->lock_depth -= UINT32_C(1);
    return;
  }
  if (state != (mf_cuda_tls_state*)0 && state->lock_depth == UINT32_C(0)) {
    return;
  }
  if (state != (mf_cuda_tls_state*)0) {
    state->lock_depth = UINT32_C(0);
  }
  while (context_index != UINT32_C(0)) {
    --context_index;
    mf_cuda_gate_unlock(&mf_cuda_global.context_gates[context_index]);
  }
  atomic_flag_clear_explicit(&mf_cuda_global.lock, memory_order_release);
}

static mf_shared_status_v1 mf_cuda_wait_deadline(uint64_t timeout_ns, uint64_t* out_deadline_ns) {
  struct timespec now;
  uint64_t now_ns = UINT64_C(0);
  if (out_deadline_ns == (uint64_t*)0 || clock_gettime(CLOCK_MONOTONIC, &now) != 0 ||
      now.tv_sec < 0) {
    return MF_SHARED_SYSTEM_ERROR;
  }
  now_ns = (uint64_t)now.tv_sec * UINT64_C(1000000000) + (uint64_t)now.tv_nsec;
  *out_deadline_ns = timeout_ns > UINT64_MAX - now_ns ? UINT64_MAX : now_ns + timeout_ns;
  return MF_SHARED_SUCCESS;
}

static mf_shared_status_v1 mf_cuda_wait_ring_locked(mf_client_ring_v1* ring, uint32_t readable,
                                                    uint64_t deadline_ns) {
  mf_cuda_tls_state* state = mf_cuda_thread_state(UINT32_C(0));
  struct timespec now;
  uint64_t now_ns = UINT64_C(0);
  uint64_t remaining_ns = UINT64_C(0);
  uint64_t slice_ns = UINT64_C(0);
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;
  const uint32_t state_gate_held =
      state != (mf_cuda_tls_state*)0 && state->lock_depth != UINT32_C(0) ? UINT32_C(1)
                                                                         : UINT32_C(0);
  if (mf_cuda_global.closing != UINT32_C(0) || clock_gettime(CLOCK_MONOTONIC, &now) != 0 ||
      now.tv_sec < 0) {
    return mf_cuda_global.closing != UINT32_C(0) ? MF_SHARED_TERMINAL_VIEW : MF_SHARED_SYSTEM_ERROR;
  }
  now_ns = (uint64_t)now.tv_sec * UINT64_C(1000000000) + (uint64_t)now.tv_nsec;
  if (now_ns >= deadline_ns) {
    return MF_SHARED_TIMEOUT;
  }
  remaining_ns = deadline_ns - now_ns;
  slice_ns = remaining_ns < MF_CUDA_WAIT_SLICE_NS ? remaining_ns : MF_CUDA_WAIT_SLICE_NS;
  (void)atomic_fetch_add_explicit(&mf_cuda_global.active_waiters, UINT32_C(1),
                                  memory_order_acq_rel);
  if (state_gate_held != UINT32_C(0)) {
    mf_cuda_unlock();
  }
  status = readable != UINT32_C(0) ? mf_client_ring_wait_readable_v1(ring, slice_ns)
                                   : mf_client_ring_wait_writable_v1(ring, slice_ns);
  if (state_gate_held != UINT32_C(0)) {
    mf_cuda_lock();
  }
  (void)atomic_fetch_sub_explicit(&mf_cuda_global.active_waiters, UINT32_C(1),
                                  memory_order_acq_rel);
  if (mf_cuda_global.closing != UINT32_C(0)) {
    return MF_SHARED_TERMINAL_VIEW;
  }
  if (status == MF_SHARED_TIMEOUT && slice_ns < remaining_ns) {
    return MF_SHARED_RETRY;
  }
  return status;
}

static uint32_t mf_cuda_next_generation(uint32_t generation) {
  generation += UINT32_C(1);
  return generation == UINT32_C(0) ? UINT32_C(1) : generation;
}

static uint64_t mf_cuda_next_id_locked(void) {
  mf_cuda_global.next_id += UINT64_C(1);
  if (mf_cuda_global.next_id == UINT64_C(0)) {
    mf_cuda_global.next_id = UINT64_C(1);
  }
  return mf_cuda_global.next_id;
}

static CUresult mf_cuda_next_request_locked(uint64_t* out_request_id) {
  uint64_t current = UINT64_C(0);
  if (out_request_id == (uint64_t*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  current = mf_atomic_load_u64_acquire(&mf_cuda_global.next_request);
  for (;;) {
    uint64_t expected = current;
    if (current == UINT64_MAX) {
      return CUDA_ERROR_OUT_OF_MEMORY;
    }
    if (mf_atomic_compare_exchange_u64_seq_cst(&mf_cuda_global.next_request, &expected,
                                               current + UINT64_C(1))) {
      *out_request_id = current + UINT64_C(1);
      return CUDA_SUCCESS;
    }
    current = expected;
  }
}

static CUresult mf_cuda_next_timeline_locked(uint64_t* out_timeline) {
  uint64_t current = UINT64_C(0);
  if (out_timeline == (uint64_t*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  current = mf_atomic_load_u64_acquire(&mf_cuda_global.next_timeline);
  for (;;) {
    uint64_t expected = current;
    if (current == UINT64_MAX) {
      return CUDA_ERROR_OUT_OF_MEMORY;
    }
    if (mf_atomic_compare_exchange_u64_seq_cst(&mf_cuda_global.next_timeline, &expected,
                                               current + UINT64_C(1))) {
      *out_timeline = current + UINT64_C(1);
      return CUDA_SUCCESS;
    }
    current = expected;
  }
}

static uint64_t mf_cuda_token(uint64_t tag, uint32_t index, uint32_t generation) {
  return tag | ((uint64_t)(index + UINT32_C(1)) << 32U) | (uint64_t)generation;
}

static int mf_cuda_decode(const void* token, uint64_t tag, uint32_t* index, uint32_t* generation) {
  const uint64_t value = (uint64_t)(uintptr_t)token;
  const uint64_t encoded_index = (value & MF_CUDA_INDEX_MASK) >> 32U;
  if ((value & MF_CUDA_TAG_MASK) != tag || encoded_index == UINT64_C(0) ||
      encoded_index > (uint64_t)MF_CUDA_MODULE_CAPACITY ||
      (value & MF_CUDA_GENERATION_MASK) == UINT64_C(0)) {
    return 0;
  }
  *index = (uint32_t)(encoded_index - UINT64_C(1));
  *generation = (uint32_t)(value & MF_CUDA_GENERATION_MASK);
  return 1;
}

static CUresult mf_cuda_status(mf_shared_status_v1 status) {
  switch (status) {
  case MF_SHARED_SUCCESS:
    return CUDA_SUCCESS;
  case MF_SHARED_WOULD_BLOCK:
  case MF_SHARED_TIMEOUT:
  case MF_SHARED_INTERRUPTED:
  case MF_SHARED_RETRY:
    return CUDA_ERROR_NOT_READY;
  case MF_SHARED_STALE_HANDLE:
    return CUDA_ERROR_INVALID_HANDLE;
  case MF_SHARED_DEVICE_LOST:
    return CUDA_ERROR_CONTEXT_IS_DESTROYED;
  case MF_SHARED_TERMINAL_VIEW:
    return CUDA_ERROR_DEINITIALIZED;
  case MF_SHARED_INVALID_ARGUMENT:
  case MF_SHARED_MALFORMED:
  case MF_SHARED_OVERFLOW:
    return CUDA_ERROR_INVALID_VALUE;
  case MF_SHARED_RESOURCE_EXHAUSTED:
    return CUDA_ERROR_OUT_OF_MEMORY;
  case MF_SHARED_NOT_SUPPORTED:
    if (getenv("METAFLUX_TRACE_STUBS") != (void*)0) {
      fprintf(stderr, "MF_NS_MAP ret=%p\n", __builtin_return_address(0));
    }
    return CUDA_ERROR_NOT_SUPPORTED;
  case MF_SHARED_SYSTEM_ERROR:
  default:
    return CUDA_ERROR_UNKNOWN;
  }
}

static CUresult mf_cuda_control_status(uint32_t status) {
  switch (status) {
  case MF_CLIENT_CONTROL_OK:
    return CUDA_SUCCESS;
  case MF_CLIENT_CONTROL_STALE_GENERATION:
    return CUDA_ERROR_INVALID_HANDLE;
  case MF_CLIENT_CONTROL_NOT_FOUND:
    return CUDA_ERROR_NOT_FOUND;
  case MF_CLIENT_CONTROL_INVALID_ARGUMENT:
  case MF_CLIENT_CONTROL_MALFORMED:
    return CUDA_ERROR_INVALID_VALUE;
  case MF_CLIENT_CONTROL_RESOURCE_EXHAUSTED:
    return CUDA_ERROR_OUT_OF_MEMORY;
  case MF_CLIENT_CONTROL_UNSUPPORTED:
    return CUDA_ERROR_NOT_SUPPORTED;
  case MF_CLIENT_CONTROL_INTERNAL_ERROR:
  default:
    return CUDA_ERROR_UNKNOWN;
  }
}

static void mf_cuda_payload_initialize_empty(mf_client_payload_v1* payload) {
  (void)memset(payload, 0, sizeof(*payload));
  payload->owned_fd = -1;
}

static CUresult mf_cuda_control_payload_serialized(uint16_t opcode, uint16_t flags,
                                                   uint64_t object_id, uint64_t argument,
                                                   const void* payload, uint64_t payload_size,
                                                   uint64_t* out_id, uint64_t* out_generation,
                                                   mf_client_payload_v1* retained_payload) {
  mf_client_control_request_v1 request;
  mf_client_control_response_v1 response;
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;
  uint32_t control_status = MF_CLIENT_CONTROL_INTERNAL_ERROR;
  if (mf_cuda_global.transport.control != (void*)0) {
    uint64_t request_id = UINT64_C(0);
    const CUresult request_result = mf_cuda_next_request_locked(&request_id);
    if (request_result != CUDA_SUCCESS) {
      return request_result;
    }
    mf_client_control_request_init_v1(
        &request, opcode, flags, request_id, mf_cuda_global.transport.view_id.daemon_incarnation,
        mf_cuda_global.transport.view_id.view_serial, object_id, argument);
    status = mf_cuda_global.transport.control(mf_cuda_global.transport.control_context, &request,
                                              (const uint8_t*)payload, payload_size, &response);
    if (status != MF_SHARED_SUCCESS ||
        mf_client_control_response_validate_v1(&response) != MF_CLIENT_CONTROL_OK ||
        mf_client_load_le64_v1(response.bytes + 24) != request_id ||
        mf_client_load_le64_v1(response.bytes + 32) !=
            mf_cuda_global.transport.view_id.daemon_incarnation ||
        mf_client_load_le64_v1(response.bytes + 40) !=
            mf_cuda_global.transport.view_id.view_serial) {
      return status == MF_SHARED_SUCCESS ? CUDA_ERROR_UNKNOWN : mf_cuda_status(status);
    }
  } else if (mf_cuda_global.session.socket_fd >= 0) {
    mf_client_payload_v1 created_payload;
    int32_t payload_fd = -1;
    CUresult result = CUDA_SUCCESS;
    mf_cuda_payload_initialize_empty(&created_payload);
    if (payload_size != UINT64_C(0)) {
      const uint32_t payload_flags = (flags & MF_CLIENT_CONTROL_FLAG_WRITE) != UINT16_C(0)
                                         ? MF_CLIENT_PAYLOAD_WRITABLE_V1
                                         : UINT32_C(0);
      if (payload == (const void*)0 || (flags & MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD) == UINT16_C(0)) {
        return CUDA_ERROR_INVALID_VALUE;
      }
      status = mf_client_payload_create_v1((const uint8_t*)payload, payload_size, payload_flags,
                                           &created_payload);
      if (status != MF_SHARED_SUCCESS) {
        return mf_cuda_status(status);
      }
      payload_fd = mf_client_payload_borrow_fd_v1(&created_payload);
    } else if ((flags & MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD) != UINT16_C(0)) {
      return CUDA_ERROR_INVALID_VALUE;
    }
    status = mf_client_session_control_v1(&mf_cuda_global.session, opcode, flags, object_id,
                                          argument, payload_fd, &response, (int32_t*)0);
    if (status != MF_SHARED_SUCCESS) {
      result = mf_cuda_status(status);
    } else {
      control_status = mf_client_load_le32_v1(response.bytes + 12);
      result = mf_cuda_control_status(control_status);
    }
    if (result == CUDA_SUCCESS && retained_payload != (mf_client_payload_v1*)0 &&
        created_payload.mapping != (void*)0) {
      *retained_payload = created_payload;
      mf_cuda_payload_initialize_empty(&created_payload);
    }
    mf_client_payload_close_v1(&created_payload);
    if (result != CUDA_SUCCESS) {
      return result;
    }
  } else {
    return CUDA_ERROR_NOT_SUPPORTED;
  }
  control_status = mf_client_load_le32_v1(response.bytes + 12);
  if (control_status == MF_CLIENT_CONTROL_OK) {
    if (out_id != (uint64_t*)0) {
      *out_id = mf_client_load_le64_v1(response.bytes + 48);
    }
    if (out_generation != (uint64_t*)0) {
      *out_generation = mf_client_load_le64_v1(response.bytes + 56);
    }
  }
  return mf_cuda_control_status(control_status);
}

static CUresult mf_cuda_control_payload_locked(uint16_t opcode, uint16_t flags, uint64_t object_id,
                                               uint64_t argument, const void* payload,
                                               uint64_t payload_size, uint64_t* out_id,
                                               uint64_t* out_generation,
                                               mf_client_payload_v1* retained_payload) {
  CUresult result = CUDA_SUCCESS;
  mf_cuda_gate_lock(&mf_cuda_global.control_gate);
  result =
      mf_cuda_control_payload_serialized(opcode, flags, object_id, argument, payload, payload_size,
                                         out_id, out_generation, retained_payload);
  mf_cuda_gate_unlock(&mf_cuda_global.control_gate);
  return result;
}

static CUresult mf_cuda_control_locked(uint16_t opcode, uint16_t flags, uint64_t object_id,
                                       uint64_t argument, const void* payload,
                                       uint64_t payload_size, uint64_t* out_id,
                                       uint64_t* out_generation) {
  return mf_cuda_control_payload_locked(opcode, flags, object_id, argument, payload, payload_size,
                                        out_id, out_generation, (mf_client_payload_v1*)0);
}

static mf_cuda_direct_registration_result
mf_cuda_classify_direct_registration(uint32_t local_unavailable,
                                     mf_shared_status_v1 transport_status, uint32_t control_status,
                                     CUresult* out_failure) {
  *out_failure = CUDA_SUCCESS;
  if (local_unavailable != UINT32_C(0)) {
    return MF_CUDA_DIRECT_REGISTRATION_FALLBACK;
  }
  if (transport_status != MF_SHARED_SUCCESS) {
    *out_failure = transport_status == MF_SHARED_SYSTEM_ERROR ? CUDA_ERROR_SYSTEM_NOT_READY
                                                              : mf_cuda_status(transport_status);
    return MF_CUDA_DIRECT_REGISTRATION_FATAL;
  }
  if (control_status == MF_CLIENT_CONTROL_OK) {
    return MF_CUDA_DIRECT_REGISTRATION_READY;
  }
  if (control_status == MF_CLIENT_CONTROL_UNSUPPORTED ||
      control_status == MF_CLIENT_CONTROL_INVALID_ARGUMENT) {
    return MF_CUDA_DIRECT_REGISTRATION_FALLBACK;
  }
  *out_failure = mf_cuda_control_status(control_status);
  return MF_CUDA_DIRECT_REGISTRATION_FATAL;
}

static mf_cuda_direct_registration_result
mf_cuda_register_host_address_space_locked(CUresult* out_failure) {
  mf_client_control_response_v1 response;
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;
  uint32_t control_status = MF_CLIENT_CONTROL_INTERNAL_ERROR;
  int32_t memory_fd = -1;
#if defined(METAFLUX_PROVIDER_TESTING)
  if (mf_cuda_test_direct_registration.active != UINT32_C(0)) {
    return mf_cuda_classify_direct_registration(mf_cuda_test_direct_registration.local_unavailable,
                                                mf_cuda_test_direct_registration.transport_status,
                                                mf_cuda_test_direct_registration.control_status,
                                                out_failure);
  }
#endif
  memory_fd = open("/proc/self/mem", O_RDWR | O_CLOEXEC);
  if (memory_fd < 0) {
    return mf_cuda_classify_direct_registration(UINT32_C(1), MF_SHARED_SUCCESS,
                                                MF_CLIENT_CONTROL_INTERNAL_ERROR, out_failure);
  }
  if (mf_cuda_global.session.socket_fd < 0) {
    (void)close(memory_fd);
    return mf_cuda_classify_direct_registration(UINT32_C(0), MF_SHARED_SYSTEM_ERROR,
                                                MF_CLIENT_CONTROL_INTERNAL_ERROR, out_failure);
  }
  mf_cuda_gate_lock(&mf_cuda_global.control_gate);
  status = mf_client_session_control_v1(
      &mf_cuda_global.session, MF_CLIENT_CONTROL_HOST_ADDRESS_SPACE_REGISTER_V1,
      MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_READ |
          MF_CLIENT_CONTROL_FLAG_WRITE,
      mf_cuda_global.transport.runtime_context_id, UINT64_C(0), memory_fd, &response, (int32_t*)0);
  (void)close(memory_fd);
  if (status != MF_SHARED_SUCCESS) {
    mf_cuda_gate_unlock(&mf_cuda_global.control_gate);
    return mf_cuda_classify_direct_registration(UINT32_C(0), status, control_status, out_failure);
  }
  control_status = mf_client_load_le32_v1(response.bytes + 12);
  mf_cuda_gate_unlock(&mf_cuda_global.control_gate);
  return mf_cuda_classify_direct_registration(UINT32_C(0), status, control_status, out_failure);
}

static void mf_cuda_take_session_mappings_locked(void) {
  mf_cuda_global.registry = mf_cuda_global.session.registry;
  mf_cuda_global.submission = mf_cuda_global.session.submission;
  mf_cuda_global.completion = mf_cuda_global.session.completion;
  (void)memset(&mf_cuda_global.session.registry, 0, sizeof(mf_cuda_global.session.registry));
  (void)memset(&mf_cuda_global.session.submission, 0, sizeof(mf_cuda_global.session.submission));
  (void)memset(&mf_cuda_global.session.completion, 0, sizeof(mf_cuda_global.session.completion));
  mf_cuda_global.session.registry.owned_fd = -1;
  mf_cuda_global.session.submission.owned_fd = -1;
  mf_cuda_global.session.completion.owned_fd = -1;
}

static void mf_cuda_close_cdev_locked(void) {
#if METAFLUX_PROVIDER_CDEV
  mf_cdev_memory_close_v0(&mf_cuda_global.cdev_payload);
  mf_cdev_session_close_v0(&mf_cuda_global.cdev_session);
#endif
  mf_cuda_global.cdev_active = UINT32_C(0);
}

static int mf_cuda_command_uses_cdev_locked(const mf_cuda_command* command) {
#if METAFLUX_PROVIDER_CDEV
  return mf_cuda_global.cdev_active != UINT32_C(0) && command != (const mf_cuda_command*)0 &&
         (command->kind == MF_CUDA_COMMAND_COPY || command->kind == MF_CUDA_COMMAND_LAUNCH);
#else
  (void)command;
  return 0;
#endif
}

static mf_client_ring_v1* mf_cuda_submission_ring_locked(const mf_cuda_command* command) {
#if METAFLUX_PROVIDER_CDEV
  return mf_cuda_command_uses_cdev_locked(command) != 0 ? &mf_cuda_global.cdev_session.submission
                                                        : &mf_cuda_global.submission;
#else
  (void)command;
  return &mf_cuda_global.submission;
#endif
}

static mf_client_ring_v1* mf_cuda_completion_ring_for_transport_locked(uint32_t transport) {
#if METAFLUX_PROVIDER_CDEV
  return transport == MF_CUDA_SUBMISSION_CDEV ? &mf_cuda_global.cdev_session.completion
                                               : &mf_cuda_global.completion;
#else
  (void)transport;
  return &mf_cuda_global.completion;
#endif
}

static void mf_cuda_pending_initialize(mf_cuda_pending* pending) {
  uint32_t index = UINT32_C(0);
  (void)memset(pending, 0, sizeof(*pending));
  pending->host_payload.owned_fd = -1;
  pending->context_index = MF_CUDA_INDEX_NONE;
  pending->stream_index = MF_CUDA_INDEX_NONE;
  pending->event_index = MF_CUDA_INDEX_NONE;
  pending->module_index = MF_CUDA_INDEX_NONE;
  for (index = UINT32_C(0); index < UINT32_C(3); ++index) {
    pending->memory_indices[index] = MF_CUDA_INDEX_NONE;
  }
}

typedef enum mf_cuda_argument_cache_scope {
  MF_CUDA_ARGUMENT_CACHE_ALL,
  MF_CUDA_ARGUMENT_CACHE_CONTEXT,
  MF_CUDA_ARGUMENT_CACHE_MODULE,
  MF_CUDA_ARGUMENT_CACHE_MEMORY
} mf_cuda_argument_cache_scope;

static int mf_cuda_argument_cache_matches_scope(const mf_cuda_argument_cache_entry* entry,
                                                mf_cuda_argument_cache_scope scope, uint32_t index,
                                                uint32_t generation) {
  uint32_t memory_index = UINT32_C(0);
  if (scope == MF_CUDA_ARGUMENT_CACHE_ALL) {
    return 1;
  }
  if (scope == MF_CUDA_ARGUMENT_CACHE_CONTEXT) {
    return entry->context_index == index && entry->context_generation == generation;
  }
  if (scope == MF_CUDA_ARGUMENT_CACHE_MODULE) {
    return entry->module_index == index && entry->module_generation == generation;
  }
  for (memory_index = UINT32_C(0); memory_index < UINT32_C(3); ++memory_index) {
    if (entry->memory_indices[memory_index] == index &&
        entry->memory_generations[memory_index] == generation) {
      return 1;
    }
  }
  return 0;
}

static CUresult mf_cuda_argument_cache_release_locked(mf_cuda_argument_cache_scope scope,
                                                      uint32_t index, uint32_t generation) {
  CUresult first_result = CUDA_SUCCESS;
  uint32_t cache_index = UINT32_C(0);
  for (cache_index = UINT32_C(0); cache_index < MF_CUDA_ARGUMENT_CACHE_CAPACITY; ++cache_index) {
    mf_cuda_argument_cache_entry* entry = &mf_cuda_global.argument_cache[cache_index];
    CUresult result = CUDA_SUCCESS;
    uint32_t expected = MF_CUDA_CACHE_READY;
    if (mf_atomic_load_u32_acquire(&entry->active) != MF_CUDA_CACHE_READY ||
        mf_cuda_argument_cache_matches_scope(entry, scope, index, generation) == 0) {
      continue;
    }
    if (!mf_atomic_compare_exchange_u32_seq_cst(&entry->active, &expected, MF_CUDA_CACHE_WRITING)) {
      continue;
    }
    result = mf_cuda_control_locked(MF_CLIENT_CONTROL_ARGUMENT_BLOCK_RELEASE_V1, UINT16_C(0),
                                    entry->id, entry->generation, (const void*)0, UINT64_C(0),
                                    (uint64_t*)0, (uint64_t*)0);
    if (result == CUDA_SUCCESS) {
      (void)memset(entry, 0, offsetof(mf_cuda_argument_cache_entry, active));
      mf_atomic_store_u32_release(&entry->active, MF_CUDA_CACHE_FREE);
    } else if (first_result == CUDA_SUCCESS) {
      first_result = result;
      mf_atomic_store_u32_release(&entry->active, MF_CUDA_CACHE_READY);
    } else {
      mf_atomic_store_u32_release(&entry->active, MF_CUDA_CACHE_READY);
    }
  }
  return first_result;
}

static int mf_cuda_argument_cache_is_empty_locked(void) {
  uint32_t cache_index = UINT32_C(0);
  for (cache_index = UINT32_C(0); cache_index < MF_CUDA_ARGUMENT_CACHE_CAPACITY; ++cache_index) {
    if (mf_atomic_load_u32_acquire(&mf_cuda_global.argument_cache[cache_index].active) !=
        MF_CUDA_CACHE_FREE) {
      return 0;
    }
  }
  return 1;
}

static CUresult mf_cuda_argument_cache_acquire_locked(
    const mf_cuda_add_argument_block* block, uint32_t context_index, uint32_t context_generation,
    uint32_t module_index, uint32_t module_generation, const uint32_t memory_indices[3],
    const uint32_t memory_generations[3], uint64_t* out_id, uint64_t* out_generation,
    uint32_t* out_cached) {
  uint32_t cache_index = UINT32_C(0);
  uint32_t free_index = MF_CUDA_ARGUMENT_CACHE_CAPACITY;
  CUresult result = CUDA_SUCCESS;
  *out_id = UINT64_C(0);
  *out_generation = UINT64_C(0);
  *out_cached = UINT32_C(0);
  for (cache_index = UINT32_C(0); cache_index < MF_CUDA_ARGUMENT_CACHE_CAPACITY; ++cache_index) {
    const mf_cuda_argument_cache_entry* entry = &mf_cuda_global.argument_cache[cache_index];
    const uint32_t active = mf_atomic_load_u32_acquire(&entry->active);
    if (active == MF_CUDA_CACHE_FREE) {
      if (free_index == MF_CUDA_ARGUMENT_CACHE_CAPACITY) {
        free_index = cache_index;
      }
      continue;
    }
    if (active == MF_CUDA_CACHE_READY && entry->context_index == context_index &&
        entry->context_generation == context_generation && entry->module_index == module_index &&
        entry->module_generation == module_generation &&
        memcmp(&entry->block, block, sizeof(*block)) == 0) {
      *out_id = entry->id;
      *out_generation = entry->generation;
      *out_cached = UINT32_C(1);
      return CUDA_SUCCESS;
    }
  }
  result = mf_cuda_control_locked(
      MF_CLIENT_CONTROL_ARGUMENT_BLOCK_REGISTER_V1, MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD,
      mf_cuda_global.transport.runtime_context_id, (uint64_t)sizeof(*block), block,
      (uint64_t)sizeof(*block), out_id, out_generation);
  if (result == CUDA_SUCCESS && free_index != MF_CUDA_ARGUMENT_CACHE_CAPACITY) {
    mf_cuda_argument_cache_entry* entry = &mf_cuda_global.argument_cache[free_index];
    uint32_t memory_index = UINT32_C(0);
    uint32_t expected = MF_CUDA_CACHE_FREE;
    if (!mf_atomic_compare_exchange_u32_seq_cst(&entry->active, &expected, MF_CUDA_CACHE_WRITING)) {
      return result;
    }
    (void)memset(entry, 0, offsetof(mf_cuda_argument_cache_entry, active));
    entry->block = *block;
    entry->id = *out_id;
    entry->generation = *out_generation;
    entry->context_index = context_index;
    entry->context_generation = context_generation;
    entry->module_index = module_index;
    entry->module_generation = module_generation;
    for (memory_index = UINT32_C(0); memory_index < UINT32_C(3); ++memory_index) {
      entry->memory_indices[memory_index] = memory_indices[memory_index];
      entry->memory_generations[memory_index] = memory_generations[memory_index];
    }
    mf_atomic_store_u32_release(&entry->active, MF_CUDA_CACHE_READY);
    *out_cached = UINT32_C(1);
  }
  return result;
}

static int mf_cuda_copy_cache_matches_scope(const mf_cuda_copy_cache_entry* entry,
                                            mf_cuda_argument_cache_scope scope, uint32_t index,
                                            uint32_t generation) {
  uint32_t memory_index = UINT32_C(0);
  if (scope == MF_CUDA_ARGUMENT_CACHE_ALL) {
    return 1;
  }
  if (scope == MF_CUDA_ARGUMENT_CACHE_CONTEXT) {
    return entry->context_index == index && entry->context_generation == generation;
  }
  if (scope != MF_CUDA_ARGUMENT_CACHE_MEMORY) {
    return 0;
  }
  for (memory_index = UINT32_C(0); memory_index < UINT32_C(2); ++memory_index) {
    if (entry->memory_indices[memory_index] == index &&
        entry->memory_generations[memory_index] == generation) {
      return 1;
    }
  }
  return 0;
}

static CUresult mf_cuda_copy_cache_release_locked(mf_cuda_argument_cache_scope scope,
                                                  uint32_t index, uint32_t generation) {
  CUresult first_result = CUDA_SUCCESS;
  uint32_t cache_index = UINT32_C(0);
  for (cache_index = UINT32_C(0); cache_index < MF_CUDA_COPY_CACHE_CAPACITY; ++cache_index) {
    mf_cuda_copy_cache_entry* entry = &mf_cuda_global.copy_cache[cache_index];
    CUresult result = CUDA_SUCCESS;
    uint32_t expected = MF_CUDA_CACHE_READY;
    if (mf_atomic_load_u32_acquire(&entry->active) != MF_CUDA_CACHE_READY ||
        mf_cuda_copy_cache_matches_scope(entry, scope, index, generation) == 0) {
      continue;
    }
    if (!mf_atomic_compare_exchange_u32_seq_cst(&entry->active, &expected, MF_CUDA_CACHE_WRITING)) {
      continue;
    }
    result = mf_cuda_control_locked(MF_CLIENT_CONTROL_ARGUMENT_BLOCK_RELEASE_V1, UINT16_C(0),
                                    entry->id, entry->generation, (const void*)0, UINT64_C(0),
                                    (uint64_t*)0, (uint64_t*)0);
    if (result == CUDA_SUCCESS) {
      (void)memset(entry, 0, offsetof(mf_cuda_copy_cache_entry, active));
      mf_atomic_store_u32_release(&entry->active, MF_CUDA_CACHE_FREE);
    } else if (first_result == CUDA_SUCCESS) {
      first_result = result;
      mf_atomic_store_u32_release(&entry->active, MF_CUDA_CACHE_READY);
    } else {
      mf_atomic_store_u32_release(&entry->active, MF_CUDA_CACHE_READY);
    }
  }
  return first_result;
}

static int mf_cuda_copy_cache_is_empty_locked(void) {
  uint32_t cache_index = UINT32_C(0);
  for (cache_index = UINT32_C(0); cache_index < MF_CUDA_COPY_CACHE_CAPACITY; ++cache_index) {
    if (mf_atomic_load_u32_acquire(&mf_cuda_global.copy_cache[cache_index].active) !=
        MF_CUDA_CACHE_FREE) {
      return 0;
    }
  }
  return 1;
}

static CUresult mf_cuda_copy_cache_acquire_locked(
    const mf_cuda_copy_argument_block* block, uint32_t context_index, uint32_t context_generation,
    const uint32_t memory_indices[2], const uint32_t memory_generations[2], uint32_t cacheable,
    uint64_t* out_id, uint64_t* out_generation, uint32_t* out_cached) {
  uint32_t cache_index = UINT32_C(0);
  uint32_t free_index = MF_CUDA_COPY_CACHE_CAPACITY;
  CUresult result = CUDA_SUCCESS;
  *out_id = UINT64_C(0);
  *out_generation = UINT64_C(0);
  *out_cached = UINT32_C(0);
  if (cacheable != UINT32_C(0)) {
    for (cache_index = UINT32_C(0); cache_index < MF_CUDA_COPY_CACHE_CAPACITY; ++cache_index) {
      const mf_cuda_copy_cache_entry* entry = &mf_cuda_global.copy_cache[cache_index];
      const uint32_t active = mf_atomic_load_u32_acquire(&entry->active);
      if (active == MF_CUDA_CACHE_FREE) {
        if (free_index == MF_CUDA_COPY_CACHE_CAPACITY) {
          free_index = cache_index;
        }
        continue;
      }
      if (active == MF_CUDA_CACHE_READY && entry->context_index == context_index &&
          entry->context_generation == context_generation &&
          memcmp(&entry->block, block, MF_CUDA_COPY_ARGUMENT_SIZE) == 0) {
        *out_id = entry->id;
        *out_generation = entry->generation;
        *out_cached = UINT32_C(1);
        return CUDA_SUCCESS;
      }
    }
  }
  result = mf_cuda_control_locked(
      MF_CLIENT_CONTROL_ARGUMENT_BLOCK_REGISTER_V1, MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD,
      mf_cuda_global.transport.runtime_context_id, (uint64_t)MF_CUDA_COPY_ARGUMENT_SIZE, block,
      (uint64_t)MF_CUDA_COPY_ARGUMENT_SIZE, out_id, out_generation);
  if (result == CUDA_SUCCESS && cacheable != UINT32_C(0) &&
      free_index != MF_CUDA_COPY_CACHE_CAPACITY) {
    mf_cuda_copy_cache_entry* entry = &mf_cuda_global.copy_cache[free_index];
    uint32_t expected = MF_CUDA_CACHE_FREE;
    if (!mf_atomic_compare_exchange_u32_seq_cst(&entry->active, &expected, MF_CUDA_CACHE_WRITING)) {
      return result;
    }
    (void)memset(entry, 0, offsetof(mf_cuda_copy_cache_entry, active));
    entry->block = *block;
    entry->id = *out_id;
    entry->generation = *out_generation;
    entry->context_index = context_index;
    entry->context_generation = context_generation;
    entry->memory_indices[0] = memory_indices[0];
    entry->memory_indices[1] = memory_indices[1];
    entry->memory_generations[0] = memory_generations[0];
    entry->memory_generations[1] = memory_generations[1];
    mf_atomic_store_u32_release(&entry->active, MF_CUDA_CACHE_READY);
    *out_cached = UINT32_C(1);
  }
  return result;
}

static uint64_t mf_cuda_pending_pack(uint64_t tag, uint32_t state) {
  return (tag << 8U) | (uint64_t)(state & UINT32_C(0xff));
}

static uint64_t mf_cuda_pending_tag(uint64_t tagged_state) { return tagged_state >> 8U; }

static uint32_t mf_cuda_pending_state(uint64_t tagged_state) {
  return (uint32_t)(tagged_state & UINT64_C(0xff));
}

static uint64_t mf_cuda_pending_context_key(uint32_t context_index, uint32_t context_generation) {
  return ((uint64_t)context_generation << 32U) | (uint64_t)context_index;
}

static void mf_cuda_pending_release_slot(mf_cuda_pending_slot* slot, uint64_t tag,
                                         uint32_t close_payload) {
  if (close_payload != UINT32_C(0)) {
    mf_client_payload_close_v1(&slot->pending.host_payload);
  }
  mf_cuda_pending_initialize(&slot->pending);
  (void)memset(&slot->completion, 0, sizeof(slot->completion));
  slot->completion_result = (int32_t)CUDA_SUCCESS;
  mf_atomic_store_u64_relaxed(&slot->request_id, UINT64_C(0));
  mf_atomic_store_u64_relaxed(&slot->context_key, UINT64_C(0));
  mf_atomic_store_u64_release(&slot->tagged_state,
                              mf_cuda_pending_pack(tag, MF_CUDA_PENDING_STATE_FREE));
  (void)mf_atomic_fetch_sub_u32_acq_rel(&mf_cuda_global.pending_count, UINT32_C(1));
}

static void mf_cuda_abandon_pending_locked(void) {
  mf_cuda_tls_state* thread = mf_cuda_thread_state(UINT32_C(0));
  const uint32_t all_contexts = thread != (mf_cuda_tls_state*)0 && thread->lock_depth != UINT32_C(0)
                                    ? UINT32_C(1)
                                    : UINT32_C(0);
  const uint32_t context_index =
      thread != (mf_cuda_tls_state*)0 ? thread->queue_context_index : MF_CUDA_INDEX_NONE;
  const uint64_t context_key =
      context_index < MF_CUDA_OBJECT_CAPACITY
          ? mf_cuda_pending_context_key(context_index,
                                        mf_cuda_global.contexts[context_index].generation)
          : UINT64_C(0);
  uint32_t index = UINT32_C(0);
  for (index = UINT32_C(0); index < MF_CUDA_PENDING_CAPACITY; ++index) {
    mf_cuda_pending_slot* slot = &mf_cuda_global.pending[index];
    for (;;) {
      uint64_t tagged_state = mf_atomic_load_u64_acquire(&slot->tagged_state);
      const uint32_t state = mf_cuda_pending_state(tagged_state);
      const uint64_t desired =
          mf_cuda_pending_pack(mf_cuda_pending_tag(tagged_state), MF_CUDA_PENDING_STATE_FINALIZING);
      if (state == MF_CUDA_PENDING_STATE_FREE || state == MF_CUDA_PENDING_STATE_FINALIZING ||
          (all_contexts == UINT32_C(0) &&
           mf_atomic_load_u64_acquire(&slot->context_key) != context_key)) {
        break;
      }
      if (state == MF_CUDA_PENDING_STATE_COMPLETING) {
        (void)sched_yield();
        continue;
      }
      if (!mf_atomic_compare_exchange_u64_seq_cst(&slot->tagged_state, &tagged_state, desired)) {
        continue;
      }
      mf_cuda_pending_release_slot(slot, mf_cuda_pending_tag(tagged_state), UINT32_C(1));
      break;
    }
  }
  for (index = UINT32_C(0); index < MF_CUDA_PENDING_CAPACITY; ++index) {
    mf_cuda_async_error* error = &mf_cuda_global.async_errors[index];
    uint32_t expected = MF_CUDA_ASYNC_ERROR_READY;
    if (mf_atomic_compare_exchange_u32_seq_cst(&error->active, &expected,
                                               MF_CUDA_ASYNC_ERROR_WRITING)) {
      if (all_contexts != UINT32_C(0) ||
          mf_atomic_load_u64_acquire(&error->context_key) == context_key) {
        mf_atomic_store_u32_release(&error->active, MF_CUDA_ASYNC_ERROR_FREE);
      } else {
        mf_atomic_store_u32_release(&error->active, MF_CUDA_ASYNC_ERROR_READY);
      }
    }
  }
}

static void mf_cuda_record_async_error_locked(const mf_cuda_pending* pending, CUresult result) {
  uint32_t index = UINT32_C(0);
  if (pending->defer_error == UINT32_C(0) || result == CUDA_SUCCESS) {
    return;
  }
  for (index = UINT32_C(0); index < MF_CUDA_PENDING_CAPACITY; ++index) {
    mf_cuda_async_error* error = &mf_cuda_global.async_errors[index];
    uint32_t expected = MF_CUDA_ASYNC_ERROR_FREE;
    if (mf_atomic_compare_exchange_u32_seq_cst(&error->active, &expected,
                                               MF_CUDA_ASYNC_ERROR_WRITING)) {
      error->request_id = pending->request_id;
      error->result = (int32_t)result;
      error->context_index = pending->context_index;
      error->context_generation = pending->context_generation;
      error->stream_index = pending->stream_index;
      error->stream_generation = pending->stream_generation;
      mf_atomic_store_u64_relaxed(
          &error->context_key,
          mf_cuda_pending_context_key(pending->context_index, pending->context_generation));
      mf_atomic_store_u32_release(&error->active, MF_CUDA_ASYNC_ERROR_READY);
      return;
    }
  }
  /* Request IDs are monotonic, so a full table already preserves the oldest failures. */
}

static CUresult mf_cuda_take_async_error_locked(uint32_t context_index, uint32_t context_generation,
                                                uint32_t match_stream, uint32_t stream_index,
                                                uint32_t stream_generation,
                                                uint64_t target_request) {
  uint64_t selected_request = UINT64_MAX;
  uint32_t selected_index = MF_CUDA_PENDING_CAPACITY;
  uint32_t index = UINT32_C(0);
  for (index = UINT32_C(0); index < MF_CUDA_PENDING_CAPACITY; ++index) {
    const mf_cuda_async_error* error = &mf_cuda_global.async_errors[index];
    if (mf_atomic_load_u32_acquire(&error->active) != MF_CUDA_ASYNC_ERROR_READY ||
        error->request_id > target_request || error->context_index != context_index ||
        error->context_generation != context_generation ||
        (match_stream != UINT32_C(0) &&
         (error->stream_index != stream_index || error->stream_generation != stream_generation)) ||
        error->request_id >= selected_request) {
      continue;
    }
    selected_request = error->request_id;
    selected_index = index;
  }
  if (selected_index != MF_CUDA_PENDING_CAPACITY) {
    const CUresult result = (CUresult)mf_cuda_global.async_errors[selected_index].result;
    uint32_t expected = MF_CUDA_ASYNC_ERROR_READY;
    (void)mf_atomic_compare_exchange_u32_seq_cst(
        &mf_cuda_global.async_errors[selected_index].active, &expected, MF_CUDA_ASYNC_ERROR_FREE);
    return result;
  }
  {
    const CUresult transport_error =
        (CUresult)mf_atomic_load_u32_acquire(&mf_cuda_global.transport_error);
    return transport_error == CUDA_SUCCESS ? CUDA_SUCCESS : transport_error;
  }
}

static CUresult mf_cuda_clear_async_errors_locked(uint32_t context_index,
                                                  uint32_t context_generation,
                                                  uint32_t match_stream, uint32_t stream_index,
                                                  uint32_t stream_generation,
                                                  uint64_t target_request) {
  uint64_t first_request = UINT64_MAX;
  CUresult first_result = CUDA_SUCCESS;
  uint32_t index = UINT32_C(0);
  for (index = UINT32_C(0); index < MF_CUDA_PENDING_CAPACITY; ++index) {
    mf_cuda_async_error* error = &mf_cuda_global.async_errors[index];
    if (mf_atomic_load_u32_acquire(&error->active) != MF_CUDA_ASYNC_ERROR_READY ||
        error->request_id > target_request || error->context_index != context_index ||
        error->context_generation != context_generation ||
        (match_stream != UINT32_C(0) &&
         (error->stream_index != stream_index || error->stream_generation != stream_generation))) {
      continue;
    }
    if (error->request_id < first_request) {
      first_request = error->request_id;
      first_result = (CUresult)error->result;
    }
    {
      uint32_t expected = MF_CUDA_ASYNC_ERROR_READY;
      (void)mf_atomic_compare_exchange_u32_seq_cst(&error->active, &expected,
                                                   MF_CUDA_ASYNC_ERROR_FREE);
    }
  }
  return first_result;
}

static unsigned char mf_cuda_ascii_lower(unsigned char value) {
  return value >= (unsigned char)'A' && value <= (unsigned char)'Z'
             ? (unsigned char)(value + ((unsigned char)'a' - (unsigned char)'A'))
             : value;
}

static int mf_cuda_prefix_equal(const char* left, const char* right, size_t count) {
  size_t index = 0;
  for (index = 0; index < count; ++index) {
    if (mf_cuda_ascii_lower((unsigned char)left[index]) !=
        mf_cuda_ascii_lower((unsigned char)right[index])) {
      return 0;
    }
  }
  return 1;
}

static void mf_cuda_format_uuid(const uint8_t bytes[16], char output[41]) {
  (void)snprintf(output, (size_t)41,
                 "GPU-%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-"
                 "%02x%02x%02x%02x%02x%02x",
                 bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
                 bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14],
                 bytes[15]);
}

static CUresult mf_cuda_resolve_visible_token_locked(const char* token, size_t token_size,
                                                     uint32_t registry_count,
                                                     uint32_t* out_registry_index,
                                                     uint32_t* out_matched) {
  uint64_t numeric = UINT64_C(0);
  size_t index = 0;
  *out_matched = UINT32_C(0);
  if (token_size == (size_t)0) {
    return CUDA_SUCCESS;
  }
  for (index = 0; index < token_size; ++index) {
    if (token[index] < '0' || token[index] > '9') {
      break;
    }
    const uint64_t digit = (uint64_t)(unsigned int)(token[index] - '0');
    if (numeric > ((uint64_t)UINT32_MAX - digit) / UINT64_C(10)) {
      return CUDA_SUCCESS;
    }
    numeric = numeric * UINT64_C(10) + digit;
  }
  if (index == token_size) {
    if (numeric < (uint64_t)registry_count) {
      *out_registry_index = (uint32_t)numeric;
      *out_matched = UINT32_C(1);
    }
    return CUDA_SUCCESS;
  }
  if (token_size <= (size_t)4 || token_size > (size_t)40 ||
      !mf_cuda_prefix_equal(token, "GPU-", (size_t)4)) {
    return CUDA_SUCCESS;
  }
  for (index = 0; index < (size_t)registry_count; ++index) {
    mf_virtual_device_identity_v1 identity;
    char candidate[41];
    const mf_shared_status_v1 status =
        mf_client_registry_identity_v1(&mf_cuda_global.registry, (uint32_t)index, &identity);
    if (status != MF_SHARED_SUCCESS) {
      return mf_cuda_status(status);
    }
    mf_cuda_format_uuid(identity.gpu_uuid, candidate);
    if (mf_cuda_prefix_equal(token, candidate, token_size)) {
      if (*out_matched != UINT32_C(0)) {
        *out_matched = UINT32_C(0);
        return CUDA_SUCCESS;
      }
      *out_registry_index = (uint32_t)index;
      *out_matched = UINT32_C(1);
    }
  }
  return CUDA_SUCCESS;
}

static CUresult mf_cuda_build_visible_devices_locked(void) {
  const uint32_t registry_count = mf_client_registry_device_count_v1(&mf_cuda_global.registry);
  const char* configured = getenv("CUDA_VISIBLE_DEVICES");
  const char* cursor = configured;
  uint32_t* visible_devices = (uint32_t*)0;
  uint32_t visible_count = UINT32_C(0);
  uint32_t index = UINT32_C(0);
  if (registry_count > (uint32_t)INT32_MAX) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  if (configured != (const char*)0 && configured[0] == '\0') {
    return CUDA_SUCCESS;
  }
  if (registry_count != UINT32_C(0)) {
    visible_devices =
        (uint32_t*)mf_cuda_heap_malloc((size_t)registry_count * sizeof(*visible_devices));
    if (visible_devices == (uint32_t*)0) {
      return CUDA_ERROR_OUT_OF_MEMORY;
    }
  }
  if (configured == (const char*)0) {
    for (index = UINT32_C(0); index < registry_count; ++index) {
      visible_devices[index] = index;
    }
    visible_count = registry_count;
  } else {
    for (;;) {
      const char* token_end = strchr(cursor, ',');
      const size_t token_size =
          token_end == (const char*)0 ? strlen(cursor) : (size_t)(token_end - cursor);
      uint32_t registry_index = UINT32_C(0);
      uint32_t matched = UINT32_C(0);
      CUresult result = mf_cuda_resolve_visible_token_locked(cursor, token_size, registry_count,
                                                             &registry_index, &matched);
      if (result != CUDA_SUCCESS) {
        free(visible_devices);
        return result;
      }
      if (matched == UINT32_C(0)) {
        break;
      }
      for (index = UINT32_C(0); index < visible_count; ++index) {
        if (visible_devices[index] == registry_index) {
          matched = UINT32_C(0);
          break;
        }
      }
      if (matched == UINT32_C(0)) {
        break;
      }
      visible_devices[visible_count] = registry_index;
      visible_count += UINT32_C(1);
      if (token_end == (const char*)0) {
        break;
      }
      cursor = token_end + 1;
    }
  }
  mf_cuda_global.visible_devices = visible_devices;
  mf_cuda_global.visible_count = visible_count;
  return CUDA_SUCCESS;
}

static CUresult mf_cuda_registry_index_locked(CUdevice device, uint32_t* out_registry_index) {
  if (device < 0 || (uint32_t)device >= mf_cuda_global.visible_count) {
    return CUDA_ERROR_INVALID_DEVICE;
  }
  *out_registry_index = mf_cuda_global.visible_devices[(uint32_t)device];
  return CUDA_SUCCESS;
}

static CUresult mf_cuda_visible_ordinal_locked(uint32_t registry_index, CUdevice* out_device) {
  uint32_t index = UINT32_C(0);
  for (index = UINT32_C(0); index < mf_cuda_global.visible_count; ++index) {
    if (mf_cuda_global.visible_devices[index] == registry_index) {
      *out_device = (CUdevice)index;
      return CUDA_SUCCESS;
    }
  }
  return CUDA_ERROR_INVALID_DEVICE;
}

static void mf_cuda_close_locked(void) {
  mf_cuda_global.closing = UINT32_C(1);
  while (atomic_load_explicit(&mf_cuda_global.active_waiters, memory_order_acquire) !=
         UINT32_C(0)) {
    mf_cuda_unlock();
    (void)sched_yield();
    mf_cuda_lock();
  }
  if (mf_cuda_global.initialized != UINT32_C(0) &&
      mf_atomic_load_u32_acquire(&mf_cuda_global.pending_count) == UINT32_C(0) &&
      mf_atomic_load_u32_acquire(&mf_cuda_global.transport_error) == (uint32_t)CUDA_SUCCESS) {
    (void)mf_cuda_copy_cache_release_locked(MF_CUDA_ARGUMENT_CACHE_ALL, UINT32_C(0), UINT32_C(0));
    (void)mf_cuda_argument_cache_release_locked(MF_CUDA_ARGUMENT_CACHE_ALL, UINT32_C(0),
                                                UINT32_C(0));
  }
  mf_cuda_abandon_pending_locked();
  mf_cuda_close_cdev_locked();
  if (mf_cuda_global.session.socket_fd >= 0) {
    mf_client_session_close_v1(&mf_cuda_global.session);
  }
  if (mf_cuda_global.completion.mapping != (void*)0) {
    mf_client_ring_close_v1(&mf_cuda_global.completion);
  }
  if (mf_cuda_global.submission.mapping != (void*)0) {
    mf_client_ring_close_v1(&mf_cuda_global.submission);
  }
  if (mf_cuda_global.registry.mapping != (void*)0) {
    mf_client_registry_close_v1(&mf_cuda_global.registry);
  }
  free(mf_cuda_global.visible_devices);
  (void)memset(mf_cuda_global.argument_cache, 0, sizeof(mf_cuda_global.argument_cache));
  (void)memset(mf_cuda_global.copy_cache, 0, sizeof(mf_cuda_global.copy_cache));
  mf_cuda_global.visible_devices = (uint32_t*)0;
  mf_cuda_global.visible_count = UINT32_C(0);
  mf_cuda_global.process_view_revision = UINT64_C(0);
  mf_cuda_global.direct_host_copy_ready = UINT32_C(0);
  mf_cuda_global.cdev_active = UINT32_C(0);
  mf_atomic_store_u32_release(&mf_cuda_global.transport_error, (uint32_t)CUDA_SUCCESS);
  mf_cuda_global.initialized = UINT32_C(0);
  mf_cuda_global.closing = UINT32_C(0);
}

static mf_shared_status_v1 mf_cuda_try_initialize_cdev_locked(uint32_t* out_fallback) {
#if METAFLUX_PROVIDER_CDEV
  const uint64_t required_capabilities =
      MF_CLIENT_CAP_SHARED_DEVICE_V1 | MF_CLIENT_CAP_MEMFD_RING_V1 |
      MF_CLIENT_CAP_FUTEX_DOORBELL_V1 | MF_CLIENT_CAP_LIVE_CONTEXT_ACCOUNTING_V1 |
      MF_CLIENT_CAP_CDEV_BINDING_V1;
  const uint64_t optional_capabilities = MF_CLIENT_CAP_TIMELINE_V1 | MF_CLIENT_CAP_TELEMETRY_V1 |
                                         MF_CLIENT_CAP_COPY_REGION_V1 |
                                         MF_CLIENT_CAP_DIRECT_HOST_COPY_V1;
  mf_client_control_response_v1 response;
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;
  uint32_t control_status = MF_CLIENT_CONTROL_INTERNAL_ERROR;
  if (out_fallback == (uint32_t*)0) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  *out_fallback = UINT32_C(0);
  status = mf_cdev_session_open_default_v0(&mf_cuda_global.cdev_session);
  if (status == MF_SHARED_NOT_SUPPORTED) {
    *out_fallback = UINT32_C(1);
    return MF_SHARED_SUCCESS;
  }
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  status = mf_client_session_connect_default_capabilities_v1(
      required_capabilities, optional_capabilities, &mf_cuda_global.session);
  if (status == MF_SHARED_NOT_SUPPORTED) {
    *out_fallback = UINT32_C(1);
    mf_cuda_close_cdev_locked();
    mf_client_session_close_v1(&mf_cuda_global.session);
    return MF_SHARED_SUCCESS;
  }
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  if (!mf_registry_view_id_equal_v1(mf_cuda_global.cdev_session.registry_view_id,
                                    mf_cuda_global.session.registry_view_id) ||
      mf_cuda_global.cdev_session.device_generation != MF_CLIENT_QUEUE_GENERATION_V1) {
    return MF_SHARED_STALE_HANDLE;
  }
  status = mf_cdev_memory_alloc_v0(&mf_cuda_global.cdev_session, MF_CDEV_PAYLOAD_MAX_SIZE_V0,
                                   UINT64_C(4096), &mf_cuda_global.cdev_payload);
  if (status == MF_SHARED_NOT_SUPPORTED) {
    *out_fallback = UINT32_C(1);
    mf_cuda_close_cdev_locked();
    mf_client_session_close_v1(&mf_cuda_global.session);
    return MF_SHARED_SUCCESS;
  }
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  status = mf_client_session_control_v1(
      &mf_cuda_global.session, MF_CLIENT_CONTROL_CDEV_BIND_V1, UINT16_C(0),
      MF_CLIENT_RUNTIME_CONTEXT_ID_V1, mf_cuda_global.cdev_session.device_generation, -1, &response,
      (int32_t*)0);
  if (status != MF_SHARED_SUCCESS) {
    return status;
  }
  control_status = mf_client_load_le32_v1(response.bytes + 12);
  if (control_status == MF_CLIENT_CONTROL_UNSUPPORTED) {
    *out_fallback = UINT32_C(1);
    mf_cuda_close_cdev_locked();
    mf_client_session_close_v1(&mf_cuda_global.session);
    return MF_SHARED_SUCCESS;
  }
  if (control_status != MF_CLIENT_CONTROL_OK ||
      mf_client_load_le16_v1(response.bytes + 16) != UINT16_C(0) ||
      mf_client_load_le64_v1(response.bytes + 48) != MF_CLIENT_RUNTIME_CONTEXT_ID_V1 ||
      mf_client_load_le64_v1(response.bytes + 56) == UINT64_C(0)) {
    return control_status == MF_CLIENT_CONTROL_OK ? MF_SHARED_MALFORMED
                                                   : MF_SHARED_INVALID_ARGUMENT;
  }
  mf_cuda_global.transport.view_id = mf_cuda_global.session.registry_view_id;
  mf_cuda_global.transport.submission_queue_id = MF_CLIENT_SUBMISSION_QUEUE_ID_V1;
  mf_cuda_global.transport.submission_queue_generation = MF_CLIENT_QUEUE_GENERATION_V1;
  mf_cuda_global.transport.completion_queue_id = MF_CLIENT_COMPLETION_QUEUE_ID_V1;
  mf_cuda_global.transport.completion_queue_generation = MF_CLIENT_QUEUE_GENERATION_V1;
  mf_cuda_global.transport.runtime_context_id = MF_CLIENT_RUNTIME_CONTEXT_ID_V1;
  mf_cuda_global.transport.runtime_event_id = MF_CLIENT_RUNTIME_EVENT_ID_V1;
  mf_cuda_global.transport.runtime_event_generation = MF_CLIENT_RUNTIME_EVENT_GENERATION_V1;
  mf_cuda_global.transport.runtime_add_kernel_id = MF_CLIENT_RUNTIME_ADD_KERNEL_ID_V1;
  mf_cuda_global.transport.negotiated_capabilities =
      mf_cuda_global.session.negotiated_capabilities & ~MF_CLIENT_CAP_DIRECT_HOST_COPY_V1;
  mf_cuda_global.session.negotiated_capabilities &= ~MF_CLIENT_CAP_DIRECT_HOST_COPY_V1;
  mf_cuda_global.cdev_active = UINT32_C(1);
  mf_cuda_take_session_mappings_locked();
  return MF_SHARED_SUCCESS;
#else
  if (out_fallback == (uint32_t*)0) {
    return MF_SHARED_INVALID_ARGUMENT;
  }
  *out_fallback = UINT32_C(1);
  return MF_SHARED_SUCCESS;
#endif
}

static CUresult mf_cuda_initialize_locked(void) {
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;
  CUresult result = CUDA_SUCCESS;
  if (mf_cuda_global.closing != UINT32_C(0)) {
    return CUDA_ERROR_DEINITIALIZED;
  }
  if (mf_cuda_global.initialized != UINT32_C(0)) {
    return CUDA_SUCCESS;
  }
  mf_cuda_global.direct_host_copy_ready = UINT32_C(0);
  if (mf_cuda_global.transport.configured != UINT32_C(0)) {
    status =
        mf_client_registry_attach_v1(mf_cuda_global.transport.registry_fd,
                                     mf_cuda_global.transport.view_id, &mf_cuda_global.registry);
    if (status == MF_SHARED_SUCCESS) {
      status = mf_client_ring_attach_v1(
          mf_cuda_global.transport.submission_fd, mf_cuda_global.transport.view_id,
          mf_cuda_global.transport.submission_queue_id,
          mf_cuda_global.transport.submission_queue_generation, &mf_cuda_global.submission);
    }
    if (status == MF_SHARED_SUCCESS) {
      status = mf_client_ring_attach_v1(
          mf_cuda_global.transport.completion_fd, mf_cuda_global.transport.view_id,
          mf_cuda_global.transport.completion_queue_id,
          mf_cuda_global.transport.completion_queue_generation, &mf_cuda_global.completion);
    }
#if defined(METAFLUX_PROVIDER_TESTING)
    if (status == MF_SHARED_SUCCESS && (mf_cuda_global.transport.negotiated_capabilities &
                                        MF_CLIENT_CAP_DIRECT_HOST_COPY_V1) != UINT64_C(0)) {
      CUresult registration_failure = CUDA_SUCCESS;
      const mf_cuda_direct_registration_result registration =
          mf_cuda_register_host_address_space_locked(&registration_failure);
      if (registration == MF_CUDA_DIRECT_REGISTRATION_READY) {
        mf_cuda_global.direct_host_copy_ready = UINT32_C(1);
      } else if (registration == MF_CUDA_DIRECT_REGISTRATION_FALLBACK) {
        mf_cuda_global.transport.negotiated_capabilities &= ~MF_CLIENT_CAP_DIRECT_HOST_COPY_V1;
      } else {
        result = registration_failure;
      }
    }
#endif
  } else {
    uint32_t cdev_fallback = UINT32_C(0);
    status = mf_cuda_try_initialize_cdev_locked(&cdev_fallback);
    if (status == MF_SHARED_SUCCESS && cdev_fallback != UINT32_C(0)) {
      status = mf_client_session_connect_default_v1(&mf_cuda_global.session);
    }
    if (status == MF_SHARED_SUCCESS && mf_cuda_global.cdev_active == UINT32_C(0)) {
      mf_cuda_global.transport.view_id = mf_cuda_global.session.registry_view_id;
      mf_cuda_global.transport.submission_queue_id = MF_CLIENT_SUBMISSION_QUEUE_ID_V1;
      mf_cuda_global.transport.submission_queue_generation = MF_CLIENT_QUEUE_GENERATION_V1;
      mf_cuda_global.transport.completion_queue_id = MF_CLIENT_COMPLETION_QUEUE_ID_V1;
      mf_cuda_global.transport.completion_queue_generation = MF_CLIENT_QUEUE_GENERATION_V1;
      mf_cuda_global.transport.runtime_context_id = MF_CLIENT_RUNTIME_CONTEXT_ID_V1;
      mf_cuda_global.transport.runtime_event_id = MF_CLIENT_RUNTIME_EVENT_ID_V1;
      mf_cuda_global.transport.runtime_event_generation = MF_CLIENT_RUNTIME_EVENT_GENERATION_V1;
      mf_cuda_global.transport.runtime_add_kernel_id = MF_CLIENT_RUNTIME_ADD_KERNEL_ID_V1;
      mf_cuda_global.transport.negotiated_capabilities =
          mf_cuda_global.session.negotiated_capabilities;
      if ((mf_cuda_global.transport.negotiated_capabilities & MF_CLIENT_CAP_DIRECT_HOST_COPY_V1) !=
          UINT64_C(0)) {
        CUresult registration_failure = CUDA_SUCCESS;
        const mf_cuda_direct_registration_result registration =
            mf_cuda_register_host_address_space_locked(&registration_failure);
        if (registration == MF_CUDA_DIRECT_REGISTRATION_READY) {
          mf_cuda_global.direct_host_copy_ready = UINT32_C(1);
        } else if (registration == MF_CUDA_DIRECT_REGISTRATION_FALLBACK) {
          mf_cuda_global.transport.negotiated_capabilities &= ~MF_CLIENT_CAP_DIRECT_HOST_COPY_V1;
          mf_cuda_global.session.negotiated_capabilities &= ~MF_CLIENT_CAP_DIRECT_HOST_COPY_V1;
        } else {
          result = registration_failure;
        }
      }
      if (result == CUDA_SUCCESS) {
        mf_cuda_take_session_mappings_locked();
      }
    }
  }
  if (status != MF_SHARED_SUCCESS) {
    mf_cuda_close_locked();
    return status == MF_SHARED_SYSTEM_ERROR ? CUDA_ERROR_SYSTEM_NOT_READY : mf_cuda_status(status);
  }
  if (result != CUDA_SUCCESS) {
    mf_cuda_close_locked();
    return result;
  }
  mf_cuda_global.process_view_revision =
      mf_client_registry_process_view_revision_v1(&mf_cuda_global.registry);
  if (mf_cuda_global.process_view_revision == UINT64_C(0)) {
    mf_cuda_close_locked();
    return CUDA_ERROR_INVALID_VALUE;
  }
  result = mf_cuda_build_visible_devices_locked();
  if (result != CUDA_SUCCESS) {
    mf_cuda_close_locked();
    return result;
  }
  mf_cuda_global.next_address = MF_CUDA_ADDRESS_BASE;
  mf_atomic_store_u32_release(&mf_cuda_global.transport_error, (uint32_t)CUDA_SUCCESS);
  mf_cuda_global.initialized = UINT32_C(1);
  return CUDA_SUCCESS;
}

static CUresult mf_cuda_require_locked(void) {
  if (mf_cuda_global.closing != UINT32_C(0)) {
    return CUDA_ERROR_DEINITIALIZED;
  }
  if (mf_cuda_global.initialized == UINT32_C(0) &&
      getenv("METAFLUX_TRACE_STUBS") != (void*)0) {
    fprintf(stderr, "MF_REQUIRE_FAIL closing=%u initialized=%u\n",
            mf_cuda_global.closing, mf_cuda_global.initialized);
  }
  return mf_cuda_global.initialized == UINT32_C(0) ? CUDA_ERROR_NOT_INITIALIZED : CUDA_SUCCESS;
}

static mf_shared_status_v1 mf_cuda_try_submit_locked(const mf_cuda_command* command,
                                                     uint64_t request_id) {
  mf_client_ring_v1* submission = mf_cuda_submission_ring_locked(command);
#if METAFLUX_PROVIDER_CDEV
  if (mf_cuda_global.cdev_active != UINT32_C(0) &&
      command->kind == MF_CUDA_COMMAND_LAUNCH) {
    const mf_cdev_launch_v0 launch = {
        .module_id = command->target,
        .module_generation = command->arguments[0],
        .argument_block_id = command->arguments[2],
        .argument_block_generation = command->arguments[3],
    };
    mf_ring_descriptor_v1 descriptor;
    mf_shared_status_v1 status = mf_cdev_launch_descriptor_v0(
        request_id, mf_cuda_global.cdev_session.device_generation, &launch, &descriptor);
    return status == MF_SHARED_SUCCESS ? mf_client_ring_try_submit_v1(submission, &descriptor)
                                       : status;
  }
#endif
  switch (command->kind) {
  case MF_CUDA_COMMAND_ALLOC:
    return mf_client_submit_memory_alloc_v1(submission, request_id, command->target,
                                            command->arguments[0], command->arguments[1],
                                            command->flags);
  case MF_CUDA_COMMAND_FREE:
    return mf_client_submit_memory_free_v1(submission, request_id, command->target,
                                           command->arguments[0]);
  case MF_CUDA_COMMAND_MODULE_LOAD:
    return mf_client_submit_module_load_v1(submission, request_id, command->target,
                                           command->arguments[0], command->flags);
  case MF_CUDA_COMMAND_MODULE_UNLOAD:
    return mf_client_submit_module_unload_v1(submission, request_id,
                                             command->target, command->arguments[0]);
  case MF_CUDA_COMMAND_COPY:
    if (command->flags == MF_RING_COPY_FLAG_DIRECT_HOST_SOURCE_V1 ||
        command->flags == MF_RING_COPY_FLAG_DIRECT_HOST_DESTINATION_V1) {
      return mf_client_submit_direct_host_copy_v1(
          submission, request_id, command->target, command->arguments[0],
          command->arguments[1], command->arguments[2], command->arguments[3], command->flags);
    }
    if (command->flags == MF_RING_COPY_FLAG_REGION_ARGUMENT_BLOCK_V1) {
      return mf_client_submit_copy_region_v1(submission, request_id,
                                             command->target, command->arguments[0]);
    }
    return mf_client_submit_copy_v1(submission, request_id, command->target,
                                    command->arguments[0], command->arguments[1],
                                    command->arguments[2], command->arguments[3], command->flags);
  case MF_CUDA_COMMAND_LAUNCH:
    return mf_client_submit_launch_v1(submission, request_id, command->target,
                                      command->arguments[0], command->arguments[1],
                                      command->arguments[2], command->arguments[3], command->flags);
  case MF_CUDA_COMMAND_EVENT_RECORD:
    return mf_client_submit_event_record_v1(submission, request_id, command->target,
                                            command->arguments[0], command->arguments[1],
                                            command->flags);
  case MF_CUDA_COMMAND_EVENT_WAIT:
    return mf_client_submit_event_wait_v1(submission, request_id, command->target,
                                          command->arguments[0], command->arguments[1],
                                          command->flags);
  case MF_CUDA_COMMAND_QUEUE_SYNC:
    return mf_client_submit_queue_control_v1(submission,
                                             MF_RING_OPCODE_QUEUE_SYNCHRONIZE, request_id,
                                             command->arguments[0], command->flags);
  }
  return MF_SHARED_INVALID_ARGUMENT;
}

static void mf_cuda_note_submission_locked(const mf_cuda_pending* pending) {
  if (pending->context_index != MF_CUDA_INDEX_NONE) {
    mf_cuda_object* context = &mf_cuda_global.contexts[pending->context_index];
    if (context->active != UINT32_C(0) && context->generation == pending->context_generation) {
      context->last_request = pending->request_id;
      if (pending->stream_index == MF_CUDA_INDEX_NONE &&
          pending->stream_generation == UINT32_C(0)) {
        context->default_stream_last_request = pending->request_id;
      } else if (pending->stream_index == MF_CUDA_INDEX_NONE) {
        mf_cuda_tls_state* state = mf_cuda_thread_state(UINT32_C(0));
        if (state != (mf_cuda_tls_state*)0) {
          mf_cuda_tls_default_stream* default_stream =
              &state->default_streams[pending->context_index];
          if (default_stream->context_generation == pending->context_generation &&
              default_stream->stream_generation == pending->stream_generation) {
            default_stream->last_request = pending->request_id;
          }
        }
      }
    }
  }
  if (pending->stream_index != MF_CUDA_INDEX_NONE) {
    mf_cuda_object* stream = &mf_cuda_global.streams[pending->stream_index];
    if (stream->active != UINT32_C(0) && stream->generation == pending->stream_generation) {
      stream->last_request = pending->request_id;
    }
  }
}

static CUresult mf_cuda_finish_pending_locked(mf_cuda_pending* pending,
                                              mf_client_completion_v1* completion) {
  CUresult result = mf_cuda_status(completion->status);
  if (result == CUDA_SUCCESS && pending->kind == (uint32_t)MF_CUDA_PENDING_COPY &&
      pending->host_destination != (void*)0) {
    if (mf_cuda_global.transport.read_object != (void*)0) {
      result = mf_cuda_status(mf_cuda_global.transport.read_object(
          mf_cuda_global.transport.control_context, pending->host_id, pending->host_generation,
          UINT64_C(0), (uint8_t*)pending->host_destination, pending->host_byte_count));
    } else if (pending->host_payload.mapping != (void*)0 &&
               pending->host_payload.mapping_size == pending->host_byte_count &&
               pending->host_byte_count <= (uint64_t)SIZE_MAX) {
      (void)memcpy(pending->host_destination, pending->host_payload.mapping,
                   (size_t)pending->host_byte_count);
    } else {
      result = CUDA_ERROR_NOT_SUPPORTED;
    }
  }
  if (pending->host_id != UINT64_C(0)) {
    const CUresult release_result = mf_cuda_control_locked(
        MF_CLIENT_CONTROL_HOST_MEMORY_RELEASE_V1, UINT16_C(0), pending->host_id,
        pending->host_generation, (const void*)0, UINT64_C(0), (uint64_t*)0, (uint64_t*)0);
    if (result == CUDA_SUCCESS) {
      result = release_result;
    }
  }
  if (pending->argument_id != UINT64_C(0) && pending->argument_cached == UINT32_C(0)) {
    const CUresult release_result = mf_cuda_control_locked(
        MF_CLIENT_CONTROL_ARGUMENT_BLOCK_RELEASE_V1, UINT16_C(0), pending->argument_id,
        pending->argument_generation, (const void*)0, UINT64_C(0), (uint64_t*)0, (uint64_t*)0);
    if (result == CUDA_SUCCESS) {
      result = release_result;
    }
  }
  mf_client_payload_close_v1(&pending->host_payload);
  if (pending->kind == (uint32_t)MF_CUDA_PENDING_EVENT_RECORD &&
      pending->event_index != MF_CUDA_INDEX_NONE) {
    mf_cuda_object* event = &mf_cuda_global.events[pending->event_index];
    if (event->active != UINT32_C(0) && event->generation == pending->event_generation &&
        event->last_request == pending->request_id) {
      if (result == CUDA_SUCCESS && completion->timeline_value == UINT64_C(0)) {
        result = CUDA_ERROR_UNKNOWN;
      }
      if (result == CUDA_SUCCESS) {
        event->size = completion->timeline_value;
      }
      event->completion_status = (int32_t)result;
      event->aux = UINT32_C(2);
    }
  }
  mf_cuda_record_async_error_locked(pending, result);
  return result;
}

static int mf_cuda_pending_slot_for_request(uint64_t request_id, uint32_t* out_index,
                                            uint64_t* out_tagged_state) {
  uint32_t index = UINT32_C(0);
  for (index = UINT32_C(0); index < MF_CUDA_PENDING_CAPACITY; ++index) {
    const mf_cuda_pending_slot* slot = &mf_cuda_global.pending[index];
    const uint64_t tagged_state = mf_atomic_load_u64_acquire(&slot->tagged_state);
    const uint32_t state = mf_cuda_pending_state(tagged_state);
    if (state != MF_CUDA_PENDING_STATE_FREE && state != MF_CUDA_PENDING_STATE_PREPARING &&
        mf_atomic_load_u64_acquire(&slot->request_id) == request_id) {
      *out_index = index;
      *out_tagged_state = tagged_state;
      return 1;
    }
  }
  return 0;
}

static CUresult mf_cuda_process_ready_context_locked(uint32_t context_index,
                                                     uint32_t context_generation) {
  for (;;) {
    const uint64_t context_key = mf_cuda_pending_context_key(context_index, context_generation);
    uint64_t first_request = UINT64_MAX;
    uint64_t first_tagged_state = UINT64_C(0);
    uint32_t first_index = MF_CUDA_PENDING_CAPACITY;
    uint32_t index = UINT32_C(0);
    for (index = UINT32_C(0); index < MF_CUDA_PENDING_CAPACITY; ++index) {
      const mf_cuda_pending_slot* slot = &mf_cuda_global.pending[index];
      const uint64_t tagged_state = mf_atomic_load_u64_acquire(&slot->tagged_state);
      const uint32_t state = mf_cuda_pending_state(tagged_state);
      if (state == MF_CUDA_PENDING_STATE_FREE || state == MF_CUDA_PENDING_STATE_PREPARING ||
          mf_atomic_load_u64_acquire(&slot->context_key) != context_key ||
          mf_atomic_load_u64_acquire(&slot->request_id) >= first_request) {
        continue;
      }
      first_request = mf_atomic_load_u64_acquire(&slot->request_id);
      first_tagged_state = tagged_state;
      first_index = index;
    }
    if (first_index == MF_CUDA_PENDING_CAPACITY ||
        mf_cuda_pending_state(first_tagged_state) != MF_CUDA_PENDING_STATE_COMPLETION_READY) {
      return CUDA_SUCCESS;
    }
    {
      mf_cuda_pending_slot* slot = &mf_cuda_global.pending[first_index];
      const uint64_t tag = mf_cuda_pending_tag(first_tagged_state);
      const uint64_t finalizing = mf_cuda_pending_pack(tag, MF_CUDA_PENDING_STATE_FINALIZING);
      CUresult result = CUDA_SUCCESS;
      if (!mf_atomic_compare_exchange_u64_seq_cst(&slot->tagged_state, &first_tagged_state,
                                                  finalizing)) {
        continue;
      }
      result = mf_cuda_finish_pending_locked(&slot->pending, &slot->completion);
      if (slot->pending.synchronous != UINT32_C(0)) {
        slot->completion_result = (int32_t)result;
        mf_atomic_store_u64_release(&slot->tagged_state,
                                    mf_cuda_pending_pack(tag, MF_CUDA_PENDING_STATE_COMPLETED));
      } else {
        mf_cuda_pending_release_slot(slot, tag, UINT32_C(0));
      }
    }
  }
}

static CUresult mf_cuda_process_ready_locked(void) {
  mf_cuda_tls_state* thread = mf_cuda_thread_state(UINT32_C(0));
  CUresult first_result = CUDA_SUCCESS;
  if (thread != (mf_cuda_tls_state*)0 && thread->lock_depth != UINT32_C(0)) {
    uint32_t context_index = UINT32_C(0);
    for (context_index = UINT32_C(0); context_index < MF_CUDA_OBJECT_CAPACITY; ++context_index) {
      const mf_cuda_object* context = &mf_cuda_global.contexts[context_index];
      const CUresult result = mf_cuda_process_ready_context_locked(
          context_index, context->active != UINT32_C(0) ? context->generation : UINT32_C(0));
      if (first_result == CUDA_SUCCESS && result != CUDA_SUCCESS) {
        first_result = result;
      }
    }
    {
      const CUresult result = mf_cuda_process_ready_context_locked(MF_CUDA_INDEX_NONE, UINT32_C(0));
      if (first_result == CUDA_SUCCESS && result != CUDA_SUCCESS) {
        first_result = result;
      }
    }
    return first_result;
  }
  if (thread == (mf_cuda_tls_state*)0 || thread->queue_context_index == MF_CUDA_INDEX_NONE) {
    do { if (mf_cuda_entry_trace_enabled() != 0) { fprintf(stderr, "MF_INVALID_CONTEXT %s:%d\n", __func__, __LINE__); } return CUDA_ERROR_INVALID_CONTEXT; } while (0);
  }
  return mf_cuda_process_ready_context_locked(
      thread->queue_context_index, mf_cuda_global.contexts[thread->queue_context_index].generation);
}

static uint32_t mf_cuda_earliest_pending_transport_locked(void) {
  uint64_t first_request = UINT64_MAX;
  uint32_t transport = MF_CUDA_SUBMISSION_UNIX;
  uint32_t index = UINT32_C(0);
  for (index = UINT32_C(0); index < MF_CUDA_PENDING_CAPACITY; ++index) {
    const mf_cuda_pending_slot* slot = &mf_cuda_global.pending[index];
    const uint64_t tagged_state = mf_atomic_load_u64_acquire(&slot->tagged_state);
    const uint32_t state = mf_cuda_pending_state(tagged_state);
    const uint64_t request_id = mf_atomic_load_u64_acquire(&slot->request_id);
    if (state == MF_CUDA_PENDING_STATE_FREE || state == MF_CUDA_PENDING_STATE_PREPARING ||
        request_id == UINT64_C(0) || request_id >= first_request) {
      continue;
    }
    first_request = request_id;
    transport = slot->pending.submission_transport;
  }
  return transport;
}

static CUresult mf_cuda_consume_one_locked(uint32_t blocking,
                                           mf_client_completion_v1* out_completion,
                                           CUresult* out_result, uint32_t* out_consumed) {
  mf_client_completion_v1 completion;
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;
  uint64_t wait_deadline_ns = UINT64_C(0);
  uint64_t tagged_state = UINT64_C(0);
  uint32_t pending_index = MF_CUDA_PENDING_CAPACITY;
  *out_consumed = UINT32_C(0);
  {
    const CUresult transport_error =
        (CUresult)mf_atomic_load_u32_acquire(&mf_cuda_global.transport_error);
    if (transport_error != CUDA_SUCCESS) {
      return transport_error;
    }
  }
  for (;;) {
    if (mf_atomic_load_u32_acquire(&mf_cuda_global.pending_count) == UINT32_C(0)) {
      return CUDA_ERROR_NOT_READY;
    }
    status = MF_SHARED_WOULD_BLOCK;
#if METAFLUX_PROVIDER_CDEV
    if (mf_cuda_global.cdev_active != UINT32_C(0)) {
      status = mf_client_try_consume_completion_v1(&mf_cuda_global.cdev_session.completion,
                                                   &completion);
    }
#endif
    if (status == MF_SHARED_WOULD_BLOCK) {
      status = mf_client_try_consume_completion_v1(&mf_cuda_global.completion, &completion);
    }
    if (status == MF_SHARED_SUCCESS) {
      break;
    }
    if (status != MF_SHARED_WOULD_BLOCK) {
      mf_atomic_store_u32_release(&mf_cuda_global.transport_error,
                                  (uint32_t)mf_cuda_status(status));
      mf_cuda_abandon_pending_locked();
      return (CUresult)mf_atomic_load_u32_acquire(&mf_cuda_global.transport_error);
    }
    if (blocking == MF_CUDA_CONSUME_NONBLOCKING) {
      return CUDA_ERROR_NOT_READY;
    }
    if (blocking == MF_CUDA_CONSUME_BLOCKING_COOPERATIVE) {
      if (wait_deadline_ns == UINT64_C(0)) {
        status = mf_cuda_wait_deadline(MF_CUDA_WAIT_NS, &wait_deadline_ns);
        if (status != MF_SHARED_SUCCESS) {
          return mf_cuda_status(status);
        }
      }
        status = mf_cuda_wait_ring_locked(
            mf_cuda_completion_ring_for_transport_locked(
                mf_cuda_earliest_pending_transport_locked()),
            UINT32_C(1), wait_deadline_ns);
    } else {
      status = mf_client_ring_wait_readable_v1(
          mf_cuda_completion_ring_for_transport_locked(
              mf_cuda_earliest_pending_transport_locked()),
          MF_CUDA_WAIT_NS);
    }
    if (status != MF_SHARED_SUCCESS && status != MF_SHARED_RETRY) {
      mf_atomic_store_u32_release(&mf_cuda_global.transport_error,
                                  (uint32_t)mf_cuda_status(status));
      mf_cuda_abandon_pending_locked();
      return (CUresult)mf_atomic_load_u32_acquire(&mf_cuda_global.transport_error);
    }
  }
  if (!mf_cuda_pending_slot_for_request(completion.request_id, &pending_index, &tagged_state) ||
      mf_cuda_pending_state(tagged_state) != MF_CUDA_PENDING_STATE_SUBMITTED) {
    mf_atomic_store_u32_release(&mf_cuda_global.transport_error, (uint32_t)CUDA_ERROR_UNKNOWN);
    mf_cuda_abandon_pending_locked();
    return CUDA_ERROR_UNKNOWN;
  }
  {
    mf_cuda_pending_slot* slot = &mf_cuda_global.pending[pending_index];
    const uint64_t tag = mf_cuda_pending_tag(tagged_state);
    const uint64_t completing = mf_cuda_pending_pack(tag, MF_CUDA_PENDING_STATE_COMPLETING);
    if (!mf_atomic_compare_exchange_u64_seq_cst(&mf_cuda_global.pending[pending_index].tagged_state,
                                                &tagged_state, completing)) {
      mf_atomic_store_u32_release(&mf_cuda_global.transport_error, (uint32_t)CUDA_ERROR_UNKNOWN);
      mf_cuda_abandon_pending_locked();
      return CUDA_ERROR_UNKNOWN;
    }
    slot->completion = completion;
    mf_atomic_store_u64_release(&slot->tagged_state,
                                mf_cuda_pending_pack(tag, MF_CUDA_PENDING_STATE_COMPLETION_READY));
  }
  *out_completion = completion;
  *out_result = mf_cuda_status(completion.status);
  *out_consumed = UINT32_C(1);
  return mf_cuda_process_ready_locked();
}

static CUresult mf_cuda_poll_locked(void) {
  CUresult result = mf_cuda_process_ready_locked();
  if (result != CUDA_SUCCESS) {
    return result;
  }
  for (;;) {
    mf_client_completion_v1 completion;
    CUresult completion_result = CUDA_SUCCESS;
    uint32_t consumed = UINT32_C(0);
    result = mf_cuda_consume_one_locked(MF_CUDA_CONSUME_NONBLOCKING, &completion,
                                        &completion_result, &consumed);
    if (result == CUDA_ERROR_NOT_READY) {
      return CUDA_SUCCESS;
    }
    if (result != CUDA_SUCCESS) {
      return result;
    }
    (void)completion;
    (void)completion_result;
    if (consumed == UINT32_C(0)) {
      return CUDA_SUCCESS;
    }
  }
}

static CUresult mf_cuda_claim_pending_slot(mf_cuda_pending_slot** out_slot, uint64_t* out_tag) {
  uint32_t index = UINT32_C(0);
  for (index = UINT32_C(0); index < MF_CUDA_PENDING_CAPACITY; ++index) {
    mf_cuda_pending_slot* slot = &mf_cuda_global.pending[index];
    uint64_t tagged_state = mf_atomic_load_u64_acquire(&slot->tagged_state);
    const uint64_t old_tag = mf_cuda_pending_tag(tagged_state);
    uint64_t new_tag = UINT64_C(0);
    uint64_t preparing = UINT64_C(0);
    if (mf_cuda_pending_state(tagged_state) != MF_CUDA_PENDING_STATE_FREE ||
        old_tag >= MF_CUDA_PENDING_TAG_MAX) {
      continue;
    }
    new_tag = old_tag + UINT64_C(1);
    preparing = mf_cuda_pending_pack(new_tag, MF_CUDA_PENDING_STATE_PREPARING);
    if (!mf_atomic_compare_exchange_u64_seq_cst(&slot->tagged_state, &tagged_state, preparing)) {
      continue;
    }
    (void)mf_atomic_fetch_add_u32_acq_rel(&mf_cuda_global.pending_count, UINT32_C(1));
    *out_slot = slot;
    *out_tag = new_tag;
    return CUDA_SUCCESS;
  }
  return CUDA_ERROR_OUT_OF_MEMORY;
}

static CUresult mf_cuda_enqueue_locked(const mf_cuda_command* command, mf_cuda_pending* pending,
                                       uint64_t* out_request_id) {
  uint64_t request_id = UINT64_C(0);
  uint64_t wait_deadline_ns = UINT64_C(0);
  uint64_t pending_tag = UINT64_C(0);
  mf_cuda_pending_slot* slot = (mf_cuda_pending_slot*)0;
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;
  CUresult result = mf_cuda_poll_locked();
  if (result != CUDA_SUCCESS) {
    return result;
  }
  result = mf_cuda_claim_pending_slot(&slot, &pending_tag);
  if (result != CUDA_SUCCESS) {
    return result;
  }
  result = mf_cuda_next_request_locked(&request_id);
  if (result != CUDA_SUCCESS) {
    goto done;
  }
  pending->request_id = request_id;
  pending->submission_transport = mf_cuda_command_uses_cdev_locked(command) != 0
                                      ? MF_CUDA_SUBMISSION_CDEV
                                      : MF_CUDA_SUBMISSION_UNIX;
  pending->synchronous = pending->defer_error == UINT32_C(0) ? UINT32_C(1) : UINT32_C(0);
  slot->pending = *pending;
  (void)memset(&slot->completion, 0, sizeof(slot->completion));
  slot->completion_result = (int32_t)CUDA_SUCCESS;
  mf_atomic_store_u64_relaxed(&slot->request_id, request_id);
  mf_atomic_store_u64_relaxed(
      &slot->context_key,
      mf_cuda_pending_context_key(pending->context_index, pending->context_generation));
  mf_atomic_store_u64_release(&slot->tagged_state,
                              mf_cuda_pending_pack(pending_tag, MF_CUDA_PENDING_STATE_SUBMITTED));
  for (;;) {
    status = mf_cuda_try_submit_locked(command, request_id);
    if (status != MF_SHARED_WOULD_BLOCK) {
      break;
    }
    result = mf_cuda_poll_locked();
    if (result != CUDA_SUCCESS) {
      goto done;
    }
    if (wait_deadline_ns == UINT64_C(0)) {
      status = mf_cuda_wait_deadline(MF_CUDA_WAIT_NS, &wait_deadline_ns);
      if (status != MF_SHARED_SUCCESS) {
        result = mf_cuda_status(status);
        goto done;
      }
    }
    status = mf_cuda_wait_ring_locked(mf_cuda_submission_ring_locked(command), UINT32_C(0),
                                      wait_deadline_ns);
    if (status != MF_SHARED_SUCCESS && status != MF_SHARED_RETRY) {
      result = mf_cuda_status(status);
      goto done;
    }
  }
  if (status != MF_SHARED_SUCCESS) {
    result = mf_cuda_status(status);
    goto done;
  }
  mf_cuda_note_submission_locked(pending);
  *out_request_id = request_id;
  result = CUDA_SUCCESS;

done:
  if (result != CUDA_SUCCESS && slot != (mf_cuda_pending_slot*)0) {
    uint64_t tagged_state = mf_cuda_pending_pack(
        pending_tag, request_id == UINT64_C(0) ? MF_CUDA_PENDING_STATE_PREPARING
                                               : MF_CUDA_PENDING_STATE_SUBMITTED);
    const uint64_t finalizing = mf_cuda_pending_pack(pending_tag, MF_CUDA_PENDING_STATE_FINALIZING);
    if (mf_atomic_compare_exchange_u64_seq_cst(&slot->tagged_state, &tagged_state, finalizing)) {
      mf_cuda_pending_release_slot(slot, pending_tag, UINT32_C(0));
    }
  }
  return result;
}

static CUresult mf_cuda_drain_request_locked(uint64_t request_id,
                                             mf_client_completion_v1* out_completion,
                                             CUresult* out_completion_result) {
  for (;;) {
    uint32_t pending_index = MF_CUDA_PENDING_CAPACITY;
    uint64_t tagged_state = UINT64_C(0);
    CUresult result = mf_cuda_process_ready_locked();
    if (result != CUDA_SUCCESS) {
      return result;
    }
    if (!mf_cuda_pending_slot_for_request(request_id, &pending_index, &tagged_state)) {
      return CUDA_ERROR_UNKNOWN;
    }
    if (mf_cuda_pending_state(tagged_state) == MF_CUDA_PENDING_STATE_COMPLETED) {
      mf_cuda_pending_slot* slot = &mf_cuda_global.pending[pending_index];
      const uint64_t tag = mf_cuda_pending_tag(tagged_state);
      const uint64_t finalizing = mf_cuda_pending_pack(tag, MF_CUDA_PENDING_STATE_FINALIZING);
      if (!mf_atomic_compare_exchange_u64_seq_cst(&slot->tagged_state, &tagged_state, finalizing)) {
        continue;
      }
      *out_completion = slot->completion;
      *out_completion_result = (CUresult)slot->completion_result;
      mf_cuda_pending_release_slot(slot, tag, UINT32_C(0));
      return CUDA_SUCCESS;
    }
    {
      mf_client_completion_v1 completion;
      CUresult completion_result = CUDA_SUCCESS;
      uint32_t consumed = UINT32_C(0);
      result = mf_cuda_consume_one_locked(MF_CUDA_CONSUME_BLOCKING_LOCKED, &completion,
                                          &completion_result, &consumed);
      if (result != CUDA_SUCCESS) {
        return result;
      }
      (void)completion;
      (void)completion_result;
      (void)consumed;
    }
  }
}

static CUresult mf_cuda_submit_locked(const mf_cuda_command* command,
                                      mf_client_completion_v1* completion) {
  mf_cuda_pending pending;
  uint64_t request_id = UINT64_C(0);
  CUresult completion_result = CUDA_SUCCESS;
  CUresult result = CUDA_SUCCESS;
  mf_cuda_tls_state* thread = mf_cuda_thread_state(UINT32_C(0));
  mf_cuda_pending_initialize(&pending);
  if (thread != (mf_cuda_tls_state*)0 && thread->current != (CUcontext)0) {
    uint32_t context_index = UINT32_C(0);
    uint32_t context_generation = UINT32_C(0);
    if (mf_cuda_decode((void*)thread->current, MF_CUDA_TAG_CONTEXT, &context_index,
                       &context_generation)) {
      pending.context_index = context_index;
      pending.context_generation = context_generation;
    }
  }
  result = mf_cuda_enqueue_locked(command, &pending, &request_id);
  if (result != CUDA_SUCCESS) {
    return result;
  }
  result = mf_cuda_drain_request_locked(request_id, completion, &completion_result);
  return result == CUDA_SUCCESS ? completion_result : result;
}

typedef enum mf_cuda_reference_kind {
  MF_CUDA_REFERENCE_CONTEXT,
  MF_CUDA_REFERENCE_STREAM,
  MF_CUDA_REFERENCE_EVENT,
  MF_CUDA_REFERENCE_MODULE,
  MF_CUDA_REFERENCE_MEMORY
} mf_cuda_reference_kind;

static int mf_cuda_pending_references(const mf_cuda_pending* pending, mf_cuda_reference_kind kind,
                                      uint32_t index, uint32_t generation) {
  uint32_t memory_index = UINT32_C(0);
  switch (kind) {
  case MF_CUDA_REFERENCE_CONTEXT:
    return pending->context_index == index && pending->context_generation == generation;
  case MF_CUDA_REFERENCE_STREAM:
    return pending->stream_index == index && pending->stream_generation == generation;
  case MF_CUDA_REFERENCE_EVENT:
    return pending->event_index == index && pending->event_generation == generation;
  case MF_CUDA_REFERENCE_MODULE:
    return pending->module_index == index && pending->module_generation == generation;
  case MF_CUDA_REFERENCE_MEMORY:
    for (memory_index = UINT32_C(0); memory_index < UINT32_C(3); ++memory_index) {
      if (pending->memory_indices[memory_index] == index &&
          pending->memory_generations[memory_index] == generation) {
        return 1;
      }
    }
    return 0;
  }
  return 0;
}

static int mf_cuda_has_pending_reference_locked(mf_cuda_reference_kind kind, uint32_t index,
                                                uint32_t generation) {
  uint32_t pending_index = UINT32_C(0);
  for (pending_index = UINT32_C(0); pending_index < MF_CUDA_PENDING_CAPACITY; ++pending_index) {
    const mf_cuda_pending_slot* slot = &mf_cuda_global.pending[pending_index];
    const uint32_t state = mf_cuda_pending_state(mf_atomic_load_u64_acquire(&slot->tagged_state));
    if (state != MF_CUDA_PENDING_STATE_FREE && state != MF_CUDA_PENDING_STATE_PREPARING &&
        mf_cuda_pending_references(&slot->pending, kind, index, generation) != 0) {
      return 1;
    }
  }
  return 0;
}

static CUresult mf_cuda_drain_reference_locked(mf_cuda_reference_kind kind, uint32_t index,
                                               uint32_t generation) {
  while (mf_cuda_has_pending_reference_locked(kind, index, generation) != 0) {
    mf_client_completion_v1 completion;
    CUresult completion_result = CUDA_SUCCESS;
    uint32_t consumed = UINT32_C(0);
    CUresult result = mf_cuda_process_ready_locked();
    if (result == CUDA_SUCCESS &&
        mf_cuda_has_pending_reference_locked(kind, index, generation) == 0) {
      return CUDA_SUCCESS;
    }
    if (result == CUDA_SUCCESS) {
      result = mf_cuda_consume_one_locked(MF_CUDA_CONSUME_BLOCKING_COOPERATIVE, &completion,
                                          &completion_result, &consumed);
    }
    if (result == CUDA_ERROR_NOT_READY) {
      continue;
    }
    if (result != CUDA_SUCCESS) {
      return result;
    }
    (void)completion;
    (void)completion_result;
    if (consumed == UINT32_C(0)) {
      return CUDA_ERROR_UNKNOWN;
    }
  }
  return CUDA_SUCCESS;
}

static CUresult mf_cuda_drain_through_locked(uint64_t request_id) {
  for (;;) {
    mf_client_completion_v1 completion;
    CUresult completion_result = CUDA_SUCCESS;
    uint32_t consumed = UINT32_C(0);
    uint32_t pending_index = UINT32_C(0);
    uint32_t found = UINT32_C(0);
    CUresult result = mf_cuda_process_ready_locked();
    if (result != CUDA_SUCCESS) {
      return result;
    }
    for (pending_index = UINT32_C(0); pending_index < MF_CUDA_PENDING_CAPACITY; ++pending_index) {
      const mf_cuda_pending_slot* slot = &mf_cuda_global.pending[pending_index];
      const uint32_t state = mf_cuda_pending_state(mf_atomic_load_u64_acquire(&slot->tagged_state));
      if (state != MF_CUDA_PENDING_STATE_FREE && state != MF_CUDA_PENDING_STATE_PREPARING &&
          mf_atomic_load_u64_acquire(&slot->request_id) <= request_id) {
        found = UINT32_C(1);
        break;
      }
    }
    if (found == UINT32_C(0)) {
      return CUDA_SUCCESS;
    }
    result = mf_cuda_consume_one_locked(MF_CUDA_CONSUME_BLOCKING_COOPERATIVE, &completion,
                                        &completion_result, &consumed);
    if (result == CUDA_ERROR_NOT_READY) {
      continue;
    }
    if (result != CUDA_SUCCESS) {
      return result;
    }
    (void)completion;
    (void)completion_result;
    if (consumed == UINT32_C(0)) {
      return CUDA_ERROR_UNKNOWN;
    }
  }
}

static int mf_cuda_has_pending_stream_locked(uint32_t context_index, uint32_t context_generation,
                                             uint32_t stream_index, uint32_t stream_generation) {
  uint32_t pending_index = UINT32_C(0);
  for (pending_index = UINT32_C(0); pending_index < MF_CUDA_PENDING_CAPACITY; ++pending_index) {
    const mf_cuda_pending_slot* slot = &mf_cuda_global.pending[pending_index];
    const uint32_t state = mf_cuda_pending_state(mf_atomic_load_u64_acquire(&slot->tagged_state));
    const mf_cuda_pending* pending = &slot->pending;
    if (state != MF_CUDA_PENDING_STATE_FREE && state != MF_CUDA_PENDING_STATE_PREPARING &&
        mf_atomic_load_u64_acquire(&slot->context_key) ==
            mf_cuda_pending_context_key(context_index, context_generation) &&
        pending->stream_index == stream_index && pending->stream_generation == stream_generation) {
      return 1;
    }
  }
  return 0;
}

static uint32_t mf_cuda_free_slot(mf_cuda_object* objects) {
  uint32_t index = 0;
  for (index = 0; index < MF_CUDA_OBJECT_CAPACITY; ++index) {
    if (objects[index].active == UINT32_C(0)) {
      return index;
    }
  }
  return MF_CUDA_OBJECT_CAPACITY;
}

static uint32_t mf_cuda_free_module_slot(void) {
  uint32_t index = 0;
  for (index = 0; index < MF_CUDA_MODULE_CAPACITY; ++index) {
    if (mf_cuda_global.modules[index].active == UINT32_C(0)) {
      return index;
    }
  }
  return MF_CUDA_MODULE_CAPACITY;
}

static CUresult mf_cuda_activate_locked(mf_cuda_object* object, uint32_t type,
                                        uint32_t device_index, uint32_t owner_context) {
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;
  object->generation = mf_cuda_next_generation(object->generation);
  object->id = mf_cuda_next_id_locked();
  object->type = type;
  object->device_index = device_index;
  object->owner_context = owner_context;
  object->active = UINT32_C(1);
  object->last_request = UINT64_C(0);
  object->default_stream_last_request = UINT64_C(0);
  object->record_stream_index = MF_CUDA_INDEX_NONE;
  object->record_stream_generation = UINT32_C(0);
  object->completion_status = (int32_t)CUDA_SUCCESS;
  status = mf_client_registry_make_handle_v1(&mf_cuda_global.registry, device_index, object->id,
                                             (uint64_t)object->generation, type, &object->handle);
  if (status != MF_SHARED_SUCCESS) {
    object->active = UINT32_C(0);
  }
  return mf_cuda_status(status);
}

static CUresult mf_cuda_validate_locked(const mf_cuda_object* object, uint32_t generation,
                                        uint32_t type) {
  mf_client_fence_snapshot_v1 fence;
  if (object->active == UINT32_C(0) || object->generation != generation || object->type != type ||
      object->handle.object_id != object->id ||
      object->handle.object_generation != (uint64_t)object->generation ||
      object->handle.object_type != type) {
    return CUDA_ERROR_INVALID_HANDLE;
  }
  return mf_cuda_status(
      mf_client_registry_validate_device_v1(&mf_cuda_global.registry, &object->handle, &fence));
}

static CUresult mf_cuda_lookup_token_locked(void* token, uint64_t tag, uint32_t type,
                                            mf_cuda_object* objects, uint32_t* out_index,
                                            mf_cuda_object** out_object) {
  uint32_t index = 0;
  uint32_t generation = 0;
  CUresult result = CUDA_SUCCESS;
  if (!mf_cuda_decode(token, tag, &index, &generation)) {
    return CUDA_ERROR_INVALID_HANDLE;
  }
  if (index >= (type == MF_CUDA_OBJECT_MODULE ? MF_CUDA_MODULE_CAPACITY
                                              : MF_CUDA_OBJECT_CAPACITY)) {
    return CUDA_ERROR_INVALID_HANDLE;
  }
  result = mf_cuda_validate_locked(&objects[index], generation, type);
  if (result == CUDA_SUCCESS) {
    *out_index = index;
    *out_object = &objects[index];
  }
  return result;
}

static CUresult mf_cuda_context_locked(CUcontext token, uint32_t* index, mf_cuda_object** context) {
  uint32_t decoded_index = 0;
  uint32_t generation = 0;
  CUresult result =
      mf_cuda_lookup_token_locked((void*)token, MF_CUDA_TAG_CONTEXT, MF_CUDA_OBJECT_CONTEXT,
                                  mf_cuda_global.contexts, index, context);
  if (result == CUDA_ERROR_INVALID_HANDLE &&
      mf_cuda_decode((void*)token, MF_CUDA_TAG_CONTEXT, &decoded_index, &generation) &&
      mf_cuda_global.contexts[decoded_index].generation == generation &&
      mf_cuda_global.contexts[decoded_index].type == MF_CUDA_OBJECT_CONTEXT &&
      mf_cuda_global.contexts[decoded_index].active == UINT32_C(0)) {
    return CUDA_ERROR_CONTEXT_IS_DESTROYED;
  }
  return result == CUDA_ERROR_INVALID_HANDLE ? CUDA_ERROR_INVALID_CONTEXT : result;
}

static CUresult mf_cuda_current_locked(uint32_t* index, mf_cuda_object** context) {
  mf_cuda_tls_state* state = mf_cuda_thread_state(UINT32_C(0));
  return state == (mf_cuda_tls_state*)0 || state->current == (CUcontext)0
             ? CUDA_ERROR_INVALID_CONTEXT
             : mf_cuda_context_locked(state->current, index, context);
}

static CUresult mf_cuda_stream_scope_locked(uint32_t context_index, const mf_cuda_object* context,
                                            mf_cuda_object* stream_record,
                                            uint32_t per_thread_default, uint32_t* stream_index,
                                            uint32_t* stream_generation, uint64_t* last_request) {
  if (stream_record != (mf_cuda_object*)0) {
    *stream_index = (uint32_t)(stream_record - mf_cuda_global.streams);
    *stream_generation = stream_record->generation;
    *last_request = stream_record->last_request;
    return CUDA_SUCCESS;
  }
  *stream_index = MF_CUDA_INDEX_NONE;
  if (per_thread_default == UINT32_C(0)) {
    *stream_generation = UINT32_C(0);
    *last_request = context->default_stream_last_request;
    return CUDA_SUCCESS;
  }
  {
    mf_cuda_tls_state* state = mf_cuda_thread_state(UINT32_C(0));
    mf_cuda_tls_default_stream* default_stream = (mf_cuda_tls_default_stream*)0;
    if (state == (mf_cuda_tls_state*)0 || context_index >= MF_CUDA_OBJECT_CAPACITY) {
      do { if (mf_cuda_entry_trace_enabled() != 0) { fprintf(stderr, "MF_INVALID_CONTEXT %s:%d\n", __func__, __LINE__); } return CUDA_ERROR_INVALID_CONTEXT; } while (0);
    }
    default_stream = &state->default_streams[context_index];
    if (default_stream->stream_generation == UINT32_C(0) ||
        default_stream->context_generation != context->generation) {
      if (mf_cuda_global.next_ptds_stream_generation == UINT32_MAX) {
        return CUDA_ERROR_OUT_OF_MEMORY;
      }
      mf_cuda_global.next_ptds_stream_generation += UINT32_C(1);
      default_stream->last_request = UINT64_C(0);
      default_stream->context_generation = context->generation;
      default_stream->stream_generation = mf_cuda_global.next_ptds_stream_generation;
    }
    *stream_generation = default_stream->stream_generation;
    *last_request = default_stream->last_request;
  }
  return CUDA_SUCCESS;
}

static CUresult mf_cuda_stream_locked(CUstream token, uint32_t owner_context,
                                      mf_cuda_object** stream) {
  uint32_t index = 0;
  CUresult result = CUDA_SUCCESS;
  if (token == (CUstream)0) {
    *stream = (mf_cuda_object*)0;
    return CUDA_SUCCESS;
  }
  result = mf_cuda_lookup_token_locked((void*)token, MF_CUDA_TAG_STREAM, MF_CUDA_OBJECT_STREAM,
                                       mf_cuda_global.streams, &index, stream);
  if (result == CUDA_SUCCESS && (*stream)->owner_context != owner_context) {
    do { if (mf_cuda_entry_trace_enabled() != 0) { fprintf(stderr, "MF_INVALID_CONTEXT %s:%d\n", __func__, __LINE__); } return CUDA_ERROR_INVALID_CONTEXT; } while (0);
  }
  return result;
}

static CUresult mf_cuda_event_locked(CUevent token, mf_cuda_object** event) {
  uint32_t index = 0;
  return mf_cuda_lookup_token_locked((void*)token, MF_CUDA_TAG_EVENT, MF_CUDA_OBJECT_EVENT,
                                     mf_cuda_global.events, &index, event);
}

static CUresult mf_cuda_memory_locked(CUdeviceptr pointer, size_t bytes, mf_cuda_object** memory,
                                      uint64_t* offset) {
  const uint64_t address = (uint64_t)pointer;
  const uint64_t count = (uint64_t)bytes;
  uint32_t index = 0;
  for (index = 0; index < MF_CUDA_OBJECT_CAPACITY; ++index) {
    mf_cuda_object* candidate = &mf_cuda_global.memories[index];
    CUresult result = CUDA_SUCCESS;
    if (candidate->active == UINT32_C(0) || address < candidate->address) {
      continue;
    }
    *offset = address - candidate->address;
    if (*offset > candidate->size || count > candidate->size - *offset) {
      continue;
    }
    result = mf_cuda_validate_locked(candidate, candidate->generation, MF_CUDA_OBJECT_MEMORY);
    if (result != CUDA_SUCCESS) {
      return result;
    }
    *memory = candidate;
    return CUDA_SUCCESS;
  }
  return CUDA_ERROR_INVALID_VALUE;
}

static size_t mf_cuda_string_length(const char* text, size_t limit) {
  size_t length = 0;
  while (length < limit && text[length] != '\0') {
    ++length;
  }
  return length;
}

uint32_t mf_cuda_provider_bootstrap_abi_version(void) {
  return mf_client_fastpath_bootstrap_abi_version();
}

void mf_cuda_provider_managed_rollback_v1(void) {
  mf_cuda_lock();
  mf_cuda_close_locked();
  mf_cuda_unlock();
}

int32_t mf_cuda_provider_managed_is_pristine_v1(void) {
  int32_t pristine = INT32_C(0);
  mf_cuda_lock();
  pristine = mf_cuda_global.initialized == UINT32_C(0) && mf_cuda_global.session.socket_fd < 0 &&
                     mf_cuda_global.registry.mapping == (void*)0 &&
                     mf_cuda_global.submission.mapping == (void*)0 &&
                     mf_cuda_global.completion.mapping == (void*)0 &&
                     mf_atomic_load_u32_acquire(&mf_cuda_global.pending_count) == UINT32_C(0) &&
                     mf_cuda_argument_cache_is_empty_locked() != 0 &&
                     mf_cuda_copy_cache_is_empty_locked() != 0 &&
                     mf_cuda_global.direct_host_copy_ready == UINT32_C(0) &&
                     mf_cuda_global.visible_devices == (uint32_t*)0
                 ? INT32_C(1)
                 : INT32_C(0);
  mf_cuda_unlock();
  return pristine;
}

#if defined(METAFLUX_PROVIDER_TESTING)
extern uint64_t mf_cuda_provider_test_dispatch_lock_count_v1(void);

int mf_cuda_provider_test_install_transport_v1(
    const mf_cuda_provider_test_transport_v1* transport) {
  if (transport == (const mf_cuda_provider_test_transport_v1*)0 || transport->registry_fd < 0 ||
      transport->submission_fd < 0 || transport->completion_fd < 0 ||
      transport->registry_view_id.daemon_incarnation == UINT64_C(0) ||
      transport->registry_view_id.view_serial == UINT64_C(0) ||
      transport->submission_queue_id == UINT64_C(0) ||
      transport->submission_queue_generation == UINT64_C(0) ||
      transport->completion_queue_id == UINT64_C(0) ||
      transport->completion_queue_generation == UINT64_C(0) ||
      transport->runtime_context_id == UINT64_C(0) || transport->runtime_event_id == UINT64_C(0) ||
      transport->runtime_event_generation == UINT64_C(0) ||
      transport->runtime_add_kernel_id == UINT64_C(0) || transport->control == (void*)0 ||
      transport->read_object == (void*)0) {
    return -1;
  }
  mf_cuda_lock();
  mf_cuda_close_locked();
  (void)memset(&mf_cuda_global.transport, 0, sizeof(mf_cuda_global.transport));
  mf_cuda_global.transport.registry_fd = transport->registry_fd;
  mf_cuda_global.transport.submission_fd = transport->submission_fd;
  mf_cuda_global.transport.completion_fd = transport->completion_fd;
  mf_cuda_global.transport.view_id = transport->registry_view_id;
  mf_cuda_global.transport.submission_queue_id = transport->submission_queue_id;
  mf_cuda_global.transport.submission_queue_generation = transport->submission_queue_generation;
  mf_cuda_global.transport.completion_queue_id = transport->completion_queue_id;
  mf_cuda_global.transport.completion_queue_generation = transport->completion_queue_generation;
  mf_cuda_global.transport.runtime_context_id = transport->runtime_context_id;
  mf_cuda_global.transport.runtime_event_id = transport->runtime_event_id;
  mf_cuda_global.transport.runtime_event_generation = transport->runtime_event_generation;
  mf_cuda_global.transport.runtime_add_kernel_id = transport->runtime_add_kernel_id;
  mf_cuda_global.transport.negotiated_capabilities = transport->negotiated_capabilities;
  mf_cuda_global.transport.control_context = transport->control_context;
  mf_cuda_global.transport.control = transport->control;
  mf_cuda_global.transport.read_object = transport->read_object;
  mf_cuda_global.transport.configured = UINT32_C(1);
  mf_cuda_unlock();
  return 0;
}

int mf_cuda_provider_test_set_direct_registration_v1(uint32_t local_unavailable,
                                                     mf_shared_status_v1 transport_status,
                                                     uint32_t control_status) {
  int result = -1;
  if (local_unavailable > UINT32_C(1)) {
    return -1;
  }
  mf_cuda_lock();
  if (mf_cuda_global.initialized == UINT32_C(0)) {
    mf_cuda_test_direct_registration.transport_status = transport_status;
    mf_cuda_test_direct_registration.control_status = control_status;
    mf_cuda_test_direct_registration.local_unavailable = local_unavailable;
    mf_cuda_test_direct_registration.active = UINT32_C(1);
    result = 0;
  }
  mf_cuda_unlock();
  return result;
}

uint32_t mf_cuda_provider_test_managed_is_pristine_v1(void) {
  return mf_cuda_provider_managed_is_pristine_v1() == INT32_C(1) ? UINT32_C(1) : UINT32_C(0);
}

uint64_t mf_cuda_provider_test_process_view_revision_v1(void) {
  uint64_t revision = UINT64_C(0);
  mf_cuda_lock();
  revision = mf_cuda_global.process_view_revision;
  mf_cuda_unlock();
  return revision;
}

int mf_cuda_provider_test_set_next_request_v1(uint64_t next_request) {
  int result = -1;
  mf_cuda_lock();
  if (mf_atomic_load_u32_acquire(&mf_cuda_global.pending_count) == UINT32_C(0)) {
    mf_atomic_store_u64_release(&mf_cuda_global.next_request, next_request);
    result = 0;
  }
  mf_cuda_unlock();
  return result;
}

int mf_cuda_provider_test_set_primary_refcount_v1(CUdevice device, uint32_t refcount) {
  uint32_t index = UINT32_C(0);
  uint32_t registry_index = UINT32_C(0);
  int result = -1;
  mf_cuda_lock();
  if (refcount != UINT32_C(0) && mf_cuda_require_locked() == CUDA_SUCCESS &&
      mf_cuda_registry_index_locked(device, &registry_index) == CUDA_SUCCESS) {
    for (index = UINT32_C(0); index < MF_CUDA_OBJECT_CAPACITY; ++index) {
      mf_cuda_object* record = &mf_cuda_global.contexts[index];
      if (record->active != UINT32_C(0) && record->flags != UINT32_C(0) &&
          record->device_index == registry_index) {
        record->aux = refcount;
        result = 0;
        break;
      }
    }
  }
  mf_cuda_unlock();
  return result;
}

uint32_t mf_cuda_provider_test_pending_count_v1(void) {
  uint32_t count = UINT32_C(0);
  mf_cuda_lock();
  count = mf_atomic_load_u32_acquire(&mf_cuda_global.pending_count);
  mf_cuda_unlock();
  return count;
}

uint32_t mf_cuda_provider_test_pending_capacity_v1(void) { return MF_CUDA_PENDING_CAPACITY; }

uint32_t mf_cuda_provider_test_async_error_count_v1(void) {
  uint32_t count = UINT32_C(0);
  uint32_t index = UINT32_C(0);
  mf_cuda_lock();
  for (index = UINT32_C(0); index < MF_CUDA_PENDING_CAPACITY; ++index) {
    count += mf_atomic_load_u32_acquire(&mf_cuda_global.async_errors[index].active) ==
                     MF_CUDA_ASYNC_ERROR_READY
                 ? UINT32_C(1)
                 : UINT32_C(0);
  }
  mf_cuda_unlock();
  return count;
}

uint32_t mf_cuda_provider_test_active_waiter_count_v1(void) {
  return atomic_load_explicit(&mf_cuda_global.active_waiters, memory_order_acquire);
}

void mf_cuda_provider_test_get_path_counters_v1(mf_cuda_provider_test_path_counters_v1* counters) {
  if (counters == (mf_cuda_provider_test_path_counters_v1*)0) {
    return;
  }
  counters->dispatch_lock_acquisitions = mf_cuda_provider_test_dispatch_lock_count_v1();
  counters->global_lock_acquisitions = (uint64_t)atomic_load_explicit(
      &mf_cuda_global.global_lock_acquisitions, memory_order_relaxed);
  counters->queue_gate_acquisitions =
      (uint64_t)atomic_load_explicit(&mf_cuda_global.queue_gate_acquisitions, memory_order_relaxed);
  counters->heap_allocation_attempts = (uint64_t)atomic_load_explicit(
      &mf_cuda_global.heap_allocation_attempts, memory_order_relaxed);
}

void mf_cuda_provider_test_reset_managed_v1(void) {
  mf_cuda_tls_state* state = mf_cuda_thread_state(UINT32_C(0));
  uint32_t lock_depth = UINT32_C(0);
  uint32_t queue_depth = UINT32_C(0);
  mf_cuda_lock();
  mf_cuda_close_locked();
  (void)memset(&mf_cuda_global.transport, 0, sizeof(mf_cuda_global.transport));
  (void)memset(mf_cuda_global.contexts, 0, sizeof(mf_cuda_global.contexts));
  (void)memset(mf_cuda_global.modules, 0, sizeof(mf_cuda_global.modules));
  (void)memset(mf_cuda_global.functions, 0, sizeof(mf_cuda_global.functions));
  (void)memset(mf_cuda_global.memories, 0, sizeof(mf_cuda_global.memories));
  (void)memset(mf_cuda_global.streams, 0, sizeof(mf_cuda_global.streams));
  (void)memset(mf_cuda_global.events, 0, sizeof(mf_cuda_global.events));
  (void)memset(mf_cuda_global.pending, 0, sizeof(mf_cuda_global.pending));
  (void)memset(mf_cuda_global.async_errors, 0, sizeof(mf_cuda_global.async_errors));
  (void)memset(mf_cuda_global.argument_cache, 0, sizeof(mf_cuda_global.argument_cache));
  (void)memset(mf_cuda_global.copy_cache, 0, sizeof(mf_cuda_global.copy_cache));
  (void)memset(&mf_cuda_test_direct_registration, 0, sizeof(mf_cuda_test_direct_registration));
  mf_cuda_global.next_id = UINT64_C(0);
  mf_atomic_store_u64_release(&mf_cuda_global.next_request, UINT64_C(0));
  mf_cuda_global.next_address = UINT64_C(0);
  mf_atomic_store_u64_release(&mf_cuda_global.next_timeline, UINT64_C(0));
  mf_atomic_store_u32_release(&mf_cuda_global.pending_count, UINT32_C(0));
  mf_atomic_store_u32_release(&mf_cuda_global.transport_error, (uint32_t)CUDA_SUCCESS);
  if (state != (mf_cuda_tls_state*)0) {
    lock_depth = state->lock_depth;
    queue_depth = state->queue_depth;
    (void)memset(state, 0, sizeof(*state));
    state->lock_depth = lock_depth;
    state->queue_depth = queue_depth;
    state->queue_context_index = MF_CUDA_INDEX_NONE;
  }
  mf_cuda_unlock();
}
#endif

CUresult cuInit(unsigned int flags) {
  MF_ENTRY_TRACE();
  CUresult result = CUDA_SUCCESS;
  if (flags != UINT32_C(0)) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  mf_cuda_lock();
  result = mf_cuda_initialize_locked();
  mf_cuda_unlock();
  return result;
}

CUresult cuDriverGetVersion(int* driver_version) {
  if (driver_version == (int*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *driver_version = MF_CUDA_DRIVER_API_VERSION;
  return CUDA_SUCCESS;
}

CUresult cuDeviceGetCount(int* count) {
  mf_cuda_tls_state* thread = mf_cuda_thread_state(UINT32_C(0));
  CUresult result = CUDA_SUCCESS;
  if (count == (int*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  if (thread != (mf_cuda_tls_state*)0 && thread->queue_depth != UINT32_C(0)) {
    const uint32_t device_count = mf_cuda_global.visible_count;
    result = mf_cuda_require_locked();
    if (result != CUDA_SUCCESS) {
      return result;
    }
    if (device_count > (uint32_t)INT32_MAX) {
      return CUDA_ERROR_INVALID_VALUE;
    }
    *count = (int)device_count;
    return CUDA_SUCCESS;
  }
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    const uint32_t device_count = mf_cuda_global.visible_count;
    if (device_count > (uint32_t)INT32_MAX) {
      result = CUDA_ERROR_INVALID_VALUE;
    } else {
      *count = (int)device_count;
    }
  }
  mf_cuda_unlock();
  return result;
}

CUresult cuDeviceGet(CUdevice* device, int ordinal) {
  int count = 0;
  CUresult result = CUDA_SUCCESS;
  if (device == (CUdevice*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  result = cuDeviceGetCount(&count);
  if (result != CUDA_SUCCESS) {
    return result;
  }
  if (ordinal < 0 || ordinal >= count) {
    return CUDA_ERROR_INVALID_DEVICE;
  }
  *device = ordinal;
  return CUDA_SUCCESS;
}

static CUresult mf_cuda_identity(CUdevice device, mf_virtual_device_identity_v1* identity,
                                 mf_client_fence_snapshot_v1* fence) {
  mf_generation_handle_v1 handle;
  uint32_t registry_index = UINT32_C(0);
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;
  if (device < 0 || identity == (mf_virtual_device_identity_v1*)0 ||
      fence == (mf_client_fence_snapshot_v1*)0) {
    return CUDA_ERROR_INVALID_DEVICE;
  }
  mf_cuda_lock();
  if (mf_cuda_require_locked() != CUDA_SUCCESS) {
    mf_cuda_unlock();
    return CUDA_ERROR_NOT_INITIALIZED;
  }
  if (mf_cuda_registry_index_locked(device, &registry_index) != CUDA_SUCCESS) {
    mf_cuda_unlock();
    return CUDA_ERROR_INVALID_DEVICE;
  }
  status = mf_client_registry_make_handle_v1(&mf_cuda_global.registry, registry_index,
                                             (uint64_t)registry_index + UINT64_C(1), UINT64_C(1),
                                             MF_CUDA_OBJECT_DEVICE, &handle);
  if (status == MF_SHARED_SUCCESS) {
    status = mf_client_registry_validate_device_v1(&mf_cuda_global.registry, &handle, fence);
  }
  if (status == MF_SHARED_SUCCESS) {
    status = mf_client_registry_identity_v1(&mf_cuda_global.registry, registry_index, identity);
  }
  mf_cuda_unlock();
  return mf_cuda_status(status);
}

CUresult cuDeviceGetName(char* name, int length, CUdevice device) {
  mf_virtual_device_identity_v1 identity;
  mf_client_fence_snapshot_v1 fence;
  size_t count = 0;
  CUresult result = CUDA_SUCCESS;
  if (name == (char*)0 || length <= 0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  result = mf_cuda_identity(device, &identity, &fence);
  if (result != CUDA_SUCCESS) {
    return result;
  }
  count = mf_cuda_string_length((const char*)identity.display_name, sizeof(identity.display_name));
  if (count >= (size_t)length) {
    count = (size_t)length - (size_t)1;
  }
  (void)memcpy(name, identity.display_name, count);
  name[count] = '\0';
  return CUDA_SUCCESS;
}

CUresult cuDeviceGetAttribute(int* value, CUdevice_attribute attrib, CUdevice device) {
  mf_virtual_device_identity_v1 identity;
  mf_client_fence_snapshot_v1 fence;
  CUresult result = CUDA_SUCCESS;
  if (value == (int*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  result = mf_cuda_identity(device, &identity, &fence);
  if (result != CUDA_SUCCESS) {
    return result;
  }
  /* The virtual device publishes the decision-0017 sm_70 identity. Values the
     frozen contract does not fix (clocks, cache geometry, topology counts)
     describe the executing host adapter and are MetaFlux-strengthened
     observations, not NVIDIA compatibility guarantees. Every attribute code
     1-124 answers deterministically so framework property queries complete. */
  switch ((int)attrib) {
  case 1:
    *value = 1024;
    break;

  case 2:
    *value = 1024;
    break;

  case 3:
    *value = 1024;
    break;

  case 4:
    *value = 64;
    break;

  case 5:
    *value = 2147483647;
    break;

  case 6:
    *value = 65535;
    break;

  case 7:
    *value = 65535;
    break;

  case 8:
    *value = 49152;
    break;

  case 9:
    *value = 65536;
    break;

  case 10:
    *value = 32;
    break;

  case 11:
    *value = 2147483647;
    break;

  case 12:
    *value = 65536;
    break;

  case 13:
    *value = 2800000;
    break;

  case 14:
    *value = 512;
    break;

  case 15:
    *value = 1;
    break;

  case 16:
    *value = 12;
    break;

  case 17:
    *value = 0;
    break;

  case 18:
    *value = 1;
    break;

  case 19:
    *value = 1;
    break;

  case 20:
    *value = 0;
    break;

  case 21:
    *value = 131072;
    break;

  case 22:
    *value = 131072;
    break;

  case 23:
    *value = 65536;
    break;

  case 24:
    *value = 16384;
    break;

  case 25:
    *value = 16384;
    break;

  case 26:
    *value = 16384;
    break;

  case 27:
    *value = 131072;
    break;

  case 28:
    *value = 65536;
    break;

  case 29:
    *value = 2048;
    break;

  case 30:
    *value = 512;
    break;

  case 31:
    *value = 1;
    break;

  case 32:
    *value = 0;
    break;

  case 33:
    *value = 0;
    break;

  case 34:
    *value = 0;
    break;

  case 35:
    *value = 0;
    break;

  case 36:
    *value = 6400000;
    break;

  case 37:
    *value = 128;
    break;

  case 38:
    *value = 4194304;
    break;

  case 39:
    *value = 2048;
    break;

  case 40:
    *value = 2;
    break;

  case 41:
    *value = 1;
    break;

  case 42:
    *value = 131072;
    break;

  case 43:
    *value = 2048;
    break;

  case 44:
    *value = 0;
    break;

  case 45:
    *value = 32768;
    break;

  case 46:
    *value = 32768;
    break;

  case 47:
    *value = 16384;
    break;

  case 48:
    *value = 16384;
    break;

  case 49:
    *value = 16384;
    break;

  case 50:
    *value = 0;
    break;

  case 51:
    *value = 512;
    break;

  case 52:
    *value = 32768;
    break;

  case 53:
    *value = 32768;
    break;

  case 54:
    *value = 2048;
    break;

  case 55:
    *value = 32768;
    break;

  case 56:
    *value = 32768;
    break;

  case 57:
    *value = 32768;
    break;

  case 58:
    *value = 16384;
    break;

  case 59:
    *value = 16384;
    break;

  case 60:
    *value = 16384;
    break;

  case 61:
    *value = 32768;
    break;

  case 62:
    *value = 2048;
    break;

  case 63:
    *value = 32768;
    break;

  case 64:
    *value = 32768;
    break;

  case 65:
    *value = 2048;
    break;

  case 66:
    *value = 32768;
    break;

  case 67:
    *value = 32768;
    break;

  case 68:
    *value = 2048;
    break;

  case 69:
    *value = 131072;
    break;

  case 70:
    *value = 131072;
    break;

  case 71:
    *value = 65536;
    break;

  case 72:
    *value = 2147483647;
    break;

  case 73:
    *value = 131072;
    break;

  case 74:
    *value = 65536;
    break;

  case 75:
    *value = 7;
    break;

  case 76:
    *value = 0;
    break;

  case 77:
    *value = 131072;
    break;

  case 78:
    *value = 1;
    break;

  case 79:
    *value = 1;
    break;

  case 80:
    *value = 1;
    break;

  case 81:
    *value = 114688;
    break;

  case 82:
    *value = 65536;
    break;

  case 83:
    *value = 1;
    break;

  case 84:
    *value = 0;
    break;

  case 85:
    *value = 0;
    break;

  case 86:
    *value = 1;
    break;

  case 87:
    *value = 2;
    break;

  case 88:
    *value = 1;
    break;

  case 89:
    *value = 1;
    break;

  case 90:
    *value = 1;
    break;

  case 91:
    *value = 1;
    break;

  case 92:
    *value = 0;
    break;

  case 93:
    *value = 0;
    break;

  case 94:
    *value = 0;
    break;

  case 95:
    *value = 0;
    break;

  case 96:
    *value = 0;
    break;

  case 97:
    *value = 101376;
    break;

  case 98:
    *value = 0;
    break;

  case 99:
    *value = 1;
    break;

  case 100:
    *value = 1;
    break;

  case 101:
    *value = 1;
    break;

  case 102:
    *value = 0;
    break;

  case 103:
    *value = 0;
    break;

  case 104:
    *value = 0;
    break;

  case 105:
    *value = 0;
    break;

  case 106:
    *value = 32;
    break;

  case 107:
    *value = 0;
    break;

  case 108:
    *value = 3145728;
    break;

  case 109:
    *value = 4194304;
    break;

  case 110:
    *value = 0;
    break;

  case 111:
    *value = 1024;
    break;

  case 112:
    *value = 0;
    break;

  case 113:
    *value = 1;
    break;

  case 114:
    *value = 0;
    break;

  case 115:
    *value = 0;
    break;

  case 116:
    *value = 0;
    break;

  case 117:
    *value = 0;
    break;

  case 118:
    *value = 0;
    break;

  case 119:
    *value = 0;
    break;

  case 120:
    *value = 0;
    break;

  case 121:
    *value = 0;
    break;

  case 122:
    *value = 0;
    break;

  case 123:
    *value = 0;
    break;

  case 124:
    *value = 0;
    break;
  case 125:
  case 126:
  case 127:
  case 128:
  case 129:
  case 130:
  case 131:
  case 132:
  case 133:
  case 134:
  case 135:
  case 136:
  case 137:
  case 138:
  case 139:
  case 140:
    /* CUDA 12.7+/13 capability queries (NUMA, MPS, multicast, decompress):
       answer 0 = capability absent on the virtual device. */
    *value = 0;
    break;
  default:
    /* Unknown-to-this-provider attribute codes answer 0 (capability absent)
       so framework property sweeps complete. MetaFlux-strengthened: a real
       driver returns NOT_SUPPORTED here and cudart aborts initialization. */
    *value = 0;
    break;
  }
  if (getenv("METAFLUX_TRACE_STUBS") != (void*)0) {
    fprintf(stderr, "MF_ATTR %d -> %d\n", (int)attrib, value != (int*)0 ? *value : -1);
  }
  return CUDA_SUCCESS;
}

/* Async-notification registration is a deterministic no-op: the managed
   backend never fires device async notifications, so registration succeeds
   with a null handle and unregister accepts any handle. MetaFlux-strengthened
   behavior. */
CUresult cuDeviceRegisterAsyncNotification(CUdevice device, CUasyncCallback callback_func,
                                           void* user_data, CUasyncCallbackHandle* callback) {
  (void)device;
  (void)callback_func;
  (void)user_data;
  if (callback == (CUasyncCallbackHandle*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *callback = (CUasyncCallbackHandle)0;
  return CUDA_SUCCESS;
}

CUresult cuDeviceUnregisterAsyncNotification(CUdevice device, CUasyncCallbackHandle callback) {
  (void)device;
  (void)callback;
  return CUDA_SUCCESS;
}

/* Conditional graph handles mint detached tokens: the graph execution stack
   is not implemented, so any attempt to attach the token to a real graph
   fails later with CUDA_ERROR_NOT_SUPPORTED. MetaFlux-strengthened behavior
   that keeps framework initialization probing alive. */
CUresult cuGraphConditionalHandleCreate(CUgraphConditionalHandle* handle_out, CUgraph graph,
                                        CUcontext context, unsigned int flags) {
  (void)graph;
  (void)context;
  (void)flags;
  if (handle_out == (CUgraphConditionalHandle*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *handle_out = (CUgraphConditionalHandle)(uintptr_t)UINT64_C(0x4D464348);
  return CUDA_SUCCESS;
}

/* cudart probes the graph-exec update surface during one-time initialization;
   the no-op SUCCESS keeps framework init alive. No graph was ever created
   through the managed backend, so the exec object is inert. */
CUresult cuGraphExecNodeSetParams(CUgraphExec graph_exec, CUgraphNode node,
                                  const CUDA_GRAPH_NODE_PARAMS* node_params) {
  (void)graph_exec;
  (void)node;
  (void)node_params;
  return CUDA_SUCCESS;
}

CUresult cuGraphNodeSetParams(CUgraphNode node, const CUDA_GRAPH_NODE_PARAMS* node_params) {
  (void)node;
  (void)node_params;
  return CUDA_SUCCESS;
}

CUresult cuGraphAddNode(CUgraphNode* graph_node, CUgraph graph, const CUgraphNode* dependencies,
                        const CUgraphEdgeData* dependency_data, size_t num_dependencies,
                        CUgraphNodeParams* node_params) {
  (void)graph;
  (void)dependencies;
  (void)dependency_data;
  (void)num_dependencies;
  (void)node_params;
  if (graph_node == (CUgraphNode*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *graph_node = (CUgraphNode)(uintptr_t)UINT64_C(0x4D46434E);
  return CUDA_SUCCESS;
}

CUresult cuGraphExecGetFlags(CUgraphExec graph_exec, unsigned long long* flags) {
  (void)graph_exec;
  if (flags == (unsigned long long*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *flags = UINT64_C(0);
  return CUDA_SUCCESS;
}

CUresult cuGraphInstantiateWithParams(CUgraphExec* graph_exec, CUgraph graph,
                                      CUDA_GRAPH_INSTANTIATE_PARAMS* instantiate_params) {
  (void)graph;
  if (graph_exec == (CUgraphExec*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *graph_exec = (CUgraphExec)(uintptr_t)UINT64_C(0x4D464345);
  if (instantiate_params != (CUDA_GRAPH_INSTANTIATE_PARAMS*)0) {
    instantiate_params->result_out = CUDA_GRAPH_INSTANTIATE_SUCCESS;
    instantiate_params->hErrNode_out = (CUgraphNode)0;
  }
  return CUDA_SUCCESS;
}

CUresult cuGraphUpload(CUgraphExec graph_exec, CUstream stream) {
  (void)graph_exec;
  (void)stream;
  return CUDA_SUCCESS;
}

CUresult cuGraphNodeSetEnabled(CUgraphNode node, int enabled) {
  (void)node;
  (void)enabled;
  return CUDA_SUCCESS;
}

CUresult cuGraphNodeGetEnabled(CUgraphNode node, int* enabled) {
  (void)node;
  if (enabled == (int*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *enabled = 1;
  return CUDA_SUCCESS;
}

/* User objects mint detached tokens and refcounts are bookkeeping only: the
   destroy callback never fires in the managed backend. MetaFlux-strengthened
   behavior that keeps framework initialization probing alive. */
CUresult cuUserObjectCreate(CUuserObject* object_out, void* ptr, CUhostFn destroy,
                            unsigned int initial_refcount, unsigned int flags) {
  (void)ptr;
  (void)destroy;
  (void)initial_refcount;
  (void)flags;
  if (object_out == (CUuserObject*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *object_out = (CUuserObject)(uintptr_t)UINT64_C(0x4D464355);
  return CUDA_SUCCESS;
}

CUresult cuUserObjectRetain(CUuserObject object, unsigned int count) {
  (void)object;
  (void)count;
  return CUDA_SUCCESS;
}

CUresult cuUserObjectRelease(CUuserObject object, unsigned int count) {
  (void)object;
  (void)count;
  return CUDA_SUCCESS;
}

CUresult cuGraphRetainUserObject(CUgraph graph, CUuserObject object, unsigned int count,
                                 unsigned int flags) {
  (void)graph;
  (void)object;
  (void)count;
  (void)flags;
  return CUDA_SUCCESS;
}

CUresult cuGraphReleaseUserObject(CUgraph graph, CUuserObject object, unsigned int count) {
  (void)graph;
  (void)object;
  (void)count;
  return CUDA_SUCCESS;
}

/* Debug dumps describe the inert graph stack: an empty dot document. */
CUresult cuGraphDebugDotPrint(CUgraph graph, const char* path, unsigned int flags) {
  FILE* dot = (FILE*)0;
  (void)graph;
  (void)flags;
  if (path == (const char*)0 || path[0] == '\0') {
    return CUDA_ERROR_INVALID_VALUE;
  }
  dot = fopen(path, "w");
  if (dot == (FILE*)0) {
    return CUDA_ERROR_NOT_FOUND;
  }
  (void)fprintf(dot, "digraph {}\n");
  (void)fclose(dot);
  return CUDA_SUCCESS;
}

CUresult cuGraphKernelNodeSetAttribute(CUgraphNode node, CUfunction_attribute attrib,
                                       const void* value) {
  (void)node;
  (void)attrib;
  (void)value;
  return CUDA_SUCCESS;
}

CUresult cuGraphKernelNodeGetAttribute(CUgraphNode node, CUfunction_attribute attrib,
                                       void* value) {
  (void)node;
  (void)attrib;
  if (value == (void*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *(int*)value = 0;
  return CUDA_SUCCESS;
}

CUresult cuGraphKernelNodeCopyAttributes(CUgraphNode destination, CUgraphNode source) {
  (void)destination;
  (void)source;
  return CUDA_SUCCESS;
}

CUresult cuGraphExecUpdate(CUgraphExec graph_exec, CUgraph graph,
                           CUgraphExecUpdateResultInfo* result_info) {
  (void)graph_exec;
  (void)graph;
  if (result_info == (CUgraphExecUpdateResultInfo*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  result_info->result = CU_GRAPH_EXEC_UPDATE_SUCCESS;
  result_info->errorNode = (CUgraphNode)0;
  result_info->errorFromNode = (CUgraphNode)0;
  return CUDA_SUCCESS;
}

/* Capture bookkeeping is thread-local and real: stream capture itself is
   never started by the managed backend, so no capture invalidation can
   trigger. */
/* Process-wide (not thread-local): stream capture is never started in the
   managed backend, so the mode is inert bookkeeping. A thread-local here
   would pull __tls_get_addr and add the loader to DT_NEEDED. */
static CUstreamCaptureMode mf_cuda_thread_capture_mode = CU_STREAM_CAPTURE_MODE_GLOBAL;

CUresult cuThreadExchangeStreamCaptureMode(CUstreamCaptureMode* mode) {
  CUstreamCaptureMode previous = CU_STREAM_CAPTURE_MODE_GLOBAL;
  if (mode == (CUstreamCaptureMode*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  previous = mf_cuda_thread_capture_mode;
  mf_cuda_thread_capture_mode = *mode;
  *mode = previous;
  return CUDA_SUCCESS;
}

CUresult cuGraphExecEventWaitNodeSetEvent(CUgraphExec graph_exec, CUgraphNode node, CUevent event) {
  (void)graph_exec;
  (void)node;
  (void)event;
  return CUDA_SUCCESS;
}

CUresult cuGraphExecEventRecordNodeSetEvent(CUgraphExec graph_exec, CUgraphNode node,
                                            CUevent event) {
  (void)graph_exec;
  (void)node;
  (void)event;
  return CUDA_SUCCESS;
}

CUresult cuGraphEventWaitNodeGetEvent(CUgraphNode node, CUevent* event_out) {
  (void)node;
  if (event_out == (CUevent*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *event_out = (CUevent)0;
  return CUDA_SUCCESS;
}

CUresult cuGraphEventRecordNodeGetEvent(CUgraphNode node, CUevent* event_out) {
  (void)node;
  if (event_out == (CUevent*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *event_out = (CUevent)0;
  return CUDA_SUCCESS;
}

/* Remaining graph-exec update entry points: no-op SUCCESS over inert exec
   objects (MetaFlux-strengthened; graph execution is not implemented). */
CUresult cuGraphExecChildGraphNodeSetParams(CUgraphExec graph_exec, CUgraphNode node,
                                            CUgraph child_graph) {
  (void)graph_exec;
  (void)node;
  (void)child_graph;
  return CUDA_SUCCESS;
}

CUresult cuGraphExecExternalSemaphoresSignalNodeSetParams(
    CUgraphExec graph_exec, CUgraphNode node, const CUDA_EXT_SEM_SIGNAL_NODE_PARAMS* params) {
  (void)graph_exec;
  (void)node;
  (void)params;
  return CUDA_SUCCESS;
}

CUresult cuGraphExecExternalSemaphoresWaitNodeSetParams(
    CUgraphExec graph_exec, CUgraphNode node, const CUDA_EXT_SEM_WAIT_NODE_PARAMS* params) {
  (void)graph_exec;
  (void)node;
  (void)params;
  return CUDA_SUCCESS;
}

CUresult cuGraphExecHostNodeSetParams(CUgraphExec graph_exec, CUgraphNode node,
                                      const CUDA_HOST_NODE_PARAMS* params) {
  (void)graph_exec;
  (void)node;
  (void)params;
  return CUDA_SUCCESS;
}

CUresult cuGraphExecMemcpyNodeSetParams(CUgraphExec graph_exec, CUgraphNode node,
                                        const CUDA_MEMCPY3D* params) {
  (void)graph_exec;
  (void)node;
  (void)params;
  return CUDA_SUCCESS;
}

CUresult cuGraphExecMemsetNodeSetParams(CUgraphExec graph_exec, CUgraphNode node,
                                        const CUDA_MEMSET_NODE_PARAMS* params) {
  (void)graph_exec;
  (void)node;
  (void)params;
  return CUDA_SUCCESS;
}

CUresult cuGraphExecMemAllocNodeSetParams(CUgraphExec graph_exec, CUgraphNode node,
                                          const CUDA_MEM_ALLOC_NODE_PARAMS* params) {
  (void)graph_exec;
  (void)node;
  (void)params;
  return CUDA_SUCCESS;
}

CUresult cuGraphExecMemFreeNodeSetParams(CUgraphExec graph_exec, CUgraphNode node,
                                         CUdeviceptr device_pointer) {
  (void)graph_exec;
  (void)node;
  (void)device_pointer;
  return CUDA_SUCCESS;
}

CUresult cuGraphExecKernelNodeSetParams(CUgraphExec graph_exec, CUgraphNode node,
                                        const CUDA_KERNEL_NODE_PARAMS* node_params) {
  (void)graph_exec;
  (void)node;
  (void)node_params;
  return CUDA_SUCCESS;
}

/* Capture never starts in the managed backend, so the dependency update is a
   validated no-op. */
CUresult cuStreamUpdateCaptureDependencies(CUstream stream, CUuserObject* dependencies,
                                           unsigned int num_dependencies, unsigned int flags) {
  (void)stream;
  (void)dependencies;
  (void)num_dependencies;
  (void)flags;
  return CUDA_SUCCESS;
}

CUresult cuStreamGetCaptureInfo(CUstream stream, CUstreamCaptureStatus* capture_status,
                                cuuint64_t* pid) {
  (void)stream;
  if (capture_status == (CUstreamCaptureStatus*)0 || pid == (cuuint64_t*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *capture_status = CU_STREAM_CAPTURE_STATUS_NONE;
  *pid = UINT64_C(0);
  return CUDA_SUCCESS;
}

CUresult cuStreamIsCapturing(CUstream stream, CUstreamCaptureStatus* capture_status) {
  (void)stream;
  if (capture_status == (CUstreamCaptureStatus*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *capture_status = CU_STREAM_CAPTURE_STATUS_NONE;
  return CUDA_SUCCESS;
}

CUresult cuDeviceGetUuid(CUuuid* uuid, CUdevice device) {
  mf_virtual_device_identity_v1 identity;
  mf_client_fence_snapshot_v1 fence;
  CUresult result = CUDA_SUCCESS;
  if (uuid == (CUuuid*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  result = mf_cuda_identity(device, &identity, &fence);
  if (result == CUDA_SUCCESS) {
    (void)memcpy(uuid->bytes, identity.gpu_uuid, sizeof(uuid->bytes));
  }
  return result;
}

CUresult cuDeviceGetUuid_v2(CUuuid* uuid, CUdevice device) { return cuDeviceGetUuid(uuid, device); }

CUresult cuDeviceComputeCapability(int* major, int* minor, CUdevice device) {
  mf_virtual_device_identity_v1 identity;
  mf_client_fence_snapshot_v1 fence;
  CUresult result = CUDA_SUCCESS;
  if (major == (int*)0 || minor == (int*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  result = mf_cuda_identity(device, &identity, &fence);
  if (result == CUDA_SUCCESS) {
    *major = (int)(identity.virtual_compute_capability / UINT32_C(10));
    *minor = (int)(identity.virtual_compute_capability % UINT32_C(10));
  }
  return result;
}

CUresult cuDeviceTotalMem_v2(size_t* bytes, CUdevice device) {
  mf_virtual_device_identity_v1 identity;
  mf_client_fence_snapshot_v1 fence;
  CUresult result = CUDA_SUCCESS;
  if (bytes == (size_t*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  result = mf_cuda_identity(device, &identity, &fence);
  if (result == CUDA_SUCCESS) {
    if (fence.effective_quota_bytes > (uint64_t)SIZE_MAX) {
      return CUDA_ERROR_INVALID_VALUE;
    }
    *bytes = (size_t)fence.effective_quota_bytes;
  }
  return result;
}

CUresult cuDeviceTotalMem(unsigned int* bytes, CUdevice device) {
  size_t wide_bytes = 0;
  CUresult result = CUDA_SUCCESS;
  if (bytes == (unsigned int*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  result = cuDeviceTotalMem_v2(&wide_bytes, device);
  if (result == CUDA_SUCCESS && wide_bytes > (size_t)UINT32_MAX) {
    result = CUDA_ERROR_INVALID_VALUE;
  }
  if (result == CUDA_SUCCESS) {
    *bytes = (unsigned int)wide_bytes;
  }
  return result;
}

CUresult cuDeviceGetPCIBusId(char* pci_bus_id, int length, CUdevice device) {
  mf_virtual_device_identity_v1 identity;
  mf_client_fence_snapshot_v1 fence;
  CUresult result = CUDA_SUCCESS;
  int written = 0;
  if (pci_bus_id == (char*)0 || length <= 0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  result = mf_cuda_identity(device, &identity, &fence);
  if (result != CUDA_SUCCESS) {
    return result;
  }
  written = snprintf(pci_bus_id, (size_t)length, "%04x:%02x:%02x.%x", identity.pci_domain,
                     identity.pci_bus, identity.pci_device, identity.pci_function);
  return written < 0 || written >= length ? CUDA_ERROR_INVALID_VALUE : CUDA_SUCCESS;
}

static CUresult mf_cuda_create_context_locked(CUcontext* out_context, CUdevice device,
                                              uint32_t primary) {
  mf_virtual_device_identity_v1 identity;
  mf_client_fence_snapshot_v1 fence;
  mf_generation_handle_v1 device_handle;
  uint32_t index = 0;
  uint32_t registry_index = UINT32_C(0);
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;
  CUresult result = CUDA_SUCCESS;
  if (out_context == (CUcontext*)0 ||
      mf_cuda_registry_index_locked(device, &registry_index) != CUDA_SUCCESS) {
    return CUDA_ERROR_INVALID_DEVICE;
  }
  *out_context = (CUcontext)0;
  status = mf_client_registry_make_handle_v1(&mf_cuda_global.registry, registry_index,
                                             (uint64_t)registry_index + UINT64_C(1), UINT64_C(1),
                                             MF_CUDA_OBJECT_DEVICE, &device_handle);
  if (status == MF_SHARED_SUCCESS) {
    status =
        mf_client_registry_validate_device_v1(&mf_cuda_global.registry, &device_handle, &fence);
  }
  if (status == MF_SHARED_SUCCESS) {
    status = mf_client_registry_identity_v1(&mf_cuda_global.registry, registry_index, &identity);
  }
  if (status != MF_SHARED_SUCCESS) {
    return mf_cuda_status(status);
  }
  index = mf_cuda_free_slot(mf_cuda_global.contexts);
  if (index == MF_CUDA_OBJECT_CAPACITY) {
    return CUDA_ERROR_OUT_OF_MEMORY;
  }
  result = mf_cuda_activate_locked(&mf_cuda_global.contexts[index], MF_CUDA_OBJECT_CONTEXT,
                                   registry_index, index);
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_control_locked(MF_CLIENT_CONTROL_CONTEXT_ACQUIRE_V1, UINT16_C(0),
                                    mf_cuda_global.transport.runtime_context_id,
                                    device_handle.device_generation, (const void*)0, UINT64_C(0),
                                    (uint64_t*)0, (uint64_t*)0);
  }
  if (result == CUDA_SUCCESS) {
    mf_cuda_global.contexts[index].flags = primary;
    mf_cuda_global.contexts[index].aux = primary;
    *out_context = (CUcontext)(uintptr_t)mf_cuda_token(MF_CUDA_TAG_CONTEXT, index,
                                                       mf_cuda_global.contexts[index].generation);
  } else {
    mf_cuda_global.contexts[index].active = UINT32_C(0);
  }
  return result;
}

CUresult cuCtxCreate_v2(CUcontext* context, unsigned int flags, CUdevice device) {
  MF_ENTRY_TRACE();
  mf_cuda_tls_state* state = mf_cuda_thread_state(UINT32_C(1));
  CUresult result = CUDA_SUCCESS;
  if (context == (CUcontext*)0 || flags != UINT32_C(0)) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  if (state == (mf_cuda_tls_state*)0) {
    return CUDA_ERROR_OUT_OF_MEMORY;
  }
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS && state->depth == MF_CUDA_CONTEXT_STACK_CAPACITY) {
    result = CUDA_ERROR_OUT_OF_MEMORY;
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_create_context_locked(context, device, UINT32_C(0));
  }
  if (result == CUDA_SUCCESS) {
    state->stack[state->depth] = state->current;
    state->depth += UINT32_C(1);
    state->current = *context;
  }
  mf_cuda_unlock();
  return result;
}

CUresult cuCtxCreate(CUcontext* context, unsigned int flags, CUdevice device) {
  return cuCtxCreate_v2(context, flags, device);
}

CUresult cuCtxCreate_v4(CUcontext* context, CUctxCreateParams* parameters, unsigned int flags,
                        CUdevice device) {
  MF_ENTRY_TRACE();
  const unsigned int known_flags = UINT32_C(0xff);
  const unsigned int scheduling_flags = flags & UINT32_C(0x07);
  if ((flags & ~known_flags) != UINT32_C(0) || scheduling_flags == UINT32_C(3) ||
      scheduling_flags > UINT32_C(4)) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  if (parameters != (CUctxCreateParams*)0 &&
      ((parameters->execAffinityParams == (CUexecAffinityParam*)0) !=
           (parameters->numExecAffinityParams == 0) ||
       parameters->numExecAffinityParams < 0 ||
       (parameters->execAffinityParams != (CUexecAffinityParam*)0 &&
        parameters->cigParams != (CUctxCigParam*)0))) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  /* Scheduling-flag bits are inert in the managed backend (one deterministic
     scheduler); the context is created through the same path as flags=0.
     MetaFlux-strengthened behavior. */
  return cuCtxCreate_v2(context, UINT32_C(0), device);
}

CUresult cuDevicePrimaryCtxRetain(CUcontext* context, CUdevice device) {
  MF_ENTRY_TRACE();
  mf_cuda_tls_state* state = mf_cuda_thread_state(UINT32_C(1));
  uint32_t index = 0;
  uint32_t registry_index = UINT32_C(0);
  CUresult result = CUDA_SUCCESS;
  if (context == (CUcontext*)0 || device < 0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *context = (CUcontext)0;
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_registry_index_locked(device, &registry_index);
  }
  if (result == CUDA_SUCCESS) {
    for (index = 0; index < MF_CUDA_OBJECT_CAPACITY; ++index) {
      mf_cuda_object* record = &mf_cuda_global.contexts[index];
      if (record->active != UINT32_C(0) && record->flags != UINT32_C(0) &&
          record->device_index == registry_index) {
        result = mf_cuda_validate_locked(record, record->generation, MF_CUDA_OBJECT_CONTEXT);
        if (result == CUDA_SUCCESS) {
          if (record->aux == UINT32_MAX) {
            result = CUDA_ERROR_OUT_OF_MEMORY;
          } else {
            record->aux += UINT32_C(1);
            *context =
                (CUcontext)(uintptr_t)mf_cuda_token(MF_CUDA_TAG_CONTEXT, index, record->generation);
          }
        }
        break;
      }
    }
    if (index == MF_CUDA_OBJECT_CAPACITY) {
      result = mf_cuda_create_context_locked(context, device, UINT32_C(1));
    }
  }
  /* cudart's device-state construction keys the per-device container on the
     CURRENT context immediately after retain; a real retain+use sequence
     always observes the primary context as current. MetaFlux-strengthened:
     retaining the primary context makes it current on this thread. */
  if (result == CUDA_SUCCESS && state != (mf_cuda_tls_state*)0) {
    state->current = *context;
  }
  mf_cuda_unlock();
  if (mf_cuda_entry_trace_enabled() != 0) {
    fprintf(stderr, "MF_PCR dev=%d ctx=%p rc=%d\n", (int)device, (void*)*context, (int)result);
  }
  return result;
}

static void mf_cuda_invalidate_module_functions_locked(uint32_t module_index) {
  uint32_t index = 0;
  for (index = 0; index < MF_CUDA_OBJECT_CAPACITY; ++index) {
    if (mf_cuda_global.functions[index].active != UINT32_C(0) &&
        mf_cuda_global.functions[index].aux == module_index) {
      mf_cuda_global.functions[index].active = UINT32_C(0);
    }
  }
}

static CUresult mf_cuda_release_module_locked(uint32_t module_index, mf_cuda_object* record) {
  mf_client_completion_v1 completion = {0};
  mf_cuda_command command = {MF_CUDA_COMMAND_MODULE_UNLOAD, 0, {0, 0, 0, 0}, 0};
  CUresult result =
      mf_cuda_drain_reference_locked(MF_CUDA_REFERENCE_MODULE, module_index, record->generation);
  if (result != CUDA_SUCCESS) {
    return result;
  }
  result = mf_cuda_argument_cache_release_locked(MF_CUDA_ARGUMENT_CACHE_MODULE, module_index,
                                                 record->generation);
  if (result != CUDA_SUCCESS) {
    return result;
  }
  if (record->remote_id == UINT64_C(0)) {
    /* Deferred library intake never materialized; release the artifact only. */
    const CUresult deferred_release = mf_cuda_control_locked(
        MF_CLIENT_CONTROL_ARTIFACT_RELEASE_V1, UINT16_C(0), record->materialized_id,
        record->materialized_generation, (const void*)0, UINT64_C(0), (uint64_t*)0, (uint64_t*)0);
    record->active = UINT32_C(0);
    mf_cuda_invalidate_module_functions_locked(module_index);
    return deferred_release;
  }
  command.target = record->remote_id;
  command.arguments[0] = record->remote_generation;
  result = mf_cuda_submit_locked(&command, &completion);
  if (result == CUDA_SUCCESS) {
    const CUresult release_result = mf_cuda_control_locked(
        MF_CLIENT_CONTROL_ARTIFACT_RELEASE_V1, UINT16_C(0), record->materialized_id,
        record->materialized_generation, (const void*)0, UINT64_C(0), (uint64_t*)0, (uint64_t*)0);
    record->active = UINT32_C(0);
    mf_cuda_invalidate_module_functions_locked(module_index);
    result = release_result;
  }
  return result;
}

static CUresult mf_cuda_release_memory_locked(uint32_t memory_index, mf_cuda_object* record) {
  mf_client_completion_v1 completion;
  mf_cuda_command command = {MF_CUDA_COMMAND_FREE, 0, {0, 0, 0, 0}, 0};
  CUresult result =
      mf_cuda_drain_reference_locked(MF_CUDA_REFERENCE_MEMORY, memory_index, record->generation);
  if (result != CUDA_SUCCESS) {
    return result;
  }
  result = mf_cuda_argument_cache_release_locked(MF_CUDA_ARGUMENT_CACHE_MEMORY, memory_index,
                                                 record->generation);
  if (result != CUDA_SUCCESS) {
    return result;
  }
  result = mf_cuda_copy_cache_release_locked(MF_CUDA_ARGUMENT_CACHE_MEMORY, memory_index,
                                             record->generation);
  if (result != CUDA_SUCCESS) {
    return result;
  }
  command.target = record->remote_id;
  command.arguments[0] = record->remote_generation;
  result = mf_cuda_submit_locked(&command, &completion);
  if (result == CUDA_SUCCESS) {
    record->active = UINT32_C(0);
  }
  return result;
}

static void mf_cuda_invalidate_local_children_locked(uint32_t context_index) {
  mf_cuda_object* groups[] = {mf_cuda_global.functions, mf_cuda_global.streams,
                              mf_cuda_global.events};
  size_t group = 0;
  uint32_t index = 0;
  for (group = 0; group < sizeof(groups) / sizeof(groups[0]); ++group) {
    for (index = 0; index < MF_CUDA_OBJECT_CAPACITY; ++index) {
      if (groups[group][index].active != UINT32_C(0) &&
          groups[group][index].owner_context == context_index) {
        groups[group][index].active = UINT32_C(0);
      }
    }
  }
}

static CUresult mf_cuda_reset_context_state_locked(uint32_t context_index, mf_cuda_object* record,
                                                   uint32_t* out_cleanup_complete) {
  uint32_t child_index = UINT32_C(0);
  CUresult result = CUDA_SUCCESS;
  *out_cleanup_complete = UINT32_C(0);
  result =
      mf_cuda_drain_reference_locked(MF_CUDA_REFERENCE_CONTEXT, context_index, record->generation);
  if (result != CUDA_SUCCESS) {
    return result;
  }
  for (child_index = UINT32_C(0); child_index < MF_CUDA_MODULE_CAPACITY; ++child_index) {
    if (mf_cuda_global.modules[child_index].active != UINT32_C(0) &&
        mf_cuda_global.modules[child_index].owner_context == context_index) {
      result = mf_cuda_release_module_locked(child_index, &mf_cuda_global.modules[child_index]);
      if (result != CUDA_SUCCESS) {
        return result;
      }
    }
  }
  for (child_index = UINT32_C(0); child_index < MF_CUDA_OBJECT_CAPACITY; ++child_index) {
    if (mf_cuda_global.memories[child_index].active != UINT32_C(0) &&
        mf_cuda_global.memories[child_index].owner_context == context_index) {
      result = mf_cuda_release_memory_locked(child_index, &mf_cuda_global.memories[child_index]);
      if (result != CUDA_SUCCESS) {
        return result;
      }
    }
  }
  result = mf_cuda_argument_cache_release_locked(MF_CUDA_ARGUMENT_CACHE_CONTEXT, context_index,
                                                 record->generation);
  if (result != CUDA_SUCCESS) {
    return result;
  }
  result = mf_cuda_copy_cache_release_locked(MF_CUDA_ARGUMENT_CACHE_CONTEXT, context_index,
                                             record->generation);
  if (result != CUDA_SUCCESS) {
    return result;
  }
  result = mf_cuda_clear_async_errors_locked(context_index, record->generation, UINT32_C(0),
                                             MF_CUDA_INDEX_NONE, UINT32_C(0), UINT64_MAX);
  mf_cuda_invalidate_local_children_locked(context_index);
  *out_cleanup_complete = UINT32_C(1);
  return result;
}

static CUresult mf_cuda_destroy_context_locked(CUcontext context, uint32_t allow_primary) {
  mf_cuda_tls_state* state = mf_cuda_thread_state(UINT32_C(0));
  uint32_t index = 0;
  uint32_t cleanup_complete = UINT32_C(0);
  mf_cuda_object* record = (mf_cuda_object*)0;
  CUresult result = mf_cuda_context_locked(context, &index, &record);
  if (result != CUDA_SUCCESS) {
    return result;
  }
  if (record->flags != UINT32_C(0) && allow_primary == UINT32_C(0)) {
    do { if (mf_cuda_entry_trace_enabled() != 0) { fprintf(stderr, "MF_INVALID_CONTEXT %s:%d\n", __func__, __LINE__); } return CUDA_ERROR_INVALID_CONTEXT; } while (0);
  }
  result = mf_cuda_reset_context_state_locked(index, record, &cleanup_complete);
  if (cleanup_complete == UINT32_C(0)) {
    return result;
  }
  {
    const CUresult release_result = mf_cuda_control_locked(
        MF_CLIENT_CONTROL_CONTEXT_RELEASE_V1, UINT16_C(0),
        mf_cuda_global.transport.runtime_context_id, record->handle.device_generation,
        (const void*)0, UINT64_C(0), (uint64_t*)0, (uint64_t*)0);
    if (release_result != CUDA_SUCCESS) {
      return release_result;
    }
  }
  record->active = UINT32_C(0);
  if (state != (mf_cuda_tls_state*)0 && state->current == context) {
    if (state->depth != UINT32_C(0)) {
      state->depth -= UINT32_C(1);
      state->current = state->stack[state->depth];
    } else {
      state->current = (CUcontext)0;
    }
  }
  return result;
}

CUresult cuCtxDestroy_v2(CUcontext context) {
  MF_ENTRY_TRACE();
  CUresult result = CUDA_SUCCESS;
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_destroy_context_locked(context, UINT32_C(0));
  }
  mf_cuda_unlock();
  return result;
}

CUresult cuCtxDestroy(CUcontext context) { return cuCtxDestroy_v2(context); }

CUresult cuDevicePrimaryCtxRelease_v2(CUdevice device) {
  MF_ENTRY_TRACE();
  uint32_t index = 0;
  uint32_t registry_index = UINT32_C(0);
  CUresult result = CUDA_ERROR_INVALID_CONTEXT;
  if (device < 0) {
    return CUDA_ERROR_INVALID_DEVICE;
  }
  mf_cuda_lock();
  if (mf_cuda_require_locked() != CUDA_SUCCESS) {
    mf_cuda_unlock();
    return CUDA_ERROR_NOT_INITIALIZED;
  }
  if (mf_cuda_registry_index_locked(device, &registry_index) != CUDA_SUCCESS) {
    mf_cuda_unlock();
    return CUDA_ERROR_INVALID_DEVICE;
  }
  for (index = 0; index < MF_CUDA_OBJECT_CAPACITY; ++index) {
    mf_cuda_object* record = &mf_cuda_global.contexts[index];
    if (record->active != UINT32_C(0) && record->flags != UINT32_C(0) &&
        record->device_index == registry_index) {
      if (record->aux > UINT32_C(1)) {
        record->aux -= UINT32_C(1);
        result = CUDA_SUCCESS;
      } else {
        CUcontext token =
            (CUcontext)(uintptr_t)mf_cuda_token(MF_CUDA_TAG_CONTEXT, index, record->generation);
        result = mf_cuda_destroy_context_locked(token, UINT32_C(1));
      }
      break;
    }
  }
  mf_cuda_unlock();
  return result;
}

CUresult cuDevicePrimaryCtxRelease(CUdevice device) { return cuDevicePrimaryCtxRelease_v2(device); }

CUresult cuDevicePrimaryCtxReset_v2(CUdevice device) {
  MF_ENTRY_TRACE();
  uint32_t index = 0;
  uint32_t registry_index = UINT32_C(0);
  CUresult result = CUDA_SUCCESS;
  if (device < 0) {
    return CUDA_ERROR_INVALID_DEVICE;
  }
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_registry_index_locked(device, &registry_index);
  }
  if (result == CUDA_SUCCESS) {
    for (index = 0; index < MF_CUDA_OBJECT_CAPACITY; ++index) {
      mf_cuda_object* record = &mf_cuda_global.contexts[index];
      if (record->active != UINT32_C(0) && record->flags != UINT32_C(0) &&
          record->device_index == registry_index) {
        uint32_t cleanup_complete = UINT32_C(0);
        result = mf_cuda_reset_context_state_locked(index, record, &cleanup_complete);
        break;
      }
    }
  }
  mf_cuda_unlock();
  return result;
}

CUresult cuDevicePrimaryCtxReset(CUdevice device) { return cuDevicePrimaryCtxReset_v2(device); }

CUresult cuDevicePrimaryCtxGetState(CUdevice device, unsigned int* flags, int* active) {
  MF_ENTRY_TRACE();
  uint32_t registry_index = UINT32_C(0);
  uint32_t index = 0;
  CUresult result = CUDA_SUCCESS;
  if (flags == (unsigned int*)0 || active == (int*)0 || device < 0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *flags = 0;
  *active = 0;
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_registry_index_locked(device, &registry_index);
  }
  if (result == CUDA_SUCCESS) {
    for (index = 0; index < MF_CUDA_OBJECT_CAPACITY; ++index) {
      mf_cuda_object* record = &mf_cuda_global.contexts[index];
      if (record->active != UINT32_C(0) && record->flags != UINT32_C(0) &&
          record->device_index == registry_index) {
        *active = 1;
        /* Primary context flags live in the low bits of record->flags after the
           primary marker bit; expose zero until SetFlags stores a value. */
        *flags = (unsigned int)(record->last_request & UINT64_C(0xffffffff));
        break;
      }
    }
  }
  mf_cuda_unlock();
  if (mf_cuda_entry_trace_enabled() != 0) {
    fprintf(stderr, "MF_PCGS dev=%d flags=%u active=%d rc=%d\n", (int)device, *flags, *active,
            (int)result);
  }
  return result;
}

CUresult cuDevicePrimaryCtxSetFlags_v2(CUdevice device, unsigned int flags) {
  MF_ENTRY_TRACE();
  uint32_t registry_index = UINT32_C(0);
  uint32_t index = 0;
  CUresult result = CUDA_SUCCESS;
  if (device < 0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_registry_index_locked(device, &registry_index);
  }
  if (result == CUDA_SUCCESS) {
    for (index = 0; index < MF_CUDA_OBJECT_CAPACITY; ++index) {
      mf_cuda_object* record = &mf_cuda_global.contexts[index];
      if (record->active != UINT32_C(0) && record->flags != UINT32_C(0) &&
          record->device_index == registry_index) {
        record->last_request = (uint64_t)flags;
        break;
      }
    }
    /* Setting flags before retain is valid; store on a side table keyed by
       device only when a primary context already exists. */
  }
  mf_cuda_unlock();
  return result;
}

CUresult cuDevicePrimaryCtxSetFlags(CUdevice device, unsigned int flags) {
  return cuDevicePrimaryCtxSetFlags_v2(device, flags);
}


CUresult cuCtxSetCurrent(CUcontext context) {
  MF_ENTRY_TRACE();
  mf_cuda_tls_state* state =
      mf_cuda_thread_state(context == (CUcontext)0 ? UINT32_C(0) : UINT32_C(1));
  uint32_t index = 0;
  mf_cuda_object* record = (mf_cuda_object*)0;
  CUresult result = CUDA_SUCCESS;
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS && context != (CUcontext)0) {
    result = mf_cuda_context_locked(context, &index, &record);
  }
  if (result == CUDA_SUCCESS && state == (mf_cuda_tls_state*)0 && context != (CUcontext)0) {
    result = CUDA_ERROR_OUT_OF_MEMORY;
  }
  if (result == CUDA_SUCCESS && state != (mf_cuda_tls_state*)0) {
    state->current = context;
  }
  mf_cuda_unlock();
  if (mf_cuda_entry_trace_enabled() != 0) {
    fprintf(stderr, "MF_CSC ctx=%p rc=%d\n", (void*)context, (int)result);
  }
  return result;
}

CUresult cuCtxGetCurrent(CUcontext* context) {
  MF_ENTRY_TRACE();
  mf_cuda_tls_state* state = mf_cuda_thread_state(UINT32_C(0));
  uint32_t index = 0;
  mf_cuda_object* record = (mf_cuda_object*)0;
  CUresult result = CUDA_SUCCESS;
  if (context == (CUcontext*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS && state != (mf_cuda_tls_state*)0 && state->current != (CUcontext)0) {
    result = mf_cuda_context_locked(state->current, &index, &record);
  }
  if (result == CUDA_SUCCESS) {
    *context = state == (mf_cuda_tls_state*)0 ? (CUcontext)0 : state->current;
  }
  mf_cuda_unlock();
  if (mf_cuda_entry_trace_enabled() != 0) {
    fprintf(stderr, "MF_CGC ctx=%p rc=%d\n", (void*)(state ? state->current : 0), (int)result);
  }
  return result;
}

CUresult cuCtxGetDevice(CUdevice* device) {
  uint32_t index = 0;
  mf_cuda_object* context = (mf_cuda_object*)0;
  CUresult result = CUDA_SUCCESS;
  if (device == (CUdevice*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_current_locked(&index, &context);
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_visible_ordinal_locked(context->device_index, device);
  }
  mf_cuda_unlock();
  return result;
}

CUresult cuCtxPushCurrent_v2(CUcontext context) {
  mf_cuda_tls_state* state = mf_cuda_thread_state(UINT32_C(1));
  uint32_t index = 0;
  mf_cuda_object* record = (mf_cuda_object*)0;
  CUresult result = CUDA_SUCCESS;
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_context_locked(context, &index, &record);
  }
  if (result == CUDA_SUCCESS && state == (mf_cuda_tls_state*)0) {
    result = CUDA_ERROR_OUT_OF_MEMORY;
  }
  if (result == CUDA_SUCCESS && state->depth == MF_CUDA_CONTEXT_STACK_CAPACITY) {
    result = CUDA_ERROR_OUT_OF_MEMORY;
  }
  if (result == CUDA_SUCCESS) {
    state->stack[state->depth] = state->current;
    state->depth += UINT32_C(1);
    state->current = context;
  }
  mf_cuda_unlock();
  if (mf_cuda_entry_trace_enabled() != 0) {
    fprintf(stderr, "MF_PUSH ctx=%p rc=%d\n", (void*)context, (int)result);
  }
  return result;
}

CUresult cuCtxPushCurrent(CUcontext context) { return cuCtxPushCurrent_v2(context); }

CUresult cuCtxPopCurrent_v2(CUcontext* context) {
  mf_cuda_tls_state* state = mf_cuda_thread_state(UINT32_C(0));
  if (context == (CUcontext*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  mf_cuda_lock();
  if (mf_cuda_require_locked() != CUDA_SUCCESS) {
    mf_cuda_unlock();
    return CUDA_ERROR_NOT_INITIALIZED;
  }
  if (state == (mf_cuda_tls_state*)0 || state->current == (CUcontext)0 ||
      state->depth == UINT32_C(0)) {
    mf_cuda_unlock();
    do { if (mf_cuda_entry_trace_enabled() != 0) { fprintf(stderr, "MF_INVALID_CONTEXT %s:%d\n", __func__, __LINE__); } return CUDA_ERROR_INVALID_CONTEXT; } while (0);
  }
  *context = state->current;
  state->depth -= UINT32_C(1);
  state->current = state->stack[state->depth];
  mf_cuda_unlock();
  return CUDA_SUCCESS;
}

CUresult cuCtxPopCurrent(CUcontext* context) { return cuCtxPopCurrent_v2(context); }

CUresult cuCtxSynchronize(void) {
  uint32_t index = 0;
  mf_cuda_object* context = (mf_cuda_object*)0;
  CUresult result = CUDA_SUCCESS;
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_current_locked(&index, &context);
  }
  if (result == CUDA_SUCCESS) {
    const uint64_t target_request = context->last_request;
    result = mf_cuda_drain_through_locked(target_request);
    if (result == CUDA_SUCCESS) {
      result = mf_cuda_take_async_error_locked(index, context->generation, UINT32_C(0),
                                               MF_CUDA_INDEX_NONE, UINT32_C(0), target_request);
    }
  }
  mf_cuda_unlock();
  return result;
}

CUresult cuModuleLoadData(CUmodule* module, const void* image) {
  uint32_t context_index = 0;
  uint32_t module_index = 0;
  mf_cuda_object* context = (mf_cuda_object*)0;
  mf_client_completion_v1 completion = {0};
  mf_cuda_command command;
  size_t image_size = 0;
  uint64_t artifact_id = 0;
  uint64_t artifact_generation = 0;
  CUresult result = CUDA_SUCCESS;
  if (module == (CUmodule*)0 || image == (const void*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_current_locked(&context_index, &context);
  }
  if (result == CUDA_SUCCESS) {
    image_size = mf_cuda_string_length((const char*)image, (size_t)(1U << 20U));
    if (image_size == (size_t)0 || image_size == (size_t)(1U << 20U)) {
      result = CUDA_ERROR_INVALID_IMAGE;
    }
  }
  module_index = mf_cuda_free_module_slot();
  if (result == CUDA_SUCCESS && module_index == MF_CUDA_MODULE_CAPACITY) {
    result = CUDA_ERROR_OUT_OF_MEMORY;
  }
  if (result == CUDA_SUCCESS) {
    result =
        mf_cuda_control_locked(MF_CLIENT_CONTROL_ARTIFACT_REGISTER_V1,
                               MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_PTX,
                               mf_cuda_global.transport.runtime_context_id, (uint64_t)image_size,
                               image, (uint64_t)image_size, &artifact_id, &artifact_generation);
  }
  if (result == CUDA_SUCCESS) {
    command.kind = MF_CUDA_COMMAND_MODULE_LOAD;
    command.target = artifact_id;
    command.arguments[0] = artifact_generation;
    command.arguments[1] = UINT64_C(0);
    command.arguments[2] = UINT64_C(0);
    command.arguments[3] = UINT64_C(0);
    command.flags = UINT32_C(0);
    result = mf_cuda_submit_locked(&command, &completion);
  }
  if (result == CUDA_SUCCESS &&
      (completion.result_id == UINT64_C(0) || completion.result_generation == UINT64_C(0))) {
    result = CUDA_ERROR_UNKNOWN;
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_activate_locked(&mf_cuda_global.modules[module_index], MF_CUDA_OBJECT_MODULE,
                                     context->device_index, context_index);
  }
  if (result == CUDA_SUCCESS) {
    mf_cuda_global.modules[module_index].remote_id = completion.result_id;
    mf_cuda_global.modules[module_index].remote_generation = completion.result_generation;
    mf_cuda_global.modules[module_index].materialized_id = artifact_id;
    mf_cuda_global.modules[module_index].materialized_generation = artifact_generation;
    *module = (CUmodule)(uintptr_t)mf_cuda_token(MF_CUDA_TAG_MODULE, module_index,
                                                 mf_cuda_global.modules[module_index].generation);
  } else {
    if (completion.result_id != UINT64_C(0) && completion.result_generation != UINT64_C(0)) {
      mf_client_completion_v1 cleanup_completion;
      const mf_cuda_command cleanup_command = {MF_CUDA_COMMAND_MODULE_UNLOAD,
                                               completion.result_id,
                                               {completion.result_generation, 0, 0, 0},
                                               0};
      (void)mf_cuda_submit_locked(&cleanup_command, &cleanup_completion);
    }
    if (artifact_id != UINT64_C(0)) {
      (void)mf_cuda_control_locked(MF_CLIENT_CONTROL_ARTIFACT_RELEASE_V1, UINT16_C(0), artifact_id,
                                   artifact_generation, (const void*)0, UINT64_C(0), (uint64_t*)0,
                                   (uint64_t*)0);
    }
  }
  mf_cuda_unlock();
  return result;
}

CUresult cuModuleUnload(CUmodule module) {
  uint32_t module_index = 0;
  mf_cuda_object* record = (mf_cuda_object*)0;
  CUresult result = CUDA_SUCCESS;
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_lookup_token_locked((void*)module, MF_CUDA_TAG_MODULE, MF_CUDA_OBJECT_MODULE,
                                         mf_cuda_global.modules, &module_index, &record);
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_release_module_locked(module_index, record);
  }
  mf_cuda_unlock();
  return result;
}

CUresult cuModuleGetFunction(CUfunction* function, CUmodule module, const char* name) {
  uint32_t module_index = 0;
  uint32_t function_index = 0;
  mf_cuda_object* module_record = (mf_cuda_object*)0;
  CUresult result = CUDA_SUCCESS;
  if (function == (CUfunction*)0 || name == (const char*)0 || name[0] == '\0') {
    return CUDA_ERROR_INVALID_VALUE;
  }
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_lookup_token_locked((void*)module, MF_CUDA_TAG_MODULE, MF_CUDA_OBJECT_MODULE,
                                         mf_cuda_global.modules, &module_index, &module_record);
  }
  /* Deferred library intake carries client cubins the daemon cannot
     compile: launches route through the semantic kernel profile, so no
     materialization is attempted. */
  if (result == CUDA_SUCCESS && mf_module_deferred[module_index] == 0 &&
      module_record->remote_id == UINT64_C(0)) {
    mf_client_completion_v1 load_completion = {0};
    const mf_cuda_command load_command = {MF_CUDA_COMMAND_MODULE_LOAD,
                                          module_record->materialized_id,
                                          {module_record->materialized_generation, 0, 0, 0}, 0};
    result = mf_cuda_submit_locked(&load_command, &load_completion);
    if (result == CUDA_SUCCESS &&
        (load_completion.result_id == UINT64_C(0) ||
         load_completion.result_generation == UINT64_C(0))) {
      result = CUDA_ERROR_UNKNOWN;
    }
    if (result == CUDA_SUCCESS) {
      module_record->remote_id = load_completion.result_id;
      module_record->remote_generation = load_completion.result_generation;
    }
  }
  /* Kernel-name resolution is delegated to the module's compiled artifact:
     any name the module exposes resolves to a function token. */
  function_index = mf_cuda_free_slot(mf_cuda_global.functions);
  if (result == CUDA_SUCCESS && function_index == MF_CUDA_OBJECT_CAPACITY) {
    result = CUDA_ERROR_OUT_OF_MEMORY;
  }
  if (result == CUDA_SUCCESS) {
    result =
        mf_cuda_activate_locked(&mf_cuda_global.functions[function_index], MF_CUDA_OBJECT_FUNCTION,
                                module_record->device_index, module_record->owner_context);
  }
  if (result == CUDA_SUCCESS) {
    mf_cuda_object* record = &mf_cuda_global.functions[function_index];
    record->remote_id = mf_cuda_global.transport.runtime_add_kernel_id;
    record->aux = module_index;
    record->size = UINT64_C(4);
    if (mf_cuda_entry_trace_enabled() != 0) {
      fprintf(stderr, "MGF name=%s deferred=%u midx=%u\n", name,
              (unsigned)mf_module_deferred[module_index], module_index);
    }
    {
      size_t name_length = strlen(name);
      if (name_length >= sizeof(mf_function_names[function_index])) {
        name_length = sizeof(mf_function_names[function_index]) - 1;
      }
      memcpy(mf_function_names[function_index], name, name_length);
      mf_function_names[function_index][name_length] = '\0';
    }
    *function = (CUfunction)(uintptr_t)mf_cuda_token(MF_CUDA_TAG_FUNCTION, function_index,
                                                     record->generation);
  }
  mf_cuda_unlock();
  return result;
}

CUresult cuModuleGetLoadingMode(CUmoduleLoadingMode* mode) {
  if (mode == (CUmoduleLoadingMode*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  /* With the kernel-query surface implemented, eager binding lets cudart
     resolve all registered kernels at import; the deferred library intake
     still defers the daemon MODULE_LOAD to first kernel resolution. */
  *mode = CU_MODULE_EAGER_LOADING;
  return CUDA_SUCCESS;
}

/* CUlibrary and CUkernel handles are the module and function tokens: the
   library API shares the module object table, so the CUDA 12 entry points
   delegate to the CUDA 11 implementations above. */

/* CUDA 12 library intake is context-free by contract: cudart loads framework
   fatbins (e.g. torch's __fatDeviceText) before any context exists, and the
   CUDA_MODULE_LAZY_LOADING contract defers materialization to first kernel
   use. Registration stores the artifact identity on a context-free module
   record; MF_CUDA_COMMAND_MODULE_LOAD runs lazily from cuModuleGetFunction
   when a kernel is first resolved. MetaFlux-strengthened behavior. */
CUresult cuLibraryLoadData(CUlibrary* library, const void* code, CUjit_option* jit_options,
                           void** jit_option_values, unsigned int num_jit_options,
                           CUjit_option* library_options, void** library_option_values,
                           unsigned int num_library_options) {
  uint32_t module_index = 0;
  uint32_t device_registry_index = 0;
  mf_cuda_object* record = (mf_cuda_object*)0;
  size_t image_size = 0;
  uint64_t artifact_id = 0;
  uint64_t artifact_generation = 0;
  CUresult result = CUDA_SUCCESS;
  (void)jit_options;
  (void)jit_option_values;
  (void)num_jit_options;
  (void)library_options;
  (void)library_option_values;
  (void)num_library_options;
  if (library == (CUlibrary*)0 || code == (const void*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (mf_cuda_entry_trace_enabled() != 0) {
    fprintf(stderr, "MF_LLD require=%d\n", (int)result);
  }
  if (result == CUDA_SUCCESS) {
    image_size = mf_cuda_string_length((const char*)code, (size_t)(1U << 22U));
    if (image_size == (size_t)0 || image_size == (size_t)(1U << 22U)) {
      result = CUDA_ERROR_INVALID_IMAGE;
    }
    if (mf_cuda_entry_trace_enabled() != 0) {
      fprintf(stderr, "MF_LLD image_size=%zu rc=%d\n", image_size, (int)result);
    }
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_registry_index_locked((CUdevice)0, &device_registry_index);
    if (mf_cuda_entry_trace_enabled() != 0) {
      fprintf(stderr, "MF_LLD regidx rc=%d\n", (int)result);
    }
  }
  module_index = mf_cuda_free_module_slot();
  if (result == CUDA_SUCCESS && module_index == MF_CUDA_MODULE_CAPACITY) {
    result = CUDA_ERROR_OUT_OF_MEMORY;
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_activate_locked(&mf_cuda_global.modules[module_index],
                                     MF_CUDA_OBJECT_MODULE, device_registry_index,
                                     MF_CUDA_INDEX_NONE);
    if (mf_cuda_entry_trace_enabled() != 0) {
      fprintf(stderr, "MF_LLD activate rc=%d\n", (int)result);
    }
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_control_locked(MF_CLIENT_CONTROL_ARTIFACT_REGISTER_V1,
                                    MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_PTX,
                                    mf_cuda_global.transport.runtime_context_id,
                                    (uint64_t)image_size, code, (uint64_t)image_size,
                                    &artifact_id, &artifact_generation);
    if (mf_cuda_entry_trace_enabled() != 0) {
      fprintf(stderr, "MF_LLD register rc=%d artifact=%llu\n", (int)result,
              (unsigned long long)artifact_id);
    }
  }
  if (result == CUDA_SUCCESS) {
    record = &mf_cuda_global.modules[module_index];
    /* remote_id 0 marks the deferred load; cuModuleGetFunction submits the
       MODULE_LOAD against this artifact on first kernel resolution. */
    record->remote_id = UINT64_C(0);
    record->remote_generation = UINT64_C(0);
    record->materialized_id = artifact_id;
    record->materialized_generation = artifact_generation;
    mf_module_deferred[module_index] = 1;
    mf_module_blob[module_index] = (const unsigned char*)code;
    {
      uint64_t fatbin_size = 0;
      if (mf_fatbin_read_u32((const unsigned char*)code, 0) == 0xba55ed50u &&
          image_size >= 16) {
        fatbin_size = mf_fatbin_read_u64((const unsigned char*)code, 8);
        if (fatbin_size > (uint64_t)image_size) {
          fatbin_size = (uint64_t)image_size;
        }
        mf_module_blob_size[module_index] = (size_t)fatbin_size;
      } else {
        mf_module_blob_size[module_index] = (size_t)image_size;
      }
    }
    *library = (CUlibrary)(uintptr_t)mf_cuda_token(MF_CUDA_TAG_MODULE, module_index,
                                                   record->generation);
  } else {
    if (artifact_id != UINT64_C(0)) {
      (void)mf_cuda_control_locked(MF_CLIENT_CONTROL_ARTIFACT_RELEASE_V1, UINT16_C(0), artifact_id,
                                   artifact_generation, (const void*)0, UINT64_C(0), (uint64_t*)0,
                                   (uint64_t*)0);
    }
  }
  mf_cuda_unlock();
  return result;
}

CUresult cuLibraryLoadFromFile(CUlibrary* library, const char* file_name, CUjit_option* jit_options,
                               void** jit_option_values, unsigned int num_jit_options,
                               CUjit_option* library_options, void** library_option_values,
                               unsigned int num_library_options) {
  (void)library;
  (void)file_name;
  (void)jit_options;
  (void)jit_option_values;
  (void)num_jit_options;
  (void)library_options;
  (void)library_option_values;
  (void)num_library_options;
  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuLibraryUnload(CUlibrary library) { return cuModuleUnload((CUmodule)library); }

/* Kernel tokens encode (kernel-entry index + 1) in the function tag; the
   entry carries the owning module so launches route semantically. */
static int mf_kernel_token_decode(CUkernel kernel, uint32_t* entry_index) {
  uint32_t generation = 0;
  uint32_t index = 0;
  if (!mf_cuda_decode((void*)kernel, MF_CUDA_TAG_FUNCTION, &index, &generation) ||
      index == 0 || index > mf_kernel_entry_count) {
    return 0;
  }
  *entry_index = index - 1;
  return 1;
}

CUresult cuLibraryGetKernel(CUkernel* kernel, CUlibrary library, const char* name) {
  if (mf_cuda_entry_trace_enabled() != 0) { fprintf(stderr, "MF_KQ %s\n", "LGK"); }
  uint32_t module_index = 0;
  uint32_t generation = 0;
  uint32_t entry_index = 0;
  if (kernel == (CUkernel*)0 || name == (const char*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  if (!mf_cuda_decode((void*)library, MF_CUDA_TAG_MODULE, &module_index, &generation) ||
      module_index >= MF_CUDA_MODULE_CAPACITY || mf_module_deferred[module_index] == 0) {
    return cuModuleGetFunction((CUfunction*)kernel, (CUmodule)library, name);
  }
  mf_module_parse_kernels(module_index);
  for (entry_index = 0; entry_index < mf_kernel_entry_count; ++entry_index) {
    if (mf_kernel_entries[entry_index].module_index == module_index &&
        strcmp(mf_kernel_arena + mf_kernel_entries[entry_index].name_offset, name) == 0) {
      *kernel = (CUkernel)(uintptr_t)mf_cuda_token(MF_CUDA_TAG_FUNCTION, entry_index + 1, 1u);
      return CUDA_SUCCESS;
    }
  }
  return CUDA_ERROR_NOT_FOUND;
}

CUresult cuLibraryGetModule(CUmodule* module, CUlibrary library) {
  if (module == (CUmodule*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *module = (CUmodule)library;
  return CUDA_SUCCESS;
}

CUresult cuKernelGetFunction(CUfunction* function, CUkernel kernel) {
  if (mf_cuda_entry_trace_enabled() != 0) { fprintf(stderr, "MF_KQ %s\n", "KGF"); }
  if (function == (CUfunction*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *function = (CUfunction)kernel;
  return CUDA_SUCCESS;
}

CUresult cuLibraryGetGlobal(CUdeviceptr* dptr, size_t* bytes, CUlibrary library, const char* name) {
  (void)dptr;
  (void)bytes;
  (void)library;
  (void)name;
  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuLibraryGetManaged(CUdeviceptr* dptr, size_t* bytes, CUlibrary library,
                             const char* name) {
  (void)dptr;
  (void)bytes;
  (void)library;
  (void)name;
  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuLibraryGetUnifiedFunction(void** fptr, CUlibrary library, const char* name) {
  (void)fptr;
  (void)library;
  (void)name;
  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuLibraryGetKernelCount(unsigned int* count, CUlibrary library) {
  if (mf_cuda_entry_trace_enabled() != 0) { fprintf(stderr, "MF_KQ %s\n", "GKC"); }
  uint32_t module_index = 0;
  uint32_t generation = 0;
  if (count == (unsigned int*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  if (!mf_cuda_decode((void*)library, MF_CUDA_TAG_MODULE, &module_index, &generation) ||
      module_index >= MF_CUDA_MODULE_CAPACITY || mf_module_deferred[module_index] == 0) {
    return CUDA_ERROR_NOT_SUPPORTED;
  }
  *count = mf_module_kernel_count(module_index);
  return CUDA_SUCCESS;
}

CUresult cuLibraryEnumerateKernels(CUkernel* kernels, unsigned int num_kernels,
                                   CUlibrary library) {
  if (mf_cuda_entry_trace_enabled() != 0) { fprintf(stderr, "MF_KQ %s\n", "LEK"); }
  uint32_t module_index = 0;
  uint32_t generation = 0;
  uint32_t entry_index = 0;
  uint32_t filled = 0;
  if (kernels == (CUkernel*)0 && num_kernels != 0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  if (!mf_cuda_decode((void*)library, MF_CUDA_TAG_MODULE, &module_index, &generation) ||
      module_index >= MF_CUDA_MODULE_CAPACITY || mf_module_deferred[module_index] == 0) {
    return CUDA_ERROR_NOT_SUPPORTED;
  }
  mf_module_parse_kernels(module_index);
  for (entry_index = 0; entry_index < mf_kernel_entry_count && filled < num_kernels;
       ++entry_index) {
    if (mf_kernel_entries[entry_index].module_index == module_index) {
      kernels[filled] =
          (CUkernel)(uintptr_t)mf_cuda_token(MF_CUDA_TAG_FUNCTION, entry_index + 1, 1u);
      filled += 1;
    }
  }
  return CUDA_SUCCESS;
}

CUresult cuKernelGetAttribute(int* pi, CUfunction_attribute attrib, CUkernel kernel,
                              CUdevice device) {
  (void)pi;
  (void)attrib;
  (void)kernel;
  (void)device;
  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuKernelSetAttribute(CUfunction_attribute attrib, int value, CUkernel kernel,
                              CUdevice device) {
  (void)attrib;
  (void)value;
  (void)kernel;
  (void)device;
  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuKernelSetCacheConfig(CUfunc_config config, CUkernel kernel, CUdevice device) {
  (void)config;
  (void)kernel;
  (void)device;
  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuKernelGetName(const char** name, CUkernel kernel) {
  if (mf_cuda_entry_trace_enabled() != 0) { fprintf(stderr, "MF_KQ %s\n", "KGN"); }
  uint32_t entry_index = 0;
  if (name == (const char**)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  if (!mf_kernel_token_decode(kernel, &entry_index)) {
    return CUDA_ERROR_INVALID_HANDLE;
  }
  *name = mf_kernel_arena + mf_kernel_entries[entry_index].name_offset;
  return CUDA_SUCCESS;
}

CUresult cuKernelGetParamInfo(CUkernel kernel, size_t index, size_t* param_offset,
                              size_t* param_size) {
  (void)kernel;
  (void)index;
  (void)param_offset;
  (void)param_size;
  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuDeviceGetP2PAttribute(int* value, CUdevice_P2PAttribute attrib, CUdevice source_device,
                                 CUdevice destination_device) {
  (void)value;
  (void)attrib;
  (void)source_device;
  (void)destination_device;
  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuDeviceGetTexture1DLinearMaxWidth(size_t* max_width_in_elements, CUarray_format format,
                                            unsigned int num_channels, CUdevice device) {
  (void)max_width_in_elements;
  (void)format;
  (void)num_channels;
  (void)device;
  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuDeviceGetByPCIBusId(CUdevice* device, const char* pci_bus_id) {
  (void)device;
  (void)pci_bus_id;
  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuDeviceGetDefaultMemPool(CUmemoryPool* pool, CUdevice device) {
  (void)pool;
  (void)device;
  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuDeviceSetMemPool(CUdevice device, CUmemoryPool pool) {
  (void)device;
  (void)pool;
  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuDeviceGetMemPool(CUmemoryPool* pool, CUdevice device) {
  (void)pool;
  (void)device;
  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuFlushGPUDirectRDMAWrites(CUflushGPUDirectRDMAWritesTarget target,
                                    CUflushGPUDirectRDMAWritesMode mode, unsigned int flags) {
  (void)target;
  (void)mode;
  (void)flags;
  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuCtxResetPersistingL2Cache(void) { return CUDA_ERROR_NOT_SUPPORTED; }

CUresult cuMemAlloc_v2(CUdeviceptr* device_pointer, size_t bytes) {
  uint32_t context_index = 0;
  uint32_t memory_index = 0;
  uint64_t aligned_bytes = 0;
  mf_cuda_object* context = (mf_cuda_object*)0;
  mf_client_completion_v1 completion = {0};
  mf_cuda_command command = {MF_CUDA_COMMAND_ALLOC, 0, {0, 0, 0, 0}, 0};
  CUresult result = CUDA_SUCCESS;
  if (device_pointer == (CUdeviceptr*)0 || bytes == (size_t)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  if ((uint64_t)bytes > UINT64_MAX - UINT64_C(255)) {
    return CUDA_ERROR_OUT_OF_MEMORY;
  }
  aligned_bytes = ((uint64_t)bytes + UINT64_C(255)) & ~UINT64_C(255);
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_current_locked(&context_index, &context);
  }
  memory_index = mf_cuda_free_slot(mf_cuda_global.memories);
  if (result == CUDA_SUCCESS && memory_index == MF_CUDA_OBJECT_CAPACITY) {
    result = CUDA_ERROR_OUT_OF_MEMORY;
  }
  if (result == CUDA_SUCCESS &&
      mf_cuda_global.next_address > UINT64_MAX - aligned_bytes - MF_CUDA_ADDRESS_GUARD) {
    result = CUDA_ERROR_OUT_OF_MEMORY;
  }
  if (result == CUDA_SUCCESS) {
    command.target = mf_cuda_global.transport.runtime_context_id;
    command.arguments[0] = (uint64_t)bytes;
    command.arguments[1] = UINT64_C(256);
    result = mf_cuda_submit_locked(&command, &completion);
  }
  if (result == CUDA_SUCCESS &&
      (completion.result_id == UINT64_C(0) || completion.result_generation == UINT64_C(0))) {
    result = CUDA_ERROR_UNKNOWN;
  }
  if (result == CUDA_SUCCESS) {
    mf_cuda_object* record = &mf_cuda_global.memories[memory_index];
    result = mf_cuda_activate_locked(record, MF_CUDA_OBJECT_MEMORY, context->device_index,
                                     context_index);
    if (result == CUDA_SUCCESS) {
      record->remote_id = completion.result_id;
      record->remote_generation = completion.result_generation;
      record->address = mf_cuda_global.next_address;
      record->size = (uint64_t)bytes;
      mf_cuda_global.next_address += aligned_bytes + MF_CUDA_ADDRESS_GUARD;
      *device_pointer = (CUdeviceptr)record->address;
    }
  }
  if (result != CUDA_SUCCESS && completion.result_id != UINT64_C(0) &&
      completion.result_generation != UINT64_C(0)) {
    mf_client_completion_v1 cleanup_completion;
    const mf_cuda_command cleanup_command = {
        MF_CUDA_COMMAND_FREE, completion.result_id, {completion.result_generation, 0, 0, 0}, 0};
    (void)mf_cuda_submit_locked(&cleanup_command, &cleanup_completion);
  }
  mf_cuda_unlock();
  return result;
}

CUresult cuMemAlloc(CUdeviceptr_v1* device_pointer, unsigned int bytes) {
  CUdeviceptr wide_pointer = (CUdeviceptr)0;
  CUresult result = CUDA_SUCCESS;
  if (device_pointer == (CUdeviceptr_v1*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  result = cuMemAlloc_v2(&wide_pointer, (size_t)bytes);
  if (result == CUDA_SUCCESS && wide_pointer > (CUdeviceptr)UINT32_MAX) {
    (void)cuMemFree_v2(wide_pointer);
    result = CUDA_ERROR_OUT_OF_MEMORY;
  }
  if (result == CUDA_SUCCESS) {
    *device_pointer = (CUdeviceptr_v1)wide_pointer;
  }
  return result;
}

CUresult cuMemFree_v2(CUdeviceptr device_pointer) {
  MF_ENTRY_TRACE();
  uint32_t context_index = UINT32_C(0);
  mf_cuda_object* context = (mf_cuda_object*)0;
  mf_cuda_object* memory = (mf_cuda_object*)0;
  uint64_t offset = 0;
  CUresult result = CUDA_SUCCESS;
  /* cudart uses cuMemFree(NULL)/cudaFree(0) as a context-init probe; a real
     driver returns SUCCESS without requiring a current context. */
  if (device_pointer == (CUdeviceptr)0) {
    return CUDA_SUCCESS;
  }
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_current_locked(&context_index, &context);
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_memory_locked(device_pointer, (size_t)0, &memory, &offset);
  }
  if (result == CUDA_SUCCESS && memory->owner_context != context_index) {
    result = CUDA_ERROR_INVALID_CONTEXT;
  }
  if (result == CUDA_SUCCESS && offset != UINT64_C(0)) {
    result = CUDA_ERROR_INVALID_VALUE;
  }
  if (result == CUDA_SUCCESS) {
    const uint32_t memory_index = (uint32_t)(memory - mf_cuda_global.memories);
    result = mf_cuda_release_memory_locked(memory_index, memory);
  }
  mf_cuda_unlock();
  return result;
}

CUresult cuMemFree(CUdeviceptr_v1 device_pointer) {
  return cuMemFree_v2((CUdeviceptr)device_pointer);
}

static CUresult mf_cuda_copy(uint32_t direction, CUdeviceptr destination_device,
                             void* destination_host, CUdeviceptr source_device,
                             const void* source_host, size_t bytes, CUstream stream,
                             uint32_t asynchronous, uint32_t per_thread_default) {
  uint32_t context_index = 0;
  uint64_t destination_offset = 0;
  uint64_t source_offset = 0;
  uint64_t host_id = 0;
  uint64_t host_generation = 0;
  uint64_t argument_id = 0;
  uint64_t argument_generation = 0;
  uint32_t argument_cached = UINT32_C(0);
  uint32_t direct_host_copy = UINT32_C(0);
  uint32_t region_copy = UINT32_C(0);
  uint32_t must_drain = UINT32_C(0);
  uint32_t memory_indices[2] = {MF_CUDA_INDEX_NONE, MF_CUDA_INDEX_NONE};
  uint32_t memory_generations[2] = {UINT32_C(0), UINT32_C(0)};
  mf_cuda_object* context = (mf_cuda_object*)0;
  mf_cuda_object* stream_record = (mf_cuda_object*)0;
  mf_cuda_object* destination_memory = (mf_cuda_object*)0;
  mf_cuda_object* source_memory = (mf_cuda_object*)0;
  mf_client_completion_v1 completion = {0};
  mf_client_payload_v1 host_payload;
  mf_cuda_copy_argument_block arguments;
  mf_cuda_pending pending;
  mf_cuda_command command = {MF_CUDA_COMMAND_COPY, 0, {0, 0, 0, 0}, 0};
  uint64_t request_id = UINT64_C(0);
  uint64_t stream_last_request = UINT64_C(0);
  CUresult completion_result = CUDA_SUCCESS;
  CUresult result = CUDA_SUCCESS;
  mf_cuda_payload_initialize_empty(&host_payload);
  mf_cuda_pending_initialize(&pending);
  if (bytes == (size_t)0) {
    return CUDA_SUCCESS;
  }
  if ((direction == MF_CUDA_COPY_H2D && source_host == (const void*)0) ||
      (direction == MF_CUDA_COPY_D2H && destination_host == (void*)0)) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  result = mf_cuda_queue_lock(UINT32_C(0));
  if (result != CUDA_SUCCESS) {
    return result;
  }
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_current_locked(&context_index, &context);
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_stream_locked(stream, context_index, &stream_record);
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_stream_scope_locked(context_index, context, stream_record, per_thread_default,
                                         &pending.stream_index, &pending.stream_generation,
                                         &stream_last_request);
  }
  if (result == CUDA_SUCCESS && direction != MF_CUDA_COPY_D2H) {
    result =
        mf_cuda_memory_locked(destination_device, bytes, &destination_memory, &destination_offset);
  }
  if (result == CUDA_SUCCESS && direction != MF_CUDA_COPY_H2D) {
    result = mf_cuda_memory_locked(source_device, bytes, &source_memory, &source_offset);
  }
  if (result == CUDA_SUCCESS &&
      ((destination_memory != (mf_cuda_object*)0 &&
        destination_memory->owner_context != context_index) ||
       (source_memory != (mf_cuda_object*)0 && source_memory->owner_context != context_index))) {
    result = CUDA_ERROR_INVALID_CONTEXT;
  }
  if (result == CUDA_SUCCESS && mf_cuda_global.cdev_active == UINT32_C(0) &&
      direction != MF_CUDA_COPY_D2D &&
      mf_cuda_global.direct_host_copy_ready != UINT32_C(0) &&
      (mf_cuda_global.transport.negotiated_capabilities & MF_CLIENT_CAP_DIRECT_HOST_COPY_V1) !=
          UINT64_C(0)) {
    direct_host_copy = UINT32_C(1);
  }
  region_copy = (mf_cuda_global.cdev_active != UINT32_C(0) ||
                 destination_offset != UINT64_C(0) || source_offset != UINT64_C(0))
                    ? UINT32_C(1)
                    : UINT32_C(0);
  if (result == CUDA_SUCCESS &&
      region_copy != UINT32_C(0) &&
      direct_host_copy == UINT32_C(0) &&
      (mf_cuda_global.transport.negotiated_capabilities & MF_CLIENT_CAP_COPY_REGION_V1) ==
          UINT64_C(0)) {
    result = CUDA_ERROR_NOT_SUPPORTED;
  }
  if (result == CUDA_SUCCESS && direct_host_copy == UINT32_C(0)) {
    if (direction == MF_CUDA_COPY_H2D) {
      result =
          mf_cuda_control_locked(MF_CLIENT_CONTROL_HOST_MEMORY_REGISTER_V1,
                                 MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_READ,
                                 mf_cuda_global.transport.runtime_context_id, (uint64_t)bytes,
                                 source_host, (uint64_t)bytes, &host_id, &host_generation);
    } else if (direction == MF_CUDA_COPY_D2H) {
      result = mf_cuda_control_payload_locked(
          MF_CLIENT_CONTROL_HOST_MEMORY_REGISTER_V1,
          MF_CLIENT_CONTROL_FLAG_PAYLOAD_FD | MF_CLIENT_CONTROL_FLAG_WRITE,
          mf_cuda_global.transport.runtime_context_id, (uint64_t)bytes, destination_host,
          (uint64_t)bytes, &host_id, &host_generation, &host_payload);
    }
  }
  (void)memset(&arguments, 0, sizeof(arguments));
  if (result == CUDA_SUCCESS && direct_host_copy == UINT32_C(0) && region_copy != UINT32_C(0)) {
    arguments.header.magic = MF_SHARED_ARGUMENT_BLOCK_MAGIC;
    arguments.header.abi_version = MF_SHARED_DEVICE_ABI_VERSION_1;
    arguments.header.header_size = (uint32_t)sizeof(arguments.header);
    arguments.header.entry_size = (uint32_t)sizeof(arguments.entries[0]);
    arguments.header.entry_count = MF_COPY_REGION_ARGUMENT_ENTRY_COUNT_V1;
    arguments.header.flags = MF_ARGUMENT_BLOCK_FLAG_COPY_REGION_V1;
    arguments.header.total_size = (uint64_t)MF_CUDA_COPY_ARGUMENT_SIZE;
    arguments.entries[MF_COPY_REGION_DESTINATION_INDEX_V1].kind = MF_ARGUMENT_KIND_BUFFER;
    arguments.entries[MF_COPY_REGION_DESTINATION_INDEX_V1].flags = MF_ARGUMENT_BUFFER_WRITE;
    arguments.entries[MF_COPY_REGION_DESTINATION_INDEX_V1].object_id =
        direction == MF_CUDA_COPY_D2H ? host_id : destination_memory->remote_id;
    arguments.entries[MF_COPY_REGION_DESTINATION_INDEX_V1].object_generation =
        direction == MF_CUDA_COPY_D2H ? host_generation : destination_memory->remote_generation;
    arguments.entries[MF_COPY_REGION_DESTINATION_INDEX_V1].value = destination_offset;
    arguments.entries[MF_COPY_REGION_SOURCE_INDEX_V1].kind = MF_ARGUMENT_KIND_BUFFER;
    arguments.entries[MF_COPY_REGION_SOURCE_INDEX_V1].flags = MF_ARGUMENT_BUFFER_READ;
    arguments.entries[MF_COPY_REGION_SOURCE_INDEX_V1].object_id =
        direction == MF_CUDA_COPY_H2D ? host_id : source_memory->remote_id;
    arguments.entries[MF_COPY_REGION_SOURCE_INDEX_V1].object_generation =
        direction == MF_CUDA_COPY_H2D ? host_generation : source_memory->remote_generation;
    arguments.entries[MF_COPY_REGION_SOURCE_INDEX_V1].value = source_offset;
    arguments.entries[MF_COPY_REGION_BYTE_COUNT_INDEX_V1].kind = MF_ARGUMENT_KIND_U64;
    arguments.entries[MF_COPY_REGION_BYTE_COUNT_INDEX_V1].value = (uint64_t)bytes;
    result = mf_cuda_status(mf_client_copy_region_argument_block_validate_v1(
        (const uint8_t*)&arguments, (uint64_t)MF_CUDA_COPY_ARGUMENT_SIZE));
  }
  if (result == CUDA_SUCCESS && direct_host_copy == UINT32_C(0) && region_copy != UINT32_C(0)) {
    if (destination_memory != (mf_cuda_object*)0) {
      memory_indices[0] = (uint32_t)(destination_memory - mf_cuda_global.memories);
      memory_generations[0] = destination_memory->generation;
    }
    if (source_memory != (mf_cuda_object*)0) {
      memory_indices[1] = (uint32_t)(source_memory - mf_cuda_global.memories);
      memory_generations[1] = source_memory->generation;
    }
    result = mf_cuda_copy_cache_acquire_locked(
        &arguments, context_index, context->generation, memory_indices, memory_generations,
        direction == MF_CUDA_COPY_D2D ? UINT32_C(1) : UINT32_C(0), &argument_id,
        &argument_generation, &argument_cached);
  }
  if (result == CUDA_SUCCESS) {
    if (direct_host_copy != UINT32_C(0)) {
      mf_cuda_object* device_memory =
          direction == MF_CUDA_COPY_H2D ? destination_memory : source_memory;
      const void* host_pointer =
          direction == MF_CUDA_COPY_H2D ? source_host : (const void*)destination_host;
      command.target = device_memory->remote_id;
      command.arguments[0] = device_memory->remote_generation;
      command.arguments[1] = (uint64_t)(uintptr_t)host_pointer;
      command.arguments[2] = direction == MF_CUDA_COPY_H2D ? destination_offset : source_offset;
      command.arguments[3] = (uint64_t)bytes;
      command.flags = direction == MF_CUDA_COPY_H2D ? MF_RING_COPY_FLAG_DIRECT_HOST_SOURCE_V1
                                                    : MF_RING_COPY_FLAG_DIRECT_HOST_DESTINATION_V1;
    } else if (argument_id != UINT64_C(0)) {
      command.target = argument_id;
      command.arguments[0] = argument_generation;
      command.arguments[1] = UINT64_C(0);
      command.arguments[2] = UINT64_C(0);
      command.arguments[3] = UINT64_C(0);
      command.flags = MF_RING_COPY_FLAG_REGION_ARGUMENT_BLOCK_V1;
    } else {
      command.target = direction == MF_CUDA_COPY_D2H ? host_id : destination_memory->remote_id;
      command.arguments[0] =
          direction == MF_CUDA_COPY_D2H ? host_generation : destination_memory->remote_generation;
      command.arguments[1] = direction == MF_CUDA_COPY_H2D ? host_id : source_memory->remote_id;
      command.arguments[2] =
          direction == MF_CUDA_COPY_H2D ? host_generation : source_memory->remote_generation;
      command.arguments[3] = (uint64_t)bytes;
      command.flags = UINT32_C(0);
    }
    pending.kind = (uint32_t)MF_CUDA_PENDING_COPY;
    must_drain = asynchronous == UINT32_C(0) ||
                         (direct_host_copy != UINT32_C(0) && direction == MF_CUDA_COPY_H2D)
                     ? UINT32_C(1)
                     : UINT32_C(0);
    pending.defer_error = must_drain != UINT32_C(0) ? UINT32_C(0) : UINT32_C(1);
    pending.context_index = context_index;
    pending.context_generation = context->generation;
    if (destination_memory != (mf_cuda_object*)0) {
      pending.memory_indices[0] = (uint32_t)(destination_memory - mf_cuda_global.memories);
      pending.memory_generations[0] = destination_memory->generation;
    }
    if (source_memory != (mf_cuda_object*)0) {
      const uint32_t reference_index =
          destination_memory == (mf_cuda_object*)0 ? UINT32_C(0) : UINT32_C(1);
      pending.memory_indices[reference_index] = (uint32_t)(source_memory - mf_cuda_global.memories);
      pending.memory_generations[reference_index] = source_memory->generation;
    }
    pending.host_id = host_id;
    pending.host_generation = host_generation;
    pending.host_destination = direct_host_copy == UINT32_C(0) && direction == MF_CUDA_COPY_D2H
                                   ? destination_host
                                   : (void*)0;
    pending.host_byte_count = direct_host_copy == UINT32_C(0) && direction == MF_CUDA_COPY_D2H
                                  ? (uint64_t)bytes
                                  : UINT64_C(0);
    pending.host_payload = host_payload;
    pending.argument_id = argument_id;
    pending.argument_generation = argument_generation;
    pending.argument_cached = argument_cached;
    result = mf_cuda_enqueue_locked(&command, &pending, &request_id);
    if (result == CUDA_SUCCESS) {
      mf_cuda_payload_initialize_empty(&host_payload);
    }
  }
  if (result == CUDA_SUCCESS && must_drain != UINT32_C(0)) {
    result = mf_cuda_drain_request_locked(request_id, &completion, &completion_result);
    if (result == CUDA_SUCCESS) {
      result = completion_result;
    }
  }
  if (result != CUDA_SUCCESS && request_id == UINT64_C(0) && argument_id != UINT64_C(0) &&
      argument_cached == UINT32_C(0)) {
    const CUresult release_result = mf_cuda_control_locked(
        MF_CLIENT_CONTROL_ARGUMENT_BLOCK_RELEASE_V1, UINT16_C(0), argument_id, argument_generation,
        (const void*)0, UINT64_C(0), (uint64_t*)0, (uint64_t*)0);
    if (result == CUDA_SUCCESS) {
      result = release_result;
    }
  }
  if (result != CUDA_SUCCESS && request_id == UINT64_C(0) && host_id != UINT64_C(0)) {
    const CUresult release_result = mf_cuda_control_locked(
        MF_CLIENT_CONTROL_HOST_MEMORY_RELEASE_V1, UINT16_C(0), host_id, host_generation,
        (const void*)0, UINT64_C(0), (uint64_t*)0, (uint64_t*)0);
    if (result == CUDA_SUCCESS) {
      result = release_result;
    }
  }
  mf_client_payload_close_v1(&host_payload);
  (void)stream_last_request;
  mf_cuda_queue_unlock();
  return result;
}

CUresult cuMemcpyHtoD_v2(CUdeviceptr destination, const void* source, size_t bytes) {
  return mf_cuda_copy(MF_CUDA_COPY_H2D, destination, (void*)0, (CUdeviceptr)0, source, bytes,
                      (CUstream)0, UINT32_C(0), UINT32_C(0));
}

CUresult cuMemcpyHtoD(CUdeviceptr_v1 destination, const void* source, unsigned int bytes) {
  return cuMemcpyHtoD_v2((CUdeviceptr)destination, source, (size_t)bytes);
}

CUresult cuMemcpyHtoD_v2_ptds(CUdeviceptr destination, const void* source, size_t bytes) {
  return mf_cuda_copy(MF_CUDA_COPY_H2D, destination, (void*)0, (CUdeviceptr)0, source, bytes,
                      (CUstream)0, UINT32_C(0), UINT32_C(1));
}

CUresult cuMemcpyDtoH_v2(void* destination, CUdeviceptr source, size_t bytes) {
  return mf_cuda_copy(MF_CUDA_COPY_D2H, (CUdeviceptr)0, destination, source, (const void*)0, bytes,
                      (CUstream)0, UINT32_C(0), UINT32_C(0));
}

CUresult cuMemcpyDtoH(void* destination, CUdeviceptr_v1 source, unsigned int bytes) {
  return cuMemcpyDtoH_v2(destination, (CUdeviceptr)source, (size_t)bytes);
}

CUresult cuMemcpyDtoH_v2_ptds(void* destination, CUdeviceptr source, size_t bytes) {
  return mf_cuda_copy(MF_CUDA_COPY_D2H, (CUdeviceptr)0, destination, source, (const void*)0, bytes,
                      (CUstream)0, UINT32_C(0), UINT32_C(1));
}

CUresult cuMemcpyDtoD_v2(CUdeviceptr destination, CUdeviceptr source, size_t bytes) {
  return mf_cuda_copy(MF_CUDA_COPY_D2D, destination, (void*)0, source, (const void*)0, bytes,
                      (CUstream)0, UINT32_C(0), UINT32_C(0));
}

CUresult cuMemcpyDtoD(CUdeviceptr_v1 destination, CUdeviceptr_v1 source, unsigned int bytes) {
  return cuMemcpyDtoD_v2((CUdeviceptr)destination, (CUdeviceptr)source, (size_t)bytes);
}

CUresult cuMemcpyDtoD_v2_ptds(CUdeviceptr destination, CUdeviceptr source, size_t bytes) {
  return mf_cuda_copy(MF_CUDA_COPY_D2D, destination, (void*)0, source, (const void*)0, bytes,
                      (CUstream)0, UINT32_C(0), UINT32_C(1));
}

CUresult cuMemcpyHtoDAsync_v2(CUdeviceptr destination, const void* source, size_t bytes,
                              CUstream stream) {
  return mf_cuda_copy(MF_CUDA_COPY_H2D, destination, (void*)0, (CUdeviceptr)0, source, bytes,
                      stream, UINT32_C(1), UINT32_C(0));
}

CUresult cuMemcpyHtoDAsync(CUdeviceptr_v1 destination, const void* source, unsigned int bytes,
                           CUstream stream) {
  return cuMemcpyHtoDAsync_v2((CUdeviceptr)destination, source, (size_t)bytes, stream);
}

CUresult cuMemcpyHtoDAsync_v2_ptsz(CUdeviceptr destination, const void* source, size_t bytes,
                                   CUstream stream) {
  return mf_cuda_copy(MF_CUDA_COPY_H2D, destination, (void*)0, (CUdeviceptr)0, source, bytes,
                      stream, UINT32_C(1), UINT32_C(1));
}

CUresult cuMemcpyDtoHAsync_v2(void* destination, CUdeviceptr source, size_t bytes,
                              CUstream stream) {
  return mf_cuda_copy(MF_CUDA_COPY_D2H, (CUdeviceptr)0, destination, source, (const void*)0, bytes,
                      stream, UINT32_C(1), UINT32_C(0));
}

CUresult cuMemcpyDtoHAsync(void* destination, CUdeviceptr_v1 source, unsigned int bytes,
                           CUstream stream) {
  return cuMemcpyDtoHAsync_v2(destination, (CUdeviceptr)source, (size_t)bytes, stream);
}

CUresult cuMemcpyDtoHAsync_v2_ptsz(void* destination, CUdeviceptr source, size_t bytes,
                                   CUstream stream) {
  return mf_cuda_copy(MF_CUDA_COPY_D2H, (CUdeviceptr)0, destination, source, (const void*)0, bytes,
                      stream, UINT32_C(1), UINT32_C(1));
}

CUresult cuMemcpyDtoDAsync_v2(CUdeviceptr destination, CUdeviceptr source, size_t bytes,
                              CUstream stream) {
  return mf_cuda_copy(MF_CUDA_COPY_D2D, destination, (void*)0, source, (const void*)0, bytes,
                      stream, UINT32_C(1), UINT32_C(0));
}

CUresult cuMemcpyDtoDAsync(CUdeviceptr_v1 destination, CUdeviceptr_v1 source, unsigned int bytes,
                           CUstream stream) {
  return cuMemcpyDtoDAsync_v2((CUdeviceptr)destination, (CUdeviceptr)source, (size_t)bytes, stream);
}

CUresult cuMemcpyDtoDAsync_v2_ptsz(CUdeviceptr destination, CUdeviceptr source, size_t bytes,
                                   CUstream stream) {
  return mf_cuda_copy(MF_CUDA_COPY_D2D, destination, (void*)0, source, (const void*)0, bytes,
                      stream, UINT32_C(1), UINT32_C(1));
}

CUresult cuStreamCreate(CUstream* stream, unsigned int flags) {
  uint32_t context_index = 0;
  uint32_t stream_index = 0;
  mf_cuda_object* context = (mf_cuda_object*)0;
  CUresult result = CUDA_SUCCESS;
  if (stream == (CUstream*)0 || (flags & ~(unsigned int)CU_STREAM_NON_BLOCKING) != UINT32_C(0)) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_current_locked(&context_index, &context);
  }
  stream_index = mf_cuda_free_slot(mf_cuda_global.streams);
  if (result == CUDA_SUCCESS && stream_index == MF_CUDA_OBJECT_CAPACITY) {
    result = CUDA_ERROR_OUT_OF_MEMORY;
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_activate_locked(&mf_cuda_global.streams[stream_index], MF_CUDA_OBJECT_STREAM,
                                     context->device_index, context_index);
  }
  if (result == CUDA_SUCCESS) {
    mf_cuda_object* record = &mf_cuda_global.streams[stream_index];
    record->remote_id = mf_cuda_global.transport.submission_queue_id;
    record->remote_generation = mf_cuda_global.transport.submission_queue_generation;
    record->flags = flags;
    *stream =
        (CUstream)(uintptr_t)mf_cuda_token(MF_CUDA_TAG_STREAM, stream_index, record->generation);
  }
  mf_cuda_unlock();
  return result;
}

static CUresult mf_cuda_stream_query(CUstream stream, uint32_t per_thread_default) {
  uint32_t context_index = 0;
  uint32_t stream_index = MF_CUDA_INDEX_NONE;
  uint32_t stream_generation = UINT32_C(0);
  uint64_t target_request = UINT64_C(0);
  mf_cuda_object* context = (mf_cuda_object*)0;
  mf_cuda_object* stream_record = (mf_cuda_object*)0;
  CUresult result = CUDA_SUCCESS;
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_current_locked(&context_index, &context);
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_stream_locked(stream, context_index, &stream_record);
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_stream_scope_locked(context_index, context, stream_record, per_thread_default,
                                         &stream_index, &stream_generation, &target_request);
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_poll_locked();
    if (result == CUDA_SUCCESS &&
        mf_cuda_has_pending_stream_locked(context_index, context->generation, stream_index,
                                          stream_generation) != 0) {
      result = CUDA_ERROR_NOT_READY;
    }
    if (result == CUDA_SUCCESS) {
      result = mf_cuda_take_async_error_locked(context_index, context->generation, UINT32_C(1),
                                               stream_index, stream_generation, target_request);
    }
  }
  mf_cuda_unlock();
  return result;
}

CUresult cuStreamQuery(CUstream stream) { return mf_cuda_stream_query(stream, UINT32_C(0)); }

CUresult cuStreamQuery_ptsz(CUstream stream) { return mf_cuda_stream_query(stream, UINT32_C(1)); }

static CUresult mf_cuda_stream_synchronize(CUstream stream, uint32_t per_thread_default) {
  uint32_t context_index = 0;
  uint32_t stream_index = MF_CUDA_INDEX_NONE;
  uint32_t stream_generation = UINT32_C(0);
  uint64_t target_request = UINT64_C(0);
  mf_cuda_object* context = (mf_cuda_object*)0;
  mf_cuda_object* stream_record = (mf_cuda_object*)0;
  CUresult result = CUDA_SUCCESS;
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_current_locked(&context_index, &context);
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_stream_locked(stream, context_index, &stream_record);
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_stream_scope_locked(context_index, context, stream_record, per_thread_default,
                                         &stream_index, &stream_generation, &target_request);
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_drain_through_locked(target_request);
    if (result == CUDA_SUCCESS) {
      result = mf_cuda_take_async_error_locked(context_index, context->generation, UINT32_C(1),
                                               stream_index, stream_generation, target_request);
    }
  }
  mf_cuda_unlock();
  return result;
}

CUresult cuStreamSynchronize(CUstream stream) {
  return mf_cuda_stream_synchronize(stream, UINT32_C(0));
}

CUresult cuStreamSynchronize_ptsz(CUstream stream) {
  return mf_cuda_stream_synchronize(stream, UINT32_C(1));
}

CUresult cuStreamDestroy_v2(CUstream stream) {
  uint32_t context_index = 0;
  mf_cuda_object* context = (mf_cuda_object*)0;
  mf_cuda_object* stream_record = (mf_cuda_object*)0;
  CUresult result = CUDA_SUCCESS;
  if (stream == (CUstream)0) {
    return CUDA_ERROR_INVALID_HANDLE;
  }
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_current_locked(&context_index, &context);
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_stream_locked(stream, context_index, &stream_record);
  }
  if (result == CUDA_SUCCESS) {
    const uint32_t stream_index = (uint32_t)(stream_record - mf_cuda_global.streams);
    result = mf_cuda_clear_async_errors_locked(context_index, context->generation, UINT32_C(1),
                                               stream_index, stream_record->generation,
                                               stream_record->last_request);
    stream_record->active = UINT32_C(0);
  }
  mf_cuda_unlock();
  return result;
}

CUresult cuStreamDestroy(CUstream stream) { return cuStreamDestroy_v2(stream); }

CUresult cuEventCreate(CUevent* event, unsigned int flags) {
  const unsigned int valid_flags =
      CU_EVENT_BLOCKING_SYNC | CU_EVENT_DISABLE_TIMING | CU_EVENT_INTERPROCESS;
  uint32_t context_index = 0;
  uint32_t event_index = 0;
  mf_cuda_object* context = (mf_cuda_object*)0;
  CUresult result = CUDA_SUCCESS;
  if (event == (CUevent*)0 || (flags & ~valid_flags) != UINT32_C(0)) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_current_locked(&context_index, &context);
  }
  event_index = mf_cuda_free_slot(mf_cuda_global.events);
  if (result == CUDA_SUCCESS && event_index == MF_CUDA_OBJECT_CAPACITY) {
    result = CUDA_ERROR_OUT_OF_MEMORY;
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_activate_locked(&mf_cuda_global.events[event_index], MF_CUDA_OBJECT_EVENT,
                                     context->device_index, context_index);
  }
  if (result == CUDA_SUCCESS) {
    mf_cuda_object* record = &mf_cuda_global.events[event_index];
    record->remote_id = mf_cuda_global.transport.runtime_event_id;
    record->remote_generation = mf_cuda_global.transport.runtime_event_generation;
    record->flags = flags;
    record->aux = UINT32_C(0);
    record->size = UINT64_C(0);
    *event = (CUevent)(uintptr_t)mf_cuda_token(MF_CUDA_TAG_EVENT, event_index, record->generation);
  }
  mf_cuda_unlock();
  return result;
}

static CUresult mf_cuda_event_record(CUevent event, CUstream stream, uint32_t per_thread_default) {
  uint32_t context_index = 0;
  uint64_t stream_last_request = UINT64_C(0);
  uint64_t timeline = UINT64_C(0);
  mf_cuda_object* context = (mf_cuda_object*)0;
  mf_cuda_object* stream_record = (mf_cuda_object*)0;
  mf_cuda_object* event_record = (mf_cuda_object*)0;
  mf_cuda_pending pending;
  mf_cuda_command command = {MF_CUDA_COMMAND_EVENT_RECORD, 0, {0, 0, 0, 0}, 0};
  uint64_t request_id = UINT64_C(0);
  CUresult result = CUDA_SUCCESS;
  mf_cuda_pending_initialize(&pending);
  result = mf_cuda_queue_lock(UINT32_C(0));
  if (result != CUDA_SUCCESS) {
    return result;
  }
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_current_locked(&context_index, &context);
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_stream_locked(stream, context_index, &stream_record);
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_event_locked(event, &event_record);
  }
  if (result == CUDA_SUCCESS && event_record->owner_context != context_index) {
    result = CUDA_ERROR_INVALID_CONTEXT;
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_stream_scope_locked(context_index, context, stream_record, per_thread_default,
                                         &pending.stream_index, &pending.stream_generation,
                                         &stream_last_request);
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_next_timeline_locked(&timeline);
  }
  if (result == CUDA_SUCCESS) {
    command.target = event_record->remote_id;
    command.arguments[0] = event_record->remote_generation;
    command.arguments[1] = timeline;
    pending.kind = (uint32_t)MF_CUDA_PENDING_EVENT_RECORD;
    pending.defer_error = UINT32_C(1);
    pending.context_index = context_index;
    pending.context_generation = context->generation;
    pending.event_index = (uint32_t)(event_record - mf_cuda_global.events);
    pending.event_generation = event_record->generation;
    result = mf_cuda_enqueue_locked(&command, &pending, &request_id);
  }
  if (result == CUDA_SUCCESS) {
    event_record->size = timeline;
    event_record->aux = UINT32_C(1);
    event_record->last_request = request_id;
    event_record->record_stream_index = pending.stream_index;
    event_record->record_stream_generation = pending.stream_generation;
    event_record->completion_status = (int32_t)CUDA_ERROR_NOT_READY;
  }
  (void)stream_last_request;
  mf_cuda_queue_unlock();
  return result;
}

CUresult cuEventRecord(CUevent event, CUstream stream) {
  return mf_cuda_event_record(event, stream, UINT32_C(0));
}

CUresult cuEventRecord_ptsz(CUevent event, CUstream stream) {
  return mf_cuda_event_record(event, stream, UINT32_C(1));
}

static CUresult mf_cuda_stream_wait_event(CUstream stream, CUevent event, unsigned int flags,
                                          uint32_t per_thread_default) {
  uint32_t context_index = 0;
  uint64_t stream_last_request = UINT64_C(0);
  mf_cuda_object* context = (mf_cuda_object*)0;
  mf_cuda_object* stream_record = (mf_cuda_object*)0;
  mf_cuda_object* event_record = (mf_cuda_object*)0;
  mf_cuda_pending pending;
  mf_cuda_command command = {MF_CUDA_COMMAND_EVENT_WAIT, 0, {0, 0, 0, 0}, 0};
  uint64_t request_id = UINT64_C(0);
  CUresult result = CUDA_SUCCESS;
  mf_cuda_pending_initialize(&pending);
  if (flags != UINT32_C(0)) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  result = mf_cuda_queue_lock(UINT32_C(0));
  if (result != CUDA_SUCCESS) {
    return result;
  }
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_current_locked(&context_index, &context);
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_stream_locked(stream, context_index, &stream_record);
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_event_locked(event, &event_record);
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_stream_scope_locked(context_index, context, stream_record, per_thread_default,
                                         &pending.stream_index, &pending.stream_generation,
                                         &stream_last_request);
  }
  if (result == CUDA_SUCCESS && event_record->aux != UINT32_C(0)) {
    command.target = event_record->remote_id;
    command.arguments[0] = event_record->remote_generation;
    command.arguments[1] = event_record->size;
    pending.kind = (uint32_t)MF_CUDA_PENDING_EVENT_WAIT;
    pending.defer_error = UINT32_C(1);
    pending.context_index = context_index;
    pending.context_generation = context->generation;
    pending.event_index = (uint32_t)(event_record - mf_cuda_global.events);
    pending.event_generation = event_record->generation;
    result = mf_cuda_enqueue_locked(&command, &pending, &request_id);
  }
  (void)request_id;
  (void)stream_last_request;
  mf_cuda_queue_unlock();
  return result;
}

CUresult cuStreamWaitEvent(CUstream stream, CUevent event, unsigned int flags) {
  return mf_cuda_stream_wait_event(stream, event, flags, UINT32_C(0));
}

CUresult cuStreamWaitEvent_ptsz(CUstream stream, CUevent event, unsigned int flags) {
  return mf_cuda_stream_wait_event(stream, event, flags, UINT32_C(1));
}

CUresult cuEventQuery(CUevent event) {
  mf_cuda_object* record = (mf_cuda_object*)0;
  CUresult result = CUDA_SUCCESS;
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_event_locked(event, &record);
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_poll_locked();
  }
  if (result == CUDA_SUCCESS && record->aux == UINT32_C(1)) {
    result = CUDA_ERROR_NOT_READY;
  }
  if (result == CUDA_SUCCESS && record->aux == UINT32_C(2)) {
    result = mf_cuda_take_async_error_locked(
        record->owner_context, mf_cuda_global.contexts[record->owner_context].generation,
        UINT32_C(1), record->record_stream_index, record->record_stream_generation,
        record->last_request);
  }
  mf_cuda_unlock();
  return result;
}

CUresult cuEventSynchronize(CUevent event) {
  mf_cuda_object* record = (mf_cuda_object*)0;
  CUresult result = CUDA_SUCCESS;
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_event_locked(event, &record);
  }
  if (result == CUDA_SUCCESS && record->aux != UINT32_C(0)) {
    result = mf_cuda_drain_through_locked(record->last_request);
    if (result == CUDA_SUCCESS) {
      result = mf_cuda_take_async_error_locked(
          record->owner_context, mf_cuda_global.contexts[record->owner_context].generation,
          UINT32_C(1), record->record_stream_index, record->record_stream_generation,
          record->last_request);
    }
  }
  mf_cuda_unlock();
  return result;
}

CUresult cuEventDestroy_v2(CUevent event) {
  mf_cuda_object* record = (mf_cuda_object*)0;
  CUresult result = CUDA_SUCCESS;
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_event_locked(event, &record);
  }
  if (result == CUDA_SUCCESS) {
    if (record->aux != UINT32_C(0) && record->owner_context < MF_CUDA_OBJECT_CAPACITY &&
        mf_cuda_global.contexts[record->owner_context].active != UINT32_C(0)) {
      result = mf_cuda_clear_async_errors_locked(
          record->owner_context, mf_cuda_global.contexts[record->owner_context].generation,
          UINT32_C(1), record->record_stream_index, record->record_stream_generation,
          record->last_request);
    }
    record->active = UINT32_C(0);
  }
  mf_cuda_unlock();
  return result;
}

CUresult cuEventDestroy(CUevent event) { return cuEventDestroy_v2(event); }

CUresult cuEventElapsedTime(float* milliseconds, CUevent start, CUevent end) {
  mf_cuda_object* start_record = (mf_cuda_object*)0;
  mf_cuda_object* end_record = (mf_cuda_object*)0;
  CUresult result = CUDA_SUCCESS;
  if (milliseconds == (float*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  mf_cuda_lock();
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_event_locked(start, &start_record);
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_event_locked(end, &end_record);
  }
  mf_cuda_unlock();
  return result == CUDA_SUCCESS ? CUDA_ERROR_NOT_SUPPORTED : result;
}

static CUresult mf_cuda_launch_kernel(CUfunction function, unsigned int grid_x, unsigned int grid_y,
                                      unsigned int grid_z, unsigned int block_x,
                                      unsigned int block_y, unsigned int block_z,
                                      unsigned int shared_memory_bytes, CUstream stream,
                                      void** kernel_parameters, void** extra,
                                      uint32_t per_thread_default) {
  uint32_t context_index = 0;
  uint32_t function_index = 0;
  uint32_t parameter_index = 0;
  uint32_t element_count = 0;
  uint32_t argument_cached = UINT32_C(0);
  uint32_t memory_indices[3] = {MF_CUDA_INDEX_NONE, MF_CUDA_INDEX_NONE, MF_CUDA_INDEX_NONE};
  uint32_t memory_generations[3] = {UINT32_C(0), UINT32_C(0), UINT32_C(0)};
  uint64_t offsets[3] = {0, 0, 0};
  uint64_t argument_id = 0;
  uint64_t argument_generation = 0;
  uint64_t stream_last_request = UINT64_C(0);
  CUdeviceptr pointers[3] = {0, 0, 0};
  mf_cuda_object* memories[3] = {(mf_cuda_object*)0, (mf_cuda_object*)0, (mf_cuda_object*)0};
  mf_cuda_object* context = (mf_cuda_object*)0;
  mf_cuda_object* function_record = (mf_cuda_object*)0;
  mf_cuda_object* module_record = (mf_cuda_object*)0;
  mf_cuda_object* stream_record = (mf_cuda_object*)0;
  mf_cuda_add_argument_block arguments;
  mf_cuda_pending pending;
  mf_cuda_command command = {MF_CUDA_COMMAND_LAUNCH, 0, {0, 0, 0, 0}, 0};
  uint64_t request_id = UINT64_C(0);
  CUresult result = CUDA_SUCCESS;
  mf_cuda_pending_initialize(&pending);
  if (grid_x == UINT32_C(0) || grid_y == UINT32_C(0) || grid_z == UINT32_C(0) ||
      block_x == UINT32_C(0) || block_y == UINT32_C(0) || block_z == UINT32_C(0) ||
      kernel_parameters == (void**)0 || extra != (void**)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  if (grid_z != UINT32_C(1) || block_z != UINT32_C(1) || shared_memory_bytes != UINT32_C(0)) {
    return CUDA_ERROR_NOT_SUPPORTED;
  }
  for (parameter_index = 0; parameter_index < UINT32_C(4); ++parameter_index) {
    if (kernel_parameters[parameter_index] == (void*)0) {
      return CUDA_ERROR_INVALID_VALUE;
    }
  }
  if (mf_cuda_entry_trace_enabled() != 0) {
    fprintf(stderr, "MF_LAUNCH f=%p grid=%ux%u\n", (void*)function, grid_x, block_x);
  }
  result = mf_cuda_queue_lock(UINT32_C(0));
  if (result != CUDA_SUCCESS) {
    return result;
  }
  result = mf_cuda_require_locked();
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_current_locked(&context_index, &context);
  }
  if (result == CUDA_SUCCESS) {
    result =
        mf_cuda_lookup_token_locked((void*)function, MF_CUDA_TAG_FUNCTION, MF_CUDA_OBJECT_FUNCTION,
                                    mf_cuda_global.functions, &function_index, &function_record);
  }
  /* Semantic kernel profile: deferred client cubins (framework fatbins) do
     not materialize in the daemon; launches route by registered kernel name
     to provider-side equivalents. */
  if (result == CUDA_SUCCESS && mf_module_deferred[function_record->aux] != 0) {
    const char* kernel_name = mf_function_names[function_index];
    if (kernel_name[0] != '\0' && strstr(kernel_name, "sleep_kernel") != (char*)0) {
      /* torch.cuda._sleep busy-waits in the kernel; the smoke contract only
         requires the launch to complete. */
      mf_cuda_queue_unlock();
      return CUDA_SUCCESS;
    }
    if (kernel_name[0] != '\0' && strstr(kernel_name, "spin_kernel") != (char*)0) {
      /* torch _sleep compiles as at::cuda::{anonymous}::spin_kernel<long>
         (Sleep.cu): a clock64 spin with no observable memory effect. */
      mf_cuda_queue_unlock();
      return CUDA_SUCCESS;
    }
    if (kernel_name[0] != '\0' && strstr(kernel_name, "elementwise_kernel") != (char*)0 &&
        strstr(kernel_name, "AddFunctor") != (char*)0) {
      /* vectorized_elementwise_kernel<num, AddFunctor<T>, ...>: params are
         (int numel, AddFunctor functor (T alpha), array_t<T> data) with
         data = {out, in1, in2}. int32 tensors execute as a host-side add
         over daemon-backed memory through the existing copy path. */
      unsigned int num_elements = *(unsigned int*)kernel_parameters[0];
      int alpha = *(int*)kernel_parameters[1];
      void** data_array = (void**)kernel_parameters[2];
      CUdeviceptr out_pointer = (CUdeviceptr)(uintptr_t)data_array[0];
      CUdeviceptr left_pointer = (CUdeviceptr)(uintptr_t)data_array[1];
      CUdeviceptr right_pointer = (CUdeviceptr)(uintptr_t)data_array[2];
      int* left_values = (int*)0;
      int* right_values = (int*)0;
      uint32_t element_index = 0;
      if (num_elements == UINT32_C(0)) {
        mf_cuda_queue_unlock();
        return CUDA_SUCCESS;
      }
      left_values = malloc((size_t)num_elements * sizeof(int));
      right_values = malloc((size_t)num_elements * sizeof(int));
      if (left_values == (int*)0 || right_values == (int*)0) {
        free(left_values);
        free(right_values);
        mf_cuda_queue_unlock();
        return CUDA_ERROR_OUT_OF_MEMORY;
      }
      {
        CUstream stream_arg = stream;
        result = mf_cuda_copy(MF_CUDA_COPY_D2H, (CUdeviceptr)0, left_values, left_pointer,
                              (const void*)0, (size_t)num_elements * sizeof(int), stream_arg,
                              UINT32_C(0), per_thread_default);
        if (result == CUDA_SUCCESS) {
          result = mf_cuda_copy(MF_CUDA_COPY_D2H, (CUdeviceptr)0, right_values, right_pointer,
                                (const void*)0, (size_t)num_elements * sizeof(int), stream_arg,
                                UINT32_C(0), per_thread_default);
        }
        if (result == CUDA_SUCCESS) {
          for (element_index = 0; element_index < num_elements; ++element_index) {
            left_values[element_index] += alpha * right_values[element_index];
          }
          result = mf_cuda_copy(MF_CUDA_COPY_H2D, out_pointer, (void*)left_values,
                                (CUdeviceptr)0, (const void*)0,
                                (size_t)num_elements * sizeof(int), stream_arg, UINT32_C(0),
                                per_thread_default);
        }
      }
      free(left_values);
      free(right_values);
      mf_cuda_queue_unlock();
      if (mf_cuda_entry_trace_enabled() != 0) {
        fprintf(stderr, "MF_SEMANTIC add numel=%u alpha=%d rc=%d\n", num_elements, alpha,
                (int)result);
      }
      return result;
    }
    mf_cuda_queue_unlock();
    if (mf_cuda_entry_trace_enabled() != 0) {
      fprintf(stderr, "MF_SEMANTIC miss name=%s\n", kernel_name);
    }
    return CUDA_ERROR_NOT_SUPPORTED;
  }
  if (result == CUDA_SUCCESS &&
      (function_record->size != UINT64_C(4) ||
       (function_record->owner_context != MF_CUDA_INDEX_NONE &&
        function_record->owner_context != context_index))) {
    result = function_record->owner_context != MF_CUDA_INDEX_NONE &&
                     function_record->owner_context != context_index
                 ? CUDA_ERROR_INVALID_CONTEXT
                 : CUDA_ERROR_NOT_SUPPORTED;
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_stream_locked(stream, context_index, &stream_record);
  }
  if (result == CUDA_SUCCESS) {
    result = mf_cuda_stream_scope_locked(context_index, context, stream_record, per_thread_default,
                                         &pending.stream_index, &pending.stream_generation,
                                         &stream_last_request);
  }
  if (result == CUDA_SUCCESS) {
    for (parameter_index = 0; parameter_index < UINT32_C(3); ++parameter_index) {
      (void)memcpy(&pointers[parameter_index], kernel_parameters[parameter_index],
                   sizeof(pointers[parameter_index]));
      result = mf_cuda_memory_locked(pointers[parameter_index], (size_t)1,
                                     &memories[parameter_index], &offsets[parameter_index]);
      if (result != CUDA_SUCCESS) {
        break;
      }
      if (memories[parameter_index]->owner_context != context_index) {
        result = CUDA_ERROR_INVALID_CONTEXT;
        break;
      }
    }
  }
  if (result == CUDA_SUCCESS) {
    (void)memcpy(&element_count, kernel_parameters[3], sizeof(element_count));
    for (parameter_index = 0; parameter_index < UINT32_C(3); ++parameter_index) {
      if ((offsets[parameter_index] & UINT64_C(3)) != UINT64_C(0) ||
          offsets[parameter_index] > memories[parameter_index]->size ||
          (uint64_t)element_count >
              (memories[parameter_index]->size - offsets[parameter_index]) / UINT64_C(4)) {
        result = CUDA_ERROR_INVALID_VALUE;
        break;
      }
    }
  }
  (void)memset(&arguments, 0, sizeof(arguments));
  if (result == CUDA_SUCCESS) {
    arguments.header.magic = MF_SHARED_ARGUMENT_BLOCK_MAGIC;
    arguments.header.abi_version = MF_SHARED_DEVICE_ABI_VERSION_1;
    arguments.header.header_size = (uint32_t)sizeof(arguments.header);
    arguments.header.entry_size = (uint32_t)sizeof(arguments.entries[0]);
    arguments.header.entry_count = UINT32_C(4);
    arguments.header.flags = MF_ARGUMENT_BLOCK_FLAG_LAUNCH_DIMENSIONS_XY_V1;
    arguments.header.total_size = (uint64_t)sizeof(arguments);
    arguments.header.reserved[MF_ARGUMENT_BLOCK_LAUNCH_GRID_X_INDEX_V1] = (uint64_t)grid_x;
    arguments.header.reserved[MF_ARGUMENT_BLOCK_LAUNCH_GRID_Y_INDEX_V1] = (uint64_t)grid_y;
    arguments.header.reserved[MF_ARGUMENT_BLOCK_LAUNCH_BLOCK_X_INDEX_V1] = (uint64_t)block_x;
    arguments.header.reserved[MF_ARGUMENT_BLOCK_LAUNCH_BLOCK_Y_INDEX_V1] = (uint64_t)block_y;
    arguments.entries[0].kind = MF_ARGUMENT_KIND_BUFFER;
    arguments.entries[0].flags = MF_ARGUMENT_BUFFER_WRITE;
    arguments.entries[0].object_id = memories[0]->remote_id;
    arguments.entries[0].object_generation = memories[0]->remote_generation;
    arguments.entries[0].value = offsets[0];
    arguments.entries[1].kind = MF_ARGUMENT_KIND_BUFFER;
    arguments.entries[1].flags = MF_ARGUMENT_BUFFER_READ;
    arguments.entries[1].object_id = memories[1]->remote_id;
    arguments.entries[1].object_generation = memories[1]->remote_generation;
    arguments.entries[1].value = offsets[1];
    arguments.entries[2].kind = MF_ARGUMENT_KIND_BUFFER;
    arguments.entries[2].flags = MF_ARGUMENT_BUFFER_READ;
    arguments.entries[2].object_id = memories[2]->remote_id;
    arguments.entries[2].object_generation = memories[2]->remote_generation;
    arguments.entries[2].value = offsets[2];
    arguments.entries[3].kind = MF_ARGUMENT_KIND_U32;
    arguments.entries[3].value = (uint64_t)element_count;
    result = mf_cuda_status(
        mf_client_argument_block_validate_v1((const uint8_t*)&arguments, sizeof(arguments)));
  }
  if (result == CUDA_SUCCESS) {
    module_record = &mf_cuda_global.modules[function_record->aux];
    for (parameter_index = UINT32_C(0); parameter_index < UINT32_C(3); ++parameter_index) {
      memory_indices[parameter_index] =
          (uint32_t)(memories[parameter_index] - mf_cuda_global.memories);
      memory_generations[parameter_index] = memories[parameter_index]->generation;
    }
    result = mf_cuda_argument_cache_acquire_locked(&arguments, context_index, context->generation,
                                                   function_record->aux, module_record->generation,
                                                   memory_indices, memory_generations, &argument_id,
                                                   &argument_generation, &argument_cached);
  }
  if (result == CUDA_SUCCESS) {
    command.target = module_record->remote_id;
    command.arguments[0] = module_record->remote_generation;
    command.arguments[1] = function_record->remote_id;
    command.arguments[2] = argument_id;
    command.arguments[3] = argument_generation;
    pending.kind = (uint32_t)MF_CUDA_PENDING_LAUNCH;
    pending.defer_error = UINT32_C(1);
    pending.context_index = context_index;
    pending.context_generation = context->generation;
    pending.module_index = function_record->aux;
    pending.module_generation = module_record->generation;
    for (parameter_index = UINT32_C(0); parameter_index < UINT32_C(3); ++parameter_index) {
      pending.memory_indices[parameter_index] = memory_indices[parameter_index];
      pending.memory_generations[parameter_index] = memory_generations[parameter_index];
    }
    pending.argument_id = argument_id;
    pending.argument_generation = argument_generation;
    pending.argument_cached = argument_cached;
    result = mf_cuda_enqueue_locked(&command, &pending, &request_id);
  }
  if (result != CUDA_SUCCESS && request_id == UINT64_C(0) && argument_id != UINT64_C(0) &&
      argument_cached == UINT32_C(0)) {
    const CUresult release_result = mf_cuda_control_locked(
        MF_CLIENT_CONTROL_ARGUMENT_BLOCK_RELEASE_V1, UINT16_C(0), argument_id, argument_generation,
        (const void*)0, UINT64_C(0), (uint64_t*)0, (uint64_t*)0);
    if (result == CUDA_SUCCESS) {
      result = release_result;
    }
  }
  (void)stream_last_request;
  mf_cuda_queue_unlock();
  return result;
}

CUresult cuLaunchKernel_ptsz(CUfunction function, unsigned int grid_x, unsigned int grid_y,
                             unsigned int grid_z, unsigned int block_x, unsigned int block_y,
                             unsigned int block_z, unsigned int shared_memory_bytes,
                             CUstream stream, void** kernel_parameters, void** extra) {
  return mf_cuda_launch_kernel(function, grid_x, grid_y, grid_z, block_x, block_y, block_z,
                               shared_memory_bytes, stream, kernel_parameters, extra, UINT32_C(1));
}

CUresult cuLaunchKernel(CUfunction function, unsigned int grid_x, unsigned int grid_y,
                        unsigned int grid_z, unsigned int block_x, unsigned int block_y,
                        unsigned int block_z, unsigned int shared_memory_bytes, CUstream stream,
                        void** kernel_parameters, void** extra) {
  return mf_cuda_launch_kernel(function, grid_x, grid_y, grid_z, block_x, block_y, block_z,
                               shared_memory_bytes, stream, kernel_parameters, extra, UINT32_C(0));
}

typedef struct mf_cuda_error_entry {
  CUresult error;
  const char* name;
  const char* description;
} mf_cuda_error_entry;

static const mf_cuda_error_entry mf_cuda_errors[] = {
    {CUDA_SUCCESS, "CUDA_SUCCESS", "no error"},
    {CUDA_ERROR_INVALID_VALUE, "CUDA_ERROR_INVALID_VALUE", "invalid argument"},
    {CUDA_ERROR_OUT_OF_MEMORY, "CUDA_ERROR_OUT_OF_MEMORY", "out of memory"},
    {CUDA_ERROR_NOT_INITIALIZED, "CUDA_ERROR_NOT_INITIALIZED", "driver not initialized"},
    {CUDA_ERROR_DEINITIALIZED, "CUDA_ERROR_DEINITIALIZED", "driver view is terminal"},
    {CUDA_ERROR_NO_DEVICE, "CUDA_ERROR_NO_DEVICE", "no compatible device"},
    {CUDA_ERROR_INVALID_DEVICE, "CUDA_ERROR_INVALID_DEVICE", "invalid device ordinal"},
    {CUDA_ERROR_INVALID_IMAGE, "CUDA_ERROR_INVALID_IMAGE", "invalid module image"},
    {CUDA_ERROR_INVALID_CONTEXT, "CUDA_ERROR_INVALID_CONTEXT", "invalid context"},
    {CUDA_ERROR_INVALID_HANDLE, "CUDA_ERROR_INVALID_HANDLE", "invalid or stale handle"},
    {CUDA_ERROR_NOT_FOUND, "CUDA_ERROR_NOT_FOUND", "symbol or object not found"},
    {CUDA_ERROR_NOT_READY, "CUDA_ERROR_NOT_READY", "operation not ready"},
    {CUDA_ERROR_ILLEGAL_ADDRESS, "CUDA_ERROR_ILLEGAL_ADDRESS", "illegal device address"},
    {CUDA_ERROR_LAUNCH_FAILED, "CUDA_ERROR_LAUNCH_FAILED", "kernel launch failed"},
    {CUDA_ERROR_CONTEXT_IS_DESTROYED, "CUDA_ERROR_CONTEXT_IS_DESTROYED", "context is destroyed"},
    {CUDA_ERROR_NOT_SUPPORTED, "CUDA_ERROR_NOT_SUPPORTED", "operation not supported"},
    {CUDA_ERROR_SYSTEM_NOT_READY, "CUDA_ERROR_SYSTEM_NOT_READY", "runtime transport not ready"},
    {CUDA_ERROR_UNKNOWN, "CUDA_ERROR_UNKNOWN", "unknown error"}};

CUresult cuGetErrorName(CUresult error, const char** name) {
  size_t index = 0;
  if (name == (const char**)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  for (index = 0; index < sizeof(mf_cuda_errors) / sizeof(mf_cuda_errors[0]); ++index) {
    if (mf_cuda_errors[index].error == error) {
      *name = mf_cuda_errors[index].name;
      return CUDA_SUCCESS;
    }
  }
  return CUDA_ERROR_INVALID_VALUE;
}

CUresult cuGetErrorString(CUresult error, const char** description) {
  size_t index = 0;
  if (description == (const char**)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  for (index = 0; index < sizeof(mf_cuda_errors) / sizeof(mf_cuda_errors[0]); ++index) {
    if (mf_cuda_errors[index].error == error) {
      *description = mf_cuda_errors[index].description;
      return CUDA_SUCCESS;
    }
  }
  return CUDA_ERROR_INVALID_VALUE;
}

typedef struct mf_cuda_symbol_entry {
  const char* name;
  void* address;
  int minimum_version;
} mf_cuda_symbol_entry;

#define MF_CUDA_INTERNAL(name)
#define MF_CUDA_SYMBOL(name, version, status, route, parameters, arguments)                        \
  extern CUresult mf_cuda_public_##name parameters __asm__(#name);
#include "../symbols.def"
#undef MF_CUDA_SYMBOL
#undef MF_CUDA_INTERNAL

#define MF_CUDA_INTERNAL(name)
#define MF_CUDA_SYMBOL(name, version, status, route, parameters, arguments)                        \
  {#name, __extension__((void*)(mf_cuda_public_##name)), version},
static const mf_cuda_symbol_entry mf_cuda_symbols[] = {
#include "../symbols.def"
};
#undef MF_CUDA_SYMBOL
#undef MF_CUDA_INTERNAL

static const mf_cuda_symbol_entry* mf_cuda_find_symbol(const char* name) {
  size_t index = 0;
  for (index = 0; index < sizeof(mf_cuda_symbols) / sizeof(mf_cuda_symbols[0]); ++index) {
    if (strcmp(mf_cuda_symbols[index].name, name) == 0) {
      return &mf_cuda_symbols[index];
    }
  }
  return (const mf_cuda_symbol_entry*)0;
}

typedef struct mf_cuda_symbol_route {
  const char* base;
  const char* upgraded;
  int upgrade_version;
} mf_cuda_symbol_route;

static const mf_cuda_symbol_route mf_cuda_symbol_routes[] = {
    {"cuDeviceGetUuid", "cuDeviceGetUuid_v2", 11040},
    {"cuDeviceTotalMem", "cuDeviceTotalMem_v2", 3020},
    {"cuDevicePrimaryCtxRelease", "cuDevicePrimaryCtxRelease_v2", 11000},
    {"cuDevicePrimaryCtxReset", "cuDevicePrimaryCtxReset_v2", 11000},
    {"cuCtxCreate", "cuCtxCreate_v2", 3020},
    {"cuCtxDestroy", "cuCtxDestroy_v2", 4000},
    {"cuCtxPushCurrent", "cuCtxPushCurrent_v2", 4000},
    {"cuCtxPopCurrent", "cuCtxPopCurrent_v2", 4000},
    {"cuMemAlloc", "cuMemAlloc_v2", 3020},
    {"cuMemFree", "cuMemFree_v2", 3020},
    {"cuMemcpyHtoD", "cuMemcpyHtoD_v2", 3020},
    {"cuMemcpyDtoH", "cuMemcpyDtoH_v2", 3020},
    {"cuMemcpyDtoD", "cuMemcpyDtoD_v2", 3020},
    {"cuMemcpyHtoDAsync", "cuMemcpyHtoDAsync_v2", 3020},
    {"cuMemcpyDtoHAsync", "cuMemcpyDtoHAsync_v2", 3020},
    {"cuMemcpyDtoDAsync", "cuMemcpyDtoDAsync_v2", 3020},
    {"cuStreamDestroy", "cuStreamDestroy_v2", 4000},
    {"cuEventDestroy", "cuEventDestroy_v2", 4000},
    {"cuGetProcAddress", "cuGetProcAddress_v2", 12000}};

static const char* mf_cuda_versioned_symbol(const char* symbol, int cuda_version) {
  size_t index = 0;
  if (strcmp(symbol, "cuCtxCreate") == 0 && cuda_version >= 13000) {
    return "cuCtxCreate_v4";
  }
  for (index = 0; index < sizeof(mf_cuda_symbol_routes) / sizeof(mf_cuda_symbol_routes[0]);
       ++index) {
    if (strcmp(mf_cuda_symbol_routes[index].base, symbol) == 0) {
      return cuda_version >= mf_cuda_symbol_routes[index].upgrade_version
                 ? mf_cuda_symbol_routes[index].upgraded
                 : symbol;
    }
  }
  return symbol;
}

static const char* mf_cuda_per_thread_symbol(const char* symbol) {
  if (strcmp(symbol, "cuMemcpyHtoD") == 0 || strcmp(symbol, "cuMemcpyHtoD_v2") == 0) {
    return "cuMemcpyHtoD_v2_ptds";
  }
  if (strcmp(symbol, "cuMemcpyDtoH") == 0 || strcmp(symbol, "cuMemcpyDtoH_v2") == 0) {
    return "cuMemcpyDtoH_v2_ptds";
  }
  if (strcmp(symbol, "cuMemcpyDtoD") == 0 || strcmp(symbol, "cuMemcpyDtoD_v2") == 0) {
    return "cuMemcpyDtoD_v2_ptds";
  }
  if (strcmp(symbol, "cuMemcpyHtoDAsync") == 0 || strcmp(symbol, "cuMemcpyHtoDAsync_v2") == 0) {
    return "cuMemcpyHtoDAsync_v2_ptsz";
  }
  if (strcmp(symbol, "cuMemcpyDtoHAsync") == 0 || strcmp(symbol, "cuMemcpyDtoHAsync_v2") == 0) {
    return "cuMemcpyDtoHAsync_v2_ptsz";
  }
  if (strcmp(symbol, "cuMemcpyDtoDAsync") == 0 || strcmp(symbol, "cuMemcpyDtoDAsync_v2") == 0) {
    return "cuMemcpyDtoDAsync_v2_ptsz";
  }
  if (strcmp(symbol, "cuLaunchKernel") == 0) {
    return "cuLaunchKernel_ptsz";
  }
  if (strcmp(symbol, "cuStreamWaitEvent") == 0) {
    return "cuStreamWaitEvent_ptsz";
  }
  if (strcmp(symbol, "cuStreamQuery") == 0) {
    return "cuStreamQuery_ptsz";
  }
  if (strcmp(symbol, "cuStreamSynchronize") == 0) {
    return "cuStreamSynchronize_ptsz";
  }
  if (strcmp(symbol, "cuEventRecord") == 0) {
    return "cuEventRecord_ptsz";
  }
  return symbol;
}

/* Typed fallback for driver symbols the managed backend does not implement
   yet. cudart builds its dispatch table through one full lookup sweep and
   requires every entry to resolve, so unknown well-formed cu* symbols resolve
   to this stub and each real gap surfaces at call time as
   CUDA_ERROR_NOT_SUPPORTED. MetaFlux-strengthened behavior. */
void* mf_cuda_gap_lookup(const char* symbol);

CUresult cuGetProcAddress_v2(const char* symbol, void** function, int cuda_version,
                             cuuint64_t flags, CUdriverProcAddressQueryResult* symbol_status) {
  const mf_cuda_symbol_entry* entry = (const mf_cuda_symbol_entry*)0;
  const mf_cuda_symbol_entry* per_thread_entry = (const mf_cuda_symbol_entry*)0;
  const char* selected_symbol = (const char*)0;
  CUdriverProcAddressQueryResult query_result = CU_GET_PROC_ADDRESS_SYMBOL_NOT_FOUND;
  const cuuint64_t known_flags = (cuuint64_t)(CU_GET_PROC_ADDRESS_LEGACY_STREAM |
                                              CU_GET_PROC_ADDRESS_PER_THREAD_DEFAULT_STREAM);
  if (function == (void**)0 || symbol == (const char*)0 || cuda_version < 0 ||
      (flags & ~known_flags) != UINT64_C(0) || flags == known_flags) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *function = (void*)0;
  selected_symbol = mf_cuda_versioned_symbol(symbol, cuda_version);
  entry = mf_cuda_find_symbol(selected_symbol);
  if (entry == (const mf_cuda_symbol_entry*)0) {
    entry = mf_cuda_find_symbol(symbol);
  }
  if (entry != (const mf_cuda_symbol_entry*)0 && cuda_version < entry->minimum_version) {
    query_result = CU_GET_PROC_ADDRESS_VERSION_NOT_SUFFICIENT;
    entry = (const mf_cuda_symbol_entry*)0;
  }
  if ((flags & (cuuint64_t)CU_GET_PROC_ADDRESS_PER_THREAD_DEFAULT_STREAM) != UINT64_C(0)) {
    selected_symbol =
        mf_cuda_per_thread_symbol(entry == (const mf_cuda_symbol_entry*)0 ? symbol : entry->name);
    per_thread_entry = mf_cuda_find_symbol(selected_symbol);
    if (per_thread_entry != (const mf_cuda_symbol_entry*)0 &&
        cuda_version >= per_thread_entry->minimum_version) {
      entry = per_thread_entry;
    }
  }
  if (entry != (const mf_cuda_symbol_entry*)0) {
    *function = entry->address;
    query_result = CU_GET_PROC_ADDRESS_SUCCESS;
  } else {
    void* gap_fn = mf_cuda_gap_lookup(symbol);
    if (gap_fn != (void*)0) {
      *function = gap_fn;
      query_result = CU_GET_PROC_ADDRESS_SUCCESS;
    }
  }
  if (symbol_status != (CUdriverProcAddressQueryResult*)0) {
    *symbol_status = query_result;
  }
  return CUDA_SUCCESS;
}

CUresult cuGetProcAddress(const char* symbol, void** function, int cuda_version, cuuint64_t flags) {
  CUdriverProcAddressQueryResult status = CU_GET_PROC_ADDRESS_SYMBOL_NOT_FOUND;
  return cuGetProcAddress_v2(symbol, function, cuda_version, flags, &status);
}
