# Object Semantics

Use one table per CUDA-visible object with these columns: create/acquire API,
owner, current context requirement, generation binding, thread rule, valid
operations, destroy/release API, stale behavior, and observable error.

## Required model

- **Device:** an ordinal view over one negotiated registry snapshot. Apply
  `CUDA_VISIBLE_DEVICES` filtering and reordering once for the provider
  initialization epoch.
- **Context:** distinguish primary-context retain/release/reset from basic
  context creation/destruction and current-context stack behavior. Context loss
  invalidates descendants deterministically.
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
- Make repeated destroy and use-after-destroy outcomes stable and tested.
- Separate API serialization requirements from internal registry locks; warm
  launch/copy/event paths must avoid global locks and allocation.
- Preserve the first relevant asynchronous error until a CUDA-defined
  observation point. Translate transport loss to a stable CUDA device-lost or
  context error selected by the provider contract.
- On fail-open/passthrough decisions, transfer before visible managed state is
  committed; never split one process view across providers.

See the [CUDA Driver API](https://docs.nvidia.com/cuda/cuda-driver-api/) for the
selected API's normative lifecycle, then qualify the exact pinned versions.
