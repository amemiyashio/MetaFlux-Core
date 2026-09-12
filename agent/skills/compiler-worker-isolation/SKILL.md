---
name: compiler-worker-isolation
description: Implement or review the daemon's spawned compiler worker, bounded structured IPC, resource limits, cancellation, deadline and kill/reap behavior. Use for compiler-process failures or isolation evidence; MLIR lowering, artifact persistence and daemon activation have separate owners.
---

# Compiler Worker Isolation

For implementation, fix the requested process or protocol boundary and show its
effect on module preparation. For analysis, review or benchmarking, trace that
boundary and report evidence within the requested mode.

Start at `compile_kernel_in_worker` in
[`compiler_worker_client.cpp`](../../../services/metafluxd/src/compiler_worker_client.cpp).
Locate the first divergence among encoding, spawn, send, receive, exit and decode.
Then inspect `run_compiler_worker_process` in
[`compiler_worker_process.cpp`](../../../services/metafluxd/src/compiler_worker_process.cpp)
or `encode_request`/`decode_response` in
[`compiler_worker_protocol.cpp`](../../../services/metafluxd/src/compiler_worker_protocol.cpp).
Change that owner; raising every limit or rerunning all compiler suites does not
explain a truncated response, leaked child or blocked cancellation.

| Task | Read only the relevant guide |
| --- | --- |
| Spawn, descriptor lifetime, deadline, cancellation, crash or process cleanup | [Process lifetime](references/process-lifetime.md) |
| New KIR field, malformed/large IPC, response identity or diagnostics | [Structured protocol](references/structured-protocol.md) |

The current CPU compiler boundary spawns the daemon's private worker entry in a separate
process group. Two bounded socketpair streams carry structured KIR and a result;
the parent retains artifact publication authority. Preserve monotonic deadline,
parent-death handling, mandatory resource limits, stable errors and child reaping
before preparation returns. A cancellation already requested must not spawn.

This isolates compiler execution and failure. The daemon image retains its
declared static compiler closure, and the worker inherits the supplied process
environment; these mechanisms do not establish a complete security sandbox.
Do not turn a worker repair into an executable-layout or deployment redesign.

Interpreter mode does not compile. Cold-JIT misses and explicit AOT preparation
may invoke the worker; warm-JIT and AOT lookup invoke no compiler process.
Keep spawn, protocol serialization and compilation out of resident warm launches.
Check real child identity/reaping and execution evidence; counters alone do not
prove compiled execution or a stock client result.

Compose [$mlir-compiler-engineering](../mlir-compiler-engineering/SKILL.md) skill for
emitter/pass failures and [$ptx-simt-semantics](../ptx-simt-semantics/SKILL.md) skill
for KIR meaning. [$compiler-artifact-cache](../compiler-artifact-cache/SKILL.md) skill
owns reservation/publication; [$cpu-backend-performance](../cpu-backend-performance/SKILL.md) skill
owns target/helper identity, code validation and execution. Session ownership and
preparation cancellation compose [$daemon-execution-runtime](../daemon-execution-runtime/SKILL.md) skill.

Use the affected cases in `metaflux.integration.daemon-compiler-worker`, adding
a bounded regression for new behavior. Preserve relevant protocol negatives,
real-process cleanup and warm bypass; broader acceptance follows the active Exit
Gate. Hand the coherent change and exact evidence to [$review](../review/SKILL.md) skill;
[$verify](../verify/SKILL.md) skill selects covering formal checks once per phase.
