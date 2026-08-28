---
id: M0001-W06
milestone: M0001
status: Queued
area: modes-release
depends_on: [M0001-W01, M0001-W03, M0001-W04, M0001-W05]
updated: 2026-08-27
---

# Modes, Performance, and Release

## Outcome

Qualify managed, passthrough, auto, fail-open, coexistence, performance, and
generic/native packaging without weakening the provider closure.

## Work

- [ ] Discover vendor CUDA/NVML libraries through validated absolute paths that
  cannot recurse to MetaFlux, and load them with local symbol scope.
- [ ] Implement deterministic managed/passthrough/auto selection and failure
  transfer without modifying vendor files or real `/dev/nvidia*` nodes.
- [ ] Test real-only, managed-only, and coexistence namespaces.
- [ ] Train provider/compiler-service PGO profiles from representative workloads
  and record hashes in the compiler epoch.
- [ ] Compare provider `-O2`/`-O3`, monitor instruction-cache growth, apply project
  ThinLTO, and defer custom PGO LLVM until the corpus is stable.
- [ ] Audit allocations, syscalls, locks, cache lines, NUMA, generated assembly,
  relocations, and private dirty RSS.
- [ ] Produce relocatable generic and native NixOS packages; run ABI, sanitizer,
  fuzz, soak, performance, and release-closure gates.

## Exit Gate

Recursion is impossible, injected managed failures leave no partial state and
transfer to the vendor stack, passthrough loss is at most 1%, and all global
acceptance criteria in [M0001](../plan.md) pass on reference Intel and AMD hosts.
