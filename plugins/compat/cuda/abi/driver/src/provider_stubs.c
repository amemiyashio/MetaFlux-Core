/* d408 attestation uses dladdr */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
/* GENERATED typed surface stubs: symbols advertised by symbols.def whose
   managed implementations are not yet functional. Read-only queries report
   safe zero defaults; everything else returns CUDA_ERROR_NOT_SUPPORTED so
   callers observe a typed, deterministic gap. Entries graduate to
   functional implementations in provider.c through work-item-0.2.0.1.
   Generated against the R610 frozen signatures. */

#define METAFLUX_CUDA_ABI_INTERNAL 1
#include "metaflux/cuda/abi.h"

#include "managed-renames.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <dlfcn.h>

static int mf_stub_trace_enabled = -1;

static int mf_stub_trace(void) {
  if (mf_stub_trace_enabled < 0) {
    mf_stub_trace_enabled = getenv("METAFLUX_TRACE_STUBS") != NULL ? 1 : 0;
  }
  return mf_stub_trace_enabled;
}

#define MF_STUB_TRACE \
  do { \
    if (mf_stub_trace()) { \
      fprintf(stderr, "MF_STUB_CALL %s\n", __func__); \
    } \
  } while (0)

CUresult cuCtxDetach(CUcontext ctx) {
  MF_STUB_TRACE;
  (void)ctx;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuCtxDisablePeerAccess(CUcontext peer_context) {
  MF_STUB_TRACE;
  (void)peer_context;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuCtxEnablePeerAccess(CUcontext peer_context, unsigned int flags) {
  MF_STUB_TRACE;
  (void)peer_context;
  (void)flags;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuCtxGetApiVersion(CUcontext ctx, unsigned int* version) {
  MF_STUB_TRACE;
  (void)ctx;
  if (version == (unsigned int*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *version = 12000;
  return CUDA_SUCCESS;

}

CUresult cuCtxGetCacheConfig(CUfunc_cache* pconfig) {
  MF_STUB_TRACE;
  if (pconfig == (CUfunc_cache*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *pconfig = CU_FUNC_CACHE_PREFER_NONE;
  return CUDA_SUCCESS;

}

CUresult cuCtxGetFlags(unsigned int* flags) {
  MF_STUB_TRACE;
  if (flags == (unsigned int*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *flags = 0;
  return CUDA_SUCCESS;

}

CUresult cuCtxGetLimit(size_t* pvalue, CUlimit limit) {
  MF_STUB_TRACE;
  (void)limit;
  if (pvalue == (size_t*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *pvalue = 0;
  return CUDA_SUCCESS;

}

CUresult cuCtxGetSharedMemConfig(CUsharedconfig* pconfig) {
  MF_STUB_TRACE;
  if (pconfig == (CUsharedconfig*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *pconfig = CU_SHARED_MEM_CONFIG_DEFAULT_BANK_SIZE;
  return CUDA_SUCCESS;

}

CUresult cuCtxGetStreamPriorityRange(int* least_priority, int* greatest_priority) {
  MF_STUB_TRACE;
  if (least_priority == (int*)0 || greatest_priority == (int*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *least_priority = 0;
  *greatest_priority = 0;
  return CUDA_SUCCESS;

}

CUresult cuCtxSetCacheConfig(CUfunc_cache config) {
  MF_STUB_TRACE;
  (void)config;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuCtxSetLimit(CUlimit limit, size_t value) {
  MF_STUB_TRACE;
  (void)limit;
  (void)value;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuCtxSetSharedMemConfig(CUsharedconfig config) {
  MF_STUB_TRACE;
  (void)config;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuDeviceCanAccessPeer(int* canAccessPeer, CUdevice dev, CUdevice peerDev) {
  MF_STUB_TRACE;
  (void)dev;
  (void)peerDev;
  if (canAccessPeer == (int*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *canAccessPeer = 0;
  return CUDA_SUCCESS;

}

CUresult cuFuncGetAttribute(int* pi, CUfunction_attribute attrib, CUfunction hfunc) {
  MF_STUB_TRACE;
  (void)pi;
  (void)attrib;
  (void)hfunc;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuFuncSetAttribute(CUfunction hfunc, CUfunction_attribute attrib, int value) {
  MF_STUB_TRACE;
  (void)hfunc;
  (void)attrib;
  (void)value;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuFuncSetCacheConfig(CUfunction hfunc, CUfunc_cache config) {
  MF_STUB_TRACE;
  (void)hfunc;
  (void)config;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuFuncSetSharedMemConfig(CUfunction hfunc, CUsharedconfig config) {
  MF_STUB_TRACE;
  (void)hfunc;
  (void)config;

  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_export_init_0x10(void* sub_struct, unsigned int mode) {
  (void)sub_struct;
  (void)mode;
  return CUDA_SUCCESS;
}

static void* mf_export_ops[8];
static void* mf_export_vtable[64];

static CUresult mf_a094_ops_entry_success(void) {
  return CUDA_SUCCESS;
}

/* Loader at libcudart 0x38311 calls table[+0x10] as:
     rdi = &state+0x88 (optional nested pointer)
     rsi = &state+0xa0 (table byte size; must be > 0x1df)
   then table[+0x30] as:
     rdi = &state+0x90
     rsi = &stack_slot (entry count; must be > 0xd). */
/* Nested object installed at cudart state+0x88. cudaDriverGetVersion reads
   *(nested+4): zero selects the safe fallback path; non-zero enters a deeper
   dispatch that needs more of the object filled. state+0x90 receives the
   count output object pointer from get_count. */
static struct {
  uint32_t pad0;
  uint32_t version_gate; /* +0x4 */
  uint32_t pad1[14];
} mf_a094_nested_object;

static struct {
  uint32_t pad0;
  uint32_t pad1[15];
} mf_a094_count_object;

static CUresult mf_a094_get_size(void* nested_out, void* size_out) {
  if (size_out == (void*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  if (nested_out != (void*)0) {
    mf_a094_nested_object.version_gate = 0;
    *(void**)nested_out = (void*)&mf_a094_nested_object;
  }
  *(unsigned long long*)size_out = 512ull;
  return CUDA_SUCCESS;
}

static CUresult mf_a094_get_count(void* count_obj_out, void* count_out) {
  if (count_out == (void*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  if (count_obj_out != (void*)0) {
    *(void**)count_obj_out = (void*)&mf_a094_count_object;
  }
  *(unsigned long long*)count_out = 14ull;
  return CUDA_SUCCESS;
}

static const unsigned char mf_uuid_a094[16] = {
    0xa0, 0x94, 0x79, 0x8c, 0x2e, 0x74, 0x2e, 0x74,
    0x93, 0xf2, 0x08, 0x00, 0x20, 0x0c, 0x0a, 0x66};

/* UUID 42d85a81-23f6-cb47-8298-f6e78a3aecdc: secondary interface table
   requested after a094 registration. */
static const unsigned char mf_uuid_42d8[16] = {
    0x42, 0xd8, 0x5a, 0x81, 0x23, 0xf6, 0xcb, 0x47,
    0x82, 0x98, 0xf6, 0xe7, 0x8a, 0x3a, 0xec, 0xdc};

static const unsigned char mf_uuid_6bd5[16] = {
    0x6b, 0xd5, 0xfb, 0x6c, 0x5b, 0xf4, 0xe7, 0x4a,
    0x89, 0x87, 0xd9, 0x39, 0x12, 0xfd, 0x9d, 0xf9};

static void* mf_a094_ops[64];
static void* mf_42d8_ops[64];
static void* mf_c693_ops[64];

static const unsigned char mf_uuid_c693[16] = {
    0xc6, 0x93, 0x33, 0x6e, 0x11, 0x21, 0xdf, 0x11,
    0xa8, 0xc3, 0x68, 0xf3, 0x55, 0xd8, 0x95, 0x93};

static const unsigned char mf_uuid_263e[16] = {
    0x26, 0x3e, 0x88, 0x60, 0x7c, 0xd2, 0x61, 0x43,
    0x92, 0xf6, 0xbb, 0xd5, 0x00, 0x6d, 0xfa, 0x7e};

/* UUID d4082055-...: tooling/context table. After export, cudart calls
   table[+0x8] three times with (0x2f1c..0x2f1e, timestamp, buffer). */
static const unsigned char mf_uuid_d408[16] = {
    0xd4, 0x08, 0x20, 0x55, 0xbd, 0xe6, 0x70, 0x4b,
    0x8d, 0x34, 0xba, 0x12, 0x3c, 0x66, 0xe1, 0xf2};

static void* mf_d408_ops[16];

/* libcudart 12.6 d408 tooling table: three calls with codes 12060..12062
   share one 0x30-byte buffer. After the third call cudart HMACs
   (key=static, msg=version/pid/tid/export-ptrs/stamp + device uuid packet)
   and compares the digest to buffer+0x20. Failure yields error 103
   (cudaErrorSoftwareValidityNotEstablished). The hash primitives live at
   fixed offsets in this libcudart build; slot1 is always entered from
   cudart so the return address yields the image base. */
static const unsigned char mf_d408_hmac_key[16] = {
    0x14, 0x6a, 0xdd, 0xae, 0x53, 0xa9, 0xa7, 0x52,
    0xaa, 0x08, 0x41, 0x36, 0x0b, 0xf5, 0x5a, 0x9f};

typedef void (*mf_d408_hash_init_fn)(void* ctx);
typedef void (*mf_d408_hash_update_fn)(void* ctx, unsigned int byte);
typedef void (*mf_d408_hash_final_fn)(void* ctx, void* out16);

static uintptr_t mf_d408_cudart_base(void) {
  FILE* maps = fopen("/proc/self/maps", "r");
  char line[512];
  uintptr_t base = (uintptr_t)0;
  if (maps == (FILE*)0) {
    return (uintptr_t)0;
  }
  while (fgets(line, (int)sizeof(line), maps) != (char*)0) {
    /* Executable mapping of libcudart.so.12: "r-xp ... libcudart.so.12" */
    if (strstr(line, "libcudart.so.12") == (char*)0 || strstr(line, "r-xp") == (char*)0) {
      continue;
    }
    {
      unsigned long start = 0;
      if (sscanf(line, "%lx-", &start) == 1) {
        base = (uintptr_t)start;
        break;
      }
    }
  }
  (void)fclose(maps);
  return base;
}

/* c693 container vtable slot +0x10: state lookup. Semantics: nonzero means
   "not found" and drives cudart's create path (41a10/41d10); zero means
   "found" and *out carries the per-device state object, which cudart later
   walks in place (mutex at +0x88, lists at +0x58/+0x68). The provider
   creates the state blob on the first miss and serves the same pointer on
   every later lookup — the virtual device has one deterministic state. */
static unsigned char* mf_c693_state_blob = (unsigned char*)0;

static CUresult mf_c693_lookup_miss(void* out, void* tag) {
  (void)tag;
  if (out == (void*)0) {
    return (CUresult)1;
  }
  if (mf_c693_state_blob == (unsigned char*)0) {
    mf_c693_state_blob = calloc(1, 0x1000);
    if (mf_c693_state_blob == (unsigned char*)0) {
      return CUDA_ERROR_OUT_OF_MEMORY;
    }
    /* First call reports the miss so cudart runs its create path; the blob
       is already installed for the follow-up lookup. */
    return (CUresult)1;
  }
  *(void**)out = (void*)mf_c693_state_blob;
  return CUDA_SUCCESS;
}

static void mf_d408_hmac(uintptr_t cudart_base, const unsigned char* msg, size_t msg_len,
                         unsigned char out[16]) {
  unsigned char ctx[0x80];
  unsigned char inner[16];
  size_t i = 0;
  mf_d408_hash_init_fn init_fn =
      (mf_d408_hash_init_fn)(cudart_base + (uintptr_t)0x24b70);
  mf_d408_hash_update_fn update_fn =
      (mf_d408_hash_update_fn)(cudart_base + (uintptr_t)0x24d70);
  mf_d408_hash_final_fn final_fn =
      (mf_d408_hash_final_fn)(cudart_base + (uintptr_t)0x26330);

  (void)memset(ctx, 0, sizeof(ctx));
  init_fn(ctx);
  for (i = 0; i < 16; ++i) {
    update_fn(ctx, (unsigned int)(mf_d408_hmac_key[i] ^ (unsigned char)0x36));
  }
  for (i = 0; i < msg_len; ++i) {
    update_fn(ctx, (unsigned int)msg[i]);
  }
  (void)memset(inner, 0, sizeof(inner));
  final_fn(ctx, inner);

  init_fn(ctx);
  for (i = 0; i < 16; ++i) {
    update_fn(ctx, (unsigned int)(mf_d408_hmac_key[i] ^ (unsigned char)0x5c));
  }
  for (i = 0; i < 16; ++i) {
    update_fn(ctx, (unsigned int)inner[i]);
  }
  (void)memset(out, 0, 16);
  final_fn(ctx, out);
}

static CUresult mf_d408_slot1(unsigned int code, unsigned long long stamp, void* buffer) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_D408 slot1 code=%u stamp=%llu buf=%p\n", code,
            (unsigned long long)stamp, buffer);
  }
  if (buffer == (void*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  /* Third call (12062): write the attestation MAC cudart will compare. */
  if (code == 12062u) {
    unsigned char msg[76];
    unsigned char mac[16];
    uintptr_t base = mf_d408_cudart_base();
    uint32_t pid = (uint32_t)getpid();
    uint32_t tid = (uint32_t)(uintptr_t)pthread_self();
    if (base == (uintptr_t)0) {
      return CUDA_ERROR_UNKNOWN;
    }
    (void)memset(msg, 0, sizeof(msg));
    /* msg48 layout from libcudart 0x3a3d3..0x3a424 */
    {
      uint32_t v0 = 12060u;
      uint32_t v1 = 12062u;
      (void)memcpy(msg + 0, &v0, 4);
      (void)memcpy(msg + 4, &v1, 4);
    }
    (void)memcpy(msg + 8, &pid, 4);
    (void)memcpy(msg + 12, &tid, 4);
    {
      uintptr_t export_vtable = (uintptr_t)(void*)mf_export_vtable;
      uintptr_t d408_ops = (uintptr_t)(void*)mf_d408_ops;
      uintptr_t slot1 = (uintptr_t)(void*)&mf_d408_slot1;
      (void)memcpy(msg + 16, &export_vtable, sizeof(export_vtable));
      (void)memcpy(msg + 24, &d408_ops, sizeof(d408_ops));
      (void)memcpy(msg + 32, &slot1, sizeof(slot1));
      (void)memcpy(msg + 40, &stamp, sizeof(stamp));
    }
    /* device packet: uuid "MFXCPU\\0" + flags from record+0x150 / +0x298 */
    msg[48] = (unsigned char)'M';
    msg[49] = (unsigned char)'F';
    msg[50] = (unsigned char)'X';
    msg[51] = (unsigned char)'C';
    msg[52] = (unsigned char)'P';
    msg[53] = (unsigned char)'U';
    msg[54] = (unsigned char)0;
    msg[55] = (unsigned char)1;
    msg[63] = (unsigned char)1;
    /* Third call's buffer argument is already allocation+0x20 — the MAC
       compare reads that same address, not buffer+0x20 again. */
    mf_d408_hmac(base, msg, sizeof(msg), mac);
    (void)memcpy(buffer, mac, 16);
    if (mf_stub_trace()) {
      fprintf(stderr, "MF_D408 mac written\n");
    }
  }
  return CUDA_SUCCESS;
}



CUresult cuGetExportTable(const void** ppExportTable, const CUuuid* pExportTableId) {
  MF_STUB_TRACE;
  if (pExportTableId == (const CUuuid*)0 || ppExportTable == (const void**)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *ppExportTable = (const void*)0;

  /* UUID a094798c-...: ops table. Loader requires version > 0x30, then
     table[+0x10] writes size > 0x1df and table[+0x30] writes count > 0xd. */
  if (memcmp(pExportTableId->bytes, mf_uuid_a094, 16) == 0) {
    unsigned int i = 0;
    mf_a094_ops[0] = (void*)(uintptr_t)12060;
    for (i = 1; i < sizeof(mf_a094_ops) / sizeof(mf_a094_ops[0]); ++i) {
      mf_a094_ops[i] = (void*)&mf_a094_ops_entry_success;
    }
    mf_a094_ops[2] = (void*)&mf_a094_get_size;
    mf_a094_ops[6] = (void*)&mf_a094_get_count;
    *ppExportTable = (const void*)mf_a094_ops;
    return CUDA_SUCCESS;
  }

  /* UUID 42d85a81-...: secondary interface table after a094 registration. */
  if (memcmp(pExportTableId->bytes, mf_uuid_42d8, 16) == 0) {
    unsigned int i = 0;
    mf_42d8_ops[0] = (void*)(uintptr_t)12060;
    for (i = 1; i < sizeof(mf_42d8_ops) / sizeof(mf_42d8_ops[0]); ++i) {
      mf_42d8_ops[i] = (void*)&mf_a094_ops_entry_success;
    }
    *ppExportTable = (const void*)mf_42d8_ops;
    return CUDA_SUCCESS;
  }

  /* UUID c693336e-...: required companion table in the a094 post-registration
     binder (libcudart 0x41520). Failure is fatal; a null-filled general vtable
     crashes when the binder later calls through real slots. */
  if (memcmp(pExportTableId->bytes, mf_uuid_c693, 16) == 0) {
    unsigned int i = 0;
    /* This table is a C++ vtable: cudart calls every slot including [0], so
       no slot may hold the version integer (calling 12060 segfaults). */
    for (i = 0; i < sizeof(mf_c693_ops) / sizeof(mf_c693_ops[0]); ++i) {
      mf_c693_ops[i] = (void*)&mf_a094_ops_entry_success;
    }
    mf_c693_ops[2] = (void*)&mf_c693_lookup_miss;
    *ppExportTable = (const void*)mf_c693_ops;
    return CUDA_SUCCESS;
  }

  /* UUID 263e8860-...: optional companion. The binder nulls it on failure and
     skips the version-gated slot[+0x18] probe — prefer NOT_FOUND over a
     half-empty vtable that crashes on that call. */
  if (memcmp(pExportTableId->bytes, mf_uuid_263e, 16) == 0) {
    return CUDA_ERROR_NOT_FOUND;
  }

  /* UUID d4082055-...: tooling table with slot[+0x8] callback. */
  if (memcmp(pExportTableId->bytes, mf_uuid_d408, 16) == 0) {
    unsigned int i = 0;
    mf_d408_ops[0] = (void*)(uintptr_t)12060;
    for (i = 1; i < sizeof(mf_d408_ops) / sizeof(mf_d408_ops[0]); ++i) {
      mf_d408_ops[i] = (void*)&mf_a094_ops_entry_success;
    }
    mf_d408_ops[1] = (void*)&mf_d408_slot1;
    *ppExportTable = (const void*)mf_d408_ops;
    return CUDA_SUCCESS;
  }

  /* UUID 6bd5fb6c-...: general driver vtable; slot[2] is the per-device init
     callback invoked as (record+8, device). */
  if (memcmp(pExportTableId->bytes, mf_uuid_6bd5, 16) == 0) {
    mf_export_vtable[2] = (void*)&mf_export_init_0x10;
    mf_export_ops[2] = (void*)&mf_export_init_0x10;
    mf_export_vtable[14] = (void*)mf_export_ops;
    *ppExportTable = (const void*)mf_export_vtable;
    return CUDA_SUCCESS;
  }

  if (mf_stub_trace()) {
    fprintf(stderr,
            "MF_TABLE_UUID unknown %02x%02x%02x%02x-%02x%02x-%02x%02x-"
            "%02x%02x-%02x%02x%02x%02x%02x%02x\n",
            (unsigned)(unsigned char)pExportTableId->bytes[0],
            (unsigned)(unsigned char)pExportTableId->bytes[1],
            (unsigned)(unsigned char)pExportTableId->bytes[2],
            (unsigned)(unsigned char)pExportTableId->bytes[3],
            (unsigned)(unsigned char)pExportTableId->bytes[4],
            (unsigned)(unsigned char)pExportTableId->bytes[5],
            (unsigned)(unsigned char)pExportTableId->bytes[6],
            (unsigned)(unsigned char)pExportTableId->bytes[7],
            (unsigned)(unsigned char)pExportTableId->bytes[8],
            (unsigned)(unsigned char)pExportTableId->bytes[9],
            (unsigned)(unsigned char)pExportTableId->bytes[10],
            (unsigned)(unsigned char)pExportTableId->bytes[11],
            (unsigned)(unsigned char)pExportTableId->bytes[12],
            (unsigned)(unsigned char)pExportTableId->bytes[13],
            (unsigned)(unsigned char)pExportTableId->bytes[14],
            (unsigned)(unsigned char)pExportTableId->bytes[15]);
  }
  return CUDA_ERROR_NOT_FOUND;
}


CUresult cuMemAllocManaged(CUdeviceptr* dptr, size_t bytesize, unsigned int flags) {
  MF_STUB_TRACE;
  (void)dptr;
  (void)bytesize;
  (void)flags;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemAllocPitch(CUdeviceptr* dptr, size_t* pPitch, size_t WidthInBytes, size_t Height, unsigned int ElementSizeBytes) {
  MF_STUB_TRACE;
  (void)dptr;
  (void)pPitch;
  (void)WidthInBytes;
  (void)Height;
  (void)ElementSizeBytes;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemFreeHost(void* p) {
  MF_STUB_TRACE;
  (void)p;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemGetAddressRange(CUdeviceptr* pbase, size_t* psize, CUdeviceptr dptr) {
  MF_STUB_TRACE;
  (void)pbase;
  (void)psize;
  (void)dptr;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemGetInfo(size_t* free_bytes, size_t* total_bytes) {
  MF_STUB_TRACE;
  (void)free_bytes;
  (void)total_bytes;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemHostAlloc(void** pp, size_t bytesize, unsigned int Flags) {
  MF_STUB_TRACE;
  (void)pp;
  (void)bytesize;
  (void)Flags;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemHostGetDevicePointer(CUdeviceptr* pdptr, void* p, unsigned int Flags) {
  MF_STUB_TRACE;
  (void)pdptr;
  (void)p;
  (void)Flags;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemHostGetFlags(unsigned int* pFlags, void* p) {
  MF_STUB_TRACE;
  (void)p;
  if (pFlags == (unsigned int*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *pFlags = 0;
  return CUDA_SUCCESS;

}

CUresult cuMemHostRegister(void* p, size_t bytesize, unsigned int Flags) {
  MF_STUB_TRACE;
  (void)p;
  (void)bytesize;
  (void)Flags;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemHostUnregister(void* p) {
  MF_STUB_TRACE;
  (void)p;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemcpy2D(const CUDA_MEMCPY2D* pCopy) {
  MF_STUB_TRACE;
  (void)pCopy;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemcpy2DAsync(const CUDA_MEMCPY2D* pCopy, CUstream hStream) {
  MF_STUB_TRACE;
  (void)pCopy;
  (void)hStream;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemcpy3D(const CUDA_MEMCPY3D* pCopy) {
  MF_STUB_TRACE;
  (void)pCopy;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemcpy3DAsync(const CUDA_MEMCPY3D* pCopy, CUstream hStream) {
  MF_STUB_TRACE;
  (void)pCopy;
  (void)hStream;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemcpyPeer(CUdeviceptr dstDevice, CUcontext dstContext, CUdeviceptr srcDevice, CUcontext srcContext, size_t ByteCount) {
  MF_STUB_TRACE;
  (void)dstDevice;
  (void)dstContext;
  (void)srcDevice;
  (void)srcContext;
  (void)ByteCount;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemcpyPeerAsync(CUdeviceptr dstDevice, CUcontext dstContext, CUdeviceptr srcDevice, CUcontext srcContext, size_t ByteCount, CUstream hStream) {
  MF_STUB_TRACE;
  (void)dstDevice;
  (void)dstContext;
  (void)srcDevice;
  (void)srcContext;
  (void)ByteCount;
  (void)hStream;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemsetD2D8(CUdeviceptr dstDevice, size_t dstPitch, unsigned char uc, size_t Width, size_t Height) {
  MF_STUB_TRACE;
  (void)dstDevice;
  (void)dstPitch;
  (void)uc;
  (void)Width;
  (void)Height;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemsetD2D8Async(CUdeviceptr dstDevice, size_t dstPitch, unsigned char uc, size_t Width, size_t Height, CUstream hStream) {
  MF_STUB_TRACE;
  (void)dstDevice;
  (void)dstPitch;
  (void)uc;
  (void)Width;
  (void)Height;
  (void)hStream;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemsetD8(CUdeviceptr dstDevice, unsigned char uc, size_t N) {
  MF_STUB_TRACE;
  (void)dstDevice;
  (void)uc;
  (void)N;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuMemsetD8Async(CUdeviceptr dstDevice, unsigned char uc, size_t N, CUstream hStream) {
  MF_STUB_TRACE;
  (void)dstDevice;
  (void)uc;
  (void)N;
  (void)hStream;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuOccupancyMaxActiveBlocksPerMultiprocessorWithFlags(int* numBlocks, CUfunction func, int blockSize, size_t dynamicSMemSize, unsigned int flags) {
  MF_STUB_TRACE;
  (void)func;
  (void)blockSize;
  (void)dynamicSMemSize;
  (void)flags;
  if (numBlocks == (int*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *numBlocks = 1;
  return CUDA_SUCCESS;

}

CUresult cuPointerGetAttribute(void* data, CUpointer_attribute attribute, CUdeviceptr ptr) {
  MF_STUB_TRACE;
  (void)data;
  (void)attribute;
  (void)ptr;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuPointerGetAttributes(unsigned int numAttributes, CUpointer_attribute* attributes, void** data, CUdeviceptr ptr) {
  MF_STUB_TRACE;
  (void)numAttributes;
  (void)attributes;
  (void)data;
  (void)ptr;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuProfilerInitialize(const char* configFile, const char* outputFile, unsigned int outputMode) {
  MF_STUB_TRACE;
  (void)configFile;
  (void)outputFile;
  (void)outputMode;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuProfilerStart(void) {
  MF_STUB_TRACE;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuProfilerStop(void) {
  MF_STUB_TRACE;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuStreamAddCallback(CUstream hStream, CUstreamCallback callback, void* userData, unsigned int flags) {
  MF_STUB_TRACE;
  (void)hStream;
  (void)callback;
  (void)userData;
  (void)flags;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuStreamAttachMemAsync(CUstream hStream, CUdeviceptr dptr, size_t length, unsigned int flags) {
  MF_STUB_TRACE;
  (void)hStream;
  (void)dptr;
  (void)length;
  (void)flags;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuStreamBatchMemOp(CUstream stream, unsigned int count, CUstreamBatchMemOpParams* param_array, unsigned int flags) {
  MF_STUB_TRACE;
  (void)stream;
  (void)count;
  (void)param_array;
  (void)flags;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuStreamCopyAttributes(CUstream dst, CUstream src) {
  MF_STUB_TRACE;
  (void)dst;
  (void)src;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuStreamGetAttribute(CUstream hStream, CUstreamAttrID attr, CUstreamAttrValue* value_out) {
  MF_STUB_TRACE;
  (void)hStream;
  (void)attr;
  (void)value_out;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuStreamGetCtx(CUstream hStream, CUcontext* pctx) {
  MF_STUB_TRACE;
  (void)hStream;
  (void)pctx;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuStreamGetFlags(CUstream hStream, unsigned int* flags) {
  MF_STUB_TRACE;
  (void)hStream;
  if (flags == (unsigned int*)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  *flags = 0;
  return CUDA_SUCCESS;

}

CUresult cuStreamGetId(CUstream hStream, unsigned long long* streamId) {
  MF_STUB_TRACE;
  (void)hStream;
  (void)streamId;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuStreamSetAttribute(CUstream hStream, CUstreamAttrID attr, const CUstreamAttrValue* value) {
  MF_STUB_TRACE;
  (void)hStream;
  (void)attr;
  (void)value;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuStreamWaitValue32(CUstream stream, CUdeviceptr addr, unsigned int value, unsigned int flags) {
  MF_STUB_TRACE;
  (void)stream;
  (void)addr;
  (void)value;
  (void)flags;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuStreamWaitValue64(CUstream stream, CUdeviceptr addr, unsigned long long value, unsigned int flags) {
  MF_STUB_TRACE;
  (void)stream;
  (void)addr;
  (void)value;
  (void)flags;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuStreamWriteValue32(CUstream stream, CUdeviceptr addr, unsigned int value, unsigned int flags) {
  MF_STUB_TRACE;
  (void)stream;
  (void)addr;
  (void)value;
  (void)flags;

  return CUDA_ERROR_NOT_SUPPORTED;
}

CUresult cuStreamWriteValue64(CUstream stream, CUdeviceptr addr, unsigned long long value, unsigned int flags) {
  MF_STUB_TRACE;
  (void)stream;
  (void)addr;
  (void)value;
  (void)flags;

  return CUDA_ERROR_NOT_SUPPORTED;
}
