#define _POSIX_C_SOURCE 200809L

#include "benchmark_common.h"
#include "m0001_ring_audit.h"
#include "metaflux/client/fastpath.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MF_BENCHMARK_RING_CAPACITY UINT32_C(64)

static int mf_ring_exchange(mf_client_ring_v1* ring, uint64_t request_id) {
  mf_ring_descriptor_v1 submitted;
  mf_ring_descriptor_v1 consumed;
  mf_shared_status_v1 status = MF_SHARED_SUCCESS;
  (void)memset(&submitted, 0, sizeof(submitted));
  (void)memset(&consumed, 0, sizeof(consumed));
  submitted.opcode = MF_RING_OPCODE_NOOP;
  submitted.request_id = request_id;
  status = mf_client_ring_try_submit_v1(ring, &submitted);
  if (status == MF_SHARED_SUCCESS) {
    status = mf_client_ring_try_consume_v1(ring, &consumed);
  }
  return status == MF_SHARED_SUCCESS && consumed.opcode == submitted.opcode &&
                 consumed.request_id == request_id
             ? 0
             : -1;
}

static int mf_ring_round_trip(mf_client_ring_v1* ring, uint64_t request_id,
                              uint64_t* out_elapsed_ns) {
  uint64_t start_ns = 0;
  uint64_t end_ns = 0;
  if (mf_benchmark_now_ns(&start_ns) != 0) {
    return -1;
  }
  if (mf_ring_exchange(ring, request_id) != 0 || mf_benchmark_now_ns(&end_ns) != 0 ||
      end_ns < start_ns) {
    return -1;
  }
  if (out_elapsed_ns != (uint64_t*)0) {
    *out_elapsed_ns = end_ns - start_ns;
  }
  return 0;
}

static int mf_clock_pair(uint64_t* out_elapsed_ns) {
  uint64_t start_ns = 0;
  uint64_t end_ns = 0;
  if (out_elapsed_ns == (uint64_t*)0 || mf_benchmark_now_ns(&start_ns) != 0 ||
      mf_benchmark_now_ns(&end_ns) != 0 || end_ns < start_ns) {
    return -1;
  }
  *out_elapsed_ns = end_ns - start_ns;
  return 0;
}

int main(int argc, char** argv) {
  const mf_registry_view_id_v1 view_id = {UINT64_C(41), UINT64_C(43)};
  mf_client_ring_v1 ring;
  uint64_t* samples = (uint64_t*)0;
  uint64_t* clock_samples = (uint64_t*)0;
  uint64_t clock_resolution_ns = 0;
  uint64_t consumer_doorbells_before = 0;
  uint64_t producer_doorbells_before = 0;
  uint32_t warmup_count = 0;
  uint32_t sample_count = 0;
  uint32_t index = 0;
  int audit_mode = 0;
  int result = 1;
  (void)memset(&ring, 0, sizeof(ring));
  ring.owned_fd = -1;
  audit_mode = argc == 4 && strcmp(argv[3], "--audit") == 0;
  if ((argc != 3 && !audit_mode) || mf_benchmark_parse_u32(argv[1], &warmup_count) != 0 ||
      mf_benchmark_parse_u32(argv[2], &sample_count) != 0) {
    (void)fprintf(stderr, "usage: %s WARMUP_COUNT SAMPLE_COUNT [--audit]\n", argv[0]);
    return 2;
  }
  samples = (uint64_t*)calloc((size_t)sample_count, sizeof(*samples));
  clock_samples = (uint64_t*)calloc((size_t)sample_count, sizeof(*clock_samples));
  if (samples == (uint64_t*)0 || clock_samples == (uint64_t*)0 ||
      mf_benchmark_clock_resolution_ns(&clock_resolution_ns) != 0 ||
      mf_client_ring_create_v1(MF_BENCHMARK_RING_CAPACITY, view_id, UINT64_C(47), UINT64_C(1),
                               &ring) != MF_SHARED_SUCCESS) {
    goto cleanup;
  }
  if (audit_mode) {
    uint64_t audit_consumer_doorbells = 0;
    uint64_t audit_producer_doorbells = 0;
    for (index = 0; index < warmup_count; ++index) {
      if (mf_ring_exchange(&ring, (uint64_t)index + UINT64_C(1)) != 0) {
        goto cleanup;
      }
    }
    consumer_doorbells_before = mf_client_ring_consumer_doorbells_v1(&ring);
    producer_doorbells_before = mf_client_ring_producer_doorbells_v1(&ring);
    if (mf_ring_audit_begin() != 0) {
      goto cleanup;
    }
    for (index = 0; index < sample_count; ++index) {
      if (mf_ring_exchange(&ring, (uint64_t)warmup_count + (uint64_t)index + UINT64_C(1)) != 0) {
        break;
      }
    }
    if (mf_ring_audit_end() != 0 || index != sample_count) {
      goto cleanup;
    }
    audit_consumer_doorbells =
        mf_client_ring_consumer_doorbells_v1(&ring) - consumer_doorbells_before;
    audit_producer_doorbells =
        mf_client_ring_producer_doorbells_v1(&ring) - producer_doorbells_before;
    mf_benchmark_emit_metadata_text("workload", "active_memfd_ring_audit");
    mf_benchmark_emit_metadata_u64("warmup_count", warmup_count);
    mf_benchmark_emit_metadata_u64("sample_count", sample_count);
    mf_benchmark_emit_metadata_u64("audit_dispatches", sample_count);
    mf_benchmark_emit_metadata_u64("audit_heap_allocation_attempts",
                                   mf_ring_audit_heap_allocation_attempts());
    mf_benchmark_emit_metadata_u64("audit_global_lock_acquisitions",
                                   mf_ring_audit_global_lock_acquisitions());
    mf_benchmark_emit_metadata_u64("audit_consumer_doorbells", audit_consumer_doorbells);
    mf_benchmark_emit_metadata_u64("audit_producer_doorbells", audit_producer_doorbells);
    result = 0;
    goto cleanup;
  }
  consumer_doorbells_before = mf_client_ring_consumer_doorbells_v1(&ring);
  producer_doorbells_before = mf_client_ring_producer_doorbells_v1(&ring);
  for (index = 0; index < warmup_count; ++index) {
    uint64_t ignored_clock_sample = 0;
    if (mf_clock_pair(&ignored_clock_sample) != 0 ||
        mf_ring_round_trip(&ring, (uint64_t)index + UINT64_C(1), (uint64_t*)0) != 0) {
      goto cleanup;
    }
  }
  for (index = 0; index < sample_count; ++index) {
    if (mf_clock_pair(&clock_samples[index]) != 0) {
      goto cleanup;
    }
  }
  for (index = 0; index < sample_count; ++index) {
    if (mf_ring_round_trip(&ring, (uint64_t)warmup_count + (uint64_t)index + UINT64_C(1),
                           &samples[index]) != 0) {
      goto cleanup;
    }
  }
  mf_benchmark_emit_metadata_text("workload", "active_memfd_ring_submit_consume");
  mf_benchmark_emit_metadata_u64("warmup_count", warmup_count);
  mf_benchmark_emit_metadata_u64("sample_count", sample_count);
  mf_benchmark_emit_metadata_u64("ring_capacity", MF_BENCHMARK_RING_CAPACITY);
  mf_benchmark_emit_metadata_u64("clock_monotonic_raw_resolution_ns", clock_resolution_ns);
  mf_benchmark_emit_metadata_u64("consumer_doorbells", mf_client_ring_consumer_doorbells_v1(&ring) -
                                                           consumer_doorbells_before);
  mf_benchmark_emit_metadata_u64("producer_doorbells", mf_client_ring_producer_doorbells_v1(&ring) -
                                                           producer_doorbells_before);
  for (index = 0; index < sample_count; ++index) {
    mf_benchmark_emit_sample("monotonic_raw_pair_overhead_ns", index, clock_samples[index], "ns");
    mf_benchmark_emit_sample("memfd_ring_submit_consume_ns", index, samples[index], "ns");
  }
  result = 0;

cleanup:
  mf_client_ring_close_v1(&ring);
  free(clock_samples);
  free(samples);
  return result;
}
