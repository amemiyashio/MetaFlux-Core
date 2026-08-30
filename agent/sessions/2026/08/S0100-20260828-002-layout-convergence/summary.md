# Summary

Converged the repository layout on patterns from comparable open-source
substrates (Mesa/Gallium, Wine's PE/Unix split, crosvm rutabaga_gfx) after the
2026-08-28 layering review. Five adjustments, structural only:

1. D0010: transports get the one-directory two-halves convention (C17 client,
   C++20 worker, no shared headers).
2. D0011: a machine-checked component dependency graph. Every boundary target
   registers at configure time; tools/check-component-graph.py enforces the
   role whitelist, the C-to-CXX language wall, and no client-side daemon links,
   running as CTest metaflux.architecture.component-graph in every preset.
   The failure path was verified by injecting an illegal edge.
3. The documented "unit tests stay beside their owners" rule was enforced by
   moving the runtime smoke test to runtime/core/tests/.
4. Placeholder directories (Vulkan backend, vfio-user service, compiler
   worker) were pruned per the create-on-implementation rule with ownership
   preserved in docs/roadmap.md.
5. docs/architecture/repo-layout.md (Verified) records the taxonomy,
   dependency map, and placement table.

Verification: dev/release/asan 15/15, provider 10/10, cuda-only and nvml-only
6/6 each, agent records ok. Revisions: base
67aa3975e37bfff5812a32b97490a1eec1c3b11b, final
cca908221cb5b265ae0944a3e87598b1f5a70956.

## roast

### light roasts

- none.

### medium roasts

- Repository taxonomy and navigation map -> docs/architecture/repo-layout.md
  (revision cca908221cb5b265ae0944a3e87598b1f5a70956; record status Verified;
  dev, release, and asan presets passed 15/15)

### dark roasts

- D0010 transport two-halves convention and repository placement ->
  transports/README.md (revision cca908221cb5b265ae0944a3e87598b1f5a70956;
  all listed presets passed; authority: D0010, SC not required)
- D0011 machine-checked dependency graph -> tools/README.md (revision
  cca908221cb5b265ae0944a3e87598b1f5a70956; illegal-edge failure path and all
  listed presets passed; authority: D0011, SC not required)

## session-only

- none.
