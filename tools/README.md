# Tools

This directory contains developer and operator command-line tools, manifest and
ABI generators, inspection utilities, and benchmark drivers. Build-time Python
is permitted for generators but must not become a runtime dependency of shipped
providers or the client fast path.

Tools consume public contracts or documented service interfaces. They do not
become an alternate application-facing MetaFlux API.

## Command resolution

D0031 makes repository command resolution Nix-first. Invoke every tool in this
directory through the Git-aware `nix develop . --command ...` environment;
do not probe ambient PATH, Python, or tool versions first. Host Git and Nix are
the only executable bootstrap exceptions. This selects declared executables
without transferring each tool's behavior or evidence ownership to Nix.

## Agent records

`check-agent-records.py` validates the repository-local collaboration records:
session metadata and JSONL events, stable record IDs, numbered output hashes,
credential redaction, safe references, and relative Markdown links, plus the
machine-enforced record-loop rules — index completeness for sessions, plans,
experience, decisions, and skills; plan/ledger open-decision identity; mandatory
session cleanup plus the ordered roast, one resolvable owner per promoted claim,
and independent session-only contract;
structured guidance dispositions and terminal guidance cleanup; staleness and
milestone mapping for terminal guidance dispositions; terminal-note cleanup;
resolvable full session references; status-drift warnings; current-progress
freshness; the schema version 2 D0029 execution-governance epoch, owner
lifecycle, product dependencies, canonical Exit Gate, governance authority,
legacy-owner rejection, compact current projection, and the destructive epoch
liquidation tombstone; exact skill catalog rows; domain section order; and the
repository's restricted `agents/openai.yaml` interface and invocation-policy
schema. Once `agent/sessions/liquidated-v1.json` exists, schema version 1
session directories, live/tombstoned ID overlap, session-detail knowledge
owners, and narrative compatibility fields are hard errors.

```sh
nix develop . --command python3 tools/check-agent-records.py .
nix develop . --command python3 tools/check-agent-records.py . --cached
```

The ordinary command validates the checkout; `--cached` materializes and checks
the exact Git index tree. CTest uses the checkout mode and the pre-commit hook
uses the staged mode. Nix only provides the fixed Python tool used to execute
it. The hook materializes that tree and executes its staged semantic-change
gate, record validator, and validator self-test, so partially staged gate edits
cannot validate different code.

Every non-empty durable commit declares `METAFLUX_SESSION_ID`. For ordinary
content or records, that value must equal the candidate
`agent/progress/focus.json.owner_session`, and the exact session must remain
top-level `in_progress` with `ended_at: null`; focus and owner must both use
schema version 2 and declare `governance_epoch: D0029`. Creating another session
does not supply coverage, and a legacy session cannot become a fallback owner. A
focus metadata update with the same owner is record-only. A focus handoff is
also record-only, is declared by the `HEAD` owner, terminally closes exactly
that owner, and installs one resolvable current-epoch in-progress candidate
owner. A current-epoch non-owner may only terminally close its exact ledger in
a record-only commit; liquidated and other pre-epoch IDs have no such path.
Neither close nor handoff authorizes product, plan, memory, template, or skill
content.

Session records are limited to `session.json`, `events.jsonl`, `summary.md`,
`notes.md`, valid `outputs/NNNN.txt`, and staged guidance deletions. Checkpoints
must use the canonical `PYYYYMMDD-NNN-slug.md` shape; unknown descendants are
not closing records.

`check-semantic-change-edits.py` is the staged-diff hard gate for D0025. It
reads only `Active` SC permits already committed to `HEAD`, requires their bound
migration session to remain in progress, and rejects unlisted edits to recorded
checkpoints, the committed liquidation tombstone, or terminal-session files.
New checkpoints and in-progress sessions remain ordinary record writes. Its
focused regression suite is:

```sh
nix develop . --command python3 tools/test-semantic-change-edits.py
```

Routed domain and workflow skills require `agents/openai.yaml`. Metadata permits
the quoted `interface` fields `display_name`, `short_description`, and
`default_prompt`, plus the optional `policy.allow_implicit_invocation` Boolean.
The prompt contains only its exact `$skill-slug` once. `$roast` additionally
requires `allow_implicit_invocation: false`, so ordinary uses of the word do not
load the project-knowledge workflow. Adding icons or dependencies requires
extending the repository validator and adding the corresponding checked
resources first.

## Skill routing

`check-skill-routing.py` validates the structured English/Chinese trigger corpus
and can score captured implicit-routing observations for an exact Codex
model/host/repetition tuple. The domain roster comes from the independent records
gate, as does the separate checked workflow roster. Coverage floors are tool
policy rather than corpus-controlled values, and duplicate locale/prompt pairs
are rejected. Each observation is bound to both the
canonical corpus SHA-256 and a digest of the catalog plus every routed
domain/workflow `SKILL.md` and `agents/openai.yaml`; its product is exactly
`Codex`:

```sh
nix develop . --command python3 -B tools/check-skill-routing.py .
nix develop . --command python3 -B tools/check-skill-routing.py . --emit-template --repetitions 3
nix develop . --command python3 -B tools/check-skill-routing.py . --observed ROUTING_RESULTS.json
nix develop . --command python3 -B tools/test-check-skill-routing.py
```

The corpus and its self-test run in CTest. CI does
not invoke a remote model: a static pass proves corpus integrity, while only a
filled observation file proves routing behavior for its recorded environment.

## Component dependency graph

`check-component-graph.py` (D0011) validates the component graph that
`cmake/MetaFluxComponentGraph.cmake` writes into every configured build tree.
It fails on any dependency edge outside the role whitelist, any link from a C
component to a CXX component, or any client-side link into the daemon, so the
application-side closure cannot silently grow a C++ runtime.

```sh
nix develop . --command python3 tools/check-component-graph.py \
  ../.metaflux-build/MetaFlux-Core/dev/metaflux-component-graph.json
```

The same check runs as the CTest `metaflux.architecture.component-graph` in
every preset that enables tests.

## Lifecycle model

`check-lifecycle-model.py` validates the M0120 lifecycle extension's one-way
base-manifest import, model and bounds hashes, then exhaustively explores the
bounded generation/epoch state machine. It emits only a compact machine-readable
evidence record containing input hashes, traversal counts, invariant results,
and counterexamples. The same record includes a bounded loss-fence and
two-bank telemetry race exploration with reader retry/final-fence checks; the
output belongs in the external build evidence tree.

```sh
nix develop . --command python3 tools/check-lifecycle-model.py \
  --base-manifest contracts/protocol/transport/v1/schema/manifest.json \
  --manifest contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/manifest.json \
  --model contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/model.json \
  --bounds tests/lifecycle/model-bounds.json \
  --output ../.metaflux-evidence/MetaFlux-Core/lifecycle/model-check.json
```

## Session scaffolding

`new-session.py` accepts an explicit four-part delivery scope, allocates the
next `S<delivery>-YYYYMMDD-NNN` id for today, creates the session directory with
a validator-clean skeleton, and appends the index row to
`agent/sessions/README.md` so the index-completeness rule stays green:

```sh
nix develop . --command python3 tools/new-session.py 0.1.0.1 my-session-slug
```

The dotted `delivery` value is authoritative; its compact body is derived by
concatenating the four decimal components. Fill the TODO fields as the session
progresses. New sessions have
`status: in_progress` and `ended_at: null`; closing the session records the end
date and a terminal status. Every scaffold uses schema version 2 and
`governance_epoch: D0029`; no old session can be upgraded or reused. The
skeleton passes `check-agent-records.py` immediately after creation. The command
reports the existing focus owner when one resolves and always states that
scaffolding does not claim execution focus; the current owner must perform a
record-only handoff before the new session can commit content.

## Validator self-test

`test-check-agent-records.py` pins the validator itself against a synthetic
golden tree. Its cases cover required session fields, lifecycle timestamps,
event sequencing, guidance disposition and terminal cleanup, roast depth,
single-owner resolution and session-only structure, index completeness,
decision identity and references,
skill catalog/interface/policy rules, staleness and status drift, Markdown
links, checkpoint identity, execution-focus product/governance modes, dependency
and Exit Gate failures, current projection bounds, exact owner commits,
legacy and non-owner rejection, atomic handoffs, strict liquidation tombstones,
schema version 1 coexistence rejection, and settled-reference resolution. It
builds fixtures in a temporary directory and loads the validator by path
without writing bytecode.

```sh
nix develop . --command python3 tools/test-check-agent-records.py
```

The same suite runs as the CTest `metaflux.architecture.agent-records-selftest`
in every preset that enables tests, so the record gate has its own gate.
