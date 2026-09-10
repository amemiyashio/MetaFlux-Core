---
id: work-item-0.2.0.1
delivery: 0.2.0.1
milestone: milestone-0.2.0.0
status: Complete
area: compat.cuda
depends_on: [work-item-0.1.0.4]
updated: 2026-09-10
---

# Stock PyTorch CUDA Baseline

## Outcome

Pinned stock PyTorch `2.11.0+cu126` reaches import, driver enumeration, runtime
copy, artifact intake, and eager add through its normal `torch.cuda` API. Eager
add crosses the neutral protocol and completes in the daemon CPU backend; the
CUDA provider neither computes the tensor nor fabricates successful execution.

PyTorch source, wheel contents, and public APIs remain unchanged. MetaFlux
package, launcher, and loader-environment activation are valid integration
surfaces.

## Current Implementation

The provider implements a broad CUDA initialization surface, primary contexts,
device attributes, CUDA library/kernel query entry points, observed cudart
export-table adapters, repeated function-token reuse, and selected
profile-specific kernel recognition. The checked-in stock-client gate provisions
pinned PyTorch `2.11.0+cu126` and reaches all five stages through the normal
`torch.cuda` API. Its eager int32 add is registered through the negotiated v1
neutral kernel request, loaded into daemon-owned canonical Kernel IR, and
completed by the CPU interpreter without provider-local tensor arithmetic or a
fabricated-success path.

The gate records the exact baseline request/profile versions, direct provider
and internal-table surface, daemon launch, CPU mode, and result bytes. It is a
single-operation baseline, not the CPU profile or a general PyTorch CUDA claim.
The exact Driver/internal-table, neutral request/lifetime, and CPU interpreter
cache-non-use decisions are closed by decisions 0048-0050. The baseline remains
integrated. The gate additionally covers eager add through cold JIT, warm JIT
and AOT with exact cache identity; the CPU work item owns expansion of that
compiled coverage. Decision-0055 clarifies the mode-specific evidence boundary.

## Decisions Before Integration

- Materialize and verify the `pytorch-v2.11.0` reference entry before
  inspecting framework call paths. Its gitlink and notes are research-only;
  any relied-upon conclusion must be promoted to an owning Core decision,
  contract, source path, or test (decision-0047).
- The exact baseline-required Driver and profile-specific internal-table surface
  is resolved by decision-0049: observed/strengthened entries are pinned to the
  stock profile and unclassified reached slots fail stably.
- The minimal versioned neutral request and module-load lifetime are resolved by
  decision-0048.
- The baseline daemon CPU interpreter mode and compiled-cache non-use are
  resolved by decision-0050; compiled CPU cache identity remains
  work-item-0.2.0.2 scope.

These are minimum vertical-slice decisions. The complete surface matrix,
handle-negative expansion, broad operator corpus, library boundary, and Vulkan
routing remain in their dependent work items.

## Work

- [x] Materialize `pytorch-v2.11.0` through the checked-in reference tool and
  use only its exact detached revision for source-path inspection.
- [x] Pin and provision stock PyTorch `2.11.0+cu126` in a checked-in gate without
  editing its source, wheel, or `torch.cuda` API.
- [x] Make import, driver enumeration, and runtime copy pass against a stock
  daemon using only the exact baseline-required provider surface.
- [x] Carry artifact intake through the minimum neutral request and canonical
  Kernel IR boundary instead of bypassing daemon module ownership.
- [x] Lower and execute eager add through the daemon CPU path, recording a
  daemon submission, backend completion, result bytes, source revision, client
  profile, request/Kernel IR versions, and interpreter cache non-use. Compiled
  modes require their own exact cache identity under decision-0050; expansion
  belongs to work-item-0.2.0.2.
- [x] Remove the provider-local eager-add arithmetic and success shortcut from
  the accepted path. Unsupported inputs fail with a stable classified error and
  no output mutation.
- [x] Keep the fake-client probe test as control-flow coverage, clearly
  separated from the real-client acceptance gate.

## Exit Gate

From a named Git revision, the checked-in gate provisions pinned stock PyTorch
`2.11.0+cu126` and reports `complete` after all five stages against a stock
daemon. Eager add has correlated daemon-submission and CPU-backend-completion
evidence, its result is bit-exact against the torch CPU reference, the provider
contains no accepted tensor-compute or fabricated-success path, the three
blocking decisions are closed, and the focused CUDA plus cumulative
milestone-0.1.x regressions remain green.
