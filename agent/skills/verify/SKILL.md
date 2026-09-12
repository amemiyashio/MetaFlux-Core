---
name: verify
description: Select and execute one evidence-bound MetaFlux verification plan for the actual reviewed tree, including focused checks, fresh integration, declared skips and live-run recovery.
---

# Verify

Role: the parent qualifying this exact reviewed phase.
Input: current parent review, exact base/tree, checks and toolchain inputs.
First action: load the verification card's rules and preflight the plan.
Read [verification contract](references/verification.md) for plan and receipt fields.

Use the clean Nix entry for `main.py preflight`, then `main.py step evaluate`.
A plan may configure/build first. Initial deferred CTest enumeration becomes
strict before execution. Full CTest covers its registered focused gates and
self-tests; preserve distinct modes/devices and required fixtures. Declared
environment skips never qualify the skipped product requirement.

During implementation, a focused check should resolve a named uncertainty.
Formal candidate and integration checks execute independently. Epoch activation
requires the complete dev build/full CTest. Batch final checks use only its fixed
acceptance metadata/state/routing plan; product checks belong to integration.

Read output from the existing live tool session; inspect exposes its check/log.
Quiet output is not failure. Fix a failed cause before a new reviewed run;
there is no retry-until-pass or reuse of another phase's receipt.

Completion: an actual passing receipt binds both review and verification rule
versions, tested content, exact plan, toolchain and executed results.
Load the delivery card and proceed. Loading another stage's rules changes no
tested content and does not itself require another review or test run.
