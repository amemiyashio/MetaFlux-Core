---
name: compiler-artifact-cache
description: Implement or review compiler artifact identity, persistent per-UID and administrator AOT tiers, reservations, atomic publication, pins, eviction and corruption recovery. Use for JIT/AOT persistence or cache-cost defects; backend code validity, compilation and verification-result reuse have separate owners.
---

# Compiler Artifact Cache

For implementation, fix the requested persistent artifact or identity behavior
and prove the affected lookup/publication/lifetime transition. For analysis,
review or benchmarking, stay within that mode and report the actual cache path.

Start storage work at `PersistentArtifactCache::lookup`, `reserve` or `publish`
in [`artifact_cache.cpp`](../../../compiler/core/src/artifact_cache.cpp).
Start key work at `make_cache_key` in
[`cache.cpp`](../../../compiler/core/src/cache.cpp), then the backend supplying
its identity. Identify a miss, invalid payload, lock wait, quota rejection,
publication failure or lost pin before changing broad cache policy.

| Task | Read only the relevant guide |
| --- | --- |
| Identity change, cold/warm/AOT selection, load cost or false cache evidence | [Identity and execution modes](references/identity-and-modes.md) |
| Reserve/publish, fsync/rename, corruption, quotas, pins or process races | [Persistent storage](references/persistent-storage.md) |

Reuse the current key and tier contract. Include every code-affecting semantic,
toolchain, target and ABI input supplied by its owner; exclude transient scheduling
state unless it changes code or ABI. Compute stable identity during preparation,
then retain the prepared artifact. Do not add hashing, disk scans, lock acquisition
or metadata rewrites to each resident warm launch.

Administrator AOT takes lookup precedence over the caller's mutable UID tier.
Keep cross-UID isolation, read-only runtime AOT behavior, per-key publication
serialization, atomic artifact/metadata visibility and live-entry pinning.
Preserve quota/free-space accounting across processes, deterministic eviction and
precise I/O errors. Corrupt mutable content is removed only when unpinned; runtime
does not repair or overwrite administrator AOT content.

Interpreter mode does not compile or use executable-cache evidence. Cold-JIT
misses may compile and publish; warm-JIT/AOT lookup misses fail without compiling.
Explicit AOT preparation is distinct from lookup. `FixtureArtifactCache` is an
in-memory selection fixture, not an executable cache or JIT/AOT qualification.
These artifact keys never cache tests or replace fresh workflow verification.

This skill owns persistence and canonical key encoding. Compose
[$cpu-backend-performance](../cpu-backend-performance/SKILL.md) skill or
[$vulkan-spirv-compute](../vulkan-spirv-compute/SKILL.md) skill for semantic target/helper
inputs, code validation and resident executable resources. MLIR pipeline meaning
belongs to [$mlir-compiler-engineering](../mlir-compiler-engineering/SKILL.md) skill;
child compilation belongs to [$compiler-worker-isolation](../compiler-worker-isolation/SKILL.md) skill.
Module pins and mode activation compose [$daemon-execution-runtime](../daemon-execution-runtime/SKILL.md) skill.

Use `metaflux.unit.artifact-cache` and affected backend pipeline cases; preserve
real cross-instance/process and crash-boundary evidence when those paths change.
A hit is not execution or zero-cost proof. Hand the change, exact tier/error and
same-path evidence to [$review](../review/SKILL.md) skill; [$verify](../verify/SKILL.md) skill
selects covering formal checks once per phase without a new default matrix run.
