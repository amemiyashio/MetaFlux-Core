# Notes

## Why each rule is shaped the way it is

- **Index completeness** compares the set of ids extracted from README link
  targets against the directory sets, both directions, so a stale row
  referencing a deleted record fails too. Unanchored companion regexes exist
  because the stable-id patterns are anchored (`^...$`) and link targets carry
  directory prefixes; this was discovered by the self-test, not by review.
- **Ledger counts, not text matching**: plan decision wording drifts, so the
  validator compares per-milestone counts of numbered Decisions-to-Close items
  against ledger rows whose first column is the milestone id. This catches
  both forgotten rows and stale rows after a closure, without brittle text
  matching.
- **Distillation grandfathering**: sessions before 2026-08-28 are exempt
  (`DISTILLATION_REQUIRED_FROM`);
  S0100-20260827-001-metaflux-bootstrap is reconstructed history and remains
  protected evidence. Ordinary corrections append; only a committed Active SC
  with an exact `Historical + Pending` row under D0025 may synchronize semantic
  wording, while raw facts stay locked.
- **Staleness is a warning, not an error**: a legitimately Active workstream
  can sit untouched while an unrelated checkpoint lands; failing the build for
  that would teach people to game the date field.

## Self-test incident

During the negative tests, a `sed '0,/pattern/d'` command deleted a range
instead of one line and a `git stash` intended to restore a file briefly
stashed the entire uncommitted working tree. The stash was popped immediately
and the damaged ledger file was rewritten in full. The lesson is recorded
here because it happened inside the record system this session hardens:
prefer surgical edits and tracked-file restores over ad-hoc sed/stash
combinations when mutating records under validation.

## Dogfooding result

The scaffolder's most important property is that its output passes the
validator at creation time, including the index row it appends — otherwise
scaffolding a session would break the index-completeness rule until manual
bookkeeping caught up. Verified: skeleton green before any fill-in.
