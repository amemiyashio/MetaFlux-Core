# Qualification

## ABI matrix

For every selected target version, record:

- header identity and manifest digest;
- expected and actual SONAME, symbols, aliases, and ELF versions;
- C and C++ sizes, alignments, offsets, enum values, and calling conventions;
- supported, typed-stubbed, and intentionally absent entries;
- provider dependency closure and highest referenced glibc symbol.

## Behavioral matrix

Exercise zero, one, and multiple logical devices; filtering/reordering; primary
and basic contexts; module/function lookup; all required copy directions;
launch; streams; events; error name/string; cleanup; and stale generations.

For every behavioral row, record the expected observable outcome and classify it
as normative for the pinned header/spec, observed on a named driver family/build,
or MetaFlux-strengthened where CUDA leaves behavior undefined or unspecified.
Observed and strengthened rows are not promoted to universal CUDA guarantees.

Include null and short outputs, invalid flags, arithmetic overflow, wrong-context
handles, duplicate destruction, repeated and concurrent initialization, daemon
loss, registry replacement, and malformed PTX. Every negative case needs an exact
expected observable outcome and classification. When CUDA defines no result for
an invalid or stale handle, test a deterministic rejection only as explicitly
labeled MetaFlux-strengthened behavior.

## End-to-end evidence

Run one unmodified Driver API binary through interpreter, cold JIT, warm cache,
and AOT. Compare integer bytes and the declared per-operation floating-point
oracle. Verify default, unfiltered CUDA/NVML enumeration when both providers
capture the same process-view revision; with `CUDA_VISIBLE_DEVICES`, correlate
common live incarnations by `(UUID, generation)`, never ordinal or BDF alone.
Providers initialized from different lifecycle revisions need no count/order
parity. Co-loading both providers must not duplicate mutable registry state.

Measure warm initialization, launch, copy, and event paths only after correctness
passes. Archive distributions, affinity/NUMA setup, toolchain fingerprint, and
raw samples. M0001 budgets remain provisional until the named harness records a
baseline.
