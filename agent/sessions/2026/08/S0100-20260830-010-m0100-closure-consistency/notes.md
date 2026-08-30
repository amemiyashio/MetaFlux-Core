# Notes

- Lock the original checkpoint contents, terminal-session counts, event
  metadata, product evidence revisions, and all existing Git identities.
- Treat `9d770d4` and `1ac597b` as retained failed-route evidence: their
  `Agent Harness (zcode)` commit identities are factual, but their protected
  edits had no Active SC.
- The user confirmed that `f808d30`, `be9a421`, `0281c8f`, `5118f4f`, and
  `350c0ad` intended the `zcode` harness subject; Git factually records
  `amamiya <amamiya@localhost>` for both roles.
- Initial SC0005 authorization evidence contained a forbidden placeholder and
  caused a HEAD-based gate deadlock. Commit `8543fd3` used the isolated
  `--no-verify` recovery route to change only that Active SC evidence cell; no
  protected history was edited in the recovery commit.
- Independent review identified candidate-code partial staging and broad close
  piggyback gaps. Revision `4d1ff2b` closes candidate-code and content-category
  bypasses; `9586b45` closes the residual unknown-descendant route. Both have
  explicit regressions.
