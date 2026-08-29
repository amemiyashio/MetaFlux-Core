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
session cleanup and distillation; staleness and status-drift warnings; current-progress
freshness; exact skill catalog rows; domain section order; and the repository's
restricted `agents/openai.yaml` interface schema.

```sh
python3 tools/check-agent-records.py .
```

The same command runs from CTest and repository hooks. Nix only provides the
fixed Python tool used to execute it.

Domain skill metadata currently permits exactly the quoted `interface` fields
`display_name`, `short_description`, and `default_prompt`; the prompt must name
its exact `$skill-slug`. Adding icons, policy, or dependencies requires extending
the repository validator and adding the corresponding checked resources first.

## Skill routing

`check-skill-routing.py` validates the structured English/Chinese trigger corpus
and can score captured implicit-routing observations for an exact Codex
model/host/repetition tuple. The domain roster comes from the independent records
gate, coverage floors are tool policy rather than corpus-controlled values, and
duplicate locale/prompt pairs are rejected. Each observation is bound to both the
canonical corpus SHA-256 and a digest of the catalog plus every domain
`SKILL.md`/`agents/openai.yaml`; its product is exactly `Codex`:

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

`new-session.py` allocates the next `SYYYYMMDD-NNN` id for today, creates the
session directory with a validator-clean skeleton, and appends the index row
to `agent/sessions/README.md` so the index-completeness rule stays green:

```sh
python3 tools/new-session.py my-session-slug
```

Fill the TODO fields as the session progresses; the skeleton passes
`check-agent-records.py` immediately after creation.

## Validator self-test

`test-check-agent-records.py` pins the validator itself against a synthetic
golden tree. Its 52 cases cover required session fields, event sequencing,
distillation, index completeness, decision identity and references, skill
catalog/metadata rules, staleness and status drift, Markdown links, checkpoint
identity, and current-progress freshness. It builds fixtures in a temporary
directory and loads the validator by path without writing bytecode.

```sh
python3 tools/test-check-agent-records.py
```

The same suite runs as the CTest `metaflux.architecture.agent-records-selftest`
in every preset that enables tests, so the record gate has its own gate.
