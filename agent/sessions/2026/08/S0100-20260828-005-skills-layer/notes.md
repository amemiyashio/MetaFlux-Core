# Notes

## Boundary design

The hardest call was keeping skills distinct from experience. The working
line: an experience record makes a claim about the world that must be
reproduced before it is trusted (`Candidate` → `Validated`); a skill makes no
claim — it prescribes steps whose correctness the repository machinery
enforces when the skill's verification command passes. That is also why skills
carry no evidence section: their proof is the gate they name.

## Validation shape

- An absent `agent/skills/` directory remains valid: the layer is optional
  until used, unlike sessions which must exist.
- Directory-name hygiene is an error (not a warning) because slug names are
  link identity, like every other stable id in this repository.
- The self-test's vacuous-pass guard matters here: a naive missing-SKILL.md
  case would have exercised the nonexistent-skill rule instead, because the
  fixture writer only creates directories through files. The case keeps the
  directory physically present via a `.keep` placeholder so the intended rule
  fires.

## Authoring errors caught by the gates

- Two `SKILL.md` links used four levels up where three reach the repository
  root — `agent/skills/<slug>/` is one level shallower than a session
  directory. This is now the fourth time the link checker has caught a depth
  mistake; the lesson for this repository's authors is to count from the file,
  not from habit.
- The skills README linked `../templates/README.md`, which had never existed
  since the bootstrap. Choosing to write the template index rather than
  de-link keeps every agent/ directory indexed uniformly.

## Future seeds

Candidates for the next skills, written when their procedures stabilize:
`run-qualification-matrix` (full preset + nix flake check evidence pattern),
`close-milestone` (DoD audit), and `promote-budgets` (provisional → binding
with archived baselines). None is written yet: a skill should encode a
procedure already practiced, not prescribe one still being invented.
