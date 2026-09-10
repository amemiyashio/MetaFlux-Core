# Kernel

Linux kernel components provide character devices, shared mappings, waits, PCI
transport binding, and the later experimental software PCI presentation. They
are written in Linux GNU C and built through the target kernel's Kbuild system.

Kernel code implements transport and lifecycle contracts rather than CUDA or
NVIDIA RM/UVM semantics. Public Linux UAPI lives under
`contracts/uapi/linux/`; kernel-internal interfaces remain source-versioned
implementation details. KUnit and fault tests are owned alongside the kernel
components.

For a local cdev deployment, `metaflux_core.ko` also brokers the exclusive
binding between one published device generation and one data-plane worker. It
validates credentials, lease identity, queue attachment, and revocation; registry
policy and backend selection remain in `metafluxd`.

Source ownership is `core/` for cdev and worker-broker objects, `pci/` for the
common static guest function driver, `vroot/` for the default-off software root,
and `tests/` for KUnit/kselftest support.
The first `pci/` stage validates and maps the milestone-0.1.1.0 BAR0/BAR2/BAR4 profile and
reserves two MSI-X vectors; ring/DMA/interrupt-arm behavior remains in work-item-0.1.1.3.
Required target-kernel compatibility shims are selected by compile/API probes
against the exact target kernel rather than broad version checks. Their future
source home is described in [Roadmap Homes](../docs/roadmap.md#kernel-compatibility-shims)
and is created only when a real shim implementation needs it.
