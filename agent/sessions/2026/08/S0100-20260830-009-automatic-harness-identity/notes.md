# Notes

The superseded implementation used a product-keyed `HARNESSES` table. Commit
`ded1dad4172b515a3b17cb66c3f7aa18df9cb20e` recorded Codex and commit
`f862852b28e5471b6533ac6754b08c3ca86cbbf4` recorded ZCode; those Git objects,
timestamps, identities, subjects, and associated test evidence remain locked.

D0028 instead derives a neutral identity from a validated lowercase subject
provided by the active agent. The agent reads its identity from the harness
context available to the AI, surfaces `Agent harness subject: <subject>` before
the first commit, and supplies the same subject command-locally through
`METAFLUX_AGENT_HARNESS`. The helper never reads `/proc`, process names, or
product-specific environment namespaces.

Commits `e5d996bacb0527a0007749cf5409cbc42b1158ab` and
`0aac137a229300ce0f5317f78a244e610caeaaac` preserve the intermediate
process-inference route in Git history. The user rejected that route before
SC0004 application; the current source removes it rather than treating it as a
fallback. This self-report is provenance rather than authentication and remains
a workflow rule, so direct human commits continue to use human-owned Git
configuration.

The corrected behavior revision is
`4c23b3f596badcb1c341780a93050917bd994c88`; final precision revision
`0101a4a549436e6dd8f4c94175685811d37c90d0` is SC0004's effective state. Both
were created only after the active agent surfaced `Agent harness subject:
codex` and passed that subject as a command-local declaration. Namespace-only
fixture signals now fail closed.

The existing uncommitted `agent/progress/current.md` P20260830-010 pointer and
`agent/progress/checkpoints/2026/P20260830-010-m0100-completion.md` belong to a
concurrent M0100 owner. This migration neither edits nor stages them. The
S0100-20260830-008-agent-harness-commit-identity event log remains
byte-identical at blob `5b16d08e62765e483aa715bded55e954ba63794d`.
