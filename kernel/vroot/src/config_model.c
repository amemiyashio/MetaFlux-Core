#include "metaflux/vroot/config_model.h"
#include "metaflux/vroot/generated_profile.h"

#include <string.h>

static bool valid_model(const mf_vroot_model* model) {
  return model != NULL && model->initialized != 0U && model->max_functions > 0U &&
         model->max_functions <= MF_VROOT_MAX_FUNCTIONS;
}

static mf_vroot_status get_function_mutable(mf_vroot_model* model, uint8_t function,
                                            mf_vroot_function** out_function) {
  if (!valid_model(model) || out_function == NULL) {
    return MF_VROOT_STATUS_INVALID_ARGUMENT;
  }
  if (function >= model->max_functions) {
    return MF_VROOT_STATUS_RANGE;
  }
  *out_function = &model->functions[function];
  return MF_VROOT_STATUS_OK;
}

static mf_vroot_status get_function_const(const mf_vroot_model* model, uint8_t function,
                                          const mf_vroot_function** out_function) {
  if (!valid_model(model) || out_function == NULL) {
    return MF_VROOT_STATUS_INVALID_ARGUMENT;
  }
  if (function >= model->max_functions) {
    return MF_VROOT_STATUS_RANGE;
  }
  *out_function = &model->functions[function];
  return MF_VROOT_STATUS_OK;
}

static void initialize_config(mf_vroot_function* function, const uint8_t uuid[16],
                              uint16_t domain, uint8_t bus, uint8_t devfn, uint64_t generation) {
  memset(function, 0, sizeof(*function));
  memcpy(function->config, mf_vroot_profile_config_template, MF_VROOT_PROFILE_CONFIG_SIZE);
  memcpy(function->writable_mask, mf_vroot_profile_writable_mask,
         MF_VROOT_PROFILE_CONFIG_SIZE);
  memcpy(function->uuid, uuid, sizeof(function->uuid));
  function->domain = domain;
  function->bus = bus;
  function->devfn = devfn;
  function->generation = generation;
  function->logical_present = 1U;
  function->present = 1U;
}

static bool valid_config_access(uint16_t offset, uint8_t width) {
  return (width == 1U || width == 2U || width == 4U) && offset < MF_VROOT_CONFIG_SIZE &&
         width <= MF_VROOT_CONFIG_SIZE - offset && (offset & (uint16_t)(width - 1U)) == 0U;
}

mf_vroot_status mf_vroot_model_init(mf_vroot_model* model, uint16_t domain, uint8_t bus,
                                     uint8_t max_functions) {
  if (model == NULL || max_functions == 0U || max_functions > MF_VROOT_MAX_FUNCTIONS) {
    return MF_VROOT_STATUS_INVALID_ARGUMENT;
  }
  memset(model, 0, sizeof(*model));
  model->domain = domain;
  model->bus = bus;
  model->max_functions = max_functions;
  model->initialized = 1U;
  return MF_VROOT_STATUS_OK;
}

mf_vroot_status mf_vroot_add(mf_vroot_model* model, const uint8_t uuid[16], uint64_t generation,
                              uint8_t* out_function) {
  if (!valid_model(model) || uuid == NULL || out_function == NULL || generation == 0U) {
    return MF_VROOT_STATUS_INVALID_ARGUMENT;
  }
  if (generation <= model->generation_high_water) {
    return MF_VROOT_STATUS_STALE;
  }
  for (uint8_t index = 0U; index < model->max_functions; ++index) {
    mf_vroot_function* function = &model->functions[index];
    if (function->logical_present != 0U) {
      continue;
    }
    initialize_config(function, uuid, model->domain, model->bus,
                      (uint8_t)(index << 3U), generation);
    model->generation_high_water = generation;
    *out_function = index;
    return MF_VROOT_STATUS_OK;
  }
  return MF_VROOT_STATUS_NO_SPACE;
}

mf_vroot_status mf_vroot_prepare_driver(mf_vroot_model* model, uint8_t function_index) {
  mf_vroot_function* function = NULL;
  const mf_vroot_status status = get_function_mutable(model, function_index, &function);
  if (status != MF_VROOT_STATUS_OK) {
    return status;
  }
  if (function->present == 0U || function->quarantined != 0U) {
    return MF_VROOT_STATUS_NOT_PRESENT;
  }
  if (function->matching_enabled != 0U) {
    return MF_VROOT_STATUS_BUSY;
  }
  function->driver_ready = 1U;
  return MF_VROOT_STATUS_OK;
}

mf_vroot_status mf_vroot_enable_matching(mf_vroot_model* model, uint8_t function_index) {
  mf_vroot_function* function = NULL;
  const mf_vroot_status status = get_function_mutable(model, function_index, &function);
  if (status != MF_VROOT_STATUS_OK) {
    return status;
  }
  if (function->present == 0U || function->quarantined != 0U) {
    return MF_VROOT_STATUS_NOT_PRESENT;
  }
  if (function->driver_ready == 0U || function->matching_enabled != 0U) {
    return MF_VROOT_STATUS_NOT_READY;
  }
  function->matching_enabled = 1U;
  return MF_VROOT_STATUS_OK;
}

mf_vroot_status mf_vroot_probe(mf_vroot_model* model, uint8_t function_index, bool success) {
  mf_vroot_function* function = NULL;
  const mf_vroot_status status = get_function_mutable(model, function_index, &function);
  if (status != MF_VROOT_STATUS_OK) {
    return status;
  }
  if (function->present == 0U || function->matching_enabled == 0U) {
    return MF_VROOT_STATUS_NOT_READY;
  }
  function->matching_enabled = 0U;
  function->driver_ready = 0U;
  if (!success) {
    function->present = 0U;
    function->bound = 0U;
    function->online = 0U;
    function->quarantined = 1U;
    return MF_VROOT_STATUS_OK;
  }
  function->bound = 1U;
  function->online = 1U;
  return MF_VROOT_STATUS_OK;
}

mf_vroot_status mf_vroot_remove(mf_vroot_model* model, uint8_t function_index) {
  mf_vroot_function* function = NULL;
  const mf_vroot_status status = get_function_mutable(model, function_index, &function);
  if (status != MF_VROOT_STATUS_OK) {
    return status;
  }
  if (function->logical_present == 0U) {
    return MF_VROOT_STATUS_NOT_PRESENT;
  }
  memset(function, 0, sizeof(*function));
  return MF_VROOT_STATUS_OK;
}

mf_vroot_status mf_vroot_rescan(mf_vroot_model* model, uint8_t function_index) {
  mf_vroot_function* function = NULL;
  const mf_vroot_status status = get_function_mutable(model, function_index, &function);
  if (status != MF_VROOT_STATUS_OK) {
    return status;
  }
  if (function->logical_present == 0U) {
    return MF_VROOT_STATUS_NOT_PRESENT;
  }
  if (function->present != 0U) {
    return MF_VROOT_STATUS_BUSY;
  }
  function->present = 1U;
  function->matching_enabled = 0U;
  function->driver_ready = 0U;
  function->bound = 0U;
  function->online = 0U;
  function->quarantined = 0U;
  return MF_VROOT_STATUS_OK;
}

mf_vroot_status mf_vroot_read_config(const mf_vroot_model* model, uint8_t function_index,
                                     uint16_t offset, uint8_t width, uint32_t* out_value) {
  const mf_vroot_function* function = NULL;
  if (out_value == NULL || width == 0U || width > MF_VROOT_MAX_CONFIG_ACCESS_WIDTH) {
    return MF_VROOT_STATUS_INVALID_ARGUMENT;
  }
  if (offset >= MF_VROOT_CONFIG_SIZE || width > MF_VROOT_CONFIG_SIZE - offset) {
    return MF_VROOT_STATUS_RANGE;
  }
  if (!valid_config_access(offset, width)) {
    return MF_VROOT_STATUS_INVALID_ARGUMENT;
  }
  const mf_vroot_status status = get_function_const(model, function_index, &function);
  if (status != MF_VROOT_STATUS_OK) {
    return status;
  }
  if (function->present == 0U) {
    return MF_VROOT_STATUS_NOT_PRESENT;
  }
  uint32_t value = 0U;
  for (uint8_t index = 0U; index < width; ++index) {
    value |= (uint32_t)function->config[offset + index] << (index * 8U);
  }
  *out_value = value;
  return MF_VROOT_STATUS_OK;
}

mf_vroot_status mf_vroot_write_config(mf_vroot_model* model, uint8_t function_index, uint16_t offset,
                                      uint8_t width, uint32_t value) {
  if (width == 0U || width > MF_VROOT_MAX_CONFIG_ACCESS_WIDTH) {
    return MF_VROOT_STATUS_INVALID_ARGUMENT;
  }
  if (offset >= MF_VROOT_CONFIG_SIZE || width > MF_VROOT_CONFIG_SIZE - offset) {
    return MF_VROOT_STATUS_RANGE;
  }
  if (!valid_config_access(offset, width)) {
    return MF_VROOT_STATUS_INVALID_ARGUMENT;
  }
  mf_vroot_function* function = NULL;
  const mf_vroot_status status = get_function_mutable(model, function_index, &function);
  if (status != MF_VROOT_STATUS_OK) {
    return status;
  }
  if (function->present == 0U) {
    return MF_VROOT_STATUS_NOT_PRESENT;
  }
  for (uint8_t index = 0U; index < width; ++index) {
    const uint8_t byte_value = (uint8_t)((value >> (index * 8U)) & 0xffU);
    if (byte_value != 0U && function->writable_mask[offset + index] == 0U) {
      return MF_VROOT_STATUS_READ_ONLY;
    }
    if ((byte_value & (uint8_t)~function->writable_mask[offset + index]) != 0U) {
      return MF_VROOT_STATUS_READ_ONLY;
    }
  }
  for (uint8_t index = 0U; index < width; ++index) {
    const uint8_t mask = function->writable_mask[offset + index];
    const uint8_t byte_value = (uint8_t)((value >> (index * 8U)) & 0xffU);
    function->config[offset + index] =
        (uint8_t)((function->config[offset + index] & (uint8_t)~mask) | (byte_value & mask));
  }
  return MF_VROOT_STATUS_OK;
}

mf_vroot_status mf_vroot_get_function(const mf_vroot_model* model, uint8_t function_index,
                                       mf_vroot_function* out_snapshot) {
  const mf_vroot_function* function = NULL;
  if (out_snapshot == NULL) {
    return MF_VROOT_STATUS_INVALID_ARGUMENT;
  }
  const mf_vroot_status status = get_function_const(model, function_index, &function);
  if (status != MF_VROOT_STATUS_OK) {
    return status;
  }
  *out_snapshot = *function;
  return MF_VROOT_STATUS_OK;
}
