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

Planned source ownership is `core/` for cdev and worker-broker objects, `pci/` for
the common function driver, `vroot/` for the default-off software root, `compat/`
for target-kernel API shims, and `tests/` for KUnit/kselftest support. Compatibility
shims are selected by compile/API probes against the exact target kernel rather
than broad version checks.
