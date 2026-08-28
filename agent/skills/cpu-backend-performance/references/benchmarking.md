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

M0001 numeric budgets are provisional until the reference-host harness archives
its baseline. A smoke result guides work but does not promote a binding gate.

Primary source: [LLVM benchmarking guidelines](https://llvm.org/docs/Benchmarking.html).
