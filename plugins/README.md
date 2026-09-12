# Plugins

MetaFlux separates first-party plugins along two independent axes:

- `compat/`: compatibility-layer plugins that present an existing
  application ecosystem and translate its ABI, management API, compiler input,
  and semantics into MetaFlux contracts.
- `backend/`: execution backend plugins that compile and execute MetaFlux IR on
  a concrete compute target.

A compatibility-layer plugin may not directly depend on a concrete execution
backend, and a backend may not expose ecosystem ordinals, handles, or error
codes. Generated files remain with the plugin that owns their source manifest.

During v0.x, all plugins are first-party, in-tree components registered
explicitly by CMake and Nix. There is no runtime directory scan, dynamic plugin
discovery, plugin manifest format, third-party SDK, or stable general plugin ABI.
New ecosystem directories are created only when implementation work begins.

Build selection follows component roles rather than treating one ecosystem as a
single package. CUDA Driver ABI, NVML, PTX frontend, CPU runtime, and later target
compiler/runtime halves have independent CMake options and Nix inputs. Component
targets carry `METAFLUX_COMPONENT_ID` and `METAFLUX_COMPONENT_ROLE` properties so
language, closure, and build-matrix policy can be applied without path-specific
conditionals.

Execution backends may remain statically linked and explicitly registered during
v0.x, but registration and all calls cross `mf_backend_api_v1`; a daemon never
includes a backend's C++ implementation API. Dynamic discovery remains a later
initialization-path decision and is not required for a valid C ABI boundary.
