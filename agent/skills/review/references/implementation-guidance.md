# From Requested Behavior To Delivery

Read this when choosing or reviewing an implementation slice. It guides work
selection within the received task; it adds no receipt, approval or route state.
An explicit inspection, benchmark, test-harness or maintenance request keeps its
own outcome. Do not convert such a request into unsolicited product work.

## Choose The Change

Start from the current Goal and the user's concrete use case. Inspect the
existing source, relevant callers and nearest tests, then state the intended
before/after behavior in the parent briefing. Prefer an end-to-end capability,
a real correctness repair, or removal of a demonstrated blocking dependency.
Group related cases when they share an implementation boundary and oracle.
Do not use changed lines, corpus counts, green runs or time spent as the target.

For a general request to advance the product, a cleanup-only proposal should
explain which needed implementation it enables. When that implementation fits
the current assignment, include it in the same coherent slice. Keep genuinely
independent maintenance separate. Once the limiting owner and a bounded design
are known, implement there; do not keep inventorying or rerunning old coverage
in place of making the change.

Use existing manifests, profiles and ownership contracts as inputs. A specialist
skill's broad workflow is not an instruction to rebuild every matrix or run all
of its verification scenarios on every invocation. Select the parts affected by
this change, retaining required prerequisites and the owning Exit Gate.

## Follow The Actual Path

Trace the real producer, descriptor, admission guard, materialization/cache,
execution and result/error consumer where relevant. A branch is not dead merely
because today's corpus never reaches it. Before removal, compare its predicate
with all admitted input classes and callers, including fresh and reused state.
If a fallback still supplies behavior, implement the intended replacement or
state the narrowed boundary explicitly and review it against the product contract.

Pair newly supported behavior with an independent expected result and the
nearest meaningful rejection/lifetime boundary. Follow errors through the stock
client too: a library rejection can trigger a second path. Stable rejection is
a correctness result, not expanded compatibility. A compiled artifact or
matching tensor alone does not identify the backend that executed it.

## Spend Verification On A Decision

During implementation, choose a focused check only when its result will resolve
a named uncertainty or reproduce a defect. When practical, demonstrate that a
new regression test fails on the defective implementation. Fix a failing cause
before retrying; inspect the original live attempt rather than launching a copy.

After the coherent change and parent review, select covering checks once per
required phase and preflight them. Broaden coverage when a shared component or
unresolved risk justifies it. Candidate and integration still need fresh checks;
Epoch activation still needs full regression. Passing the required plan leads
to delivery and publication, not another unchanged confidence run. Test commands
and receipts use the existing controller contract, without a new test budget or
process log.

For zero-overhead work, identify the avoidable warm-path operation and move it
to preparation or remove it while preserving semantics. Separate source/trace
proof of no allocation, registration, compilation or extra dispatch from measured
end-to-end latency and throughput. Compare the same workload and execution path;
suite duration and standalone GPU arithmetic are not PyTorch CUDA performance.

## Review And Hand Off

The parent checks the achieved behavior against the briefing and source path,
including unsupported inputs and reused state, before accepting the test plan.
An implementation request ends with implemented behavior and actual evidence;
a list of possible changes is still planning. Report the useful delta, measured
limits and next dependency within the existing delivery, without a second ledger.
