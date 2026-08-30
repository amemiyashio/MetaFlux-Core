---
id: P20260831-032
status: Recorded
captured: 2026-08-31
milestone: M0120
workstream: W0121
branch: main
git_revision: 42ded57
workspace: bounded lifecycle loss-fence and telemetry-bank race model is implemented; runtime provider publication wiring and qualification remain open
---

# M0120 W0121 Fence and Telemetry Publication Races

## Outcome

W0121 now has a separate `fence_telemetry` model branch. It interleaves
even/odd lifecycle-fence latch acquisition, loss-fence commit or abort, a
two-bank telemetry writer, and reader capture/bank-read/final-fence recheck.
Loss invalidates telemetry readiness before a stale writer can restore an
acceptable `ONLINE` result. Reader retries are bounded by the versioned bounds
file, and stale or odd-latch reads never return `ONLINE`.

## Verification evidence

| Gate | Result |
|---|---|
| Exact lifecycle model command | Passed: 949 states, 4,012 transitions, 326 complete sequences, 15 direct checks |
| Fence/telemetry exploration | Passed: 337 states, 565 transitions, 117 complete sequences, 6 direct checks |
| Lifecycle checker self-test | Passed: 5/5, including publication evidence and tampered publication/latch inputs |
| Lifecycle model CTest selection | Passed: 2/2 |
| Full development CTest | Passed: 79/79; the first run's daemon compiler-worker isolation flake passed on focused rerun and clean full rerun |
| Commit identity | `Agent Harness (codex)` as Author and Committer for content `860764a` and documentation `42ded57` |

## Boundary

This checkpoint proves the publication ordering and reader decision model only.
It does not claim live provider hooks, telemetry writer call-site binding,
runtime fence publication integration, 1,000-cycle fault qualification, or
lifecycle ABI freeze. Those remain active W0121/W0123 work.

## Cleanup

- Removed: temporary model-check outputs and temporary tamper fixtures.
- Retained: versioned model inputs, checker self-test, and this compact checkpoint.

## roast

### light roasts

- Fence/telemetry race model -> `tools/check-lifecycle-model.py` (337-state bounded exploration and 6 direct checks)
- Publication bounds -> `tests/lifecycle/model-bounds.json` (`fence_telemetry` limits and deterministic event order)
- Publication contract -> `contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/model.json` (loss precedence, even-latch reads, stale `ONLINE` rejection)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- none.

## Handoff

Resume S0121/W0121 from this checkpoint. Keep the model's loss-fence
precedence and final-fence reader recheck when wiring the runtime provider and
telemetry publication paths; do not promote this model evidence to runtime or
qualification evidence.
