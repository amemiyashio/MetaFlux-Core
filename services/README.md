# Services

Long-lived processes live here. `metafluxd` is the sole registry, policy,
generation, and worker-lease authority. Compiler workers isolate LLVM/MLIR work.
The planned vfio-user service is both the guest PCI server and the leased guest
data-plane worker that owns the backend instance for that generation; planned
service homes are recorded in [`docs/roadmap.md`](../docs/roadmap.md) until
their implementation work begins.

A local data-plane worker may be embedded in `metafluxd` during v0.x, but it has
the same explicit worker interface and exclusive lease as an out-of-process
worker. Authority does not imply that `metafluxd` must sit in every execution
path, and two workers never consume the same live generation.

Services keep compiler frameworks and slow control paths outside application
processes. Shared-memory readers and cache-hit execution avoid service RPC on the
steady-state path. Each executable remains an independently buildable and
packageable target.
