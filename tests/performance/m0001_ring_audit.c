#define _GNU_SOURCE

#include "m0001_ring_audit.h"

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

static atomic_uint mf_ring_audit_active;
static atomic_uint_fast64_t mf_ring_audit_allocations;
static atomic_uint_fast64_t mf_ring_audit_locks;

static int mf_ring_audit_marker(const char* marker, size_t size) {
  ssize_t written = -1;
  do {
    written = write(STDERR_FILENO, marker, size);
  } while (written < 0 && errno == EINTR);
  return written == (ssize_t)size ? 0 : -1;
}

static void mf_ring_audit_note_allocation(void) {
  if (atomic_load_explicit(&mf_ring_audit_active, memory_order_relaxed) != 0U) {
    (void)atomic_fetch_add_explicit(&mf_ring_audit_allocations, UINT64_C(1), memory_order_relaxed);
  }
}

static void mf_ring_audit_note_lock(int result) {
  if (result == 0 && atomic_load_explicit(&mf_ring_audit_active, memory_order_relaxed) != 0U) {
    (void)atomic_fetch_add_explicit(&mf_ring_audit_locks, UINT64_C(1), memory_order_relaxed);
  }
}

int mf_ring_audit_begin(void) {
  static const char marker[] = "METAFLUX_RING_AUDIT_BEGIN_V1\n";
  atomic_store_explicit(&mf_ring_audit_allocations, UINT64_C(0), memory_order_relaxed);
  atomic_store_explicit(&mf_ring_audit_locks, UINT64_C(0), memory_order_relaxed);
  if (mf_ring_audit_marker(marker, sizeof(marker) - 1U) != 0) {
    return -1;
  }
  atomic_store_explicit(&mf_ring_audit_active, 1U, memory_order_release);
  return 0;
}

int mf_ring_audit_end(void) {
  static const char marker[] = "METAFLUX_RING_AUDIT_END_V1\n";
  atomic_store_explicit(&mf_ring_audit_active, 0U, memory_order_release);
  return mf_ring_audit_marker(marker, sizeof(marker) - 1U);
}

uint64_t mf_ring_audit_heap_allocation_attempts(void) {
  return (uint64_t)atomic_load_explicit(&mf_ring_audit_allocations, memory_order_relaxed);
}

uint64_t mf_ring_audit_global_lock_acquisitions(void) {
  return (uint64_t)atomic_load_explicit(&mf_ring_audit_locks, memory_order_relaxed);
}

void* __real_malloc(size_t size);
void* __real_calloc(size_t count, size_t size);
void* __real_realloc(void* pointer, size_t size);
void* __real_aligned_alloc(size_t alignment, size_t size);
int __real_posix_memalign(void** pointer, size_t alignment, size_t size);

void* __wrap_malloc(size_t size) {
  mf_ring_audit_note_allocation();
  return __real_malloc(size);
}

void* __wrap_calloc(size_t count, size_t size) {
  mf_ring_audit_note_allocation();
  return __real_calloc(count, size);
}

void* __wrap_realloc(void* pointer, size_t size) {
  mf_ring_audit_note_allocation();
  return __real_realloc(pointer, size);
}

void* __wrap_aligned_alloc(size_t alignment, size_t size) {
  mf_ring_audit_note_allocation();
  return __real_aligned_alloc(alignment, size);
}

int __wrap_posix_memalign(void** pointer, size_t alignment, size_t size) {
  mf_ring_audit_note_allocation();
  return __real_posix_memalign(pointer, alignment, size);
}

int __real_pthread_mutex_lock(pthread_mutex_t* mutex);
int __real_pthread_mutex_trylock(pthread_mutex_t* mutex);
int __real_pthread_rwlock_rdlock(pthread_rwlock_t* lock);
int __real_pthread_rwlock_tryrdlock(pthread_rwlock_t* lock);
int __real_pthread_rwlock_wrlock(pthread_rwlock_t* lock);
int __real_pthread_rwlock_trywrlock(pthread_rwlock_t* lock);
int __real_pthread_spin_lock(pthread_spinlock_t* lock);
int __real_pthread_spin_trylock(pthread_spinlock_t* lock);

int __wrap_pthread_mutex_lock(pthread_mutex_t* mutex) {
  const int result = __real_pthread_mutex_lock(mutex);
  mf_ring_audit_note_lock(result);
  return result;
}

int __wrap_pthread_mutex_trylock(pthread_mutex_t* mutex) {
  const int result = __real_pthread_mutex_trylock(mutex);
  mf_ring_audit_note_lock(result);
  return result;
}

int __wrap_pthread_rwlock_rdlock(pthread_rwlock_t* lock) {
  const int result = __real_pthread_rwlock_rdlock(lock);
  mf_ring_audit_note_lock(result);
  return result;
}

int __wrap_pthread_rwlock_tryrdlock(pthread_rwlock_t* lock) {
  const int result = __real_pthread_rwlock_tryrdlock(lock);
  mf_ring_audit_note_lock(result);
  return result;
}

int __wrap_pthread_rwlock_wrlock(pthread_rwlock_t* lock) {
  const int result = __real_pthread_rwlock_wrlock(lock);
  mf_ring_audit_note_lock(result);
  return result;
}

int __wrap_pthread_rwlock_trywrlock(pthread_rwlock_t* lock) {
  const int result = __real_pthread_rwlock_trywrlock(lock);
  mf_ring_audit_note_lock(result);
  return result;
}

int __wrap_pthread_spin_lock(pthread_spinlock_t* lock) {
  const int result = __real_pthread_spin_lock(lock);
  mf_ring_audit_note_lock(result);
  return result;
}

int __wrap_pthread_spin_trylock(pthread_spinlock_t* lock) {
  const int result = __real_pthread_spin_trylock(lock);
  mf_ring_audit_note_lock(result);
  return result;
}
