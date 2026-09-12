# Artifact Identity and Execution Modes

Start with `CacheIdentity` in
[`cache.hpp`](../../../../compiler/core/include/metaflux/compiler/cache.hpp)
and `make_cache_key` in
[`cache.cpp`](../../../../compiler/core/src/cache.cpp). The key uses length-delimited
fields, canonical sorted/deduplicated features and SHA-256 over compiler epoch,
toolchain fingerprint, KIR schema/content, pipeline, target triple/CPU/features,
optimization, FP semantics, backend/helper ABI and PGO identity. Preserve field
boundaries and canonical ordering so distinct inputs do not alias by concatenation.

The backend supplies the meaning of these fields. CPU `canonical_identity`,
`lookup_cached_artifact`, `acquire_artifact` and `prewarm_aot` live in
[`pipeline.cpp`](../../../../plugins/backend/cpu/compiler/src/pipeline.cpp).
Its signature payload, helper version, FP usage and ELF validator remain CPU
responsibilities. A Vulkan portable artifact and driver/device-bound pipeline
residency have different identities; do not force them into a CPU ELF payload or
declare the generic persistent cache their current implementation.

## Preserve the mode boundary

Trace `CpuExecutionEngine::prepare` and `PreparedModule::launch` in
[`execution.cpp`](../../../../services/metafluxd/src/execution.cpp). Mode is selected
before listener admission, and compatible administrator AOT has lookup priority.

| Mode | Artifact behavior |
| --- | --- |
| Interpreter | Retain canonical KIR for the independent runtime; no compilation or executable-cache qualification. |
| Cold JIT | Reuse a compatible entry or reserve a mutable miss, invoke compilation, atomically publish, validate and load. |
| Warm JIT | Lookup administrator AOT then caller UID content; a miss does not compile. |
| AOT | Accept administrator content only; a miss does not compile or initialize mutable state. |

Explicit `prewarm_aot` may compile and call `install_aot`; it is not runtime AOT
lookup. Current AOT publication is independent of mutable-tier state/reservations.
Do not infer that every configured cold-JIT preparation spawns: a compatible
existing mutable or AOT entry is reused. Preserve mode-specific errors and actual
artifact provenance when reporting counters or request execution.

`FixtureArtifactCache` in `cache.cpp` stores canonical KIR selection records,
not loaded executable code. Its phase-1 tests do not qualify real JIT/AOT.
Use the persistent entry plus backend loader/executor evidence for that claim.

## Avoid repeated warm work

Identity generation and `lookup` are preparation/load work. Mutable `lookup`
currently reads and hashes the artifact, acquires a shared pin, updates
`last_used` through synced metadata publication and takes cache locks. A warm
lookup means no compiler invocation; it is not zero I/O or zero allocation.
Resident launch should retain the prepared artifact and pin instead of repeating
key generation, validation or lookup per kernel dispatch. Mutable arguments,
resource generations and cancellation still need their owning use-time checks.

For tuning, measure preparation, cold acquisition, warm loading and resident
execution separately along the same request path. For identity changes, vary
the affected field and prove an incompatible miss while equivalent canonical
feature ordering still hits. Compose the backend for code/ABI validation and
the daemon owner for pin retention through unload/teardown. Artifact digest
equality does not authorize reuse of tests or workflow verification receipts.
