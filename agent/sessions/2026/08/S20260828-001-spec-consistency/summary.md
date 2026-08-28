# Summary

Reviewed the MetaFlux-Core specifications against the repository's own stated
aspirations (Wine-style compatibility, microsecond warm paths, one
authoritative device view, contracts on demand, performance budgets as
acceptance gates) and removed five internal contradictions through
record-level amendments. No product functionality was implemented or altered.

Decisions recorded: D0008 (vroot synthetic NVIDIA identity is a presentation
disguise with registration/legal review required before release promotion) and
D0009 (userspace glibc floor 2.31 / Ubuntu 20.04 with a restricted provider
DT_NEEDED universe). Contracts amended: the v0.1 memfd wake budget (cold
syscalls free, at most one wake syscall per active dispatch, zero-syscall
obligation deferred to M0002 doorbell transports), provisional/binding status
for numeric performance budgets, and per-managed-domain scoping of the
authoritative logical-device view.

Git moved from zero commits to a reviewed history: baseline
`9703559ef0056b6dd8ef5432b645a1362e72d734` (174 files) followed by spec
revision `5360d51a09234f9753f260f218dc5a87e52c7eef` (11 files, +69/-20).
Agent record validation passed after the amendments. M0001 remains the active
milestone with M0001-W01 active; its performance budgets are now explicitly
provisional pending the W01 reference-host harness.

## Distillation

- Distilled: decisions D0008 and D0009 into decisions-index; glibc floor and
  per-domain view into memory/constraints.md and memory/project.md; the
  provisional/binding budget rule into agent/README.md and the milestone
  template; the memfd wake budget into the control-and-data-plane record.
  No experience records (practices were repository-specific wording, not
  reusable procedures).
