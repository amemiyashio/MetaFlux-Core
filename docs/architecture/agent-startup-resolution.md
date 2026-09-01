# Agent Startup Resolution Order

| Field | Value |
| --- | --- |
| Status | Verified |
| Decision | D0031 |
| Classification | Breaking (destructive) governance |
| Applies to | Every repository agent cold start and agent-created commit |
| Migration | SC0008 |
| Amended by | D0032 host privilege escalation |

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

This ownership limit is strict. Nix declarations contain tool versions, source
identities, inputs, patches, hashes, and shell exposure only. They do not encode
task routing, Git operations, project configure/build/test/package commands,
qualification meaning, focus/session policy, evidence retention, cleanup, or
host installation. `nix develop . --command TOOL ...` supplies the
environment; `TOOL` and its owning workflow define the operation.

"Fixed" means that the current repository revision has a clear, reproducible,
stable tool identity. It does not mean permanent immutability. A later
`manage-toolchain` change may update the canonical manifest and lock together,
then revalidate affected consumers.

Ambient host tools are not a discovery fallback. An agent does not run
`which`, `command -v`, `--version`, `env`, or an agent CLI search before
entering the declared environment. The flake entry is always Git-aware
`nix develop .`; `path:.` is excluded because it bypasses Git's source
boundary.

## Tool Acquisition Order

When a workflow needs an executable absent from the declared shell, the first
action is to add the narrow tool package to the repository Nix declaration and
verify it through the Git-aware flake. Absence from the current shell does not
authorize an ambient host lookup or moving that tool's workflow into Nix.

If `manage-toolchain` proves Nix cannot provide or materialize the tool,
`manage-host-privilege` governs the next step. An exact pacman-resolvable
package may be installed through the bounded root-owned helper, and its intended
absolute executable may then run from inside the Nix entry environment. That
host copy is local prerequisite state, not declared repeatable tool identity or
release evidence. Every sudo/su or privileged MetaFlux driver operation routes
to that skill; start-work does not own its actions, authorization, credentials,
or revocation.

## Failure And Compatibility

A missing stable harness subject, a failed identity preflight, or an unavailable
declared Nix environment stops the affected executable/change path at the
boundary. A confirmed Nix package gap proceeds through D0032 when its exact
bounded host path is available; an unresolved package, unsupported debug
action, invalid artifact path, or authorization failure is reported precisely.

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
