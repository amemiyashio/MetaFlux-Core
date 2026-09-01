---
id: work-item-1.0.0.1
delivery: 1.0.0.1
milestone: milestone-1.0.0.0
status: Queued
area: backend.cpu
depends_on: [milestone-0.1.0.0, milestone-0.1.3.0]
updated: 2026-08-30
---

# Intel x86_64 Host Support

## Outcome

Qualify the cumulative MetaFlux compiler, CPU runtime, compatibility providers,
services, and generic release path on the approved Intel x86_64 support matrix.
Intel support is a `v1.0.0` release obligation, not a milestone-0.1.0.0 completion gate and
not a claim of Intel GPU execution.

## Work

- [ ] Close the exact Intel CPU generation, topology, firmware, kernel, and
  distribution support matrix.
- [ ] Verify CPUID, affinity, SMT sibling, cgroup cpuset, NUMA memory-node, and
  worker placement behavior without AMD-specific assumptions.
- [ ] Run clean dev, sanitizer, release, ABI, interpreter/JIT/AOT differential,
  daemon, provider, recovery, and package suites on every required host row.
- [ ] Verify the portable x86_64 baseline and every enabled ISA specialization,
  including deterministic fallback when a feature is absent.
- [ ] Archive host fingerprints, commands, tool identities, source revisions,
  failures, and accepted results without substituting AMD observations.

## Exit Gate

Every approved Intel row passes the cumulative correctness, ABI, topology,
fault, and release suite. No result is inferred from an AMD host, and no missing
Intel row is recorded as a provisional pass.
