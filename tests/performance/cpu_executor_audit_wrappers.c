// Allocation interposition for the executor steady-state audit.
//
// Unlike the client ring audit (pure C), the executor path allocates through
// C++ operator new, whose calls land inside the shared C++ runtime and are
// invisible to --wrap=malloc. This wrapper set interposes BOTH the malloc
// family and the operator new/delete symbols referenced from the statically
// linked program code, so the audit counts every allocation attempt the
// executor launch path can issue.

#include <stddef.h>
#include <stdint.h>

static volatile int mf_exec_audit_armed = 0;
static volatile uint64_t mf_exec_audit_allocations = 0;

int mf_exec_audit_begin(void) {
  mf_exec_audit_allocations = 0;
  mf_exec_audit_armed = 1;
  return 0;
}

int mf_exec_audit_end(void) {
  mf_exec_audit_armed = 0;
  return 0;
}

uint64_t mf_exec_audit_heap_allocation_attempts(void) {
  return mf_exec_audit_allocations;
}

static void count(void) {
  if (mf_exec_audit_armed) {
    ++mf_exec_audit_allocations;
  }
}

void *__wrap_malloc(size_t size) {
  extern void *__real_malloc(size_t);
  count();
  return __real_malloc(size);
}

void *__wrap_calloc(size_t members, size_t size) {
  extern void *__real_calloc(size_t, size_t);
  count();
  return __real_calloc(members, size);
}

void *__wrap_realloc(void *pointer, size_t size) {
  extern void *__real_realloc(void *, size_t);
  count();
  return __real_realloc(pointer, size);
}

void *__wrap_aligned_alloc(size_t alignment, size_t size) {
  extern void *__real_aligned_alloc(size_t, size_t);
  count();
  return __real_aligned_alloc(alignment, size);
}

int __wrap_posix_memalign(void **pointer, size_t alignment, size_t size) {
  extern int __real_posix_memalign(void **, size_t, size_t);
  count();
  return __real_posix_memalign(pointer, alignment, size);
}

// C++ operator new/delete (Itanium ABI) plus the aligned C++17 variants.
void *__wrap__Znwm(size_t size) {
  extern void *__real__Znwm(size_t);
  count();
  return __real__Znwm(size);
}

void *__wrap__Znam(size_t size) {
  extern void *__real__Znam(size_t);
  count();
  return __real__Znam(size);
}

void __wrap__ZdlPv(void *pointer) {
  extern void __real__ZdlPv(void *);
  __real__ZdlPv(pointer);
}

void __wrap__ZdaPv(void *pointer) {
  extern void __real__ZdaPv(void *);
  __real__ZdaPv(pointer);
}

void __wrap__ZdlPvm(void *pointer, size_t size) {
  extern void __real__ZdlPvm(void *, size_t);
  __real__ZdlPvm(pointer, size);
}

void __wrap__ZdaPvm(void *pointer, size_t size) {
  extern void __real__ZdaPvm(void *, size_t);
  __real__ZdaPvm(pointer, size);
}

void *__wrap__ZnwmSt11align_val_t(size_t size, size_t alignment) {
  extern void *__real__ZnwmSt11align_val_t(size_t, size_t);
  count();
  return __real__ZnwmSt11align_val_t(size, alignment);
}

void *__wrap__ZnamSt11align_val_t(size_t size, size_t alignment) {
  extern void *__real__ZnamSt11align_val_t(size_t, size_t);
  count();
  return __real__ZnamSt11align_val_t(size, alignment);
}
