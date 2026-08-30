---
id: P20260830-018
status: Recorded
captured: 2026-08-31
milestone: M0120
workstream: W0121
branch: main
git_revision: 164a09fc0e528e447ea0f9c63564eb6960357b91
workspace: W0121 lifecycle model and bounded checker are implemented; transport adapters and lifecycle qualification remain active
---

# M0120 W0121 lifecycle model

## Outcome

The M0120 lifecycle extension has a one-way manifest import of the frozen M0110
transport root, a canonical generation/epoch state model, versioned exploration
bounds, and a deterministic Python checker. The model covers add, remove, reset,
transport loss, recover, replay/conflict handling, generation and epoch
exhaustion, old-generation tombstones, and CUDA/NVML provider-view freeze.

## Verification evidence

| Gate | Result |
|---|---|
| Lifecycle model command | Passed: 949 states, 4,012 transitions, 326 complete sequences, 13 direct boundary checks, no counterexamples |
| Input integrity | Passed: base, extension, model, and bounds content hashes verified |
| Checker self-test | Passed: valid run and tampered base-import rejection, 2/2 |
| Full dev CTest | Passed: 74/74 |

## Boundary

W0121 and M0120 remain Active. W0122 must consume this model for memfd, cdev,
and guest vfio-user lifecycle adapters. QMP/vPCI presentation, provider-view
integration, failure injection, 1,000-cycle qualification, and lifecycle ABI
freeze are not implemented by this checkpoint. The M0110 root manifest and data
plane remain unchanged by the lifecycle extension.

## Cleanup

- Removed: external model-check output remains outside Git; no source snapshots or build artifacts were added.
- Retained: canonical extension/model/bounds/checker, CTest self-test, and compact records.

## Handoff

Run the exact W0121 command from the repository root before implementing W0122;
keep lifecycle imports one-way and route transport mechanics through their domain
skills.
