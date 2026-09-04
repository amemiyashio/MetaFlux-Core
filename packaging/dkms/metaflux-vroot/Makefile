# Packaged Kbuild entry for metaflux-vroot DKMS source.
# The staging tool freezes generated/include before packaging so module builds
# do not require repository Python or contract trees on the target host.

obj-m += metaflux_vroot.o
metaflux_vroot-objs := metaflux_vroot_main.o

ccflags-y += -I$(src)/generated/include

$(obj)/metaflux_vroot_main.o: $(src)/generated/include/metaflux/vroot/generated_profile.h
