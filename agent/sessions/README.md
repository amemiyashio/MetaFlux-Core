# MetaFlux Project Work Sessions

This directory contains repository-local evidence for work performed on
`MetaFlux-Core`. A session may record a project objective, technical decision,
tool call, tool result, work note, changed paths, and verification evidence.

It does not archive conversations, user profiles, personal preferences, or work
from another project. Every event must materially relate to this repository.

Sessions use the date hierarchy
`sessions/YYYY/MM/SYYYYMMDD-NNN-slug/`. The directory contains `session.json`,
`events.jsonl`, `summary.md`, `notes.md`, and an `outputs/` directory. IDs and
event sequence numbers are immutable once published.

| Session | Date | Fidelity | Status | Summary |
| --- | --- | --- | --- | --- |
| [S20260827-001-metaflux-bootstrap](2026/08/S20260827-001-metaflux-bootstrap/summary.md) | 2026-08-27 | Reconstructed | Complete | MetaFlux planning, bootstrap, architecture review, and build hardening |

## Fidelity and retention

Future project sessions use the same schema. Allowed event types are `objective`,
`decision`, `tool_call`, `tool_result`, and `work_note`. Set `fidelity` to
`exact` when project events are captured as they occur, or `reconstructed` when
an older project state is rebuilt from repository evidence. Reconstruction gaps
use a `work_note` with `omitted: true` and a concrete `reason`.

Inline `content` is UTF-8 and limited to 65,536 bytes. Larger project command
output is stored as numbered `outputs/NNNN.txt` files and referenced by relative
path, exact byte count, and SHA-256. `outputs/README.md` is only an index and is
never an event output. Output paths may not traverse directories or use symlinks.

Secrets, credentials, and unrelated environment data are excluded entirely.
Do not preserve even a redacted credential-bearing command; record a sanitized
project action or reference a safe repository script instead.

Run the record validator from the repository root:

```sh
python3 tools/check-agent-records.py .
```
