# Services

Long-lived processes live here. `metafluxd` is the sole registry, policy,
generation, and worker-lease authority. Compiler workers isolate LLVM/MLIR work.
`metaflux-vfio-userd` is the guest PCI control-plane service and the future
leased guest data-plane worker that owns the backend instance for that
generation. Its current entrypoint owns the bounded Unix socket and delegates
the generated vfio-user control protocol to the transport adapter; the
libvfio-user integration and steady-state guest data plane remain explicit
0.x qualification work.

A local data-plane worker may be embedded in `metafluxd` during v0.x, but it has
the same explicit worker interface and exclusive lease as an out-of-process
worker. Authority does not imply that `metafluxd` must sit in every execution
path, and two workers never consume the same live generation.

Services keep compiler frameworks and slow control paths outside application
processes. Shared-memory readers and cache-hit execution avoid service RPC on the
steady-state path. Each executable remains an independently buildable and
packageable target.
