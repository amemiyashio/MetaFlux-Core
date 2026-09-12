# Lifecycle and Versioning

For the affected entry point read [symbols.def](../../../../plugins/compat/cuda/management/nvml/symbols.def),
[dispatch.c](../../../../plugins/compat/cuda/management/nvml/src/dispatch.c),
[provider.c](../../../../plugins/compat/cuda/management/nvml/src/provider.c), and
the selected [decision-0016 header inputs](../../../../toolchains/nvidia-headers-1.json).
Reuse their frozen versions and mode contract before changing a handle or API.

## Initialization contract

- Maintain one process-visible initialization reference count with explicit
  uninitialized, initializing, ready, shutting-down, and failed states.
- Make concurrent first initialization converge on one registry view. A second
  provider in the process must join the committed mode, transport, and
  `registry_view_id`.
- Define behavior before init, after final shutdown, during initialization
  failure, and for excess shutdown calls. Tests must assert exact NVML errors.
- Keep DSO loading inert and initialization reentrant. Avoid callbacks that can
  recurse while a global lifecycle lock is held.

## Versioned API contract

Generate the selected symbol set and structure assertions from pinned R535,
R550, R570, R580, and R610 inputs only when those inputs are present in the
repository manifest. Track canonical name, suffix, declaration, structure
version, minimum target, implementation status, and expected error.

For count/fill APIs, specify:

1. whether a null array queries count;
2. how input capacity and output required count are encoded;
3. whether a short array returns partial results;
4. the exact insufficient-size error;
5. which immutable snapshot makes count and fill coherent.

Never copy a newer structure into an older caller size. Validate version, size,
reserved fields, alignment, and array multiplication before writing.

Drive the changed symbol/structure rows from that manifest. Exercise repeated
and concurrent init/shutdown, pre-init/post-shutdown calls, invalid handles,
short capacities and concurrent count changes for the affected behavior. Keep
per-field errors separate from whole-call errors and preserve inert DSO loading,
provider dependency closure and alias/layout checks when those boundaries change.

## Primary source

- [NVML API reference](https://docs.nvidia.com/deploy/nvml-api/)

The selected headers and stock binaries decide exact ABI behavior where releases
differ.
