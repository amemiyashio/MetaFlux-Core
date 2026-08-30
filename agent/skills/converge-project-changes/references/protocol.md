# Change-Set And Conformance Protocol

Use this protocol to identify the candidate batch, decide who may repair it,
and apply project-level checks without creating a second authority source.

## Scope Precedence

Choose the first resolvable source below. A lower row never broadens a higher
row.

| Priority | Scope source | Committed interval | Working layers |
| --- | --- | --- | --- |
| 1 | User-named revision, patch, or exact paths | Exact user range | Only explicitly included paths |
| 2 | Current integration session plus an explicit collaborator handoff | Session base to delivered end | Delivered paths plus current-session corrections |
| 3 | Terminal collaborator session | `base_revision..final_revision` | Current working layers are ambient and unowned |
| 4 | No supplied scope | None (`HEAD..HEAD`) | Inventory only; ownership remains unknown |

`change_inventory.py --base REVISION` compares that ancestor to current `HEAD`.
`--session ID` uses the session's base and, for a terminal session, its final
revision. An active session ends at current `HEAD` for inventory only; its
status does not prove that every intervening commit or dirty path belongs to it.

Reject an invalid or non-ancestor base. For a divergent branch, the caller must
select and name the intended merge base instead of letting the tool guess.
The helper also refuses a stable result when Git collapses an untracked nested
repository into an opaque directory, or when an untracked entry is a special
file. It never skips content it cannot fingerprint. Resolve that path's scope
or repository disposition explicitly, then rerun the inventory.

## Ownership And Disposition

| Observed material | Integration authority | Required disposition |
| --- | --- | --- |
| Exact increment produced by the current session or its subagent | Current session | Repair compatible gaps and verify |
| Terminal session or commit range explicitly handed to the current session | Current session for follow-up commits | Repair forward; never rewrite terminal records or commits |
| Source or records still owned by another `in_progress` session | Foreign session owner | Publish evidence and expected verification through `session-guidance` |
| Pre-existing, ambiguous, or concurrent working material | Unknown | Preserve unchanged and report the boundary |
| Approved breaking replacement of established meaning | Decision plus semantic-migration owner | Compose `govern-semantic-change` and its exact D/SC authority |
| Unresolved public contract or irreversible tradeoff | Plan or decision owner | Keep open or use `close-decision` only when its closure evidence exists |

An author or harness label is provenance for a Git object, not proof that the
current reviewer owns its behavior or every adjacent working-tree hunk.

## Conformance Matrix

| Lane | Inspect | Canonical owner or gate |
| --- | --- | --- |
| Intent and scope | Latest user request, session objective, milestone/work item, exclusions, unrelated changes | Active session and approved plan |
| Authority | Source precedence, durable constraints, open decisions, duplicate truth, protected history | `agent/README.md`, memory indexes, D/SC workflow |
| Architecture | Directory ownership, language wall, dependency direction, new target registration | Repository layout, component graph, `add-component` |
| Contracts | ABI/schema/version ownership, generated projections, compatibility and failure behavior | `contracts/` plus matching domain skill |
| Implementation | Real behavior versus fixture/stub, error and recovery paths, cross-component integration | Source, focused tests, domain acceptance |
| Evidence | Commands, revision binding, maturity/release claims, missing harnesses, stale pass assertions | Current test output and named evidence owner |
| Agent workflow | Session state, guidance disposition, progress truth, skill routing, cleanup, commit identity | Agent validators and workflow skills |
| Tool and release boundaries | Nix scope, manifests, packaging ownership, glibc and supported-environment claims | `manage-toolchain`, packaging and release gates |

Load only the lanes touched by the change set. A project-level review does not
authorize redesign of an otherwise compliant domain implementation.

## Findings And Completion

Use three severities:

- `blocker`: ownership is unresolved, user scope or canonical authority is
  violated, protected history would be edited, a contract/architecture boundary
  is broken, or evidence is materially false.
- `required`: an integration-owned project rule, synchronized projection,
  regression test, record, or verification gate is missing or inconsistent.
- `advisory`: a bounded improvement with no violated rule. It may remain and
  cannot justify unrelated cleanup.

Each finding has exactly one disposition: `fixed`, `guidance`, `decision`,
`semantic-migration`, `deferred-owner`, or `accepted-risk`. `accepted-risk` is
valid only for advisory material or an explicitly authorized non-blocking risk.

The final review is temporary. Persist only a compact session event for material
repository changes, a transient guidance packet for a foreign active owner, or
the existing canonical decision/SC record when those workflows apply.
