#ifndef METAFLUX_TESTS_PERFORMANCE_BENCHMARK_COMMON_H
#define METAFLUX_TESTS_PERFORMANCE_BENCHMARK_COMMON_H

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static inline int mf_benchmark_parse_u32(const char* text, uint32_t* out_value) {
  char* end = (char*)0;
  unsigned long value = 0;
  if (text == (const char*)0 || out_value == (uint32_t*)0 || text[0] == '\0') {
    return -1;
  }
  errno = 0;
  value = strtoul(text, &end, 10);
  if (errno != 0 || end == text || *end != '\0' || value == 0 || value > UINT32_MAX) {
    return -1;
  }
  *out_value = (uint32_t)value;
  return 0;
}

static inline int mf_benchmark_parse_u64(const char* text, uint64_t* out_value) {
  char* end = (char*)0;
  unsigned long long value = 0;
  if (text == (const char*)0 || out_value == (uint64_t*)0 || text[0] == '\0') {
    return -1;
  }
  errno = 0;
  value = strtoull(text, &end, 10);
  if (errno != 0 || end == text || *end != '\0' || value == 0) {
    return -1;
  }
  *out_value = (uint64_t)value;
  return 0;
}

static inline int mf_benchmark_now_ns(uint64_t* out_value) {
  struct timespec timestamp;
  if (out_value == (uint64_t*)0 || clock_gettime(CLOCK_MONOTONIC_RAW, &timestamp) != 0 ||
      timestamp.tv_sec < 0 || timestamp.tv_nsec < 0) {
    return -1;
  }
  *out_value = (uint64_t)timestamp.tv_sec * UINT64_C(1000000000) + (uint64_t)timestamp.tv_nsec;
  return 0;
}

static inline int mf_benchmark_clock_resolution_ns(uint64_t* out_value) {
  struct timespec resolution;
  if (out_value == (uint64_t*)0 || clock_getres(CLOCK_MONOTONIC_RAW, &resolution) != 0 ||
      resolution.tv_sec < 0 || resolution.tv_nsec < 0) {
    return -1;
  }
  *out_value = (uint64_t)resolution.tv_sec * UINT64_C(1000000000) + (uint64_t)resolution.tv_nsec;
  return 0;
}

static inline void mf_benchmark_emit_sample(const char* metric, uint32_t sample_index,
                                            uint64_t value, const char* unit) {
  (void)printf("METAFLUX_SAMPLE\t%s\t%" PRIu32 "\t%" PRIu64 "\t%s\n", metric, sample_index, value,
               unit);
}

static inline void mf_benchmark_emit_metadata_u64(const char* key, uint64_t value) {
  (void)printf("METAFLUX_METADATA\t%s\t%" PRIu64 "\n", key, value);
}

static inline void mf_benchmark_emit_metadata_text(const char* key, const char* value) {
  (void)printf("METAFLUX_METADATA\t%s\t%s\n", key, value);
}

#endif
