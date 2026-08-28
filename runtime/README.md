# Runtime

The runtime source boundary owns ecosystem-neutral mechanisms and types for
device, registry, execution, memory, queue, lifecycle, and scheduling behavior.
At run time, the authoritative registry, policy, generation, and worker lease
belong to the `metafluxd` instance. The runtime also owns the application-side
fast path used by compatibility providers.

The core must not contain CUDA, NVML, PTX, HIP, HSA, or AMD SMI types. Client
code remains small, allocation-free on steady-state operations, and independent
of LLVM/MLIR and concrete execution backends. Cross-component communication uses
the versioned contracts in `contracts/`.

The application-side chain is `compat provider -> client protocol -> runtime ->
backend plugin ABI`. Client protocol versions are independent of backend ABI
versions; changing an execution backend does not force a provider ABI change.

Provider DSOs may each statically embed stateless fast-path code for inlining and
closure isolation. Mutable mode, registry-view, queue, and generation state must
live in the one negotiated shared mapping, never in per-DSO globals. CUDA and NVML
therefore join the same process view without requiring a public helper DSO or
duplicating mutable state.
