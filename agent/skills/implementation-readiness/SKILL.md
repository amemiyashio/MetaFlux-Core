---
name: implementation-readiness
description: Assess whether a repository or workstream is ready for implementation by separating architecture, activation, implementation-maturity, and release evidence. Use for readiness reviews, go/no-go questions, open-decision triage, or selecting the next vertical slice. Do not use as release certification or as a substitute for measured acceptance evidence.
---

# Implementation Readiness

Use this skill when the question is whether engineering can start, what must
close first, or which implementation boundary should come next. Read
[the evidence framework](references/evidence-framework.md) for a full review;
for a narrow work-item question, load only the relevant lane.

## Workflow

1. State the decision being made. Distinguish "may implementation start?" from
   "is the feature integrated?" and "may it release?" These are different
   evidence thresholds.
2. Read repository truth in precedence order: current source and tests,
   verified contracts and architecture, active plan, durable memory, then
   progress records. Label proposed intent separately from implemented fact.
3. Build four independent evidence lanes:
   - **Architecture baseline:** boundaries, contracts, legal dependency edges,
     quality-attribute tradeoffs, and executable architecture fitness checks.
   - **Workstream activation:** prerequisites, owned decisions, required inputs,
     acceptance evidence, and an executable first slice.
   - **Implementation maturity:** real behavior versus fixtures, vertical-slice
     integration, failure handling, tests, and measured budgets.
   - **Release and operations:** packaging, compatibility, security, recovery,
     observability, performance qualification, and external obligations.
4. Give each lane one verdict: `Ready`, `Ready with bounded gaps`, `Not ready`,
   or `Not assessed`. Never average the lanes into a single score.
5. Classify every relevant open decision by closure timing:
   - must close before the first implementation edit;
   - may close before integration, qualification, or release;
   - deliberately left open because the first slice must generate its evidence.
6. Convert each claimed quality attribute into a fitness function: an existing
   test or check, a command with an objective threshold, or a named missing
   harness. An adjective without observable evidence is an assumption.
7. Select the smallest vertical slice that respects dependencies, exercises a
   real contract end to end, and retires the highest uncertainty. Do not add
   governance unless it closes a concrete blocker or automates a fitness
   function.
8. Verify claims with the narrowest relevant owner commands. In this repository
   use `python3 tools/check-agent-records.py .`, the owning CTest preset, and the
   component-graph gate at the scope their evidence justifies. A Nix tool-version
   probe establishes tool availability, not implementation or release readiness.

## Output

Return these sections:

1. **Conclusion:** answer the exact readiness question in one paragraph.
2. **Readiness matrix:** lane, verdict, strongest evidence, and blocking gap.
3. **Must close now:** only blockers to the stated next boundary.
4. **Can close later:** deferred decisions with their latest safe closure point.
5. **Next actions:** one to three ordered, executable actions with expected
   evidence.
6. **Confidence:** distinguish inspected evidence from inference and name any
   lane not assessed.

Reference exact repository paths and command results. A strong architecture
baseline can coexist with fixture-level implementation; say both explicitly.
