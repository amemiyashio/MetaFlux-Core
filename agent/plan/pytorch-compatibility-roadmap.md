# PyTorch Compatibility Research Roadmap

This roadmap tracks optional ecosystem probes across releases. It is not a
milestone, product support promise, or acceptance gate for milestone-0.1.0.0.

## Client Profiles

| Profile | Pinned client | Purpose | Product boundary |
| --- | --- | --- | --- |
| `pytorch-baseline` | PyTorch `2.11.0+cu126` | Observe the current `sm_70` path and retain a stable comparison point. | Reports gaps only; it does not expand decision-0017 or milestone-0.1.0.0. |
| `pytorch-frontier` | PyTorch `2.13.0+cu132` | Track the intended future `sm_80` path and newer CUDA-facing gaps. | Research target until a later milestone supplies complete evidence. |

Both profiles are exact, isolated, on-demand Nix tool outputs. Neither enters a
default, provider, runtime, or release shell. CTest and repository test scripts
own probe behavior and qualification; Nix only materializes the clients.

## Promotion Boundary

[milestone-0.1.0.0](milestone-0.1.0.0-core-foundation/plan.md) remains on its frozen PTX 9.0/`sm_70`
semantic contract. The existing
`capability_profile` field remains unused, and reserved ABI fields remain zero.
Any product-code expansion motivated by these probes starts under a new formal
milestone rather than being folded into milestone-0.1.0.0.

An `sm_80` claim requires an immutable capability descriptor, compiler-epoch and
cache identity changes, and matching parser, verifier, Kernel IR, interpreter,
lowering, provider, and differential evidence. Only that milestone may decide
negotiation and publication semantics; a successful framework import or probe
alone does not promote the frontier profile.
