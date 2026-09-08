#!/usr/bin/env python3

import hashlib
import json
import pathlib
import sys


def digest(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition: bool, message: str, failures: list[str]) -> None:
    if not condition:
        failures.append(message)


def main() -> int:
    if len(sys.argv) != 4:
        print("usage: validate_manifest.py CAPABILITIES FORMS CORPUS_INDEX", file=sys.stderr)
        return 2

    capabilities_path = pathlib.Path(sys.argv[1]).resolve()
    forms_path = pathlib.Path(sys.argv[2]).resolve()
    index_path = pathlib.Path(sys.argv[3]).resolve()
    capabilities = json.loads(capabilities_path.read_text(encoding="utf-8"))
    index = json.loads(index_path.read_text(encoding="utf-8"))
    forms = [json.loads(line) for line in forms_path.read_text(encoding="utf-8").splitlines() if line]
    failures: list[str] = []

    required_form_fields = {
        "id", "spelling", "types", "spaces", "modifiers", "min_sm", "kir_op", "oracle",
        "fixture_ids", "stable_diagnostic",
    }
    require(len(forms) == 36, "forms.jsonl must contain exactly 36 advertised forms", failures)
    form_ids: set[str] = set()
    for line_number, form in enumerate(forms, start=1):
        require(set(form) == required_form_fields,
                f"form line {line_number} has a missing or unknown field", failures)
        form_id = form.get("id", "")
        require(isinstance(form_id, str) and form_id != "", f"form line {line_number} has no id", failures)
        require(form_id not in form_ids, f"duplicate form id {form_id}", failures)
        form_ids.add(form_id)
        require(form.get("min_sm") == 70, f"{form_id} must be pinned to sm_70", failures)
        for field in ("spelling", "kir_op", "oracle"):
            require(isinstance(form.get(field), str) and form[field] != "",
                    f"{form_id}.{field} must be a non-empty string", failures)
        for field in ("types", "spaces", "modifiers"):
            require(isinstance(form.get(field), list), f"{form_id}.{field} must be an array", failures)
        fixture_ids = form.get("fixture_ids", {})
        require(set(fixture_ids) == {"positive", "malformed", "unsupported", "edge"},
                f"{form_id} must name all four fixture classes", failures)
        for fixture_class in ("positive", "malformed", "unsupported", "edge"):
            values = fixture_ids.get(fixture_class, [])
            require(isinstance(values, list) and values and all(isinstance(value, str) for value in values),
                    f"{form_id}.{fixture_class} fixture ids must be a non-empty string array", failures)
        stable = form.get("stable_diagnostic", {})
        require(set(stable) == {"malformed", "unsupported"},
                f"{form_id} must pin malformed and unsupported diagnostics", failures)
        require(all(isinstance(value, str) and value.startswith("MF_PTX_") for value in stable.values()),
                f"{form_id} diagnostics must use stable MF_PTX names", failures)

    require(capabilities.get("schema_version") == 1, "capability schema must be version 1", failures)
    require(capabilities.get("ptx_isa") == "9.0", "capability PTX ISA must be 9.0", failures)
    require(capabilities.get("target") == "sm_70" and capabilities.get("minimum_sm") == 70,
            "capability target must be sm_70", failures)
    require(capabilities.get("kernel_ir_schema") == 2, "capability KIR schema must be 2", failures)
    require(capabilities.get("dimensions") == ["x", "y"], "only x/y dimensions are advertised", failures)
    require(capabilities.get("barrier_ids") == [0], "only barrier id 0 is advertised", failures)
    require(capabilities.get("static_shared_limit_bytes") == 49152,
            "static shared limit must be 49152 bytes", failures)
    excluded = set(capabilities.get("excluded", []))
    require({"atomics", "warp operations", "cluster operations", "approx", "ftz"} <= excluded,
            "capability exclusions must name atomics/warp/cluster/approx/ftz", failures)

    forms_sha256 = digest(forms_path)
    require(index.get("forms_sha256") == forms_sha256,
            f"corpus index forms digest mismatch: actual {forms_sha256}", failures)
    require(capabilities.get("forms_sha256") == forms_sha256,
            f"capability forms digest mismatch: actual {forms_sha256}", failures)
    require(index.get("ptx_isa") == "9.0" and index.get("target") == "sm_70",
            "corpus index must be pinned to PTX 9.0/sm_70", failures)

    fixtures = index.get("fixtures", [])
    require(isinstance(fixtures, list) and fixtures, "corpus index fixtures must be non-empty", failures)
    fixture_by_id: dict[str, dict] = {}
    fixture_paths: set[str] = set()
    for fixture in fixtures:
        fixture_id = fixture.get("id", "")
        path_text = fixture.get("path", "")
        require(fixture_id not in fixture_by_id, f"duplicate fixture id {fixture_id}", failures)
        require(path_text not in fixture_paths, f"duplicate fixture path {path_text}", failures)
        fixture_by_id[fixture_id] = fixture
        fixture_paths.add(path_text)
        classes = fixture.get("classes", [])
        require(isinstance(classes, list) and classes and
                set(classes) <= {"positive", "malformed", "unsupported", "edge"},
                f"{fixture_id} has invalid classes", failures)
        fixture_forms = fixture.get("forms", [])
        require(isinstance(fixture_forms, list) and set(fixture_forms) <= form_ids,
                f"{fixture_id} references an unknown form", failures)
        fixture_path = (index_path.parent / path_text).resolve()
        require(fixture_path.is_file(), f"missing fixture file {path_text}", failures)
        if fixture_path.is_file():
            actual = digest(fixture_path)
            require(fixture.get("sha256") == actual,
                    f"fixture digest mismatch for {fixture_id}: actual {actual}", failures)
            require(".target sm_70" in fixture_path.read_text(encoding="utf-8"),
                    f"fixture {fixture_id} is not pinned to sm_70", failures)
        if "malformed" in classes or "unsupported" in classes:
            require(isinstance(fixture.get("stable_diagnostic"), str),
                    f"negative fixture {fixture_id} must pin a stable diagnostic", failures)
        else:
            require(isinstance(fixture.get("oracle"), str) and fixture["oracle"] != "",
                    f"executable fixture {fixture_id} must name an oracle", failures)

    for form in forms:
        form_id = form["id"]
        for fixture_class, ids in form["fixture_ids"].items():
            for fixture_id in ids:
                fixture = fixture_by_id.get(fixture_id)
                require(fixture is not None, f"{form_id} references missing fixture {fixture_id}", failures)
                if fixture is None:
                    continue
                require(fixture_class in fixture.get("classes", []),
                        f"{form_id} fixture {fixture_id} is not classed {fixture_class}", failures)
                if fixture_class in {"positive", "edge"}:
                    require(form_id in fixture.get("forms", []),
                            f"{form_id} is absent from executable fixture {fixture_id}", failures)

    aggregate = hashlib.sha256()
    for fixture_id in sorted(fixture_by_id):
        aggregate.update(fixture_id.encode("utf-8"))
        aggregate.update(b"\0")
        aggregate.update(fixture_by_id[fixture_id].get("sha256", "").encode("ascii"))
        aggregate.update(b"\n")
    corpus_sha256 = aggregate.hexdigest()
    require(index.get("corpus_sha256") == corpus_sha256,
            f"corpus aggregate mismatch: actual {corpus_sha256}", failures)

    index_sha256 = digest(index_path)
    require(capabilities.get("corpus_index_sha256") == index_sha256,
            f"capability corpus-index digest mismatch: actual {index_sha256}", failures)

    if failures:
        for failure in failures:
            print(f"PTX manifest validation failure: {failure}", file=sys.stderr)
        return 1
    print(f"forms_sha256={forms_sha256}")
    print(f"corpus_sha256={corpus_sha256}")
    print(f"corpus_index_sha256={index_sha256}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
