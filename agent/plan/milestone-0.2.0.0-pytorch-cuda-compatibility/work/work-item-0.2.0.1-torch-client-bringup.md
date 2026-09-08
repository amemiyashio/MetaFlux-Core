---
id: work-item-0.2.0.1
delivery: 0.2.0.1
milestone: milestone-0.2.0.0
status: Active
area: compat.cuda
depends_on: [work-item-0.1.0.4]
updated: 2026-09-08
---

# Pinned CUDA/PyTorch Client Contract

## Outcome

The pinned baseline client resolves one explicit CUDA-facing surface and passes
import, driver enumeration, and runtime copy through a stock MetaFlux daemon.
Every accepted behavior has provenance and executable positive and negative
evidence.

## Current Implementation

The provider implements the broad CUDA 12 initialization surface, primary
context and device attributes, CUDA library/kernel query entry points, observed
cudart export-table adapters, and repeated function-token reuse. The baseline
probe and client profile manifest are checked in.

The registered CTest probe is a fake-client self-test. There is no checked-in
gate that provisions and runs the pinned real client, and the required
Driver/internal-table surface is not yet a single versioned status matrix.
Function-token reuse also lacks the full invalid, stale, destroyed, and
cross-module negative matrix. This work item remains Active.

## Work

- [ ] Generate one versioned surface matrix from the pinned headers, provider
  exports, typed stubs, and profile-specific internal-table observations.
  Classify each behavior as normative, observed on the pinned client/build, or
  MetaFlux-strengthened.
- [ ] Add focused handle tests for repeated live lookup, invalid module,
  destroyed module, stale generation, cross-module name collision, duplicate
  teardown, and capacity reuse.
- [ ] Add a checked-in acceptance gate that runs the pinned real client through
  import, driver enumeration, and runtime copy against a stock daemon and
  records exact client, provider, source, and backend identities.
- [ ] Bind the surface matrix, client profile, probe schema, compiler inputs,
  and cache identity so incompatible artifacts miss.
- [ ] Keep internal cudart table behavior profile-scoped and fail unsupported
  UUIDs or slots deterministically; do not expose it as a general CUDA Driver
  guarantee.

## Exit Gate

The generated surface/status matrix and handle-negative suite pass; the pinned
real-client gate reaches `runtime-copy` through a stock daemon from a named
Git revision; every observed or strengthened behavior is labeled; and ABI,
dependency-closure, simultaneous CUDA/NVML load, and milestone-0.1.x regression
gates remain green.
