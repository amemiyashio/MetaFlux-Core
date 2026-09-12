# Object Semantics

Use one table per CUDA-visible object with these columns: create/acquire API,
owner, current context requirement, generation binding, thread rule, valid
operations, destroy/release API, stale behavior, observable outcome, behavior
classification, and evidence source.

## Behavior provenance

Classify every row as exactly one of:

1. **Normative:** required by the selected CUDA headers or pinned specification.
2. **Observed:** reproduced on a named driver family/build with an archived probe;
   it is qualification evidence, not a universal CUDA guarantee.
3. **MetaFlux-strengthened:** a deterministic product rule where upstream behavior
   is undefined or unspecified. State the upstream gap and do not present the
   rule as CUDA-specified or as a universal compatibility guarantee.

## Required model

Trace the affected handle through
[provider.c](../../../../plugins/compat/cuda/abi/driver/src/provider.c) and its
[semantics test](../../../../plugins/compat/cuda/abi/driver/tests/provider_test.c).
For each changed class retain owner, generation, valid transitions, concurrency
rule, destruction behavior and error classification. Initialization acquires the
shared view lazily and reentrantly; DSO loading alone remains inert.

- **Device:** consume the runtime-owned process-view contract. The default,
  unfiltered ordinal view follows the membership/order revision captured at CUDA
  initialization. Apply `CUDA_VISIBLE_DEVICES` filtering and reordering once,
  only to that CUDA view. UUID or logical ID correlates persistent identity, but
  live cross-provider parity requires `(UUID, generation)` from the same view
  revision; BDF alone is insufficient. Filtered views and providers initialized
  from different revisions need no ordinal/count parity.
- **Context:** distinguish primary-context retain/release/reset from basic
  context creation/destruction and current-context stack behavior. Track when
  descendants become unusable; if CUDA leaves a post-destruction operation
  undefined, label MetaFlux's deterministic rejection as strengthened behavior.
- **Module/function:** bind parsed PTX, Kernel IR identity, compiler epoch, and
  function lookup to the owning context. Unknown entry points fail before
  submission.
- **Memory:** track allocation kind, size, permissions, context, generation, and
  transport handle. Validate every range and overflow before enqueue.
- **Stream:** preserve per-stream FIFO, legacy-default and per-thread-default
  stream rules included in the selected surface. Internal queue ownership must
  not become a public handle contract.
- **Event:** define record generations, query/wait states, elapsed-time support,
  and when an asynchronous launch/copy failure becomes observable.

## Failure and concurrency rules

- Validate handle type, owner, liveness, and generation on every call.
- Make repeated destroy and use-after-destroy outcomes stable and tested, but
  classify them as MetaFlux-strengthened wherever the selected CUDA contract
  leaves the call undefined rather than claiming a normative CUDA error.
- Separate API serialization requirements from internal registry locks; warm
  launch/copy/event paths must avoid global locks and allocation.
- Surface asynchronous failures only at observation points permitted by the
  selected CUDA contract. Where multiple failures are pending and selection or
  ordering is not normative, archive named-driver observations and label any
  deterministic MetaFlux rule as strengthened. Translate transport loss to the
  stable CUDA device-lost or context outcome selected and classified by the
  provider contract.
- On fail-open/passthrough decisions, transfer before visible managed state is
  committed; never split one process view across providers.

See the [CUDA Driver API](https://docs.nvidia.com/cuda/cuda-driver-api/) for the
selected API's normative lifecycle, then qualify the exact pinned versions.
