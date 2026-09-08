# PyTorch Compatibility Research Roadmap

This roadmap owns exact client profiles and exploratory probes. Product scope,
execution ownership, and acceptance belong to
[milestone-0.2.0.0](milestone-0.2.0.0-pytorch-cuda-compatibility/plan.md).

## Client Profiles

| Profile | Pinned client | Purpose | Product boundary |
| --- | --- | --- | --- |
| `pytorch-baseline` | PyTorch `2.11.0+cu126` | Stable client contract and milestone acceptance input | Support is limited to the versioned milestone corpus and gates |
| `pytorch-frontier` | PyTorch `2.13.0+cu132` | Discover newer CUDA-facing gaps | Research-only until its own compiler, provider, and differential evidence closes |

Both profiles are exact, isolated, on-demand Nix tool outputs. Neither enters a
default provider, runtime, or release shell. Nix materializes the clients; tests
own probe and qualification behavior.

## Promotion Boundary

milestone-0.2.0.0 is Active and is the only promotion vehicle for PyTorch CUDA
compatibility. Its selected route is pinned client input -> neutral
framework-kernel request -> canonical Kernel IR -> backend-owned MLIR lowering.
A successful import, fake-client probe self-test, manual operator run, or
provider-local semantic result does not establish product qualification.

The baseline becomes accepted only through a checked-in gate that runs the
pinned real client against a stock daemon with exact source, client, provider,
and backend identity. The frontier remains a gap profile until equivalent
evidence exists. An `sm_80` claim additionally requires an immutable capability
descriptor, compiler-epoch and cache-identity changes, and matching parser,
verifier, Kernel IR, interpreter, lowering, provider, and differential evidence.

## Current Evidence

The five-stage probe and both client manifests are checked in. Repository CTest
validates probe control flow with a fake torch object and validates manifest
shape; it does not currently run either real client. The provider contains an
application-side semantic prototype for selected baseline kernels, while
deferred cubins bypass daemon artifact registration and backend launch.

These facts make the next boundary concrete: first add the real baseline gate
and surface matrix, then replace provider-local execution with the selected
neutral Kernel IR and compiler-worker route. CPU lowers through MLIR/LLVM;
Vulkan lowers independently through MLIR/SPIR-V. Exact acceptance and remaining
schema, library, execution-mode, and target-routing choices live only in the
active milestone and its work items.
