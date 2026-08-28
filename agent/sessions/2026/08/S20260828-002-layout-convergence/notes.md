# Notes

The five adjustments came from the 2026-08-28 layering review and were shaped
against three deployed analogues: Mesa `src/gallium/` (frontends = compat
plugins, drivers = backends, winsys = transports, auxiliary = neutral core),
Wine's `dlls/<name>/unix/` (one directory, two load images, narrow crossing
ABI), and crosvm's rutabaga_gfx (neutral virtio-gpu protocol with pluggable
renderer backends).

## Component graph mechanics

- `metaflux_add_component` now registers plugin components automatically;
  contracts, runtime core, client fastpath, compiler core, and the daemon
  register explicitly via `metaflux_register_component_graph_entry`.
- The graph writer resolves ALIAS targets back to their real targets, strips
  `$<LINK_ONLY:...>` wrappers, skips keywords and non-target link items, and
  deduplicates edges before writing JSON to the build directory.
- Whitelist roles: client-protocol, backend-plugin-api, client-fastpath,
  runtime-core, compiler-core, compat-provider, management-provider,
  compiler-frontend, backend-runtime, daemon, transport-client,
  transport-worker. The two transport roles have no components yet; their rows
  freeze the rules the first transport implementation must satisfy.
- The language wall is a second invariant: any edge from a C component to a
  CXX component fails, independent of the role matrix. Client-side roles
  linking the daemon also fail.

## Verification detail

- Failure path: injected edge compat.cuda.driver -> backend.cpu.runtime
  produced both a whitelist violation and a language-wall violation and
  exited nonzero.
- cuda-only and nvml-only trees (6/6 each) confirm the architecture test runs
  in reduced component sets where the graph is smaller but still validated.
- The stable CTest name metaflux.unit.runtime survived the file move, so
  history and Nix label aliases remain continuous.

## Deliberate non-changes

- The three-axis top-level taxonomy is kept; navigation is paid down by
  docs/architecture/repo-layout.md instead of a flattening reorganization.
- contracts zoning, the dual plugin axes, the C/C++ language wall, and the
  kernel-out-of-CMake arrangement are untouched.
- No transport source directories were created; D0010 fixes the convention so
  the first implementation lands in the right shape.
