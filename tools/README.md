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
credential redaction, safe references, and relative Markdown links.

```sh
python3 tools/check-agent-records.py .
```

The same command runs as the independent Nix check
`checks.x86_64-linux.agent-records`. Agent records are deliberately excluded
from the runtime, provider, daemon, and toolchain package source sets.

## Component dependency graph

`check-component-graph.py` (D0011) validates the component graph that
`cmake/MetaFluxComponentGraph.cmake` writes into every configured build tree.
It fails on any dependency edge outside the role whitelist, any link from a C
component to a CXX component, or any client-side link into the daemon, so the
application-side closure cannot silently grow a C++ runtime.

```sh
python3 tools/check-component-graph.py build/dev/metaflux-component-graph.json
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
