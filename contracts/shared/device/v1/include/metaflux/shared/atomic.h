#ifndef METAFLUX_SHARED_ATOMIC_H
#define METAFLUX_SHARED_ATOMIC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__clang__) || defined(__GNUC__)
#define MF_SHARED_ALIGNED(bytes) __attribute__((aligned(bytes)))
#else
#error "MetaFlux shared-memory atomics require Clang or GCC"
#endif

#if defined(__cplusplus)
#define MF_SHARED_STATIC_ASSERT(condition, message) static_assert(condition, message)
#else
#define MF_SHARED_STATIC_ASSERT(condition, message) _Static_assert(condition, message)
#endif

MF_SHARED_STATIC_ASSERT(sizeof(uint32_t) == sizeof(unsigned int) && __GCC_ATOMIC_INT_LOCK_FREE == 2,
                        "MetaFlux requires lock-free 32-bit atomics");
MF_SHARED_STATIC_ASSERT(
    (sizeof(uint64_t) == sizeof(unsigned long) && __GCC_ATOMIC_LONG_LOCK_FREE == 2) ||
        (sizeof(uint64_t) == sizeof(unsigned long long) && __GCC_ATOMIC_LLONG_LOCK_FREE == 2),
    "MetaFlux requires lock-free 64-bit atomics");

static inline uint32_t mf_atomic_load_u32_relaxed(const uint32_t* value) {
  return __atomic_load_n(value, __ATOMIC_RELAXED);
}

static inline uint32_t mf_atomic_load_u32_acquire(const uint32_t* value) {
  return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

static inline uint32_t mf_atomic_load_u32_seq_cst(const uint32_t* value) {
  return __atomic_load_n(value, __ATOMIC_SEQ_CST);
}

static inline void mf_atomic_store_u32_relaxed(uint32_t* value, uint32_t desired) {
  __atomic_store_n(value, desired, __ATOMIC_RELAXED);
}

static inline void mf_atomic_store_u32_release(uint32_t* value, uint32_t desired) {
  __atomic_store_n(value, desired, __ATOMIC_RELEASE);
}

static inline void mf_atomic_store_u32_seq_cst(uint32_t* value, uint32_t desired) {
  __atomic_store_n(value, desired, __ATOMIC_SEQ_CST);
}

static inline uint32_t mf_atomic_exchange_u32_acq_rel(uint32_t* value, uint32_t desired) {
  return __atomic_exchange_n(value, desired, __ATOMIC_ACQ_REL);
}

static inline uint32_t mf_atomic_fetch_add_u32_acq_rel(uint32_t* value, uint32_t operand) {
  return __atomic_fetch_add(value, operand, __ATOMIC_ACQ_REL);
}

static inline uint32_t mf_atomic_fetch_sub_u32_acq_rel(uint32_t* value, uint32_t operand) {
  return __atomic_fetch_sub(value, operand, __ATOMIC_ACQ_REL);
}

static inline int mf_atomic_compare_exchange_u32_seq_cst(uint32_t* value, uint32_t* expected,
                                                         uint32_t desired) {
  return __atomic_compare_exchange_n(value, expected, desired, 0, __ATOMIC_SEQ_CST,
                                     __ATOMIC_SEQ_CST);
}

static inline uint64_t mf_atomic_load_u64_relaxed(const uint64_t* value) {
  return __atomic_load_n(value, __ATOMIC_RELAXED);
}

static inline uint64_t mf_atomic_load_u64_acquire(const uint64_t* value) {
  return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

static inline uint64_t mf_atomic_load_u64_seq_cst(const uint64_t* value) {
  return __atomic_load_n(value, __ATOMIC_SEQ_CST);
}

static inline void mf_atomic_store_u64_relaxed(uint64_t* value, uint64_t desired) {
  __atomic_store_n(value, desired, __ATOMIC_RELAXED);
}

static inline void mf_atomic_store_u64_release(uint64_t* value, uint64_t desired) {
  __atomic_store_n(value, desired, __ATOMIC_RELEASE);
}

static inline void mf_atomic_store_u64_seq_cst(uint64_t* value, uint64_t desired) {
  __atomic_store_n(value, desired, __ATOMIC_SEQ_CST);
}

static inline uint64_t mf_atomic_exchange_u64_acq_rel(uint64_t* value, uint64_t desired) {
  return __atomic_exchange_n(value, desired, __ATOMIC_ACQ_REL);
}

static inline uint64_t mf_atomic_fetch_add_u64_acq_rel(uint64_t* value, uint64_t operand) {
  return __atomic_fetch_add(value, operand, __ATOMIC_ACQ_REL);
}

static inline int mf_atomic_compare_exchange_u64_seq_cst(uint64_t* value, uint64_t* expected,
                                                         uint64_t desired) {
  return __atomic_compare_exchange_n(value, expected, desired, 0, __ATOMIC_SEQ_CST,
                                     __ATOMIC_SEQ_CST);
}

static inline int mf_atomic_compare_exchange_u64_weak_relaxed(uint64_t* value, uint64_t* expected,
                                                              uint64_t desired) {
  return __atomic_compare_exchange_n(value, expected, desired, 1, __ATOMIC_RELAXED,
                                     __ATOMIC_RELAXED);
}

static inline void mf_atomic_thread_fence_acquire(void) { __atomic_thread_fence(__ATOMIC_ACQUIRE); }

static inline void mf_atomic_thread_fence_release(void) { __atomic_thread_fence(__ATOMIC_RELEASE); }

static inline void mf_atomic_thread_fence_seq_cst(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }

static inline void mf_atomic_signal_fence_seq_cst(void) { __atomic_signal_fence(__ATOMIC_SEQ_CST); }

#ifdef __cplusplus
}
#endif

#endif
