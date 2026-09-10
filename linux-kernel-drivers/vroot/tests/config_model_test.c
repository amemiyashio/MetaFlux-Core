#include "metaflux/vroot/config_model.h"

#include <stdio.h>
#include <string.h>

#define EXPECT(condition)                                                                  \
  do {                                                                                     \
    if (!(condition)) {                                                                     \
      (void)fprintf(stderr, "vroot config test failed at %s:%d: %s\n", __FILE__, __LINE__, \
                    #condition);                                                           \
      return 1;                                                                             \
    }                                                                                        \
  } while (0)

static int test_add_and_config(void) {
  mf_vroot_model model;
  const uint8_t uuid[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  uint8_t function = 0xffU;
  uint32_t value = 0U;
  mf_vroot_function snapshot;

  EXPECT(mf_vroot_model_init(&model, 0x12U, 0x34U, 2U) == MF_VROOT_STATUS_OK);
  EXPECT(mf_vroot_add(&model, uuid, 7U, &function) == MF_VROOT_STATUS_OK);
  EXPECT(function == 0U);
  EXPECT(mf_vroot_get_function(&model, function, &snapshot) == MF_VROOT_STATUS_OK);
  EXPECT(snapshot.domain == 0x12U && snapshot.bus == 0x34U && snapshot.devfn == 0U);
  EXPECT(snapshot.present != 0U && snapshot.logical_present != 0U);
  EXPECT(snapshot.matching_enabled == 0U && snapshot.bound == 0U && snapshot.online == 0U);
  EXPECT(snapshot.config[0] == 0x46U && snapshot.config[1] == 0x4dU);
  EXPECT(snapshot.config[2] == 0x01U && snapshot.config[3] == 0x00U);
  EXPECT(snapshot.config[9] == 0x00U && snapshot.config[10] == 0x00U &&
         snapshot.config[11] == 0x12U);
  EXPECT(snapshot.config[44] == 0x46U && snapshot.config[45] == 0x4dU);
  EXPECT(mf_vroot_read_config(&model, function, 0U, 4U, &value) == MF_VROOT_STATUS_OK);
  EXPECT(value == 0x00014d46U);
  EXPECT(mf_vroot_read_config(&model, function, 255U, 2U, &value) == MF_VROOT_STATUS_RANGE);
  EXPECT(mf_vroot_read_config(&model, function, 0U, 3U, &value) == MF_VROOT_STATUS_INVALID_ARGUMENT);
  EXPECT(mf_vroot_read_config(&model, function, 1U, 2U, &value) == MF_VROOT_STATUS_INVALID_ARGUMENT);
  EXPECT(mf_vroot_write_config(&model, function, 0U, 2U, 0U) == MF_VROOT_STATUS_OK);
  EXPECT(mf_vroot_write_config(&model, function, 1U, 2U, 0U) == MF_VROOT_STATUS_INVALID_ARGUMENT);
  EXPECT(mf_vroot_write_config(&model, function, 0U, 2U, 1U) == MF_VROOT_STATUS_READ_ONLY);
  return 0;
}

static int test_prebind_and_probe(void) {
  mf_vroot_model model;
  const uint8_t uuid[16] = {0};
  uint8_t function = 0xffU;
  mf_vroot_function snapshot;

  EXPECT(mf_vroot_model_init(&model, 0U, 0U, 1U) == MF_VROOT_STATUS_OK);
  EXPECT(mf_vroot_add(&model, uuid, 1U, &function) == MF_VROOT_STATUS_OK);
  EXPECT(mf_vroot_enable_matching(&model, function) == MF_VROOT_STATUS_NOT_READY);
  EXPECT(mf_vroot_prepare_driver(&model, function) == MF_VROOT_STATUS_OK);
  EXPECT(mf_vroot_enable_matching(&model, function) == MF_VROOT_STATUS_OK);
  EXPECT(mf_vroot_get_function(&model, function, &snapshot) == MF_VROOT_STATUS_OK);
  EXPECT(snapshot.matching_enabled != 0U && snapshot.online == 0U);
  EXPECT(mf_vroot_probe(&model, function, true) == MF_VROOT_STATUS_OK);
  EXPECT(mf_vroot_get_function(&model, function, &snapshot) == MF_VROOT_STATUS_OK);
  EXPECT(snapshot.matching_enabled == 0U && snapshot.bound != 0U && snapshot.online != 0U);
  EXPECT(mf_vroot_probe(&model, function, true) == MF_VROOT_STATUS_NOT_READY);
  return 0;
}

static int test_probe_failure_rescan_remove(void) {
  mf_vroot_model model;
  const uint8_t uuid[16] = {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
  uint8_t function = 0xffU;
  mf_vroot_function snapshot;

  EXPECT(mf_vroot_model_init(&model, 0U, 1U, 1U) == MF_VROOT_STATUS_OK);
  EXPECT(mf_vroot_add(&model, uuid, 9U, &function) == MF_VROOT_STATUS_OK);
  EXPECT(mf_vroot_prepare_driver(&model, function) == MF_VROOT_STATUS_OK);
  EXPECT(mf_vroot_enable_matching(&model, function) == MF_VROOT_STATUS_OK);
  EXPECT(mf_vroot_probe(&model, function, false) == MF_VROOT_STATUS_OK);
  EXPECT(mf_vroot_get_function(&model, function, &snapshot) == MF_VROOT_STATUS_OK);
  EXPECT(snapshot.present == 0U && snapshot.quarantined != 0U && snapshot.online == 0U);
  EXPECT(mf_vroot_read_config(&model, function, 0U, 1U, &(uint32_t){0}) ==
         MF_VROOT_STATUS_NOT_PRESENT);
  EXPECT(mf_vroot_rescan(&model, function) == MF_VROOT_STATUS_OK);
  EXPECT(mf_vroot_prepare_driver(&model, function) == MF_VROOT_STATUS_OK);
  EXPECT(mf_vroot_enable_matching(&model, function) == MF_VROOT_STATUS_OK);
  EXPECT(mf_vroot_probe(&model, function, true) == MF_VROOT_STATUS_OK);
  EXPECT(mf_vroot_remove(&model, function) == MF_VROOT_STATUS_OK);
  EXPECT(mf_vroot_rescan(&model, function) == MF_VROOT_STATUS_NOT_PRESENT);
  EXPECT(mf_vroot_add(&model, uuid, 10U, &function) == MF_VROOT_STATUS_OK);
  EXPECT(mf_vroot_get_function(&model, function, &snapshot) == MF_VROOT_STATUS_OK);
  EXPECT(snapshot.generation == 10U && snapshot.devfn == 0U && snapshot.online == 0U);
  return 0;
}

static int test_allocation(void) {
  mf_vroot_model model;
  const uint8_t uuid[16] = {0};
  uint8_t first = 0xffU;
  uint8_t second = 0xffU;
  uint8_t third = 0xffU;

  EXPECT(mf_vroot_model_init(&model, 0U, 0U, 2U) == MF_VROOT_STATUS_OK);
  EXPECT(mf_vroot_add(&model, uuid, 1U, &first) == MF_VROOT_STATUS_OK);
  EXPECT(mf_vroot_add(&model, uuid, 2U, &second) == MF_VROOT_STATUS_OK);
  EXPECT(first == 0U && second == 1U);
  EXPECT(mf_vroot_add(&model, uuid, 3U, &third) == MF_VROOT_STATUS_NO_SPACE);
  EXPECT(mf_vroot_remove(&model, first) == MF_VROOT_STATUS_OK);
  EXPECT(mf_vroot_add(&model, uuid, 3U, &third) == MF_VROOT_STATUS_OK);
  EXPECT(third == 0U);
  EXPECT(mf_vroot_remove(&model, second) == MF_VROOT_STATUS_OK);
  EXPECT(mf_vroot_add(&model, uuid, 2U, &second) == MF_VROOT_STATUS_STALE);
  EXPECT(mf_vroot_add(&model, uuid, 4U, &second) == MF_VROOT_STATUS_OK);
  EXPECT(second == 1U);
  return 0;
}

static int test_repeated_lifecycle_cycles(void) {
  mf_vroot_model model;
  uint8_t uuid[16] = {0};
  uint8_t function = 0xffU;
  mf_vroot_function snapshot;

  EXPECT(mf_vroot_model_init(&model, 0x21U, 0x43U, 2U) == MF_VROOT_STATUS_OK);
  for (uint64_t generation = 1U; generation <= 1000U; ++generation) {
    uuid[0] = (uint8_t)(generation & 0xffU);
    uuid[1] = (uint8_t)((generation >> 8U) & 0xffU);
    EXPECT(mf_vroot_add(&model, uuid, generation, &function) == MF_VROOT_STATUS_OK);
    EXPECT(function == 0U);
    EXPECT(mf_vroot_prepare_driver(&model, function) == MF_VROOT_STATUS_OK);
    EXPECT(mf_vroot_enable_matching(&model, function) == MF_VROOT_STATUS_OK);
    if ((generation % 3U) == 0U) {
      EXPECT(mf_vroot_probe(&model, function, false) == MF_VROOT_STATUS_OK);
      EXPECT(mf_vroot_get_function(&model, function, &snapshot) == MF_VROOT_STATUS_OK);
      EXPECT(snapshot.present == 0U && snapshot.quarantined != 0U &&
             snapshot.generation == generation);
      EXPECT(mf_vroot_rescan(&model, function) == MF_VROOT_STATUS_OK);
      EXPECT(mf_vroot_prepare_driver(&model, function) == MF_VROOT_STATUS_OK);
      EXPECT(mf_vroot_enable_matching(&model, function) == MF_VROOT_STATUS_OK);
    }
    EXPECT(mf_vroot_probe(&model, function, true) == MF_VROOT_STATUS_OK);
    EXPECT(mf_vroot_get_function(&model, function, &snapshot) == MF_VROOT_STATUS_OK);
    EXPECT(snapshot.present != 0U && snapshot.bound != 0U && snapshot.online != 0U &&
           snapshot.generation == generation && snapshot.devfn == 0U);
    EXPECT(mf_vroot_remove(&model, function) == MF_VROOT_STATUS_OK);
    EXPECT(mf_vroot_get_function(&model, function, &snapshot) == MF_VROOT_STATUS_OK);
    EXPECT(snapshot.logical_present == 0U && snapshot.present == 0U && snapshot.online == 0U);
  }
  return 0;
}

static int test_config_offset_width_mask_fuzz(void) {
  mf_vroot_model model;
  const uint8_t uuid[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  uint8_t function = 0xffU;
  uint32_t value = 0U;
  static const uint8_t widths[] = {1U, 2U, 3U, 4U, 5U, 8U};
  uint32_t accepted_reads = 0U;
  uint32_t rejected = 0U;

  EXPECT(mf_vroot_model_init(&model, 0x7U, 0x1U, 1U) == MF_VROOT_STATUS_OK);
  EXPECT(mf_vroot_add(&model, uuid, 11U, &function) == MF_VROOT_STATUS_OK);
  for (uint16_t offset = 0U; offset < 512U; ++offset) {
    for (size_t index = 0U; index < sizeof(widths) / sizeof(widths[0]); ++index) {
      const uint8_t width = widths[index];
      const mf_vroot_status read_status =
          mf_vroot_read_config(&model, function, offset, width, &value);
      const mf_vroot_status write_status =
          mf_vroot_write_config(&model, function, offset, width, 0xffffffffU);
      const int legal_width = (width == 1U || width == 2U || width == 4U);
      const int aligned = ((offset & (uint16_t)(width - 1U)) == 0U);
      const int in_range = offset < MF_VROOT_CONFIG_SIZE &&
                           width <= MF_VROOT_CONFIG_SIZE - offset;
      if (legal_width && aligned && in_range) {
        EXPECT(read_status == MF_VROOT_STATUS_OK);
        /* CI Type-0 identity fields are read-only under the writable mask. */
        EXPECT(write_status == MF_VROOT_STATUS_OK ||
               write_status == MF_VROOT_STATUS_READ_ONLY);
        ++accepted_reads;
      } else {
        EXPECT(read_status != MF_VROOT_STATUS_OK);
        EXPECT(write_status != MF_VROOT_STATUS_OK);
        ++rejected;
      }
    }
  }
  EXPECT(accepted_reads > 0U && rejected > 0U);

  /* Init-failure cleanup: remove then further access is not-present. */
  EXPECT(mf_vroot_remove(&model, function) == MF_VROOT_STATUS_OK);
  EXPECT(mf_vroot_read_config(&model, function, 0U, 4U, &value) ==
         MF_VROOT_STATUS_NOT_PRESENT);
  EXPECT(mf_vroot_write_config(&model, function, 0U, 4U, 0U) == MF_VROOT_STATUS_NOT_PRESENT);
  EXPECT(mf_vroot_prepare_driver(&model, function) == MF_VROOT_STATUS_NOT_PRESENT);
  return 0;
}

int main(void) {
  if (test_add_and_config() != 0 || test_prebind_and_probe() != 0 ||
      test_probe_failure_rescan_remove() != 0 || test_allocation() != 0 ||
      test_repeated_lifecycle_cycles() != 0 || test_config_offset_width_mask_fuzz() != 0) {
    return 1;
  }
  (void)puts("vroot config model: ok");
  return 0;
}
