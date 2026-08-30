obj-m += metaflux_core.o
metaflux_core-y := metaflux_core_main.o

METAFLUX_ROOT := $(abspath $(src)/../..)
METAFLUX_GENERATED_HEADER := $(src)/generated/include/metaflux/transport/generated.h
METAFLUX_SCHEMA_FILES := \
	$(METAFLUX_ROOT)/tools/validate-transport-schema.py \
	$(METAFLUX_ROOT)/contracts/protocol/transport/v1/schema/manifest.json \
	$(METAFLUX_ROOT)/contracts/protocol/device/v1/schema/descriptor.json \
	$(METAFLUX_ROOT)/contracts/shared/device/v1/schema/ring.json \
	$(METAFLUX_ROOT)/contracts/protocol/transport/v1/schema/negotiation.json \
	$(METAFLUX_ROOT)/contracts/uapi/linux/v1/schema/uapi.json

ccflags-y += -I$(src)/generated/include
ccflags-y += -I$(METAFLUX_ROOT)/contracts/shared/device/v1/include
ccflags-y += -I$(METAFLUX_ROOT)/contracts/uapi/linux/v1/include

$(METAFLUX_GENERATED_HEADER): $(METAFLUX_SCHEMA_FILES)
	@mkdir -p $(dir $@)
	python3 $(METAFLUX_ROOT)/tools/validate-transport-schema.py \
		--root $(METAFLUX_ROOT) --generate-header $@

$(obj)/metaflux_core_main.o: $(METAFLUX_GENERATED_HEADER)
