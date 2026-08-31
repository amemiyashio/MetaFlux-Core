# Notes

The migration targets the incentive failure identified by the read-only audit:
local testability and any unrelated in-progress session currently satisfy the
mechanical gates, while `progress/current.md` exposes many competing next-work
items. Preserve checkpoint evidence and terminal sessions. The new policy must
distinguish the single default product focus from bounded repository-governance
work so the governance mechanism can be changed without pretending it is a
product Exit Gate.

Implemented decisions:

- A new session scaffold is evidence/cleanup scope only; it does not claim
  execution focus.
- Ordinary content requires the exact candidate focus owner and explicit
  `METAFLUX_SESSION_ID`.
- A non-owner may terminally close exactly itself in a record-only commit.
- Focus transfer is record-only, declared by the old owner, and closes that
  owner while installing one in-progress successor.
- Local fixture availability and downstream partial evidence do not override
  milestone dependencies or the focused Exit Gate.

Rejected loopholes included unstaged owner records, unrelated active sessions,
missing owner declarations, nested `in_progress` strings, product/skill content
piggybacked on close, handoff without old-owner closure, malformed Exit Gate
anchors, and cumulative next-action archives.

Historical blob lock at close, in SC0006 inventory order:
`6ed78aeee9756a967bce40958e145bb93b5c1980`,
`b0cef09667429cddd95d85c3fabb65294cd82bb5`,
`2557ed2756360b14526a3d6af9e64dcfbc5ce51e`,
`b2e5e6225abf9d5fcba540414044e5e4e7552fae`,
`2df92427ee4b56236112327342c3bba899c3c766`,
`1fd88feb1ddf6c132da6f33c6c9840dae4f1a22c`,
`7963125724e4c87a4342c615ea4e12783e440f70`,
`df14c344f2b657c44bc0f90be68a16e26494783c`, and
`2b2c13da8b4423d57baa090dd980a0a9f57057f9`.
