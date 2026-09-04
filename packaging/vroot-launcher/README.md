# metaflux-vroot-launcher

Packaging-owned helper for the experimental bare-metal vroot package. It
validates `metaflux-vroot-dkms` metadata, refuses `metaflux-vpci` dependencies,
and emits a namespace bind plan limited to canonical MetaFlux nodes.

It does not:

- load `metaflux_vroot.ko` or any other module
- create `/dev/nvidia*` nodes or hide vendor devices
- start launch, copy, event, or metrics work through the presentation module

Live mount-namespace execution remains a host qualification step under the
tests-owned `baremetal-vpci` gate.
