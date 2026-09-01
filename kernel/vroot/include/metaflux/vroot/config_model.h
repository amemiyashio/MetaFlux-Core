#ifndef METAFLUX_VROOT_CONFIG_MODEL_H
#define METAFLUX_VROOT_CONFIG_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MF_VROOT_CONFIG_SIZE 256U
#define MF_VROOT_MAX_FUNCTIONS 8U
#define MF_VROOT_MAX_CONFIG_ACCESS_WIDTH 4U

typedef enum mf_vroot_status {
  MF_VROOT_STATUS_OK = 0,
  MF_VROOT_STATUS_INVALID_ARGUMENT = 1,
  MF_VROOT_STATUS_RANGE = 2,
  MF_VROOT_STATUS_NOT_PRESENT = 3,
  MF_VROOT_STATUS_BUSY = 4,
  MF_VROOT_STATUS_NO_SPACE = 5,
  MF_VROOT_STATUS_NOT_READY = 6,
  MF_VROOT_STATUS_READ_ONLY = 7,
  MF_VROOT_STATUS_STALE = 8,
} mf_vroot_status;

typedef struct mf_vroot_function {
  uint8_t config[MF_VROOT_CONFIG_SIZE];
  uint8_t writable_mask[MF_VROOT_CONFIG_SIZE];
  uint8_t logical_present;
  uint8_t present;
  uint8_t matching_enabled;
  uint8_t driver_ready;
  uint8_t bound;
  uint8_t online;
  uint8_t quarantined;
  uint8_t reserved;
  uint16_t domain;
  uint8_t bus;
  uint8_t devfn;
  uint8_t uuid[16];
  uint64_t generation;
} mf_vroot_function;

typedef struct mf_vroot_model {
  mf_vroot_function functions[MF_VROOT_MAX_FUNCTIONS];
  uint16_t domain;
  uint8_t bus;
  uint8_t max_functions;
  uint8_t initialized;
  uint8_t reserved[3];
} mf_vroot_model;

mf_vroot_status mf_vroot_model_init(mf_vroot_model* model, uint16_t domain, uint8_t bus,
                                     uint8_t max_functions);

mf_vroot_status mf_vroot_add(mf_vroot_model* model, const uint8_t uuid[16], uint64_t generation,
                              uint8_t* out_function);

mf_vroot_status mf_vroot_prepare_driver(mf_vroot_model* model, uint8_t function);
mf_vroot_status mf_vroot_enable_matching(mf_vroot_model* model, uint8_t function);
mf_vroot_status mf_vroot_probe(mf_vroot_model* model, uint8_t function, bool success);
mf_vroot_status mf_vroot_remove(mf_vroot_model* model, uint8_t function);
mf_vroot_status mf_vroot_rescan(mf_vroot_model* model, uint8_t function);

mf_vroot_status mf_vroot_read_config(const mf_vroot_model* model, uint8_t function,
                                     uint16_t offset, uint8_t width, uint32_t* out_value);
mf_vroot_status mf_vroot_write_config(mf_vroot_model* model, uint8_t function, uint16_t offset,
                                      uint8_t width, uint32_t value);

mf_vroot_status mf_vroot_get_function(const mf_vroot_model* model, uint8_t function,
                                       mf_vroot_function* out_snapshot);

#endif
