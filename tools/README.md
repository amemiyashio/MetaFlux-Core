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
