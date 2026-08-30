# Linux UAPI v1

The candidate ABI `0.x` cdev and worker-broker records are defined in
[`schema/uapi.json`](schema/uapi.json), imported by the M0110 base manifest, and
generated with the transport projection. Structures use fixed-width types,
explicit padding, sized inputs, and reserved-zero validation. The public
include wrapper is [`include/metaflux/uapi/transport.h`](include/metaflux/uapi/transport.h);
the generated header is installed alongside it. Compatibility and ioctl
qualification remain W0112/W0114 work before a `v1` freeze.
