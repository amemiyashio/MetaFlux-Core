# Tools

This directory contains developer and operator command-line tools, manifest and
ABI generators, inspection utilities, and benchmark drivers. Build-time Python
is permitted for generators but must not become a runtime dependency of shipped
providers or the client fast path.

Tools consume public contracts or documented service interfaces. They do not
become an alternate application-facing MetaFlux API.

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
freshness; exact skill catalog rows; domain section order; and the repository's
restricted `agents/openai.yaml` interface and invocation-policy schema.

```sh
python3 tools/check-agent-records.py .
python3 tools/check-agent-records.py . --cached
```

The ordinary command validates the checkout; `--cached` materializes and checks
the exact Git index tree. CTest uses the checkout mode and the pre-commit hook
uses the staged mode. Nix only provides the fixed Python tool used to execute
it. The hook also parses each candidate-index `session.json` and derives
coverage from its top-level status rather than the working tree or a nested
status string: every non-empty durable commit requires an `in_progress` session
in the candidate tree. The hook materializes that tree and executes its staged
semantic-change gate, record validator, and validator self-test, so partially
staged gate edits cannot validate different code. The narrow final-close
exception accepts only session, progress, checkpoint, and semantic-change
records whose staged `session.json` changes a session from `in_progress` in
`HEAD` to `complete`, `blocked`, or `abandoned`; it never authorizes product,
plan, memory, template, or skill content in the closing commit. This lets the
first session scaffold establish coverage and lets the last record commit close
it without leaving a synthetic activity record behind.

`check-semantic-change-edits.py` is the staged-diff hard gate for D0025. It
reads only `Active` SC permits already committed to `HEAD`, requires their bound
migration session to remain in progress, and rejects unlisted edits to recorded
checkpoints or terminal-session files. New checkpoints and in-progress sessions
remain ordinary record writes. Its focused regression suite is:

```sh
python3 tools/test-semantic-change-edits.py
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
python3 -B tools/check-skill-routing.py .
python3 -B tools/check-skill-routing.py . --emit-template --repetitions 3
python3 -B tools/check-skill-routing.py . --observed ROUTING_RESULTS.json
python3 -B tools/test-check-skill-routing.py
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
python3 tools/check-component-graph.py \
  ../.metaflux-build/MetaFlux-Core/dev/metaflux-component-graph.json
```

The same check runs as the CTest `metaflux.architecture.component-graph` in
every preset that enables tests.

## Session scaffolding

`new-session.py` accepts an explicit four-part delivery scope, allocates the
next `S<delivery>-YYYYMMDD-NNN` id for today, creates the session directory with
a validator-clean skeleton, and appends the index row to
`agent/sessions/README.md` so the index-completeness rule stays green:

```sh
python3 tools/new-session.py 0.1.0.1 my-session-slug
```

The dotted `delivery` value is authoritative; its compact body is derived by
concatenating the four decimal components. Fill the TODO fields as the session
progresses. New sessions have
`status: in_progress` and `ended_at: null`; closing the session records the end
date and a terminal status. The skeleton passes `check-agent-records.py`
immediately after creation.

## Validator self-test

`test-check-agent-records.py` pins the validator itself against a synthetic
golden tree. Its cases cover required session fields, lifecycle timestamps,
event sequencing, guidance disposition and terminal cleanup, roast depth,
single-owner resolution and session-only structure, index completeness,
decision identity and references,
skill catalog/interface/policy rules, staleness and status drift, Markdown
links, checkpoint identity, and current-progress freshness. It builds fixtures
in a temporary directory and loads the validator by path without writing
bytecode.

```sh
python3 tools/test-check-agent-records.py
```

The same suite runs as the CTest `metaflux.architecture.agent-records-selftest`
in every preset that enables tests, so the record gate has its own gate.
