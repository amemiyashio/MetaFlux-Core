#!/usr/bin/env python3
"""Validate the MetaFlux component dependency graph (decision-0011).

Consumes the JSON written by cmake/MetaFluxComponentGraph.cmake at configure
time and asserts two invariants:

1. Role whitelist: every direct dependency edge must be explicitly allowed by
   the ALLOWED_DEPENDENCIES matrix below.
2. Language wall: a C component may never link a CXX component, so the
   application-side closure cannot silently grow a C++ runtime.

Uses only the standard library.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ALLOWED_DEPENDENCIES: dict[str, set[str]] = {
    # Contracts depend on nothing.
    "client-protocol": set(),
    "backend-plugin-api": set(),
    "shared-device-layout": set(),
    # Neutral core.
    "client-fastpath": {"client-protocol", "shared-device-layout"},
    "runtime-core": {"client-protocol", "shared-device-layout"},
    "compiler-core": set(),
    # Application-side plugins (C17 closure).
    "passthrough-helper": set(),
    "compat-provider": {
        "client-protocol",
        "client-fastpath",
        "passthrough-helper",
        "transport-client",
    },
    "management-provider": {
        "client-protocol",
        "client-fastpath",
        "passthrough-helper",
        "transport-client",
    },
    "compiler-frontend": {"compiler-core"},
    # Worker-side plugins.
    "backend-compiler": {"compiler-core"},
    "backend-runtime": {"backend-plugin-api"},
    # A v0.x daemon may embed the local worker. It consumes the canonical C17
    # queue implementation and enabled ecosystem frontends on the worker side;
    # neither dependency permits an application-side edge back to the daemon.
    "daemon": {
        "client-fastpath",
        "runtime-core",
        "compiler-core",
        "compiler-frontend",
        "backend-compiler",
        "backend-runtime",
        "transport-worker",
    },
    # Transport halves (decision-0010). No transport code exists yet; the rows freeze
    # the rules the first implementation must satisfy.
    "transport-client": {"client-protocol", "client-fastpath"},
    "transport-worker": {"runtime-core", "compiler-core", "backend-plugin-api"},
}

CLIENT_SIDE_ROLES = {
    "compat-provider",
    "management-provider",
    "passthrough-helper",
    "transport-client",
    "client-fastpath",
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("graph", type=Path, help="path to metaflux-component-graph.json")
    arguments = parser.parse_args()

    try:
        document = json.loads(arguments.graph.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        print(f"error: cannot load {arguments.graph}: {error}", file=sys.stderr)
        return 1

    components = document.get("components")
    edges = document.get("edges")
    if not isinstance(components, list) or not components:
        print("error: graph contains no components", file=sys.stderr)
        return 1
    if not isinstance(edges, list):
        print("error: graph has no edges array", file=sys.stderr)
        return 1

    by_id = {entry["id"]: entry for entry in components}
    if len(by_id) != len(components):
        print("error: duplicate component ids in graph", file=sys.stderr)
        return 1

    errors: list[str] = []
    for source_id, target_id in edges:
        source = by_id.get(source_id)
        target = by_id.get(target_id)
        if source is None or target is None:
            errors.append(f"edge {source_id} -> {target_id} references an unknown component")
            continue

        source_role = source["role"]
        target_role = target["role"]
        allowed = ALLOWED_DEPENDENCIES.get(source_role)
        if allowed is None:
            errors.append(f"component {source_id} has unknown role '{source_role}'")
            continue
        if target_role not in allowed:
            errors.append(
                f"component {source_id} (role '{source_role}') links "
                f"{target_id} (role '{target_role}'), which is not allowed"
            )
        if source["language"] == "C" and target["language"] == "CXX":
            errors.append(
                f"language wall violated: C component {source_id} links "
                f"CXX component {target_id}"
            )
        if source_role in CLIENT_SIDE_ROLES and target_role == "daemon":
            errors.append(f"client-side component {source_id} links the daemon")

    if errors:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
        print(
            f"component graph validation failed with {len(errors)} error(s)",
            file=sys.stderr,
        )
        return 1

    print(
        f"component graph: ok ({len(components)} component(s), "
        f"{len(edges)} dependency edge(s))"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
