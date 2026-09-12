# Verification Contract

Checks are objects with unique `id`, non-empty `argv`, and optional
`optional_skip_reason` for an expected environment exit 77. Direct CTest checks
may declare unique `allowed_ctest_skips`. Exit-zero CTest alone never proves
all selected tests ran. Skips remain explicit and do not satisfy an Exit Gate.

Preflight enumerates direct CTest commands with `--show-only=json-v1`, rejecting
empty selections, duplicate commands, overlapping test definitions in the same
execution context and missing fixtures/dependencies. Opaque wrappers are not
CTest declarations. Keep distinct modes and physical environments.
Configure/build may precede tests; deferred enumeration becomes strict after
preparation. Each result records actual resolved tests and a JUnit report.

Parent review binds current content, plan and its complete review rule
certificate. Evaluation additionally requires the current verification
certificate at the same request, base and HEAD; integration's combined module
contains both review and verify rules. Schema-3 receipts embed both certificates,
input/toolchain identity, command results, unique attempt/log identities and
digests. Historical validation uses the candidate's tested file versions,
independently of the integrator's current rules.

Each evaluation owns one exclusive attempt directory. A common-Git verification
lock prevents concurrent runs across worktrees. Inspect is read-only and points
to the actual session/check/log. A quiet running check is not a failure.
Interruption or failed execution produces no passing receipt; fix its cause
before fresh review/evaluation. Logs are never a cross-phase test cache.

Changing current action modules after success permits delivery of unchanged
content. Changed bytes, modes, HEAD, toolchain, review or plan invalidate evidence.
A missing git add preserves the receipt; stage the exact reviewed bytes.
[$batch](../../batch/SKILL.md) skill's exact before/after metadata proof is the
only final-acceptance exemption from a third product run.

Repair may revise a check plan within the same task. Removed CTest checks need
actual coverage proof from replacements in the same context with no new skips;
other required checks stay required. The changed request needs fresh rules,
parent review and actual execution, never a receipt edit.
