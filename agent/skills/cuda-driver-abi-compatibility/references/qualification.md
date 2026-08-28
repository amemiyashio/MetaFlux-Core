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

Include null and short outputs, invalid flags, arithmetic overflow, wrong-context
handles, duplicate destruction, repeated and concurrent initialization, daemon
loss, registry replacement, and malformed PTX. Every negative case needs an
exact expected CUDA error.

## End-to-end evidence

Run one unmodified Driver API binary through interpreter, cold JIT, warm cache,
and AOT. Compare integer bytes and the declared floating-point tolerance. Verify
that CUDA and NVML report the same registry identity and that co-loading both
providers does not duplicate mutable state.

Measure warm initialization, launch, copy, and event paths only after correctness
passes. Archive distributions, affinity/NUMA setup, toolchain fingerprint, and
raw samples. M0001 budgets remain provisional until the named harness records a
baseline.
