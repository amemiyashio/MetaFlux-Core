---
name: add-component
description: Register a new boundary target in the build so language, closure, and dependency rules apply to it automatically.
status: Active
---

# Add a Component

Use when adding any new boundary target: provider, backend, transport half,
frontend, service, or contract target. The registry, not the directory name,
is what makes the rules bind.

## Steps

1. Claim a component id (`<area>.<name>`, dotted) and a role. Roles must be
   one of the whitelist in `ALLOWED_DEPENDENCIES` inside
   [check-component-graph.py](../../../tools/check-component-graph.py);
   a new role requires extending that whitelist first.
2. Register through `metaflux_add_component` in the owning parent
   `CMakeLists.txt` (`ID`, `ROLE`, `OPTION`, `DIRECTORY`, `TARGET`,
   `LANGUAGE`). `LANGUAGE C` triggers the C-only directory rule
   automatically; application-side halves are C17 by constraint.
3. Create the leaf directory with `CMakeLists.txt`, `include/`, `src/`.
   Component-owned tests go in `<component>/tests/`, gated by
   `BUILD_TESTING AND METAFLUX_BUILD_TESTS`.
4. Add the build option to `cmake/MetaFluxOptions.cmake` if it is new, wire
   the subdir condition in the parent chain, and add a preset variant only if
   the component needs standalone qualification.
5. Update package filesets in
   [source.nix](../../../nix/lib/source.nix): each deployment package
   (runtime, provider, daemon) must include exactly the components it ships.
6. For transport components, create `client/` and `worker/` halves per D0010
   with roles `transport-client` / `transport-worker`.

## Verification

```sh
nix develop path:. -c cmake --preset dev
nix develop path:. -c ctest --preset dev
```

`metaflux.architecture.component-graph` passing proves the new target's edges
are role-legal and language-legal; the provider ELF gates apply closure
purity automatically for provider targets.
