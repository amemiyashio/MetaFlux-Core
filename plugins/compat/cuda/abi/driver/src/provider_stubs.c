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

static CUresult mf_a094_get_size(void* in, void* out) {
  (void)in;
  *(unsigned long long*)out = 512;
  return CUDA_SUCCESS;
}

static CUresult mf_a094_get_count(void* in, void* out) {
  (void)in;
  *(unsigned long long*)out = 14;
  return CUDA_SUCCESS;
}

static const unsigned char mf_uuid_a094[16] = {
    0xa0, 0x94, 0x79, 0x8c, 0x2e, 0x74, 0x2e, 0x74,
    0x93, 0xf2, 0x08, 0x00, 0x20, 0x0c, 0x0a, 0x66};

static void* mf_a094_ops[64];

static CUresult mf_a094_e0(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 0\n", 0);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e1(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 1\n", 1);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e2(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 2\n", 2);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e3(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 3\n", 3);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e4(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 4\n", 4);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e5(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 5\n", 5);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e6(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 6\n", 6);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e7(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 7\n", 7);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e8(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 8\n", 8);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e9(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 9\n", 9);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e10(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 10\n", 10);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e11(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 11\n", 11);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e12(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 12\n", 12);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e13(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 13\n", 13);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e14(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 14\n", 14);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e15(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 15\n", 15);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e16(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 16\n", 16);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e17(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 17\n", 17);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e18(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 18\n", 18);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e19(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 19\n", 19);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e20(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 20\n", 20);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e21(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 21\n", 21);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e22(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 22\n", 22);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e23(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 23\n", 23);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e24(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 24\n", 24);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e25(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 25\n", 25);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e26(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 26\n", 26);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e27(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 27\n", 27);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e28(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 28\n", 28);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e29(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 29\n", 29);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e30(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 30\n", 30);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e31(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 31\n", 31);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e32(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 32\n", 32);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e33(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 33\n", 33);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e34(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 34\n", 34);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e35(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 35\n", 35);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e36(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 36\n", 36);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e37(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 37\n", 37);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e38(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 38\n", 38);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e39(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 39\n", 39);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e40(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 40\n", 40);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e41(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 41\n", 41);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e42(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 42\n", 42);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e43(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 43\n", 43);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e44(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 44\n", 44);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e45(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 45\n", 45);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e46(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 46\n", 46);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e47(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 47\n", 47);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e48(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 48\n", 48);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e49(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 49\n", 49);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e50(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 50\n", 50);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e51(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 51\n", 51);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e52(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 52\n", 52);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e53(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 53\n", 53);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e54(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 54\n", 54);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e55(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 55\n", 55);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e56(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 56\n", 56);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e57(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 57\n", 57);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e58(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 58\n", 58);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e59(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 59\n", 59);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e60(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 60\n", 60);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e61(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 61\n", 61);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e62(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 62\n", 62);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static CUresult mf_a094_e63(void) {
  if (mf_stub_trace()) {
    fprintf(stderr, "MF_A094 slot 63\n", 63);
  }
  return CUDA_ERROR_NOT_SUPPORTED;
}

static void mf_a094_fill(void) {
  for (unsigned int i = 0; i < sizeof(mf_a094_ops) / sizeof(mf_a094_ops[0]); ++i) {
    const char* gather = "gather";
    (void)gather;
    switch (i) {    case 0: mf_a094_ops[i] = (void*)&mf_a094_e0; break;
    case 1: mf_a094_ops[i] = (void*)&mf_a094_e1; break;
    case 2: mf_a094_ops[i] = (void*)&mf_a094_e2; break;
    case 3: mf_a094_ops[i] = (void*)&mf_a094_e3; break;
    case 4: mf_a094_ops[i] = (void*)&mf_a094_e4; break;
    case 5: mf_a094_ops[i] = (void*)&mf_a094_e5; break;
    case 6: mf_a094_ops[i] = (void*)&mf_a094_e6; break;
    case 7: mf_a094_ops[i] = (void*)&mf_a094_e7; break;
    case 8: mf_a094_ops[i] = (void*)&mf_a094_e8; break;
    case 9: mf_a094_ops[i] = (void*)&mf_a094_e9; break;
    case 10: mf_a094_ops[i] = (void*)&mf_a094_e10; break;
    case 11: mf_a094_ops[i] = (void*)&mf_a094_e11; break;
    case 12: mf_a094_ops[i] = (void*)&mf_a094_e12; break;
    case 13: mf_a094_ops[i] = (void*)&mf_a094_e13; break;
    case 14: mf_a094_ops[i] = (void*)&mf_a094_e14; break;
    case 15: mf_a094_ops[i] = (void*)&mf_a094_e15; break;
    case 16: mf_a094_ops[i] = (void*)&mf_a094_e16; break;
    case 17: mf_a094_ops[i] = (void*)&mf_a094_e17; break;
    case 18: mf_a094_ops[i] = (void*)&mf_a094_e18; break;
    case 19: mf_a094_ops[i] = (void*)&mf_a094_e19; break;
    case 20: mf_a094_ops[i] = (void*)&mf_a094_e20; break;
    case 21: mf_a094_ops[i] = (void*)&mf_a094_e21; break;
    case 22: mf_a094_ops[i] = (void*)&mf_a094_e22; break;
    case 23: mf_a094_ops[i] = (void*)&mf_a094_e23; break;
    case 24: mf_a094_ops[i] = (void*)&mf_a094_e24; break;
    case 25: mf_a094_ops[i] = (void*)&mf_a094_e25; break;
    case 26: mf_a094_ops[i] = (void*)&mf_a094_e26; break;
    case 27: mf_a094_ops[i] = (void*)&mf_a094_e27; break;
    case 28: mf_a094_ops[i] = (void*)&mf_a094_e28; break;
    case 29: mf_a094_ops[i] = (void*)&mf_a094_e29; break;
    case 30: mf_a094_ops[i] = (void*)&mf_a094_e30; break;
    case 31: mf_a094_ops[i] = (void*)&mf_a094_e31; break;
    case 32: mf_a094_ops[i] = (void*)&mf_a094_e32; break;
    case 33: mf_a094_ops[i] = (void*)&mf_a094_e33; break;
    case 34: mf_a094_ops[i] = (void*)&mf_a094_e34; break;
    case 35: mf_a094_ops[i] = (void*)&mf_a094_e35; break;
    case 36: mf_a094_ops[i] = (void*)&mf_a094_e36; break;
    case 37: mf_a094_ops[i] = (void*)&mf_a094_e37; break;
    case 38: mf_a094_ops[i] = (void*)&mf_a094_e38; break;
    case 39: mf_a094_ops[i] = (void*)&mf_a094_e39; break;
    case 40: mf_a094_ops[i] = (void*)&mf_a094_e40; break;
    case 41: mf_a094_ops[i] = (void*)&mf_a094_e41; break;
    case 42: mf_a094_ops[i] = (void*)&mf_a094_e42; break;
    case 43: mf_a094_ops[i] = (void*)&mf_a094_e43; break;
    case 44: mf_a094_ops[i] = (void*)&mf_a094_e44; break;
    case 45: mf_a094_ops[i] = (void*)&mf_a094_e45; break;
    case 46: mf_a094_ops[i] = (void*)&mf_a094_e46; break;
    case 47: mf_a094_ops[i] = (void*)&mf_a094_e47; break;
    case 48: mf_a094_ops[i] = (void*)&mf_a094_e48; break;
    case 49: mf_a094_ops[i] = (void*)&mf_a094_e49; break;
    case 50: mf_a094_ops[i] = (void*)&mf_a094_e50; break;
    case 51: mf_a094_ops[i] = (void*)&mf_a094_e51; break;
    case 52: mf_a094_ops[i] = (void*)&mf_a094_e52; break;
    case 53: mf_a094_ops[i] = (void*)&mf_a094_e53; break;
    case 54: mf_a094_ops[i] = (void*)&mf_a094_e54; break;
    case 55: mf_a094_ops[i] = (void*)&mf_a094_e55; break;
    case 56: mf_a094_ops[i] = (void*)&mf_a094_e56; break;
    case 57: mf_a094_ops[i] = (void*)&mf_a094_e57; break;
    case 58: mf_a094_ops[i] = (void*)&mf_a094_e58; break;
    case 59: mf_a094_ops[i] = (void*)&mf_a094_e59; break;
    case 60: mf_a094_ops[i] = (void*)&mf_a094_e60; break;
    case 61: mf_a094_ops[i] = (void*)&mf_a094_e61; break;
    case 62: mf_a094_ops[i] = (void*)&mf_a094_e62; break;
    case 63: mf_a094_ops[i] = (void*)&mf_a094_e63; break;
    }
  }
}



CUresult cuGetExportTable(const void** ppExportTable, const CUuuid* pExportTableId) {
  MF_STUB_TRACE;
  mf_a094_fill();
  if (pExportTableId == (const CUuuid*)0 || ppExportTable == (const void**)0) {
    return CUDA_ERROR_INVALID_VALUE;
  }
  /* UUID a094798c-...: the driver ops table. cudart's loader requires
     table[0] (interface version) > 0x30, then invokes table[+0x10] to fetch
     the table size (must be > 0x1df = 479) and table[+0x30] for an entry
     count (must be > 13), before continuing initialization. */
  if (memcmp(pExportTableId->bytes, mf_uuid_a094, 16) == 0) {
    mf_a094_ops[0] = (void*)(uintptr_t)64;
    mf_a094_ops[2] = (void*)&mf_a094_get_size;
    mf_a094_ops[6] = (void*)&mf_a094_get_count;
    for (unsigned int i = 1; i < sizeof(mf_a094_ops) / sizeof(mf_a094_ops[0]); ++i) {
      mf_a094_ops[i] = (void*)&mf_a094_ops_entry_success;
    }
    *ppExportTable = (const void*)mf_a094_ops;
    return CUDA_SUCCESS;
  }
  /* UUID 6bd5fb6c-...: the general driver vtable; slot[2] (offset 0x10) is
     the initialization entry cudart exercises. */
  mf_export_vtable[2] = (void*)&mf_export_init_0x10;
  mf_export_ops[2] = (void*)&mf_export_init_0x10;
  mf_export_vtable[14] = (void*)mf_export_ops;
  *ppExportTable = (const void*)mf_export_vtable;
  return CUDA_SUCCESS;
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
