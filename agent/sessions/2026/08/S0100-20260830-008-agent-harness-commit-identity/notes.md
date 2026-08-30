# Notes

At capture time the helper used `Codex <codex@localhost>` and `Claude Code
<claude-code@localhost>` as static identities. Revision `f862852` later added
`ZCode <zcode@localhost>`, exposing the need for another product row. D0028 and
SC0004 supersede those identities as future derivation rules while retaining
them as exact historical commit evidence. The command-local `GIT_AUTHOR_*` and
`GIT_COMMITTER_*` isolation remains current and repository/global Git
configuration is still not modified.

The helper rejects author overrides, amend, and authorship-reusing message
options, including Git long-option abbreviations and clustered `-C`/`-c` forms
such as `-qCHEAD`. Its conservative short-token check can also reject an
attached safe argument containing `c` or `C`; use separated arguments such as
`-m "Subject"`, which is the documented interface.

This is a workflow rule, not a claim that direct `git commit` is rejected by a
hook. The hook cannot reliably infer a human-versus-agent initiator before the
commit object exists. The helper supplies deterministic identity per commit,
and the post-commit `git show` check proves the recorded result.
