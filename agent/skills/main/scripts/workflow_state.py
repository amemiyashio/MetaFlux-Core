#!/usr/bin/env python3
"""Content-bound verification and recoverable local workflow transactions.

Receipts attest to executed commands, not to product semantics or user authority.
The parent reviews the exact content and supplies the check plan before evaluation.
"""

from __future__ import annotations

import contextlib
import fcntl
import hashlib
import json
import os
import re
import stat
import subprocess
import tempfile
from pathlib import Path
from typing import Any


class WorkflowError(RuntimeError):
    pass


ACTIVE_LOCK_FD: int | None = None


def require(condition: bool, message: str) -> None:
    if not condition:
        raise WorkflowError(message)


def digest(value: Any) -> str:
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def environment(root: Path) -> dict[str, str]:
    result = dict(os.environ)
    names = subprocess.run(["git", "rev-parse", "--local-env-vars"], cwd=root,
                           capture_output=True, text=True, check=True).stdout.splitlines()
    for name in names:
        result.pop(name, None)
    return result


def git(root: Path, *args: str, data: bytes | None = None, check: bool = True) -> bytes:
    result = subprocess.run(["git", *args], cwd=root, env=environment(root), input=data,
                            capture_output=True)
    if check and result.returncode:
        raise WorkflowError(result.stderr.decode(errors="replace") or "Git query failed")
    return result.stdout


def oid(root: Path, name: str = "HEAD") -> str:
    if name == ":":
        return git(root, "write-tree").decode().strip()
    return git(root, "rev-parse", "--verify", name).decode().strip()


def exact_commit(root: Path, revision: str) -> str:
    require(bool(re.fullmatch(r"[0-9a-f]{40}", revision)), "A full commit object ID is required")
    require(oid(root, revision + "^{commit}") == revision, "Commit object identity changed")
    return revision


def ancestor(root: Path, base: str, tip: str) -> bool:
    return subprocess.run(["git", "merge-base", "--is-ancestor", base, tip], cwd=root,
                          env=environment(root), capture_output=True).returncode == 0


def entries(root: Path, revision: str | None = None) -> dict[str, tuple[str, str]]:
    result: dict[str, tuple[str, str]] = {}
    if revision is not None:
        for row in git(root, "ls-tree", "-rz", "--full-tree", revision).split(b"\0"):
            if row:
                meta, path = row.split(b"\t", 1)
                mode, _, blob = meta.decode().split()
                result[os.fsdecode(path)] = (mode, blob)
        return result
    indexed: dict[str, tuple[str, str]] = {}
    for row in git(root, "ls-files", "--stage", "-z").split(b"\0"):
        if row:
            meta, path = row.split(b"\t", 1)
            mode, blob, stage = meta.decode().split()
            require(stage == "0", "Resolve unmerged index entries before verification")
            indexed[os.fsdecode(path)] = (mode, blob)
    names = set(indexed)
    names.update(os.fsdecode(p) for p in git(root, "ls-files", "--others", "--exclude-standard", "-z").split(b"\0") if p)
    for name in sorted(names):
        require(name != "agent/tmp" and not name.startswith("agent/tmp/"), "agent/tmp contains a tracked authority input")
        path = root / name
        if indexed.get(name, (None,))[0] == "160000":
            value = indexed[name]
            if (path / ".git").exists():
                value = ("160000", oid(path))
                require(not git(path, "status", "--porcelain").strip(), "Reference submodule is dirty: " + name)
            result[name] = value
        elif path.is_symlink():
            data = os.fsencode(os.readlink(path))
            result[name] = ("120000", hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest())
        elif path.is_file():
            metadata = path.stat()
            mode = "100755" if metadata.st_mode & stat.S_IXUSR else "100644"
            hashed = hashlib.sha1(b"blob " + str(metadata.st_size).encode() + b"\0")
            with path.open("rb") as stream:
                for block in iter(lambda: stream.read(1024 * 1024), b""):
                    hashed.update(block)
            result[name] = (mode, hashed.hexdigest())
    return result


def content(root: Path, revision: str | None = None) -> str:
    return digest(entries(root, revision))


def toolchain(root: Path, revision: str | None = None) -> str:
    return toolchain_entries(entries(root, revision))


def toolchain_entries(values: dict[str, tuple[str, str]]) -> str:
    return digest({p: value for p, value in values.items()
                   if p in {"flake.nix", "flake.lock"} or p.startswith(("nix/", "toolchains/"))})


def snapshot(root: Path) -> dict[str, str]:
    values = entries(root)
    return {"head": oid(root), "content": digest(values),
            "toolchain": digest({p: v for p, v in values.items()
                                 if p in {"flake.nix", "flake.lock"} or p.startswith(("nix/", "toolchains/"))})}


def local_path(root: Path, name: str) -> Path:
    return root / "agent/tmp/main" / name


def atomic_json(path: Path, document: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=".pending-", dir=path.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            json.dump(document, stream, indent=2, sort_keys=True)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
        directory = os.open(path.parent, os.O_RDONLY)
        try:
            os.fsync(directory)
        finally:
            os.close(directory)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def read_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


@contextlib.contextmanager
def lock(root: Path):
    global ACTIVE_LOCK_FD
    common = Path(git(root, "rev-parse", "--path-format=absolute", "--git-common-dir").decode().strip())
    inherited = os.environ.get("METAFLUX_MAIN_LOCK_FD")
    if inherited is not None:
        fd = int(inherited)
        actual, expected = os.fstat(fd), (common / "metaflux-main.lock").stat()
        require((actual.st_dev, actual.st_ino) == (expected.st_dev, expected.st_ino), "Inherited workflow lock belongs to another repository")
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        yield
        return
    with (common / "metaflux-main.lock").open("a") as stream:
        try:
            fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise WorkflowError("Another workflow transition holds the repository lock") from error
        try:
            ACTIVE_LOCK_FD = stream.fileno()
            yield
        finally:
            ACTIVE_LOCK_FD = None
            fcntl.flock(stream, fcntl.LOCK_UN)


def validate_plan(checks: Any) -> None:
    require(isinstance(checks, list) and bool(checks), "An explicit non-empty check plan is required")
    names: set[str] = set()
    for check in checks:
        require(isinstance(check, dict) and set(check) <= {"id", "argv", "optional_skip_reason", "allowed_ctest_skips"}, "Invalid check fields")
        require(isinstance(check.get("id"), str) and bool(check["id"]) and check["id"] not in names, "Check IDs must be unique")
        names.add(check["id"])
        require(isinstance(check.get("argv"), list) and bool(check["argv"]) and
                all(isinstance(x, str) and x for x in check["argv"]), "Checks require an argv array")
        if "optional_skip_reason" in check:
            require(isinstance(check["optional_skip_reason"], str) and bool(check["optional_skip_reason"].strip()), "Optional skip needs a reason")
        if "allowed_ctest_skips" in check:
            skips = check["allowed_ctest_skips"]
            require(Path(check["argv"][0]).name == "ctest" and isinstance(skips, list) and
                    all(isinstance(x, str) and x for x in skips) and len(skips) == len(set(skips)),
                    "CTest skips must be explicit unique test names")


def review(root: Path, summary: str, checks: list[dict[str, Any]]) -> dict[str, Any]:
    import rule_loading
    validate_plan(checks)
    require(bool(summary.strip()), "The parent review must state its semantic conclusion")
    return {"content": content(root), "summary": summary, "plan": digest(checks), "rules": rule_loading.current(root)}


def evaluate(root: Path, kind: str, base: str, checks: list[dict[str, Any]],
             reviewed: dict[str, Any]) -> dict[str, Any]:
    validate_plan(checks)
    exact_commit(root, base)
    before = snapshot(root)
    import rule_loading
    rule_loading.validate(root, reviewed.get("rules"), kind=kind)
    require(reviewed["rules"]["head"] == before["head"] and reviewed["rules"]["base_revision"] == base,
            "Review used rules from another verification baseline")
    require(ancestor(root, base, before["head"]), "Verification baseline is outside current history")
    require(reviewed.get("content") == before["content"] and reviewed.get("plan") == digest(checks)
            and bool(reviewed.get("summary")), "Review or check plan is stale")
    import verification
    return verification.execute(root, kind, base, checks, reviewed, before)


def validate_receipt(root: Path, receipt: Any, *, kind: str,
                     revision: str | None = None, base: str | None = None,
                     logs: bool = True, tested_entries: dict[str, tuple[str, str]] | None = None) -> None:
    require(isinstance(receipt, dict) and receipt.get("schema_version") == 2, "Missing current actual verification receipt")
    require(receipt.get("digest") == digest({k: v for k, v in receipt.items() if k != "digest"}), "Receipt digest mismatch")
    require(receipt.get("kind") == kind, "Wrong verification phase")
    validate_plan(receipt.get("checks"))
    require(base is None or receipt.get("base_revision") == base, "Receipt has a different base")
    inputs = receipt.get("input", {})
    exact_commit(root, receipt["base_revision"])
    require(ancestor(root, receipt["base_revision"], inputs.get("head", "")), "Receipt baseline is not ancestral")
    values = tested_entries if tested_entries is not None else entries(root, revision)
    require(inputs.get("content") == digest(values) and inputs.get("toolchain") == toolchain_entries(values),
            "Tested content or toolchain has changed")
    if revision is None:
        require(inputs.get("head") == oid(root), "Verification HEAD has changed")
    else:
        exact_commit(root, revision)
        require(inputs.get("head") == revision or
                inputs.get("head") in git(root, "show", "-s", "--format=%P", revision).decode().split(),
                "Candidate was not committed from its tested HEAD")
    reviewed = receipt.get("review", {})
    require(reviewed.get("content") == inputs.get("content") and
            reviewed.get("plan") == digest(receipt["checks"]) and bool(reviewed.get("summary")), "Missing bound parent review")
    import rule_loading
    rule_loading.validate(root, reviewed.get("rules"), kind=kind, entries=values)
    require(reviewed["rules"]["head"] == inputs["head"] and reviewed["rules"]["base_revision"] == receipt["base_revision"],
            "Verification rules disagree with the tested baseline")
    results = receipt.get("results", [])
    require(len(results) == len(receipt["checks"]), "Incomplete executed check results")
    import verification
    verification.validate_execution(root, receipt, logs=logs)
    for check, result in zip(receipt["checks"], results):
        require(result.get("id") == check["id"] and result.get("argv") == check["argv"], "Executed check differs from plan")
        require(result.get("returncode") == 0 or
                (result.get("returncode") == 77 and bool(check.get("optional_skip_reason")) and
                 result.get("optional_skip_reason") == check["optional_skip_reason"]), "Verification did not pass")
        if logs:
            path = Path(result["log"])
            require(path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() == result.get("log_digest"),
                    "Verification log is missing or changed; rerun evaluation")


def blob(root: Path, path: str) -> dict[str, str] | None:
    value = entries(root).get(path)
    if value is None:
        return None
    mode, object_id = value
    require(mode != "160000", "Authority transactions do not edit submodules")
    p = root / path
    data = os.fsencode(os.readlink(p)) if p.is_symlink() else p.read_bytes()
    require(git(root, "hash-object", "-w", "--stdin", data=data).decode().strip() == object_id, "Authority blob changed")
    return {"mode": mode, "oid": object_id}


def begin_transaction(root: Path, replacements: dict[str, str], identity: dict[str, Any]) -> dict[str, Any]:
    path = local_path(root, "acceptance.json")
    require(not path.exists(), "An unfinished acceptance transaction needs $main skill (main.py resume)")
    files = {}
    for name, text in replacements.items():
        transaction_path(root, name)
        before = blob(root, name)
        after = {"mode": before["mode"] if before else "100644",
                 "oid": git(root, "hash-object", "-w", "--stdin", data=text.encode()).decode().strip()}
        files[name] = {"before": before, "after": after}
    txn = {"schema_version": 1, "head": oid(root), "index": oid(root, ":"),
           "identity": identity, "files": files}
    atomic_json(path, txn)
    return txn


def restore_blob(root: Path, name: str, value: dict[str, str] | None) -> None:
    path = transaction_path(root, name)
    if value is None:
        path.unlink(missing_ok=True)
        return
    data = git(root, "cat-file", "blob", value["oid"])
    fd, temporary = tempfile.mkstemp(prefix=".accept-", dir=path.parent)
    if value["mode"] == "120000":
        os.close(fd)
        os.unlink(temporary)
        os.symlink(os.fsdecode(data), temporary)
    else:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fchmod(stream.fileno(), 0o755 if value["mode"] == "100755" else 0o644)
            os.fsync(stream.fileno())
    os.replace(temporary, path)
    directory = os.open(path.parent, os.O_RDONLY)
    try:
        os.fsync(directory)
    finally:
        os.close(directory)


def transaction_path(root: Path, name: str) -> Path:
    require(isinstance(name, str) and not Path(name).is_absolute() and
            all(p not in {".", "..", ""} for p in name.split("/")), "Invalid transaction path")
    require(name == "agent/goal.json" or (name.startswith("agent/plan/") and name.endswith(".md")), "Unexpected acceptance path")
    path = root / name
    require(path.parent.resolve().is_relative_to(root.resolve()), "Transaction parent escapes repository")
    return path


def validate_transaction(root: Path, txn: dict[str, Any]) -> None:
    require(txn.get("schema_version") == 1 and isinstance(txn.get("files"), dict) and bool(txn["files"]), "Invalid transaction")
    exact_commit(root, txn["head"])
    require(bool(re.fullmatch(r"[0-9a-f]{40}", txn["index"])), "Invalid transaction index")
    for name, values in txn["files"].items():
        transaction_path(root, name)
        require(set(values) == {"before", "after"}, "Invalid transaction blob pair")
        for value in values.values():
            require(value is None or (set(value) == {"mode", "oid"} and value["mode"] in {"100644", "100755", "120000"}
                    and bool(re.fullmatch(r"[0-9a-f]{40}", value["oid"]))), "Invalid transaction blob identity")


def apply_transaction(root: Path, txn: dict[str, Any]) -> None:
    validate_transaction(root, txn)
    require(oid(root) == txn["head"] and oid(root, ":") == txn["index"], "Acceptance HEAD/index changed")
    for name, values in txn["files"].items():
        require(blob(root, name) == values["before"], "Acceptance input changed: " + name)
    for name, values in txn["files"].items():
        restore_blob(root, name, values["after"])


def recover_transaction(root: Path) -> str:
    path = local_path(root, "acceptance.json")
    if not path.exists():
        return "no-transaction"
    txn = read_json(path)
    validate_transaction(root, txn)
    require(oid(root) == txn["head"], "HEAD changed; inspect committed acceptance before rollback")
    old_index = entries(root, txn["index"])
    current_index = entries(root, oid(root, ":"))
    for name in old_index.keys() | current_index.keys():
        if name in txn["files"]:
            after = txn["files"][name]["after"]
            require(current_index.get(name) in (old_index.get(name), (after["mode"], after["oid"]) if after else None),
                    "Third-party staged authority change preserved: " + name)
        else:
            require(current_index.get(name) == old_index.get(name), "Third-party staged change preserved: " + name)
    for name, values in txn["files"].items():
        require(blob(root, name) in (values["before"], values["after"]), "Third-party authority change preserved: " + name)
    for name, values in txn["files"].items():
        restore_blob(root, name, values["before"])
        before_index = old_index.get(name)
        if before_index:
            git(root, "update-index", "--add", "--cacheinfo", before_index[0], before_index[1], name)
        else:
            git(root, "update-index", "--force-remove", "--", name)
    path.unlink()
    return "rolled-back; review and evaluate before retry"


def validate_acceptance(root: Path) -> None:
    import importlib.util
    path = Path(__file__).resolve().parents[2] / "batch/scripts/batch.py"
    spec = importlib.util.spec_from_file_location("metaflux_batch_commit_guard", path)
    require(spec is not None and spec.loader is not None, "Batch guard is missing")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.validate_pending(root)


def commit_guard(root: Path, expected_head: str, expected_tree: str, receipt: dict[str, Any], *, kind: str) -> None:
    require(oid(root) == expected_head, "Expected HEAD changed")
    require(oid(root, ":") == expected_tree, "Expected staged tree changed")
    validate_receipt(root, receipt, kind=kind)
    import main as controller
    state_path = local_path(root, "state.json")
    require(state_path.is_file(), "A guarded commit requires an active $main skill delivery")
    state = read_json(state_path)
    require(isinstance(state, dict) and state.get("schema_version") == 1 and
            state.get("stage") == "delivery" and not state.get("commit"),
            "A guarded commit requires the current $main skill delivery stage")
    controller.validate_state(root, state)
    request = state["request"]
    require(request["kind"] == kind and request["base_revision"] == expected_head and
            receipt["base_revision"] == request["base_revision"], "Commit belongs to another operation or baseline")
    require(isinstance(state.get("receipt"), dict) and state["receipt"].get("digest") == receipt["digest"] and
            state.get("review") == receipt["review"] and request["checks"] == receipt["checks"],
            "Current operation receipt or reviewed check plan differs from the supplied commit evidence")
    controller.require_rules(root, state)
    require(read_json(local_path(root, "rules.json")) == receipt["review"]["rules"],
            "Current loaded rules differ from the reviewed commit evidence")
    staged, reviewed = entries(root, expected_tree), entries(root)
    unstaged = sorted(name for name in staged.keys() | reviewed.keys() if staged.get(name) != reviewed.get(name))
    require(not unstaged,
            "Verified content is not fully staged: " + json.dumps(unstaged, ensure_ascii=True) +
            ". Stage these reviewed paths with git add -- inside the clean Nix entry, "
            "then retry main.py step deliver with the same receipt. "
            "Do not resume, review, or rerun evaluation for staging alone.")
    if kind == "batch":
        validate_acceptance(root)
    elif kind in {"maintenance", "iteration"}:
        require(git(root, "show", expected_head + ":agent/goal.json", check=False) ==
                ((root / "agent/goal.json").read_bytes() if (root / "agent/goal.json").exists() else b""),
                "This operation may not change Goal")
    elif kind == "epoch":
        before = json.loads(git(root, "show", expected_head + ":agent/goal.json"))
        after = read_json(root / "agent/goal.json")
        old = int(before["epoch"].removeprefix("epoch-"))
        require(old < 9999 and after["epoch"] == f"epoch-{old + 1:04d}", "Epoch governance requires the next exact Epoch; no-op does not commit")
        require(after["batch"]["id"] == "batch-0001", "Epoch cutover resets Batch numbering")
        require([x["iteration"] for x in after["lanes"]] == [f"iteration-{i + 1:04d}" for i in range(len(after["lanes"]))],
                "Epoch cutover resets Iteration numbering")
