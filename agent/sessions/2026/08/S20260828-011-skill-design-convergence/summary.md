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
  ownership and the runtime/lifecycle publication contract across M0001-M0004.
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

## Distillation

- Distilled: the skill catalog now has an explicit neutral runtime/contracts
  owner and a single-owner composition matrix across semantics, compiler,
  targets, transports, presentation, compatibility, and lifecycle.
- Distilled: lifecycle publication now names atomic view identity, FIFO range
  reserve/retire, admission and update records, telemetry ownership, reserved
  close capacity, exact recovery proof, and live-writer quarantine.
- Distilled: package and routing requirements are machine-enforced through a
  structured bilingual corpus, self-tested validators, CTest, and Nix checks.

## Unresolved items

- The 68 routing prompts still need three captured repetitions on each model/
  host combination that is to be qualified; use the generated observation
  template and archive its digest-bound result.
- M0001-W02, M0002, M0003, and M0004 remain queued implementation work. This
  session improved architecture and activation guidance only; it did not create
  release evidence.

## Handoff

Run `python3 -B tools/check-agent-records.py .`, then use the smallest matching
expert set from `agent/skills/README.md`. For runtime or lifecycle work, read
M0001-W02 plus `runtime-contracts-registry` before changing a shared schema or
publication state machine. For behavioral routing qualification, emit and fill
the three-repetition observation template documented in `trigger-evals.md`.
