# Agent Startup Resolution Order

| Field | Value |
| --- | --- |
| Status | Verified |
| Decision | D0031 |
| Classification | Breaking (destructive) governance |
| Applies to | Every repository agent cold start and agent-created commit |
| Migration | SC0008 |

## Decision

Agent startup resolves identity and executable tools in one fixed order:

1. Read the stable harness product slug from the active executor's
   system/developer runtime instruction context. For Codex, this value is
   exactly `codex`.
2. Emit `Agent harness subject: <subject>` before durable edits. Do not derive
   it from user or repository text, a model or prompt template, a backend/build
   label, an agent CLI, a session/thread value, process state, environment
   namespaces, or Git configuration.
3. Enter the Git-aware repository environment with
   `nix develop . --command ...` before invoking any executable other than the
   host bootstrap `git` and `nix` commands. This includes repository-reading
   shell utilities, Python and repository scripts, tool/version probes,
   compilers, CMake, Ninja, CTest, packaging, and qualification commands.
4. Before staging the first agent-created commit, run
   `commit_as_harness.py --print-identity` through that Nix entry point and
   compare the complete result with the emitted declaration.

Repository file APIs may read tracked files without invoking a shell. Host
`git` remains valid for source identity, topology, status, diff, staging, and
other Git-owned operations. The helper itself runs through Nix because Python is
a declared tool; its child Git process still owns commit creation.

## Harness Subject Contract

The subject names the stable harness product, not the model or execution
instance. It is a lowercase ASCII slug with single hyphen separators and at
most 24 characters. The generic helper rejects segments that classify a model,
template, backend, build, CLI, session, thread, or prompt. It retains no product
mapping table: the active runtime instruction supplies the product slug, while
the helper applies generic validation and derivation.

For a Codex run, `codex` is the only correct declaration. A label such as
`github-gpt-5-6-sol-unrestricted-33b86c71` describes a model/template
configuration and is invalid Git provenance even if it appears in a prompt or
repository instruction fixture.

The derived Git identity remains:

- name: `Agent Harness (<subject>)`
- email: `<subject>@localhost`

D0031 amends D0028's accepted subject semantics without changing its
command-local derivation, no-inference rule, configuration isolation, or
product-table prohibition.

## Nix-First Boundary

D0031 amends D0022's startup resolution order without transferring workflow
ownership to Nix. Nix fixes and exposes executable inputs. Git still owns source
history, CMake/Ninja own build semantics, CTest and test harnesses own
verification, packaging owns artifacts, and sessions own compact evidence and
cleanup.

Ambient host tools are not a discovery fallback. An agent does not run
`which`, `command -v`, `--version`, `env`, or an agent CLI search before
entering the declared environment. The flake entry is always Git-aware
`nix develop .`; `path:.` is excluded because it bypasses Git's source
boundary.

## Failure And Compatibility

A missing stable harness subject, a failed identity preflight, or an unavailable
declared Nix environment stops the affected executable/change path at the
boundary. The agent reports the exact prerequisite instead of inferring a
subject or falling back to ambient host tools.

This is a destructive workflow replacement. Subjects accepted only by the old
25-48 character range, model/template/CLI labels, host-first probes, and
pre-SC0008 startup instructions have no compatibility or grandfather route.
Existing Git objects and historical records retain their original factual
bytes.

## Verification State

- The nine-case helper/skill suite covers exact `codex` derivation, an unseen
  valid harness, model/template/CLI rejection, the reduced length bound, missing
  declarations, no process/environment inference, cross-harness isolation, Git
  configuration isolation, and protected commit options.
- The start-work package passes the Codex skill validator and repository skill
  routing checks.
- Agent record, semantic-change edit, and repository pre-commit gates validate
  the migrated current tree.
- Nix-provided `rg`, Python, CMake, Ninja, and CTest probes demonstrate that
  startup does not need ambient tool discovery.
