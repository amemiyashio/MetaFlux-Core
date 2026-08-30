---
id: P20260831-031
status: Recorded
captured: 2026-08-31
milestone: M0120
workstream: W0121
branch: main
git_revision: 3c4de28
workspace: lifecycle model checker now enforces initialized CUDA/NVML membership, loss propagation, and NVML reinitialization invariants; provider runtime integration remains open
---

# M0120 W0121 Provider-View Invariants

## Outcome

The bounded lifecycle checker now rejects provider state before initialization,
membership entries that were never committed, and loss states that fail to mark
an initialized provider entry. CUDA repeated capture is asserted to remain
frozen. NVML initialization requires a positive init epoch, and reinitialization
must advance that epoch, clear prior loss markers, and capture the current online
generation. The direct provider scenario now covers both the frozen pre-reinit
NVML view and the post-reinit replacement view.

## Verification evidence

| Gate | Result |
|---|---|
| Exact lifecycle model command | Passed: 949 states, 4,012 transitions, 326 complete sequences, 15 direct checks |
| Lifecycle checker self-test | Passed: 2/2 |
| Lifecycle model CTest selection | Passed: 2/2 |
| Full development CTest | Passed: 79/79 |
| Commit identity | `Agent Harness (codex)` as Author and Committer for `3c4de28` |

## Boundary

These are model-level provider-view invariants. They do not add provider runtime
hooks, CUDA/NVML shared-memory publication, telemetry-bank fencing, or
qualification evidence. W0121 remains Active for runtime provider integration,
fence publication races, and the broader lifecycle/fault gates.

## Cleanup

- Removed: no session-owned disposable artifact.
- Retained: checker source, bounded model inputs, CTest self-test, and this
  compact checkpoint. Generated model JSON remains external evidence.

## roast

### light roasts

- Provider membership and loss invariants -> `tools/check-lifecycle-model.py` (exact 949-state run and 15 direct checks)
- Provider-model completion boundary -> `agent/plan/M0120-vpci-lifecycle/work/W0121-lifecycle-model.md` (lifecycle model CTest 2/2)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- none.

## Handoff

Continue S0121/W0121 with runtime provider publication/fence integration. Keep
CUDA membership frozen after initialization, allow NVML replacement visibility
only at a later zero-to-one initialization epoch, and retain `(UUID, generation)`
matching for common live entries.
