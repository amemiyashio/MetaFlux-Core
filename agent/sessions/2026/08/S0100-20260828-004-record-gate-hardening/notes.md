# Notes

## Why two grades of consistency

- Current-progress freshness is binary: the newest checkpoint either is or is
  not the one referenced, there is no legitimate reason for the reference to
  lag, and the fix is mechanical. Error-grade.
- Session-versus-plan status drift is genuinely ambiguous: a plan may have
  advanced after the last complete session (the movement itself should be
  recorded in a newer session, but until then the warning is correct, not
  fatal), and rewriting old sessions to match current plans would destroy the
  historical evidence the sessions exist to hold. Warning-grade, and only the
  newest complete session is compared.

## Self-test design notes

- The suite builds a fresh synthetic tree per case in a temporary directory
  rather than shipping static fixture directories: one base dict, one mutation
  per case, no fixture sprawl, and the base itself is asserted valid first.
- The validator is loaded by path via importlib because its filename contains
  a hyphen; constructing `Validator(root)` directly allows asserting on the
  `errors` and `warnings` lists instead of parsing stderr.
- The suite's one development bug is instructive: a mutation key was written
  as a table row instead of a file path, so the missing-index-row case
  silently tested an unmutated tree and passed vacuously. Vacuous passes are
  the failure mode of golden-tree suites; every case's expectation was
  re-checked against the validator output line by line after the fix.

## Scaffold dogfood

This session was created by `tools/new-session.py` after its index-row
insertion fix; the row landed at the end of the table (line 21), confirming
the fix under real use.
