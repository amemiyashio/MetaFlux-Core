"""Single action-to-module map shared by cards, rule certificates and hooks."""
from __future__ import annotations

from pathlib import Path

ACTIONS = ("prepare", "implement", "review", "verify", "deliver", "publish", "recover", "handoff", "integration")
STATE_ACTION = {"preparation": "prepare", "implementation": "implement", "review": "review",
                "evaluation": "verify", "delivery": "deliver", "publication": "publish",
                "handoff": "handoff", "complete": "handoff"}
EVENT_ACTION = {"prepared": "prepare", "review": "review", "evaluate": "verify",
                "deliver": "deliver", "publish": "publish", "handoff": "handoff"}
ACTION_SKILLS = {"prepare": ("prepare",), "implement": (), "review": ("review", "verify"),
                 "verify": ("verify",), "deliver": ("deliver",), "publish": ("publish",),
                 "recover": ("recover",), "handoff": (), "integration": ("review", "verify")}
COVERS = {action: {action} for action in ACTIONS}
COVERS["review"].add("verify")
COVERS["integration"].update(("review", "verify"))
DOMAIN_ACTIONS = {"implement", "review", "integration"}
DONE = {
    "prepare": "The exact request is prepared; load implementation rules before editing.",
    "implement": "The requested coherent behavior is implemented and ready for parent review.",
    "review": "The parent accepts the actual diff and covering check plan; enter evaluation.",
    "verify": "The reviewed plan has an actual passing receipt; proceed to delivery.",
    "deliver": "The exact reviewed tree has a guarded commit and its delivery identity.",
    "publish": "The exact commit is published with remote equality or ancestry proof.",
    "recover": "The concrete cause is resolved and the next action has valid inputs.",
    "handoff": "The exact next assignment is reused or requested from the application.",
    "integration": "Fresh combined evidence permits the exact Batch acceptance transaction.",
}


def action_for(kind: str, state: dict | None = None) -> str:
    if kind == "integration":
        return "integration"
    return STATE_ACTION.get((state or {}).get("stage"), "review")


def references(action: str, kind: str) -> list[str]:
    shared = "agent/skills/"
    names = {
        "prepare": ["prepare/references/request.md"],
        "implement": ["review/references/implementation-guidance.md"],
        "review": ["review/references/implementation-guidance.md", "verify/references/verification.md"],
        "verify": ["verify/references/verification.md"],
        "deliver": [], "publish": ["publish/references/transport.md"],
        "recover": ["recover/references/diagnostics.md", "recover/references/recovery.md"],
        "handoff": ["main/references/handoff.md"],
        "integration": ["batch/references/integration.md", "iteration/references/delivery.md",
                        "review/references/implementation-guidance.md", "verify/references/verification.md"],
    }[action].copy()
    if action in {"implement", "review", "integration"} and kind in {"epoch", "batch", "integration"}:
        names.append("review/references/knowledge-promotion.md")
    if action == "implement" and kind == "epoch":
        names.append("epoch/references/cutover.md")
    if action == "implement" and kind == "batch":
        names.extend(("batch/references/integration.md", "iteration/references/delivery.md"))
    if action == "deliver" and kind == "iteration":
        names.append("iteration/references/delivery.md")
    return [shared + name for name in names]


def role(kind: str) -> str:
    return {"epoch": "governing parent", "batch": "controlling Batch integrator",
            "integration": "controlling Batch integrator",
            "iteration": "candidate-delivery parent"}.get(kind, "controlling parent")
