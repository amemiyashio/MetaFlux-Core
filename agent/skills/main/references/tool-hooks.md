# Tool Hooks

The project [.codex/hooks.json](../../../../.codex/hooks.json) connects the
current Codex lifecycle to [tool_gate.py](../scripts/tool_gate.py). Use this
reference when enabling hooks or diagnosing a tool call stopped before execution.
The gate prevents missed workflow preparation in supported calls. It is not a
shell sandbox or a user-authorization service.

## Loading And Context

`SessionStart` and `SubagentStart` insert the [$main](../SKILL.md) skill entrypoint reminder without
creating or replacing an operation. A subagent follows its parent briefing;
startup never grants it the parent's loaded-rule binding. The first prospective
write uses that call's session/turn context, additionally distinguishing a
host-supplied child ID when available. Missing session/turn fields stop a write;
the gate never falls back to a session-only binding.

`PreToolUse` validates the active request, exact baseline, present scope and
required stage. The action-to-module map in `agent/lib/stage_rules.py` selects
complete current-stage bodies, including semantic owners during implementation
and review. Delivery and publication load their own mechanisms; they do not
reissue passed product checks. If rules are missing, changed, or associated with another
context, it calls the shared rule loader, returns the full bodies as
`hookSpecificOutput.additionalContext`, and denies that invocation. The next
invocation revalidates those facts. `rules.json` records the emitted bodies and
versions. One ignored `agent/tmp/main/hook-context.json` binds their digest and
the operation digest to an irreversible context digest. It stores no raw
session/turn/child identifier, transcript, tool input, credential, or identity.
It is neither product progress nor proof of understanding or permission.

The hook uses `additionalContextLimit: 0` so Codex's default spill/truncation
does not turn partial text into a claimed full load. Its output buffer rejects
rule text exceeding 128 KiB before the loader writes a receipt. Narrow an
oversized operation to its actual required scope; do not truncate the rules.

`PostCompact` invalidates an existing binding for any active mutable operation,
including committed publication and handoff. It emits the supported
`systemMessage`; the next write or publication reloads the bodies. It leaves
the controller's stage, Goal, commit, receipt, and publication recovery intact.
Ordinary read-only tools never create temporary state or reset an active task.

## Supported Calls And Stage Exceptions

The matcher covers `Bash`, `apply_patch`, MCP, and other local function tools.
A deliberately small set of ordinary file/Git reads and known read-only tools
passes without an operation. Unknown syntax and tool names are treated as
potential mutations. An unfamiliar read command can be expressed through a
recognized simple read or inspected in the current bounded workflow.

Repository executable calls use the clean Git-aware Nix entry from
[$main](../SKILL.md#bootstrap) skill. The parser requires `--ignore-environment`
and permits only HOME/USER in `--keep`; ambient PATH, preloads, Python paths
and startup variables are rejected. The hook command uses that same entry.
Already initialized repository commands retain their own nested tool calls;
the parser does not inspect arbitrary child programs. Keep controller
commands as one explicit invocation, including when wrapped in Nix bash.
`inspect`, `preflight`, `begin`, `load-rules`, `resume`, `rescope`, `supersede`, and the two bootstrap helpers retain
their own parsers and validation. `begin --request-json` and
`step --payload-json` avoid an ungoverned temporary-input write during bootstrap.
`batch.py load-rules DELIVERY` validates the exact candidate and emits integration
rules. Its `verify` and `advance` hooks use the delivery base and actual changed
paths, distinct from the current-main Batch operation base. They keep candidate
and integration receipts independent. Other controller transitions follow their exact stage; no broad shell compound
receives the controller exception.

The exact `rescope` and `supersede` commands reach their own token/request
validation before the old scope check. Otherwise an omitted path would also
block its recovery. This exception permits only invoking that parser, never
the subsequent file write: success returns to preparation and invalidates the
old context. A compound shell containing another operation gets no exception.

Structured patches validate add, update, delete, and both move paths against
`request.allowed_paths`, including traversal and symlink resolution. Known MCP
filesystem writes receive the same target check. Maintenance and Iteration
calls retain the Goal ownership restriction. Direct writes to `agent/tmp/`
use controller commands instead. Generic mutations require implementation;
after review, use `main.py step repair` through [$main](../SKILL.md) skill before editing again. Exact declared check
commands remain available during the working/evaluation stages. Concrete
`git add` paths and the guarded commit helper require delivery; the publication
helper requires publication and the operation's exact full commit.

For arbitrary shell programs or opaque MCP calls, the gate checks preparation,
rule context, baseline and existing request scope. It does not infer every
future write made by a program from its command text. Use structured patches
for target checking and keep the independent controller, receipt and candidate
commit gates. An already running shell's `write_stdin`, specialized tool paths
that opt out of hooks, or another same-user process remain outside this guard.
The hook does not make those channels isolated or prove arbitrary commands
read-only. Hook context also depends on the host supplying distinct current
turn/child context fields; a host that reuses all such fields cannot establish
that distinction through this interface.

## Activation And Verification

Codex discovers project hooks only through a trusted project configuration
layer. Each exact non-managed hook definition additionally needs the host's
hook trust review. Use the host's hook review interface (`/hooks` in the CLI);
do not write trust hashes or change global settings from this workflow. Managed
policy may restrict local hooks. Merely checking in this file does not prove
the current running session has activated it.

After host review, inspect discovered hooks and observe an actual supported
tool call producing a hook callback. If configuration refresh is needed,
reload/resume the existing application session; do not create another task.
Official documentation does not promise automatic hot reload for an already
running session. Fixture tests verify repository behavior separately from host
discovery, trust and activation.

The maintained event/output contract and coverage limits are documented in
[official Codex Hooks documentation](https://learn.chatgpt.com/docs/hooks).
