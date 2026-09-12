# AGENTS.md - MetaFlux-Core Agent Rules

These repository-wide invariants apply in every phase. Stage procedures belong
to their owning skills; the controller returns the exact current reading set.
Write repository content, documentation, skill instructions, comments and
diagnostics in English. Keep routing examples in English as well.

1. Enter through [$main](agent/skills/main/SKILL.md) skill. Before repository
   executables other than host Git/Nix, use
   `nix develop . --ignore-environment --keep HOME --keep USER --command ...`.
   Shell grammar runs inside its `--command bash -c '...'`, never the ambient
   shell. Keep only HOME/USER; do not preserve PATH, preload or startup variables.
   Consume only the conversation-emitted harness name for identity. Never infer
   it from processes, executable probes, Git configuration, model or session data.
2. Preserve the user's task and publication limits. Read `agent/README.md` and
   Goal, then the relevant memory/authority owners. Product implementation also
   reads its assigned work item and Exit Gate. Read-only questions preserve an
   active operation and create no state. Start a mutation with a bounded request,
   load its preparation rules, and enter prepared before editing. Existing edits
   belong to the user; edits between begin and preparation invalidate that step.
3. Follow the controller's action card. Load the complete current stage modules
   and required domain rules before its action; later stages load their own
   modules. Certificates bind actual emitted bodies, modes/blobs, request, base
   and HEAD, not comprehension or authorization. Changed rules require reloading;
   review and verification stay bound to the versions they actually used.
   [Tool hooks](agent/skills/main/references/tool-hooks.md) require application
   trust in their exact definition and cover supported calls, not arbitrary shell.
4. Deliver coherent requested behavior. Use focused checks to resolve specific
   uncertainty, then parent review and one covering formal plan per required
   phase. Keep required modes and prerequisites. Passing unchanged tests or
   counting deleted lines is not product progress. Preserve the zero-overhead
   design boundary and distinguish structural hot-path evidence from measurement.
5. Product work uses the application-supplied exact Epoch/Batch/Iteration/lane
   and base. Coding subagents receive a self-contained parent briefing and may
   change only its scope; they never edit Goal, accept, govern, commit, push or
   create contexts. Parent conversational review precedes further coding dispatch
   or evaluation. Uncommitted work is never an integration input. Context creation
   stays application-owned; reuse a matching context or report the exact request.
6. [$batch](agent/skills/batch/SKILL.md) skill automatically accepts qualified exact
   candidates with fresh integration evidence. It alone advances accepted
   Goal/work-item state. Slice acceptance preserves lane/target and allocates the
   Batch maximum Iteration plus one; full acceptance requires the whole Exit Gate.
   Follow DAG dependencies and lane order. Batch completion never closes a
   milestone or starts an Epoch. Repeated delivery first checks existing acceptance.
7. [$epoch](agent/skills/epoch/SKILL.md) skill alone performs explicitly requested
   governance/replanning. Confirmed unchanged proposals proceed; an approved full
   plan supplies confirmation. Rewrite affected current authority and remove
   obsolete semantics, then full regression and exact publication. No aliases,
   migrations or history ledgers. A semantic no-op leaves Epoch unchanged.
   Completed product work stays completed; older-base work needs current evidence.
8. Promote material facts to one canonical owner through
   [knowledge promotion](agent/skills/review/references/knowledge-promotion.md).
   Goal schema v4 is the sole product route and accepted-progress authority.
   Ignored `agent/tmp/main/` holds only current operation/evidence/transactions;
   it is forbidden from tracking and never grants authority. Root `tmp/` owns
   build/test outputs. Git preserves prior states; no progress diary or archive.
9. Nix owns tool provisioning only. Add required tools there first; confirmed
   host gaps and every sudo/su or privileged driver action use
   [$manage-host-privilege](agent/skills/manage-host-privilege/SKILL.md) skill.
   Exhaust both provisioning paths before treating a missing tool as a blocker.
   Never persist or print credentials. Research `references/` materializes only
   declared lane prerequisites on demand and remains read-only/non-product;
   build or qualification inputs belong to `toolchains/`.
10. Guarded delivery requires exact expected HEAD/tree and actual content-bound
    review/verification receipts. Stage reviewed paths before delivery. Staging
    unchanged bytes needs no new tests. Run Agent state/routing and relevant
    checks; full CTest covers registered focused checks/self-tests. Candidate and
    integration still execute independently. Batch metadata proof permits only
    its exact acceptance transaction. Use the shared Git lock, and clear Git
    local variables in nested-repository tests; attribute unexpected changes
    using the exact hook/test and reflog before retrying.
11. Maintenance, Epoch activation and Batch acceptance automatically publish the
    exact guarded commit unless limited by the user. Worker candidates go to
    Batch. [$publish](agent/skills/publish/SKILL.md) skill alone owns canonical
    GitHub/Nix Git/OpenSSH transport and full remote revision/ancestry verification.
    Never force, broaden refspecs, read private-key bytes, or change fetch origin.
    Failed publication preserves the same commit; remote advancement needs the
    application's new exact base. Standalone pushes require an explicit request
    and one full commit. Only an explicit first push initializes canonical main.
12. Use [$recover](agent/skills/recover/SKILL.md) skill for concrete failed causes,
    same-task rescope, interruptions and exact committed recovery. Preserve raw
    child diagnostics. Lost pre-commit state needs fresh review/verification;
    committed state recovers from Git. Never edit receipts to unlock work.
    No implicit external coordination, cleanup, privilege or unchanged retries.

Product boundaries live in `contracts/README.md` and
`docs/architecture/repo-layout.md`; the language/dependency wall remains checked
by `metaflux.architecture.component-graph`.
