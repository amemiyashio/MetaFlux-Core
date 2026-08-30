# Session Summary

## Objective and outcome

Defined and applied one product SemVer and delivery-coordinate policy across the
repository. Every current and historical M/W/S record now uses the derived
identity and product artifacts read `0.1.0` from one source. Under the policy
captured by this session, the unsupported Intel, physical-NVIDIA, and native-
NixOS gates were assigned to `v0.2.0`. D0027 later superseded that destination
for Intel support qualification and physical NVIDIA binding performance by
assigning them to M1000 / `v1.0.0`; native NixOS qualification remains assigned
to the unallocated `v0.2.0` expansion. This later interpretation does not create
hardware qualification evidence for this session.

## Durable changes

- `docs/release-versioning.md`, `VERSION`, CMake, packaging, and daemon version
  output: establish the three-part product version and four-part delivery model.
- `agent/plan/`, `agent/sessions/`, and `agent/progress/`: migrate all milestone,
  work-item, session, and historical references without compatibility aliases.
- `tools/`, `.githooks/`, `.claude/`, templates, and repository skills: create,
  validate, route, and consume only the new identities; ambiguous compact bodies
  are rejected before session creation.

## Verification

| Command/gate | Result |
| --- | --- |
| Dev CMake configure/build and CTest | Passed; 63/63 tests |
| Agent record regression suite | Passed; 111/111 cases |
| Session guidance regression suite | Passed; 17/17 cases |
| Skill routing corpus / self-test | Passed; 68 cases / 23 of 23 |
| Skill package quick validation | Passed; 18 packages |
| Historical inventory | Passed; 25 directories equal 25 index rows |
| Legacy identifier and diff scans | Passed; no residual legacy identity or whitespace error |

## Cleanup

- Removed: all pre-D0024 M/W/S names and compatibility aliases; no build tree,
  source snapshot, dependency store, download, or routine log was added.
- Retained: G002 only in the target owner's active guidance inbox; it is
  intentionally untracked and must be disposed by that session owner.

## Decisions and experience

- At capture time, D0024 was canonical in `docs/release-versioning.md`; D0012
  and D0023 stated the then-current `v0.1.0`/`v0.2.0` qualification boundary.
  D0027 later superseded only the Intel and physical-NVIDIA destination with
  M1000 / `v1.0.0`; native NixOS remains assigned to `v0.2.0`.

## roast

### light roasts

- Single product-version source -> `VERSION` (`0.1.0` consumed by CMake,
  packaging, and daemon version output)

### medium roasts

- none.

### dark roasts

- Product SemVer and delivery-coordinate identity ->
  `docs/release-versioning.md` (63/63 CTest, 111/111 Agent cases, 17/17
  guidance cases, complete 25-directory inventory, and first pre-D0025
  semantic migration; authority: D0024, SC not required)

## session-only

- none.

## Unresolved items

- The target completion-session owner still needs to process G002.
- At capture time, `v0.2.0` had no allocated milestone pending plan approval.
  D0027 later allocated M1000 / `v1.0.0` for Intel support and physical NVIDIA
  binding-performance qualification while leaving native NixOS in the
  unallocated `v0.2.0` expansion.

## Handoff

Use `python3 tools/new-session.py <MAJOR.MINOR.PATCH.WORK> <slug>` for all new
work. At this session's handoff boundary, the M0100 completion owner still had
to process G002 before its next checkpoint or close boundary. D0027 later
superseded G002's destination only for Intel support and physical NVIDIA
binding-performance qualification; it did not alter this session's recorded
guidance disposition or evidence.
