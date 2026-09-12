# Benchmarking

## Experiment contract

Record the hypothesis, metric, direct baseline, workload/corpus, input sizes,
warm-up, samples, stopping rule, host identity, compiler/backend build, topology,
online CPU set, `sched_getaffinity`, effective cpuset CPU/memory-node masks, NUMA
placement and mempolicy, service/container restrictions, frequency policy, and
raw-output path. Change one independent variable at a time or use a declared
factorial design.

## MetaFlux comparisons

- Dispatch latency: empty versus busy backend, poll versus block, single stream
  versus same-stream contention, p50/p90/p99 and tails.
- Kernel overhead: compare against identical generated code, target features,
  worker topology, and data placement without the MetaFlux scheduling layer.
- Copy throughput: use the same pages, NUMA placement, direction, registration,
  and copy count; state whether any whole-buffer extra copy occurs.
- Compiler/cache: separate parse, optimization, object emission, publication,
  cold load, warm load, and first execution.

Use monotonic timing, pin benchmark and worker CPUs, disclose SMT siblings,
avoid unrelated machine load, and collect enough repetitions to compare
distributions with confidence intervals or another stated uncertainty method.
Never report only the best sample. Preserve failures and outliers with reasons
instead of silently deleting them.

The [core performance contract](../../../plan/milestone-0.1.0.0-core-foundation/plan.md)
keeps milestone-0.1.0.0 numeric budgets diagnostic. Under decision-0040, only the
complete milestone-2.0.0.0 physical NVIDIA H2D/D2H, passthrough and device-identity
harness may promote them to binding gates. An AMD-only baseline does not do so.
Keep structural zero-allocation/copy/lock obligations distinct from these numeric
budgets and disclose the exact production boundary each measurement covers.

For a stock client result, correlate the module, selected CPU mode and executed
entry with the client request. Separate preparation/first launch from steady
launches, and include argument preparation and completion in a claimed request
cost. The executor-only allocation audit demonstrates its own boundary and
constant counts, not zero allocations throughout the service. Read
[execution path](execution-path.md) before attributing that result to JIT/AOT or
the stock PyTorch route. Reuse the existing harness/profile for the changed path;
broader qualification belongs to the active Exit Gate.

Primary source: [LLVM benchmarking guidelines](https://llvm.org/docs/Benchmarking.html).
