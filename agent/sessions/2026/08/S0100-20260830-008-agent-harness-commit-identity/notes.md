# Notes

The stable identities are `Codex <codex@localhost>` and `Claude Code
<claude-code@localhost>`. The helper sets all four `GIT_AUTHOR_*` and
`GIT_COMMITTER_*` variables only for its `git commit` child process, so inherited
agent variables are overwritten and repository/global Git configuration is not
modified.

The helper rejects author overrides, amend, and authorship-reusing message
options, including Git long-option abbreviations and clustered `-C`/`-c` forms
such as `-qCHEAD`. Its conservative short-token check can also reject an
attached safe argument containing `c` or `C`; use separated arguments such as
`-m "Subject"`, which is the documented interface.

This is a workflow rule, not a claim that direct `git commit` is rejected by a
hook. The hook cannot reliably infer a human-versus-agent initiator before the
commit object exists. The helper supplies deterministic identity per commit,
and the post-commit `git show` check proves the recorded result.
