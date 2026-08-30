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

## Distillation

- Distilled: decisions D0010 and D0011 into decisions-index; the repo-layout
  record into docs/architecture/repo-layout.md; the component-map navigation
  row. No experience records (the graph-check pattern is documented in
  tools/README.md rather than as reusable procedure).
