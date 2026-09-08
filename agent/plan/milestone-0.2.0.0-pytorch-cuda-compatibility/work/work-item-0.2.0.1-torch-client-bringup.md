---
id: work-item-0.2.0.1
delivery: 0.2.0.1
milestone: milestone-0.2.0.0
status: Active
area: compat.cuda
depends_on: [work-item-0.1.0.4]
updated: 2026-09-08
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
profile-specific kernel recognition. The five-stage probe and pinned client
profile manifest are checked in.

The registered probe test uses a fake torch object. Deferred client cubins skip
daemon artifact registration and launch, while selected eager operations are
computed over host buffers inside the application-side provider. The current
tree therefore does not satisfy this work item.

## Decisions Before Integration

- Materialize and verify the `pytorch-v2.11.0` reference entry before
  inspecting framework call paths. Its gitlink and notes are research-only;
  any relied-upon conclusion must be promoted to an owning Core decision,
  contract, source path, or test (decision-0047).
- Close the exact baseline-required Driver and profile-specific internal-table
  surface. Every entry records normative, pinned observation, or
  MetaFlux-strengthened provenance; unsupported slots fail stably.
- Close the minimal versioned neutral request schema and lifetime required for
  the eager-add launch to become canonical Kernel IR.
- Close the daemon CPU execution-mode surface and cache identity used by this
  real-client path.

These are minimum vertical-slice decisions. The complete surface matrix,
handle-negative expansion, broad operator corpus, library boundary, and Vulkan
routing remain in their dependent work items.

## Work

- [ ] Materialize `pytorch-v2.11.0` through the checked-in reference tool and
  use only its exact detached revision for source-path inspection.
- [ ] Pin and provision stock PyTorch `2.11.0+cu126` in a checked-in gate without
  editing its source, wheel, or `torch.cuda` API.
- [ ] Make import, driver enumeration, and runtime copy pass against a stock
  daemon using only the exact baseline-required provider surface.
- [ ] Carry artifact intake through the minimum neutral request and canonical
  Kernel IR boundary instead of bypassing daemon module ownership.
- [ ] Lower and execute eager add through the daemon CPU path, recording a
  daemon submission, backend completion, result bytes, source revision, client
  profile, request/Kernel IR versions, compiler inputs, and cache identity.
- [ ] Remove the provider-local eager-add arithmetic and success shortcut from
  the accepted path. Unsupported inputs fail with a stable classified error and
  no output mutation.
- [ ] Keep the fake-client probe test as control-flow coverage, clearly
  separated from the real-client acceptance gate.

## Exit Gate

From a named Git revision, the checked-in gate provisions pinned stock PyTorch
`2.11.0+cu126` and reports `complete` after all five stages against a stock
daemon. Eager add has correlated daemon-submission and CPU-backend-completion
evidence, its result is bit-exact against the torch CPU reference, the provider
contains no accepted tensor-compute or fabricated-success path, the three
blocking decisions are closed, and the focused CUDA plus cumulative
milestone-0.1.x regressions remain green.
