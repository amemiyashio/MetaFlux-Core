#include "metaflux/client/fastpath.h"

#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define TEST_OPERATION_COUNT UINT64_C(1000000)
#define TEST_WAIT_NS UINT64_C(100000000)
#define TEST_MPMC_PRODUCERS UINT32_C(4)
#define TEST_MPMC_CONSUMERS UINT32_C(4)
#define TEST_MPMC_OPERATIONS_PER_PRODUCER UINT32_C(50000)
#define TEST_MPMC_OPERATION_COUNT (TEST_MPMC_PRODUCERS * TEST_MPMC_OPERATIONS_PER_PRODUCER)

_Static_assert(sizeof(mf_client_telemetry_snapshot_v1) == 80, "client telemetry snapshot size");
_Static_assert(offsetof(mf_client_telemetry_snapshot_v1, memory_active_time_ns) == 40,
               "client telemetry memory activity offset");

typedef struct argument_block_fixture {
  mf_argument_block_header_v1 header;
  mf_argument_entry_v1 entries[4];
} argument_block_fixture;

typedef struct mpmc_fixture {
  mf_client_ring_v1* ring;
  atomic_uint consumed;
  atomic_uint error;
  atomic_uint seen[TEST_MPMC_OPERATION_COUNT];
} mpmc_fixture;

typedef struct mpmc_producer {
  mpmc_fixture* fixture;
  uint32_t index;
} mpmc_producer;

static uint64_t mpmc_checksum(uint64_t request_id) {
  return (request_id * UINT64_C(0x9e3779b185ebca87)) ^ UINT64_C(0xa5a5a5a55a5a5a5a);
}

static void* run_mpmc_producer(void* opaque) {
  mpmc_producer* producer = (mpmc_producer*)opaque;
  uint32_t offset = 0;
  for (offset = 0; offset < TEST_MPMC_OPERATIONS_PER_PRODUCER; ++offset) {
    const uint32_t logical_index = producer->index * TEST_MPMC_OPERATIONS_PER_PRODUCER + offset;
    const uint64_t request_id = (uint64_t)logical_index + UINT64_C(1);
    mf_ring_descriptor_v1 descriptor = {0};
    mf_shared_status_v1 status = MF_SHARED_WOULD_BLOCK;
    descriptor.opcode = MF_RING_OPCODE_NOOP;
    descriptor.request_id = request_id;
    descriptor.target_id = (uint64_t)producer->index + UINT64_C(1);
    descriptor.arguments[0] = mpmc_checksum(request_id);
    while ((status = mf_client_ring_try_submit_v1(producer->fixture->ring, &descriptor)) ==
           MF_SHARED_WOULD_BLOCK) {
      if (atomic_load_explicit(&producer->fixture->error, memory_order_acquire) != UINT32_C(0)) {
        return (void*)0;
      }
      (void)sched_yield();
    }
    if (status != MF_SHARED_SUCCESS) {
      atomic_store_explicit(&producer->fixture->error, UINT32_C(1), memory_order_release);
      return (void*)0;
    }
  }
  return (void*)0;
}

static void* run_mpmc_consumer(void* opaque) {
  mpmc_fixture* fixture = (mpmc_fixture*)opaque;
  for (;;) {
    mf_ring_descriptor_v1 descriptor = {0};
    const mf_shared_status_v1 status = mf_client_ring_try_consume_v1(fixture->ring, &descriptor);
    if (status == MF_SHARED_WOULD_BLOCK) {
      if (atomic_load_explicit(&fixture->consumed, memory_order_acquire) >=
              TEST_MPMC_OPERATION_COUNT ||
          atomic_load_explicit(&fixture->error, memory_order_acquire) != UINT32_C(0)) {
        return (void*)0;
      }
      (void)sched_yield();
      continue;
    }
    if (status != MF_SHARED_SUCCESS || descriptor.opcode != MF_RING_OPCODE_NOOP ||
        descriptor.request_id == UINT64_C(0) ||
        descriptor.request_id > (uint64_t)TEST_MPMC_OPERATION_COUNT ||
        descriptor.target_id == UINT64_C(0) ||
        descriptor.target_id > (uint64_t)TEST_MPMC_PRODUCERS ||
        descriptor.arguments[0] != mpmc_checksum(descriptor.request_id) ||
        atomic_exchange_explicit(&fixture->seen[descriptor.request_id - UINT64_C(1)], UINT32_C(1),
                                 memory_order_acq_rel) != UINT32_C(0)) {
      atomic_store_explicit(&fixture->error, UINT32_C(2), memory_order_release);
      return (void*)0;
    }
    (void)atomic_fetch_add_explicit(&fixture->consumed, UINT32_C(1), memory_order_acq_rel);
  }
}

static int run_mpmc_test(void) {
  const mf_registry_view_id_v1 view_id = {UINT64_C(0x61), UINT64_C(0x67)};
  mf_client_ring_v1 ring;
  mpmc_fixture fixture;
  mpmc_producer producers[TEST_MPMC_PRODUCERS];
  pthread_t producer_threads[TEST_MPMC_PRODUCERS];
  pthread_t consumer_threads[TEST_MPMC_CONSUMERS];
  uint32_t index = 0;
  (void)memset(&fixture, 0, sizeof(fixture));
  if (mf_client_ring_create_v1(UINT32_C(64), view_id, UINT64_C(71), UINT64_C(73), &ring) !=
      MF_SHARED_SUCCESS) {
    return 1;
  }
  fixture.ring = &ring;
  for (index = 0; index < TEST_MPMC_CONSUMERS; ++index) {
    if (pthread_create(&consumer_threads[index], (const pthread_attr_t*)0, run_mpmc_consumer,
                       &fixture) != 0) {
      atomic_store_explicit(&fixture.error, UINT32_C(3), memory_order_release);
      break;
    }
  }
  if (index != TEST_MPMC_CONSUMERS) {
    while (index != UINT32_C(0)) {
      --index;
      (void)pthread_join(consumer_threads[index], (void**)0);
    }
    mf_client_ring_close_v1(&ring);
    return 2;
  }
  for (index = 0; index < TEST_MPMC_PRODUCERS; ++index) {
    producers[index].fixture = &fixture;
    producers[index].index = index;
    if (pthread_create(&producer_threads[index], (const pthread_attr_t*)0, run_mpmc_producer,
                       &producers[index]) != 0) {
      atomic_store_explicit(&fixture.error, UINT32_C(4), memory_order_release);
      break;
    }
  }
  {
    const uint32_t producer_count = index;
    for (index = 0; index < producer_count; ++index) {
      (void)pthread_join(producer_threads[index], (void**)0);
    }
  }
  for (index = 0; index < TEST_MPMC_CONSUMERS; ++index) {
    (void)pthread_join(consumer_threads[index], (void**)0);
  }
  if (atomic_load_explicit(&fixture.error, memory_order_acquire) != UINT32_C(0) ||
      atomic_load_explicit(&fixture.consumed, memory_order_acquire) != TEST_MPMC_OPERATION_COUNT) {
    mf_client_ring_close_v1(&ring);
    return 3;
  }
  for (index = 0; index < TEST_MPMC_OPERATION_COUNT; ++index) {
    if (atomic_load_explicit(&fixture.seen[index], memory_order_acquire) != UINT32_C(1)) {
      mf_client_ring_close_v1(&ring);
      return 4;
    }
  }
  mf_client_ring_close_v1(&ring);
  return 0;
}

static int run_capacity_test(void) {
  const mf_registry_view_id_v1 view_id = {UINT64_C(0x79), UINT64_C(0x7f)};
  mf_client_ring_v1 ring;
  mf_ring_descriptor_v1 descriptor = {0};
  mf_ring_descriptor_v1 consumed = {0};
  uint32_t index = 0;
  if (mf_client_ring_create_v1(UINT32_C(4), view_id, UINT64_C(83), UINT64_C(89), &ring) !=
      MF_SHARED_SUCCESS) {
    return 1;
  }
  descriptor.opcode = MF_RING_OPCODE_NOOP;
  for (index = 0; index < UINT32_C(4); ++index) {
    descriptor.request_id = (uint64_t)index + UINT64_C(1);
    if (mf_client_ring_try_submit_v1(&ring, &descriptor) != MF_SHARED_SUCCESS) {
      mf_client_ring_close_v1(&ring);
      return 2;
    }
  }
  if (mf_client_ring_try_submit_v1(&ring, &descriptor) != MF_SHARED_WOULD_BLOCK ||
      mf_client_ring_try_consume_v1(&ring, &consumed) != MF_SHARED_SUCCESS ||
      mf_client_ring_try_submit_v1(&ring, &descriptor) != MF_SHARED_SUCCESS) {
    mf_client_ring_close_v1(&ring);
    return 3;
  }
  mf_client_ring_close_v1(&ring);
  return 0;
}

static int run_argument_block_test(void) {
  argument_block_fixture block;
  uint32_t index = 0;
  (void)memset(&block, 0, sizeof(block));
  block.header.magic = MF_SHARED_ARGUMENT_BLOCK_MAGIC;
  block.header.abi_version = MF_SHARED_DEVICE_ABI_VERSION_1;
  block.header.header_size = (uint32_t)sizeof(block.header);
  block.header.entry_size = (uint32_t)sizeof(block.entries[0]);
  block.header.entry_count = UINT32_C(4);
  block.header.total_size = (uint64_t)sizeof(block);
  for (index = 0; index < UINT32_C(4); ++index) {
    block.entries[index].kind = MF_ARGUMENT_KIND_U32;
    block.entries[index].value = index;
  }
  if (mf_client_argument_block_validate_v1((const uint8_t*)&block, sizeof(block)) !=
      MF_SHARED_SUCCESS) {
    return 1;
  }
  block.header.reserved[0] = UINT64_C(1);
  if (mf_client_argument_block_validate_v1((const uint8_t*)&block, sizeof(block)) !=
      MF_SHARED_MALFORMED) {
    return 2;
  }
  block.header.flags = MF_ARGUMENT_BLOCK_FLAG_LAUNCH_DIMENSIONS_XY_V1;
  for (index = 0; index < UINT32_C(4); ++index) {
    block.header.reserved[index] = UINT64_C(1);
  }
  if (mf_client_argument_block_validate_v1((const uint8_t*)&block, sizeof(block)) !=
      MF_SHARED_SUCCESS) {
    return 3;
  }
  block.header.reserved[MF_ARGUMENT_BLOCK_LAUNCH_GRID_X_INDEX_V1] = UINT64_C(0);
  if (mf_client_argument_block_validate_v1((const uint8_t*)&block, sizeof(block)) !=
      MF_SHARED_MALFORMED) {
    return 4;
  }
  block.header.reserved[MF_ARGUMENT_BLOCK_LAUNCH_GRID_X_INDEX_V1] =
      (uint64_t)UINT32_MAX + UINT64_C(1);
  if (mf_client_argument_block_validate_v1((const uint8_t*)&block, sizeof(block)) !=
      MF_SHARED_MALFORMED) {
    return 5;
  }
  block.header.reserved[MF_ARGUMENT_BLOCK_LAUNCH_GRID_X_INDEX_V1] = UINT64_C(1);
  block.header.flags = UINT32_C(2);
  if (mf_client_argument_block_validate_v1((const uint8_t*)&block, sizeof(block)) !=
      MF_SHARED_MALFORMED) {
    return 6;
  }
  block.header.flags = MF_ARGUMENT_BLOCK_FLAG_LAUNCH_DIMENSIONS_XY_V1;
  block.entries[0].kind = MF_ARGUMENT_KIND_BUFFER;
  block.entries[0].flags = MF_ARGUMENT_BUFFER_READ;
  block.entries[0].object_id = UINT64_C(1);
  block.entries[0].object_generation = UINT64_C(1);
  block.entries[0].value = UINT64_C(1);
  if (mf_client_argument_block_validate_v1((const uint8_t*)&block, sizeof(block)) !=
      MF_SHARED_MALFORMED) {
    return 7;
  }
  block.entries[0].value = sizeof(uint32_t);
  return mf_client_argument_block_validate_v1((const uint8_t*)&block, sizeof(block)) ==
                 MF_SHARED_SUCCESS
             ? 0
             : 8;
}

static int run_copy_region_argument_test(void) {
  argument_block_fixture block;
  uint64_t byte_count = UINT64_C(0);
  (void)memset(&block, 0, sizeof(block));
  if (mf_client_argument_block_size_v1(MF_COPY_REGION_ARGUMENT_ENTRY_COUNT_V1, &byte_count) !=
      MF_SHARED_SUCCESS) {
    return 1;
  }
  block.header.magic = MF_SHARED_ARGUMENT_BLOCK_MAGIC;
  block.header.abi_version = MF_SHARED_DEVICE_ABI_VERSION_1;
  block.header.header_size = (uint32_t)sizeof(block.header);
  block.header.entry_size = (uint32_t)sizeof(block.entries[0]);
  block.header.entry_count = MF_COPY_REGION_ARGUMENT_ENTRY_COUNT_V1;
  block.header.flags = MF_ARGUMENT_BLOCK_FLAG_COPY_REGION_V1;
  block.header.total_size = byte_count;
  block.entries[MF_COPY_REGION_DESTINATION_INDEX_V1].kind = MF_ARGUMENT_KIND_BUFFER;
  block.entries[MF_COPY_REGION_DESTINATION_INDEX_V1].flags = MF_ARGUMENT_BUFFER_WRITE;
  block.entries[MF_COPY_REGION_DESTINATION_INDEX_V1].object_id = UINT64_C(11);
  block.entries[MF_COPY_REGION_DESTINATION_INDEX_V1].object_generation = UINT64_C(13);
  block.entries[MF_COPY_REGION_DESTINATION_INDEX_V1].value = UINT64_C(1);
  block.entries[MF_COPY_REGION_SOURCE_INDEX_V1].kind = MF_ARGUMENT_KIND_BUFFER;
  block.entries[MF_COPY_REGION_SOURCE_INDEX_V1].flags = MF_ARGUMENT_BUFFER_READ;
  block.entries[MF_COPY_REGION_SOURCE_INDEX_V1].object_id = UINT64_C(17);
  block.entries[MF_COPY_REGION_SOURCE_INDEX_V1].object_generation = UINT64_C(19);
  block.entries[MF_COPY_REGION_SOURCE_INDEX_V1].value = UINT64_C(3);
  block.entries[MF_COPY_REGION_BYTE_COUNT_INDEX_V1].kind = MF_ARGUMENT_KIND_U64;
  block.entries[MF_COPY_REGION_BYTE_COUNT_INDEX_V1].value = UINT64_C(0x100000001);
  if (mf_client_copy_region_argument_block_validate_v1((const uint8_t*)&block, byte_count) !=
      MF_SHARED_SUCCESS) {
    return 2;
  }
  block.entries[MF_COPY_REGION_DESTINATION_INDEX_V1].flags |= MF_ARGUMENT_BUFFER_READ;
  if (mf_client_copy_region_argument_block_validate_v1((const uint8_t*)&block, byte_count) !=
      MF_SHARED_MALFORMED) {
    return 3;
  }
  block.entries[MF_COPY_REGION_DESTINATION_INDEX_V1].flags = MF_ARGUMENT_BUFFER_WRITE;
  block.entries[MF_COPY_REGION_BYTE_COUNT_INDEX_V1].value = UINT64_C(0);
  if (mf_client_copy_region_argument_block_validate_v1((const uint8_t*)&block, byte_count) !=
      MF_SHARED_MALFORMED) {
    return 4;
  }
  block.entries[MF_COPY_REGION_BYTE_COUNT_INDEX_V1].value = UINT64_C(7);
  block.header.reserved[0] = UINT64_C(1);
  return mf_client_copy_region_argument_block_validate_v1((const uint8_t*)&block, byte_count) ==
                 MF_SHARED_MALFORMED
             ? 0
             : 5;
}

static int run_copy_region_submission_test(void) {
  const mf_registry_view_id_v1 view_id = {UINT64_C(5), UINT64_C(7)};
  mf_client_ring_v1 ring;
  mf_ring_descriptor_v1 descriptor = {0};
  if (mf_client_ring_create_v1(UINT32_C(4), view_id, UINT64_C(11), UINT64_C(13), &ring) !=
      MF_SHARED_SUCCESS) {
    return 1;
  }
  if (mf_client_submit_copy_v1(
          &ring, UINT64_C(17), UINT64_C(19), UINT64_C(23), UINT64_C(29), UINT64_C(31), UINT64_C(37),
          MF_RING_COPY_FLAG_REGION_ARGUMENT_BLOCK_V1) != MF_SHARED_INVALID_ARGUMENT ||
      mf_client_submit_copy_region_v1(&ring, UINT64_C(41), UINT64_C(43), UINT64_C(47)) !=
          MF_SHARED_SUCCESS ||
      mf_client_ring_try_consume_v1(&ring, &descriptor) != MF_SHARED_SUCCESS ||
      descriptor.opcode != MF_RING_OPCODE_COPY ||
      descriptor.flags != MF_RING_COPY_FLAG_REGION_ARGUMENT_BLOCK_V1 ||
      descriptor.request_id != UINT64_C(41) || descriptor.target_id != UINT64_C(43) ||
      descriptor.arguments[0] != UINT64_C(47) || descriptor.arguments[1] != UINT64_C(0) ||
      descriptor.arguments[2] != UINT64_C(0) || descriptor.arguments[3] != UINT64_C(0)) {
    mf_client_ring_close_v1(&ring);
    return 2;
  }
  if (mf_client_submit_direct_host_copy_v1(
          &ring, UINT64_C(53), UINT64_C(59), UINT64_C(61), UINT64_C(67), UINT64_C(71), UINT64_C(73),
          MF_RING_COPY_FLAG_DIRECT_HOST_SOURCE_V1) != MF_SHARED_SUCCESS ||
      mf_client_ring_try_consume_v1(&ring, &descriptor) != MF_SHARED_SUCCESS ||
      descriptor.opcode != MF_RING_OPCODE_COPY ||
      descriptor.flags != MF_RING_COPY_FLAG_DIRECT_HOST_SOURCE_V1 ||
      descriptor.request_id != UINT64_C(53) || descriptor.target_id != UINT64_C(59) ||
      descriptor.arguments[0] != UINT64_C(61) || descriptor.arguments[1] != UINT64_C(67) ||
      descriptor.arguments[2] != UINT64_C(71) || descriptor.arguments[3] != UINT64_C(73) ||
      mf_client_submit_direct_host_copy_v1(
          &ring, UINT64_C(79), UINT64_C(83), UINT64_C(89), UINT64_C(97), UINT64_C(101),
          UINT64_C(103), MF_RING_COPY_FLAG_DIRECT_HOST_DESTINATION_V1) != MF_SHARED_SUCCESS ||
      mf_client_ring_try_consume_v1(&ring, &descriptor) != MF_SHARED_SUCCESS ||
      descriptor.flags != MF_RING_COPY_FLAG_DIRECT_HOST_DESTINATION_V1 ||
      descriptor.request_id != UINT64_C(79) || descriptor.target_id != UINT64_C(83) ||
      descriptor.arguments[0] != UINT64_C(89) || descriptor.arguments[1] != UINT64_C(97) ||
      descriptor.arguments[2] != UINT64_C(101) || descriptor.arguments[3] != UINT64_C(103)) {
    mf_client_ring_close_v1(&ring);
    return 3;
  }
  if (mf_client_submit_direct_host_copy_v1(&ring, UINT64_C(107), UINT64_C(109), UINT64_C(113),
                                           UINT64_C(127), UINT64_C(131), UINT64_C(137),
                                           MF_RING_COPY_FLAG_DIRECT_HOST_SOURCE_V1 |
                                               MF_RING_COPY_FLAG_DIRECT_HOST_DESTINATION_V1) !=
          MF_SHARED_INVALID_ARGUMENT ||
      mf_client_submit_direct_host_copy_v1(
          &ring, UINT64_C(139), UINT64_C(0), UINT64_C(149), UINT64_C(151), UINT64_C(157),
          UINT64_C(163), MF_RING_COPY_FLAG_DIRECT_HOST_SOURCE_V1) != MF_SHARED_INVALID_ARGUMENT ||
      mf_client_submit_direct_host_copy_v1(
          &ring, UINT64_C(167), UINT64_C(173), UINT64_C(179), UINT64_MAX, UINT64_C(0), UINT64_C(1),
          MF_RING_COPY_FLAG_DIRECT_HOST_DESTINATION_V1) != MF_SHARED_INVALID_ARGUMENT ||
      mf_client_submit_direct_host_copy_v1(
          &ring, UINT64_C(181), UINT64_C(191), UINT64_C(193), UINT64_C(197), UINT64_MAX,
          UINT64_C(1), MF_RING_COPY_FLAG_DIRECT_HOST_SOURCE_V1) != MF_SHARED_INVALID_ARGUMENT) {
    mf_client_ring_close_v1(&ring);
    return 4;
  }
  mf_client_ring_close_v1(&ring);
  return 0;
}

static int run_batch_submission_test(void) {
  const mf_registry_view_id_v1 view_id = {UINT64_C(31), UINT64_C(37)};
  mf_client_ring_v1 ring;
  mf_ring_descriptor_v1 batch[3] = {0};
  mf_ring_descriptor_v1 consumed = {0};
  uint64_t producer_position = 0;
  uint64_t consumer_position = 0;
  uint32_t index = 0;

  if (mf_client_ring_create_v1(UINT32_C(4), view_id, UINT64_C(41), UINT64_C(43), &ring) !=
      MF_SHARED_SUCCESS) {
    return 1;
  }
  for (index = 0; index < 3U; ++index) {
    batch[index].opcode = MF_RING_OPCODE_COPY;
    batch[index].request_id = (uint64_t)index + UINT64_C(1);
    batch[index].target_id = (uint64_t)index + UINT64_C(11);
  }
  if (mf_client_ring_try_submit_batch_v1(&ring, batch, 0U) != MF_SHARED_INVALID_ARGUMENT ||
      mf_client_ring_try_submit_batch_v1(&ring, batch, 5U) != MF_SHARED_WOULD_BLOCK ||
      mf_client_ring_try_submit_batch_v1(&ring, batch, 3U) != MF_SHARED_SUCCESS ||
      mf_client_ring_try_submit_batch_v1(&ring, batch, 2U) != MF_SHARED_WOULD_BLOCK) {
    mf_client_ring_close_v1(&ring);
    return 2;
  }
  producer_position = mf_atomic_load_u64_relaxed(&ring.header->producer.position);
  if (producer_position != 3U ||
      mf_client_ring_try_consume_v1(&ring, &consumed) != MF_SHARED_SUCCESS ||
      consumed.request_id != UINT64_C(1) ||
      mf_client_ring_try_submit_batch_v1(&ring, batch, 2U) != MF_SHARED_SUCCESS) {
    mf_client_ring_close_v1(&ring);
    return 3;
  }
  consumer_position = mf_atomic_load_u64_relaxed(&ring.header->consumer.position);
  if (consumer_position != 1U ||
      mf_client_ring_try_consume_v1(&ring, &consumed) != MF_SHARED_SUCCESS ||
      consumed.request_id != UINT64_C(2) ||
      mf_client_ring_try_consume_v1(&ring, &consumed) != MF_SHARED_SUCCESS ||
      consumed.request_id != UINT64_C(3)) {
    mf_client_ring_close_v1(&ring);
    return 4;
  }
  mf_client_ring_close_v1(&ring);
  return 0;
}

static int run_wrap_test(void) {
  const mf_registry_view_id_v1 view_id = {UINT64_C(9), UINT64_C(7)};
  mf_client_ring_v1 ring;
  mf_ring_descriptor_v1 submitted = {0};
  mf_ring_descriptor_v1 consumed = {0};
  const uint64_t start = UINT64_MAX - UINT64_C(1);
  uint64_t position = 0;

  if (mf_client_ring_create_v1(UINT32_C(4), view_id, UINT64_C(1), UINT64_C(1), &ring) !=
      MF_SHARED_SUCCESS) {
    return 1;
  }
  mf_atomic_store_u64_relaxed(&ring.header->producer.position, start);
  mf_atomic_store_u64_relaxed(&ring.header->consumer.position, start);
  mf_atomic_store_u64_relaxed(&ring.descriptors[0].sequence, UINT64_C(0));
  mf_atomic_store_u64_relaxed(&ring.descriptors[1].sequence, UINT64_C(1));
  mf_atomic_store_u64_relaxed(&ring.descriptors[2].sequence, start);
  mf_atomic_store_u64_relaxed(&ring.descriptors[3].sequence, UINT64_MAX);

  for (position = 0; position < UINT64_C(4); ++position) {
    submitted.opcode = (uint32_t)(position + UINT64_C(1));
    submitted.arguments[0] = position;
    if (mf_client_ring_try_submit_v1(&ring, &submitted) != MF_SHARED_SUCCESS ||
        mf_client_ring_try_consume_v1(&ring, &consumed) != MF_SHARED_SUCCESS ||
        consumed.opcode != submitted.opcode || consumed.arguments[0] != position) {
      mf_client_ring_close_v1(&ring);
      return 2;
    }
  }
  mf_client_ring_close_v1(&ring);
  return 0;
}

static int run_metadata_snapshot_test(void) {
  const mf_registry_view_id_v1 view_id = {UINT64_C(15), UINT64_C(17)};
  mf_client_ring_v1 owner;
  mf_client_ring_v1 attached;
  mf_ring_descriptor_v1 descriptor = {0};
  mf_ring_descriptor_v1 consumed = {0};
  uint32_t index = 0;

  if (mf_client_ring_create_v1(UINT32_C(4), view_id, UINT64_C(19), UINT64_C(23), &owner) !=
          MF_SHARED_SUCCESS ||
      mf_client_ring_attach_v1(mf_client_ring_borrow_fd_v1(&owner), view_id, UINT64_C(19),
                               UINT64_C(23), &attached) != MF_SHARED_SUCCESS) {
    mf_client_ring_close_v1(&owner);
    return 1;
  }

  descriptor.opcode = MF_RING_OPCODE_EVENT_WAIT;
  descriptor.request_id = UINT64_C(1);
  descriptor.target_id = UINT64_C(1);
  for (index = 0; index < UINT32_C(4); ++index) {
    descriptor.arguments[0] = index;
    if (mf_client_ring_try_submit_v1(&attached, &descriptor) != MF_SHARED_SUCCESS ||
        mf_client_ring_try_consume_v1(&attached, &consumed) != MF_SHARED_SUCCESS) {
      mf_client_ring_close_v1(&attached);
      mf_client_ring_close_v1(&owner);
      return 2;
    }
  }

  owner.header->metadata.capacity = UINT32_C(0);
  owner.header->metadata.queue_id = UINT64_C(29);
  owner.header->metadata.queue_generation = UINT64_C(31);
  owner.header->metadata.registry_view_id.daemon_incarnation = UINT64_C(37);
  owner.header->metadata.registry_view_id.view_serial = UINT64_C(41);
  if (mf_client_submit_queue_control_v1(&attached, MF_RING_OPCODE_QUEUE_SYNCHRONIZE, UINT64_C(43),
                                        UINT64_C(0), UINT32_C(0)) != MF_SHARED_SUCCESS ||
      mf_client_ring_try_consume_v1(&attached, &consumed) != MF_SHARED_SUCCESS ||
      consumed.target_id != UINT64_C(19) || attached.capacity != UINT32_C(4) ||
      attached.queue_id != UINT64_C(19) || attached.queue_generation != UINT64_C(23) ||
      !mf_registry_view_id_equal_v1(attached.registry_view_id, view_id) ||
      mf_client_ring_wait_writable_v1(&attached, UINT64_C(0)) != MF_SHARED_SUCCESS) {
    mf_client_ring_close_v1(&attached);
    mf_client_ring_close_v1(&owner);
    return 3;
  }

  mf_client_ring_close_v1(&attached);
  mf_client_ring_close_v1(&owner);
  return 0;
}

static int run_consumer(int32_t fd, mf_registry_view_id_v1 view_id) {
  mf_client_ring_v1 ring;
  mf_ring_descriptor_v1 descriptor;
  uint64_t expected = 0;

  if (mf_client_ring_attach_v1(fd, view_id, UINT64_C(17), UINT64_C(3), &ring) !=
      MF_SHARED_SUCCESS) {
    return 10;
  }
  while (expected < TEST_OPERATION_COUNT) {
    const mf_shared_status_v1 status = mf_client_ring_try_consume_v1(&ring, &descriptor);
    if (status == MF_SHARED_WOULD_BLOCK) {
      const mf_shared_status_v1 wait_status = mf_client_ring_wait_readable_v1(&ring, TEST_WAIT_NS);
      if (wait_status == MF_SHARED_TIMEOUT || wait_status == MF_SHARED_SYSTEM_ERROR) {
        mf_client_ring_close_v1(&ring);
        return 11;
      }
      continue;
    }
    if (status != MF_SHARED_SUCCESS || descriptor.opcode != UINT32_C(1) ||
        descriptor.request_id != UINT64_C(23) || descriptor.arguments[0] != expected ||
        descriptor.arguments[1] != (expected ^ UINT64_C(0xa5a5a5a5a5a5a5a5))) {
      mf_client_ring_close_v1(&ring);
      return 12;
    }
    ++expected;
  }
  mf_client_ring_close_v1(&ring);
  return 0;
}

int main(void) {
  const mf_registry_view_id_v1 view_id = {UINT64_C(0x1234), UINT64_C(0x5678)};
  mf_client_ring_v1 ring;
  mf_client_ring_v1 invalid;
  mf_ring_descriptor_v1 descriptor = {0};
  pid_t child = -1;
  int child_status = 0;
  uint64_t sequence = 0;

  if (run_argument_block_test() != 0 || run_copy_region_argument_test() != 0 ||
      run_copy_region_submission_test() != 0 || run_batch_submission_test() != 0 ||
      run_wrap_test() != 0 ||
      run_metadata_snapshot_test() != 0 || run_capacity_test() != 0 || run_mpmc_test() != 0 ||
      mf_client_ring_create_v1(UINT32_C(1024), view_id, UINT64_C(17), UINT64_C(3), &ring) !=
          MF_SHARED_SUCCESS) {
    return 1;
  }
  if (mf_client_ring_attach_v1(mf_client_ring_borrow_fd_v1(&ring), view_id, UINT64_C(17),
                               UINT64_C(4), &invalid) != MF_SHARED_MALFORMED) {
    mf_client_ring_close_v1(&ring);
    return 2;
  }
  ring.header->metadata.flags = UINT32_C(1);
  if (mf_client_ring_attach_v1(mf_client_ring_borrow_fd_v1(&ring), view_id, UINT64_C(17),
                               UINT64_C(3), &invalid) != MF_SHARED_MALFORMED) {
    mf_client_ring_close_v1(&ring);
    return 2;
  }
  ring.header->metadata.flags = UINT32_C(0);

  child = fork();
  if (child < 0) {
    mf_client_ring_close_v1(&ring);
    return 3;
  }
  if (child == 0) {
    const int result = run_consumer(mf_client_ring_borrow_fd_v1(&ring), view_id);
    mf_client_ring_close_v1(&ring);
    _exit(result);
  }

  descriptor.opcode = UINT32_C(1);
  descriptor.request_id = UINT64_C(23);
  for (sequence = 0; sequence < TEST_OPERATION_COUNT; ++sequence) {
    mf_shared_status_v1 status = MF_SHARED_WOULD_BLOCK;
    descriptor.arguments[0] = sequence;
    descriptor.arguments[1] = sequence ^ UINT64_C(0xa5a5a5a5a5a5a5a5);
    while ((status = mf_client_ring_try_submit_v1(&ring, &descriptor)) == MF_SHARED_WOULD_BLOCK) {
      const mf_shared_status_v1 wait_status = mf_client_ring_wait_writable_v1(&ring, TEST_WAIT_NS);
      if (wait_status == MF_SHARED_SYSTEM_ERROR) {
        mf_client_ring_close_v1(&ring);
        return 4;
      }
      (void)sched_yield();
    }
    if (status != MF_SHARED_SUCCESS) {
      mf_client_ring_close_v1(&ring);
      return 5;
    }
  }

  if (waitpid(child, &child_status, 0) != child || !WIFEXITED(child_status) ||
      WEXITSTATUS(child_status) != 0 ||
      mf_client_ring_consumer_doorbells_v1(&ring) > TEST_OPERATION_COUNT ||
      mf_client_ring_producer_doorbells_v1(&ring) > TEST_OPERATION_COUNT) {
    mf_client_ring_close_v1(&ring);
    return 6;
  }
  mf_client_ring_close_v1(&ring);
  return 0;
}
