# Compiler Process Lifetime

Trace `CpuExecutionEngine::prepare` in
[`execution.cpp`](../../../../services/metafluxd/src/execution.cpp) into
`compile_kernel_in_worker`/`spawn_worker` in
[`compiler_worker_client.cpp`](../../../../services/metafluxd/src/compiler_worker_client.cpp).
An ordinary warm-JIT/AOT lookup has no worker invocation. Distinguish a cache
reservation wait from an already spawned compiler before changing a timeout.

## Spawn and I/O ownership

`spawn_worker` creates separate request and response `AF_UNIX` stream socketpairs
with `SOCK_CLOEXEC`. Descriptors are moved above standard streams before the
`posix_spawn` file actions map the child ends to stdin/stdout and close the pair
ends that each process does not own. Preserve cleanup on every partial failure,
including the case where the invoking process starts with a standard fd closed.
The private entry is `--compiler-worker-v1 <parent-pid>` on an absolute executable
path. `POSIX_SPAWN_SETPGROUP` creates the group used for failure cleanup.

The parent ends become nonblocking. `send_request` handles partial sends and
EINTR, uses `MSG_NOSIGNAL`, then shuts down the request write direction so the
child can observe EOF. `receive_response` consumes bounded data until EOF.
Do not replace these with an unbounded read/write or wait that hides cancellation.
The parent currently passes `environ` to `posix_spawn`; it does not install a
namespace/seccomp sandbox or filter all inherited process state.

## Deadline, cancellation and reap

The policy in [compiler_worker.hpp](../../../../services/metafluxd/src/compiler_worker.hpp)
defaults to a 120-second deadline. After spawn, one steady-clock deadline covers
send, receive and process exit. The parent polls cancellation in bounded slices;
the deadline is not restarted after each I/O stage. Encoding/spawn and cache
reservation are distinct boundaries, so report exactly which interval was
measured instead of claiming a universal end-to-end deadline.

Pre-cancellation fails before spawn. Cancellation, timeout, oversized response
and I/O failure close owned endpoints and use `ChildProcess::terminate_and_reap`:
kill the child process group and child PID, then `waitpid` through EINTR. Normal
completion also waits/reaps and checks exit/signal status before accepting the
response. Preserve cleanup before `prepare` returns and the distinction among
cancelled/deadline, resource-limit and I/O/child-failure diagnostics.

`apply_worker_limits` in
[`compiler_worker_process.cpp`](../../../../services/metafluxd/src/compiler_worker_process.cpp)
checks the expected parent before and after `PR_SET_PDEATHSIG`, sets private
creation permissions and tightens inherited limits: zero core, 512 MiB file
size, 60 seconds CPU and 64 fds. Production also limits address space to 4 GiB;
ASan omits only that limit for shadow memory. The compiler's own deadline is
110 seconds; the parent still owns forced cleanup. Limit setup failure emits a
structured resource error before compilation. Keep these distinct protections
when changing a specific bound; a longer wait is not a diagnosis of stuck work.

## Evidence and scope

Use [compiler_worker_test.cpp](../../../../services/metafluxd/tests/compiler_worker_test.cpp):
`test_real_worker_and_warm_bypass`, `test_worker_fault` and
`test_worker_cancellation` cover separate child identity/reaping, missing-worker
warm success, crash, timeout, malformed response and cancellation. Extend the
case matching the changed transition, including group descendants or fd closure
when those mechanics change. Assert response/error and cleanup, not a sleep or
counter alone. Session disconnect/shutdown cancellation belongs with
[$daemon-execution-runtime](../../daemon-execution-runtime/SKILL.md) skill.

Decision-0019 retains static MLIR/LLVM components in the daemon image. Actual
child execution proves process isolation; lack of dynamic `DT_NEEDED` entries
does not prove that the parent ELF excludes static compiler code. Packaging,
activation and whole-system isolation claims require their own scoped evidence.
