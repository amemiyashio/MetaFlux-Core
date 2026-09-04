# Packaged Kbuild entry for metaflux-vpci DKMS source.
# The staging tool freezes generated/include before packaging so module builds
# do not require repository Python or contract trees on the target host.

obj-m += metaflux_pci.o
metaflux_pci-objs := metaflux_pci_main.o

ccflags-y += -I$(src)/generated/include

$(obj)/metaflux_pci_main.o: $(src)/generated/include/metaflux/pci/generated_guest_profile.h
