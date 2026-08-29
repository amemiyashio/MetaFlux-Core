---
id: E0002
status: Validated
validated: 2026-08-27
applies_to: application-side CUDA/NVML provider DSOs
---

# Provider Closure and Symbol Gates

## Observation

Source-level C boundaries do not prove a small runtime closure. A static helper
can leak symbols into every DSO, a transitive link can add compiler libraries,
and a combined test condition can leave single-provider builds unqualified.

## Validated practice

- Keep provider targets C17 and link the stateless client fast path privately.
- Compile with hidden visibility and expose only a checked version-script list.
- Test bootstrap version, exact SONAME, exact `DT_NEEDED`, and exact versioned
  MetaFlux exports independently for each provider.
- Keep the simultaneous CUDA/NVML `dlopen` test as a separate dual-provider gate.
- Check the installed package payload as well as build-tree ELF metadata.

The implementation lives in the [provider targets](../../plugins/compat/cuda/),
[ABI test registration](../../tests/CMakeLists.txt), and repository-owned
[release package matrices](../../tests/release/README.md).

## Evidence and revalidation

At checkpoint
[P20260827-001](../progress/checkpoints/2026/P20260827-001-engineering-bootstrap-baseline.md):

- Provider preset: 9/9 tests passed.
- CUDA-only and NVML-only: 5/5 tests passed independently.
- Installed providers exposed their intended versioned bootstrap fixture and
  depended only on `libc.so.6`.
- The then-current broad Nix check reported a passing provider release-closure
  gate. This is preserved as a historical result, not current qualification
  evidence; the owning CTest and release-package harnesses must revalidate it.

Revalidate whenever a provider manifest, link dependency, compiler flags,
sysroot, or fast-path implementation changes.
