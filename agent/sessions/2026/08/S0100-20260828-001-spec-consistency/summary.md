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
obligation deferred to M0110 doorbell transports), provisional/binding status
for numeric performance budgets, and per-managed-domain scoping of the
authoritative logical-device view.

Git moved from zero commits to a reviewed history: baseline
`9703559ef0056b6dd8ef5432b645a1362e72d734` (174 files) followed by spec
revision `5360d51a09234f9753f260f218dc5a87e52c7eef` (11 files, +69/-20).
Agent record validation passed after the amendments. M0100 remains the active
milestone with W0101 active; its performance budgets are now explicitly
provisional pending the W0101 reference-host harness.

## Distillation

- Promoted: D0008/D0009 and their constraint consequences -> decisions-index,
  memory/constraints.md, memory/project.md, agent/README.md, the milestone
  template, and the control-and-data-plane record (session verification above).
- Session-only: none; the wording practices were repository-specific rather
  than reusable experience.
