# PyTorch Compatibility Research Roadmap

This roadmap owns exact client profiles and exploratory probes. Product scope,
execution ownership, and acceptance belong to
[milestone-0.2.0.0](milestone-0.2.0.0-pytorch-cuda-compatibility/plan.md), whose
current priority is transparent stock-client execution rather than an internal
compiler-mechanism headline.

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
compatibility. Its selected route is pinned stock-client input -> neutral
framework-kernel request -> canonical Kernel IR -> daemon CPU execution ->
Vulkan qualification. MLIR remains inside backend-owned compiler pipelines.
A successful import, fake-client probe self-test, manual operator run, or
provider-local semantic result does not establish product qualification.

The first work item owns all five baseline stages and must correlate eager add
with daemon submission and CPU-backend completion. The second expands the
complete provider/handle profile and versioned CPU corpus. The third qualifies
lifecycle behavior and the same Kernel IR corpus on Vulkan. Exact acceptance
and remaining surface, schema, library, execution-mode, and target-routing
choices live only in the active milestone and its work items.
