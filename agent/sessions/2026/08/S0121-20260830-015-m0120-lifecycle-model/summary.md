# Session Summary

## Objective and outcome

W0121 now has the canonical lifecycle extension and a deterministic bounded
checker. The extension imports the frozen M0110 root by content hash; the
checker exercises idempotent add/remove/reset/recover/loss transitions,
generation-candidate exhaustion, epoch retirement, tombstones, and provider
view freeze without changing M0110 bytes. The checker now enforces provider
membership, loss propagation, and NVML reinitialization invariants directly,
and separately explores loss-fence versus telemetry-bank publication races with
bounded reader retry and final-fence validation.

## Durable changes

- `contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/`: one-way
  extension manifest and canonical transition model.
- `tests/lifecycle/`: versioned exploration bounds and checker self-test.
- `tools/check-lifecycle-model.py`: deterministic validator and bounded DFS with
  explicit initialized CUDA/NVML view invariants plus a fence/telemetry race
  branch.

## Verification

| Command/gate | Result |
| --- | --- |
| Model checker | Passed: 949 states, 4,012 transitions, 326 complete sequences, 15 direct boundary checks; CUDA/NVML membership and loss invariants enforced |
| Fence/telemetry branch | Passed: 337 states, 565 transitions, 117 complete sequences, 6 direct race checks; loss fence wins stale publication and reader retry is bounded |
| Hash and negative fixtures | Passed: valid run, tampered base-import rejection, tampered publication rule, and tampered latch bounds (5/5 self-test) |
| CTest registration | Passed: lifecycle model and self-test 2/2 |
| Full dev CTest | Passed: 79/79 (one prior compiler-worker flake passed on focused rerun and clean full rerun) |

## Cleanup

- Removed: none.
- Retained: none.

## Decisions and experience

- W0121 follows the lifecycle authority and provider-view rules in M0120 and
  `device-lifecycle-resilience`; no new decision is introduced.
- Provider rules are model invariants, not runtime publication evidence: CUDA
  capture remains frozen, while NVML replacement visibility requires a later
  initialization epoch.
- Fence/telemetry publication rules are a bounded model projection: an even
  telemetry latch and final fence recheck are required before returning
  `ONLINE`; runtime provider publication integration remains a separate gate.

## roast

### light roasts

- Lifecycle extension -> `contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/` (checker input hash)
- Exploration bounds -> `tests/lifecycle/model-bounds.json` (versioned traversal input hash)
- Deterministic lifecycle checker -> `tools/check-lifecycle-model.py` (949-state lifecycle run plus 337-state publication run)
- Fence/telemetry race contract -> `contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/model.json` (loss precedence and stale `ONLINE` rejection)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- External model-check JSON - reason: generated bounded evidence is a build/run
  artifact and is not retained in the repository.

## Unresolved items

- W0121 remains Active until lifecycle adapters consume the model and the
  broader provider runtime publication, fence wiring, fault, and qualification
  gates pass.

## Handoff

Resume from checkpoint `P20260831-032`; run
`python3 tools/check-lifecycle-model.py` with the W0121 root command after the
inputs are present; keep the M0110 base manifest unmodified.
