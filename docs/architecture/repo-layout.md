# Repository Layout

| Field | Value |
| --- | --- |
| Status | Verified |
| Scope | Directory taxonomy, dependency direction, navigation |

The tree mixes three axes on purpose; the cost is navigation, which this
document pays down. Do not flatten the axes into one.

## The three axes

- **By function**: what the code is. `contracts/`, `runtime/`, `compiler/`,
  `transports/` are functional layers with one directory per boundary.
- **By deployment image**: what process or package the code ships in.
  `services/` (daemon and workers), `kernel/` (out-of-CMake Kbuild modules),
  and the halves of each transport (`client/` into the application closure,
  `worker/` into the daemon) follow the load image, not the function.
- **By artifact**: `tests/` (cross-component qualification), `cmake/` `nix/`
  `toolchains/` (build), `packaging/` (release), `docs/` `agent/` (records).

The plugin tree combines function and deployment: `plugins/compat/` presents
an application ecosystem (application-side, C17), `plugins/backend/` executes
(workers-side, C++20); they never link each other.

## Dependency direction

Dependency points from volatile to stable. Contracts have no dependencies.

```text
contracts (client protocol, backend plugin API)
   ^                      ^
runtime client fastpath   backend compilers and runtimes (CPU, later Vulkan)
(C, app closure)          (C++20, compiler core or mf_backend_api_v1)
   ^                          ^
compat providers            services/metafluxd (also <- runtime core,
(libcuda.so.1,                compiler core, transport workers)
 libnvidia-ml.so.1)
```

Machine checks: every boundary target registers with the component graph
(`cmake/MetaFluxComponentGraph.cmake`), and `tools/check-component-graph.py`
fails the build on any edge outside the role whitelist, any C-to-CXX link, or
any client-side link into the daemon. Provider ELF gates and cross-component
qualification remain under `tests/`; Nix only provides their fixed tools.

## Where to put things

| Change | Home |
| --- | --- |
| Wire protocol, shared-memory layout, kernel UAPI, plugin ABI | `contracts/<zone>/...` |
| Application-side fast path | `runtime/client/` (C17 enforced) |
| Neutral registry/runtime mechanisms | `runtime/core/` |
| Neutral IR, passes, cache keys | `compiler/` |
| Ecosystem ABI presentation (CUDA, NVML, PTX input) | `plugins/compat/<ecosystem>/...` |
| Execution target | `plugins/backend/<target>/...` |
| Transport implementation | `transports/<name>/client/` and `transports/<name>/worker/` |
| Long-lived process | `services/<name>/` |
| Kernel module | `kernel/<name>/` (Kbuild, not CMake) |
| Component-owned unit tests | `<component>/tests/` |
| Cross-component qualification | `tests/` |
| Planned-but-unimplemented component | a section in [`docs/roadmap.md`](../roadmap.md), no directory |

## Prior art

The layout mirrors `src/gallium/` in Mesa (frontends = compat plugins, drivers
= backends, winsys = transports, auxiliary = neutral core) and the
single-directory two-image split of Wine's `dlls/<name>/unix/` (transport
halves). rutabaga_gfx in crosvm is the closest deployed example of the neutral
protocol plus pluggable backends shape.
