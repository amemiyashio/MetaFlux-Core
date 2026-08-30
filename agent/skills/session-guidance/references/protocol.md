# Session Guidance Protocol

## Purpose And Ownership

A guidance packet lets a specialist point the owner of an active session toward
a better route without taking over that owner's task. It is deliberately
session-local and disposable. The owning domain skill controls technical
semantics; source and verified tests control current implementation truth;
canonical records control durable policy.

Use ordinary collaborator tasks for delegated implementation. Use this protocol
when the payload is direction, critique, evidence, or a candidate approach that
the receiving agent must independently assess.

## Location And States

Packets live only under the target session:

```text
agent/sessions/YYYY/MM/S<delivery>-YYYYMMDD-NNN-slug/guidance/
  G001-topic.draft.md
  G002-other-topic.ready.md
  G003-third-topic.processing.md
  G003-third-topic.patch
```

`GNNN` is allocated within one session. The filename is the sole state source;
packet frontmatter intentionally has no duplicate state field.

```text
draft --publish--> ready --claim--> processing --resolve--> removed
```

`publish` and `claim` use same-directory atomic renames while holding a lock on
the target `session.json`. The script rejects duplicate IDs and destination
collisions. A no-material discard may remove a packet from any state. Because it
creates no durable reference, its ID may be reused after every trace of that
packet is removed; IDs referenced by session events remain reserved.

`create` first writes and `fsync`s hidden same-directory files named
`.GNNN-slug.draft.md.tmp` and, when needed, `.GNNN-slug.patch.tmp`. It publishes
the complete patch first and the complete draft last by atomic rename, with a
directory `fsync` at each boundary. Hidden temporary files and a final patch
without its draft are interruption residue, never consumable guidance states.

The target session must have `status: in_progress` and `ended_at: null` for
every operation, including listing and cleanup. Resolve all packets before the
session becomes terminal.

## Packet Contract

The Markdown frontmatter contains:

| Field | Meaning |
| --- | --- |
| `schema_version` | Packet schema; currently `1` |
| `guidance_id` | Session-local `GNNN`, matching the filename |
| `target_session` | Exact target session ID |
| `author` | Collaborator or agent identifier |
| `expert_role` | Domain perspective supplying the direction |
| `created_at` | ISO 8601 timestamp with an explicit UTC offset |
| `scope` | Bounded area the direction may affect |
| `supersedes` | Earlier guidance IDs explicitly replaced by this packet |
| `candidate_patch` | Exact sibling `.patch` filename or `null` |

A publishable packet has non-empty `Direction`, `Evidence`, `Constraints`, and
`Expected Verification` sections. `Candidate Patch` is optional and advisory.
The template is [guidance-packet.md](../assets/guidance-packet.md).

A candidate patch is named `GNNN-slug.patch`. The script copies it at creation,
checks that the metadata names the exact sibling, never applies it, and deletes
it with the packet.

Do not place credentials, raw command transcripts, build trees, source copies,
or unrelated attachments in `guidance/`.

## Command Reference

The helper is stdlib-only:

```sh
python3 agent/skills/session-guidance/scripts/guidance.py --help
```

`SESSION` may be a session ID or its directory path. `--repo ROOT` overrides
repository discovery and is primarily for isolated tests.

Create a draft:

```sh
python3 agent/skills/session-guidance/scripts/guidance.py create \
  --session SESSION --slug cache-publication --author A004 \
  --role "compiler cache reviewer" --scope "W0103 cache publication" \
  --supersedes G001 --patch /tmp/candidate.patch
```

The four body options (`--direction`, `--evidence`, `--constraints`, and
`--verification`) may populate a complete draft non-interactively. When they
are omitted, edit the scaffold before publishing.

If `create` reports an interrupted-create residue, inspect the exact files and
remove only that identity:

```sh
python3 agent/skills/session-guidance/scripts/guidance.py recover \
  --session SESSION --guidance-id G003 --slug cache-publication \
  --reason "interrupted before draft publication"
```

`recover` is intentionally narrow. It accepts only one active-session `GNNN`
and slug, rejects symlinks, published/claimed packets, identity conflicts, and
any ID already referenced by a disposition event. It removes recognized temp
files, an orphan patch, or a structurally malformed draft. A structurally valid
draft must use ordinary `resolve --no-material` if it is genuinely disposable.

Publish, scan, and claim:

```sh
python3 agent/skills/session-guidance/scripts/guidance.py publish PATH.draft.md
python3 agent/skills/session-guidance/scripts/guidance.py list --session SESSION
python3 agent/skills/session-guidance/scripts/guidance.py list \
  --session SESSION --state all --json
python3 agent/skills/session-guidance/scripts/guidance.py claim PATH.ready.md
```

The default list shows only `ready` packets. Process packets in numeric order.
An explicit `supersedes` relationship wins over that order for the overlapping
scope.

## Material Disposition

After validating the advice, append one existing-schema `decision` or
`work_note` event to the target `events.jsonl`. Add these top-level extension
fields:

```json
{"schema_version":1,"seq":7,"timestamp":"2026-08-30","type":"work_note","actor":"A001","content":"Adapted the proposed cache key to retain the frozen compiler epoch.","guidance_id":"G003","disposition":"adapted"}
```

Allowed dispositions are:

- `adopted`: applied as directed after verification.
- `adapted`: useful direction was changed to fit current evidence or constraints.
- `rejected`: evaluated and declined with an evidence-backed reason.
- `deferred`: valid direction moved to an unresolved session item or an
  open-decision entry. The event must also carry a non-empty `deferred_to`.

Then resolve the processing packet:

```sh
python3 agent/skills/session-guidance/scripts/guidance.py resolve \
  PATH.processing.md --disposition adapted
```

Before deleting material guidance, resolution validates the complete event log:
required fields, session schema version, contiguous sequence, timestamps, event
types, actors, content/output references, omission metadata, and guidance-field
rules. `session.json`, its canonical `events.jsonl`, and any output reference
must be regular non-symlink files whose resolved paths remain inside the target
session. It then requires exactly one matching disposition. Any malformed event,
missing pair, duplicate disposition, invalid deferral, or escaping path leaves
the packet in place. Cleanup tolerates an already-removed declared patch so an
interrupted resolve can finish on retry; publish, list, and claim continue to
require it.

## No-Material Discard

Use this only when evaluating the packet changes no decision, verification
claim, durable result, or resume point. The reason is required at the command
boundary but is not added to the curated session ledger:

```sh
python3 agent/skills/session-guidance/scripts/guidance.py resolve \
  PATH.ready.md --no-material --reason "duplicate of current source evidence"
```

Advice with unresolved technical value is `deferred`, not no-material. Advice
that exposed a reusable failed route gets a concise material disposition before
cleanup. No-material discard validates the event log and refuses the operation
when any `adopted`, `adapted`, `rejected`, or `deferred` event already references
the guidance ID.

## Control-Boundary Loop

Scan published guidance when:

1. resuming an active session;
2. receiving a specialist handoff or completion notification;
3. entering the next coherent work unit;
4. preparing a stage checkpoint; or
5. preparing session closure.

Do not interrupt a running compiler, test, download, or other complete command.
At the next boundary, claim one packet, validate it, record any material
disposition, resolve it, and continue. This is a cooperative pull loop, not a
background watcher or a new source of task authority.
