# Summary

Converged the repository's expert-skill design from a useful but overlapping
catalog into 16 validated Codex packages with explicit ownership boundaries.
Eleven domain skills now cover neutral runtime contracts, ecosystem
compatibility, source semantics, compiler mechanics, target execution,
transports, presentation, lifecycle, and Vulkan without assigning the same
decision to multiple experts.

The review also made routing and package integrity executable. A structured
English/Chinese corpus contains 68 single-skill, near-miss, and composition
cases; repository validators enforce its coverage and metadata, while a
separate observation scorer evaluates real model/host runs without treating
static validation as behavioral evidence. Runtime/lifecycle planning was
hardened around explicit publication, admission, recovery, range, telemetry,
and quarantine invariants. These are implementation requirements, not evidence
that queued product work is complete.

## Changed paths

- `agent/skills/`: added `runtime-contracts-registry`, corrected the ten
  neighboring domain experts, added lifecycle model-checking guidance, and
  replaced prose-only trigger examples with the structured 68-case corpus.
- `agent/plan/`, `agent/memory/`, and `runtime/README.md`: synchronized domain
  ownership and the runtime/lifecycle publication contract across M0100-M0130.
- `tools/check-agent-records.py` and its self-test: enforce exact package,
  catalog, metadata, section-order, and repository-discovery contracts.
- `tools/check-skill-routing.py` and its self-test: validate the corpus and
  score digest-bound model observations independently of static package checks.
- `tests/CMakeLists.txt`, `nix/checks/default.nix`, and `nix/lib/source.nix`:
  make routing checks part of CTest and `nix flake check`.

## Verification

| Command/gate | Result |
| --- | --- |
| Bundled Codex `quick_validate.py` | Passed for all 16 skill packages |
| `python3 -B tools/check-agent-records.py .` | Passed before record finalization: 13 sessions, 105 events, 148 Markdown files |
| `python3 -B tools/test-check-agent-records.py` | 52 cases passed |
| `python3 -B tools/check-skill-routing.py .` | 11 domain skills and 68 cases passed |
| `python3 -B tools/test-check-skill-routing.py` | 23/23 cases passed |
| `nix develop path:. -c cmake --preset dev` | Passed |
| `nix develop path:. -c ctest --preset dev --output-on-failure` | 18/18 tests passed |
| `nix flake check path:. -L` | Passed, including Agent records, routing, build, ABI, integration, format, SDK, and closure checks |
| `git diff --check` and staged equivalent | Passed |

## Decisions and experience

- Clarified [D0008](../../../../memory/decisions-index.md) without changing its
  identity: Linux scan may expose an unbound device before the staged MetaFlux
  override is ready, but only successful MetaFlux binding may publish `ONLINE`.
- Added no decision ID and closed no open-decision row. The ownership and
  concurrency text refines implementation contracts subordinate to existing
  milestone decisions.
- No experience record was promoted; routing observations remain model/host-
  specific evidence and have not completed the full corpus run.

## roast

### light roasts

- none.

### medium roasts

- Neutral runtime/contracts owner and single-owner composition matrix ->
  agent/skills/README.md (all 16 skill packages passed bundled quick validation;
  68 routing cases passed)
- Runtime-global FIFO publication and atomic view/telemetry invariants ->
  runtime/README.md (runtime and lifecycle planning synchronized; architecture
  CTest passed 18/18)
- M0120 admission, recovery, and quarantine requirements ->
  agent/plan/M0120-vpci-lifecycle/work/W0121-lifecycle-model.md (record validator
  passed for 13 sessions, 105 events, and 148 Markdown files)
- Skill package and catalog integrity requirements -> tools/check-agent-records.py
  (validator self-test passed 52 cases)
- Structured bilingual routing corpus -> agent/skills/trigger-evals.json
  (11 domain skills and 68 cases passed routing validation)
- Static corpus and digest-bound observation validation ->
  tools/check-skill-routing.py (routing self-test passed 23/23)
- Routing validators are wired into CTest -> tests/CMakeLists.txt (development
  preset CTest passed 18/18)

### dark roasts

- none.

## session-only

- none.

## Unresolved items

- The 68 routing prompts still need three captured repetitions on each model/
  host combination that is to be qualified; use the generated observation
  template and archive its digest-bound result.
- W0102, M0110, M0120, and M0130 remain queued implementation work. This
  session improved architecture and activation guidance only; it did not create
  release evidence.

## Handoff

Run `python3 -B tools/check-agent-records.py .`, then use the smallest matching
expert set from `agent/skills/README.md`. For runtime or lifecycle work, read
W0102 plus `runtime-contracts-registry` before changing a shared schema or
publication state machine. For behavioral routing qualification, emit and fill
the three-repetition observation template documented in `trigger-evals.md`.
