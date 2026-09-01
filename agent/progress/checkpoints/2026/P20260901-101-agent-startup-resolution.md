---
id: P20260901-101
status: Recorded
captured: 2026-09-01
milestone: M0110
workstream: W0112
branch: main
git_revision: a7e370d68c660a1d42210631641bef0d3406cf91
workspace: applied destructive D0031 agent startup resolution
---

# Applied Destructive Agent Startup Resolution

## Outcome

SC0008 applies D0031 at revision `a7e370d`. Future sessions resolve the stable
harness product directly from system/developer runtime instruction context;
Codex declares exactly `codex`. Model, template, backend, build, CLI, session,
thread, and prompt labels are invalid provenance, and no agent CLI or ambient
process, environment, filesystem, or PATH search participates in identity.

Startup and tool acquisition are Nix-first. Git and Nix are the only host
bootstrap tools; every other repository executable and tool/version probe runs
through the Git-aware development environment. Missing tools enter the governed
Nix declaration first, with exact host prerequisites reported only when Nix
cannot provide or materialize them. Nix owns only tool-version identity,
materialization, and exposure. Fixed versions are clear, reproducible, and
stable for the current revision, while later `manage-toolchain` changes may
deliberately evolve manifests and locks.

The migration has no compatibility route. Product focus transfers to
`S0112-20260901-003-m0110-w0112-post-startup-governance`; the W0112 Exit Gate
is unchanged and remains open.

## Verification evidence

| Gate | Result |
| --- | --- |
| Harness helper and startup policy | Passed: 9/9; wrong model/template subject rejected and `codex` resolved exactly |
| Codex skill package | Passed the skill validator |
| Skill routing | Passed: 89 cases and 34/34 self-tests |
| Agent-record implementation | Passed: validator and 197 self-test cases |
| Semantic-change implementation | Passed: 23/23 edit self-tests |
| Nix-first tool probes | Passed: ripgrep 15.2.0, Python 3.13.15, CMake/CTest 4.1.6, and Ninja 1.13.2 |
| Content commits | Passed repository pre-commit at `7db7bcf`, `73b471f`, and `a7e370d` |

## Cleanup

- Removed: no product source or historical Git evidence.
- Retained: current D0031/SC0008 authority, P100 product evidence, and the
  current W0112 work boundary.

## roast

### light roasts

- none.

### medium roasts

- none.

### dark roasts

- Destructive stable-harness and Nix-first startup resolution ->
  `docs/architecture/agent-startup-resolution.md` (`a7e370d`; helper/skill
  9/9, routing 89 plus 34/34, record self-test 197, semantic edits 23/23;
  authority: D0031, SC0008)

## session-only

- none.

## Handoff

Continue as `S0112-20260901-003-m0110-w0112-post-startup-governance`. Read the
current focus, current progress, D0031/start-work, P100, and the W0112 Exit Gate
before resuming live device qualification.
