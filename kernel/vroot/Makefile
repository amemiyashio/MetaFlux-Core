obj-m += metaflux_vroot.o
metaflux_vroot-objs := metaflux_vroot_main.o

METAFLUX_ROOT := $(abspath $(src)/../..)
METAFLUX_GENERATED_HEADER := $(src)/generated/include/metaflux/vroot/generated_profile.h
METAFLUX_SCHEMA_FILES := \
	$(METAFLUX_ROOT)/tools/validate-vroot-profile.py \
	$(METAFLUX_ROOT)/contracts/protocol/transport/v1/schema/manifest.json \
	$(METAFLUX_ROOT)/contracts/protocol/transport/v1/schema/extensions/vroot/v1/manifest.json \
	$(METAFLUX_ROOT)/contracts/protocol/transport/v1/schema/extensions/vroot/v1/profile.json \
	$(METAFLUX_ROOT)/contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/manifest.json \
	$(METAFLUX_ROOT)/contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/model.json

ccflags-y += -I$(src)/generated/include

$(METAFLUX_GENERATED_HEADER): $(METAFLUX_SCHEMA_FILES)
	@mkdir -p $(dir $@)
	python3 $(METAFLUX_ROOT)/tools/validate-vroot-profile.py \
		--root $(METAFLUX_ROOT) \
		--manifest $(METAFLUX_ROOT)/contracts/protocol/transport/v1/schema/extensions/vroot/v1/manifest.json \
		--generate-header $@ --kernel-header

$(obj)/metaflux_vroot_main.o: $(METAFLUX_GENERATED_HEADER)
