---
name: main
description: Route MetaFlux tasks to the current workflow and stage, report the next concrete action and required readings, and preserve read-only scope and active operations.
---

# Main

Role: controlling parent. Input: the user's task and existing repository state.
First action: bootstrap below, then run `main.py inspect`. For a new mutation,
select its workflow and follow [$prepare](../prepare/SKILL.md) skill.
Output: one action card; call its stage skill and continue within existing scope.

## Bootstrap

Every application command uses
`nix develop . --ignore-environment --keep HOME --keep USER --command ...`.
Shell grammar uses its `--command bash -c '...'`; keep only HOME/USER.
Use the conversation-emitted harness name, never model/process/probe facts:

```sh
nix develop . --ignore-environment --keep HOME --keep USER --command python3 -B agent/skills/prepare/scripts/detect_agent_tool.py --agent-tool HARNESS_NAME --json
nix develop . --ignore-environment --keep HOME --keep USER --command python3 -B agent/skills/prepare/scripts/check_git_topology.py --json
nix develop . --ignore-environment --keep HOME --keep USER --command python3 -B agent/skills/main/scripts/main.py inspect
```

Read `agent/README.md` and `agent/goal.json`, then only the relevant memory and
authority owners. Product work also reads its assigned work item and Exit Gate.
Missing tools use [$manage-toolchain](../manage-toolchain/SKILL.md) skill;
confirmed host gaps compose [$manage-host-privilege](../manage-host-privilege/SKILL.md) skill.

## Dispatch

| Request | Owner |
| --- | --- |
| Inspect, explain, status, readiness | Read-only here; full readiness uses [$prepare](../prepare/SKILL.md) skill |
| Bounded maintenance | Preparation → implementation → review → verification → delivery → publication |
| Assigned product change | [$iteration](../iteration/SKILL.md) skill → exact candidate → [$batch](../batch/SKILL.md) skill |
| Explicit governance or route change | [$epoch](../epoch/SKILL.md) skill |
| Interrupted work or concrete failure | [$recover](../recover/SKILL.md) skill |
| Exact publication or transport diagnosis | [$publish](../publish/SKILL.md) skill |

The action card gives role, required readings, next action and completion
condition. Load its complete modules with `load-rules`; before parent review use
`load-rules --for review`. Loading does not execute the action or grant authority.
An implementation card calls the owning workflow/domain skills. A status
question preserves active work. The script creates no agent or execution context.

## Skill References

Name a skill as a clickable `$name` link followed by `skill`. Tracked Markdown
uses relative links; user-facing replies use the current checkout's absolute
SKILL.md path. Diagnostics/UI use `$name skill`; schema identifiers and commands
keep their syntax. Epoch/Batch/Iteration are work concepts, stages are actions,
and Git main is a branch.

Shared state and rule mechanisms live in `agent/lib/`. For command/schema
changes read [controller](references/controller.md); for stopped tool calls read
[hooks](references/tool-hooks.md). Errors use
[diagnostics](../recover/references/diagnostics.md).
