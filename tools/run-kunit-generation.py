#!/usr/bin/env python3
"""Run the mf_cdev_generation KUnit suite using the pinned linux-debug source.

This runner is invoked by the CTest gate metaflux.kernel.kunit-generation.
It exits 77 (CTest SKIP_RETURN_CODE) when no valid linux source is available.
When METAFLUX_LINUX_SRC is set, it builds UML via kunit.py after injecting the
out-of-tree generation tests as built-in KUnit cases. The host kernel is never
insmod'd.

METAFLUX_LINUX_SRC must point at a linux 6.12 source tree (typically the
``$out/src`` directory of the ``linux-debug-tools`` Nix package).

Exit codes
----------
  0  TAP output contains passing mf_cdev_generation cases
  77 METAFLUX_LINUX_SRC is unset or missing Makefile/kunit.py
  1  configure/build/run failed, or the suite did not appear in TAP
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def _skip(message: str) -> int:
    print(f"SKIP: {message}", file=sys.stderr)
    return 77


def _source_root() -> Path | None:
    src_value = os.environ.get("METAFLUX_LINUX_SRC", "").strip()
    if not src_value:
        return None
    src_root = Path(src_value).resolve()
    if not (src_root / "Makefile").exists():
        return None
    if not (src_root / "tools" / "testing" / "kunit" / "kunit.py").exists():
        return None
    return src_root


def _symlink_children(source: Path, dest: Path, skip: set[str]) -> None:
    dest.mkdir(parents=True, exist_ok=True)
    for entry in source.iterdir():
        if entry.name in skip:
            continue
        os.symlink(entry, dest / entry.name)


def _prepare_overlay(src_root: Path, overlay: Path, repo_root: Path) -> None:
    """Writable kernel tree that injects mf_cdev_generation as built-in KUnit.

    The kunit harness itself is copied as real files because kunit.py derives
    the kernel root from its own realpath (``get_kernel_root_path``); a
    symlinked harness would keep pointing the build at the read-only store
    source, and the injected lib/kunit changes would never participate.
    """
    _symlink_children(src_root, overlay, skip={"lib", "tools"})

    tools_src = src_root / "tools"
    tools_dest = overlay / "tools"
    _symlink_children(tools_src, tools_dest, skip={"testing"})
    testing_src = tools_src / "testing"
    testing_dest = tools_dest / "testing"
    _symlink_children(testing_src, testing_dest, skip={"kunit"})

    kunit_tool_src = testing_src / "kunit"
    kunit_tool_dest = testing_dest / "kunit"
    shutil.copytree(
        kunit_tool_src,
        kunit_tool_dest,
        ignore=shutil.ignore_patterns("__pycache__"),
    )
    # Store copies are read-only; the harness patch below must be writable.
    for copied in kunit_tool_dest.rglob("*"):
        if copied.is_file():
            copied.chmod(copied.stat().st_mode | 0o200)
    # The non-PIE Nix-built UML binary carries the Nix loader as its
    # interpreter, but its libc search still lands on the host /lib64, which
    # fails on GLIBC_PRIVATE symbols.  Replace the copied harness UML start()
    # so the kernel process gets METAFLUX_UML_LIB prepended to its
    # LD_LIBRARY_PATH.  The build environment itself stays clean: exporting
    # LD_LIBRARY_PATH during the kunit.py build aborts the toolchain, and the
    # runner builds with NIX_HARDENING_ENABLE minus "pie" because PIE load
    # bases overflow UML's exec-shield memory accounting.
    kernel_py = kunit_tool_dest / "kunit_kernel.py"
    text = kernel_py.read_text(encoding="utf-8")
    start_marker = "params.extend(['mem=1G', 'console=tty', 'kunit_shutdown=halt'])"
    end_marker = "\ndef get_kconfig_path"
    try:
        begin = text.index(start_marker)
        finish = text.index(end_marker, begin)
    except ValueError as error:
        raise RuntimeError(
            "kunit_kernel.py UML start() anchors not found; "
            "cannot patch loader/ASLR handling"
        ) from error
    block = (
        start_marker + "\n"
        "\t\tprint('Running tests with:', linux_bin)\n"
        "\t\tuml_lib = os.environ.get('METAFLUX_UML_LIB')\n"
        "\t\tenv = dict(os.environ)\n"
        "\t\tif uml_lib:\n"
        "\t\t\tprior = env.get('LD_LIBRARY_PATH')\n"
        "\t\t\tenv['LD_LIBRARY_PATH'] = uml_lib + (':' + prior if prior else '')\n"
        "\t\treturn subprocess.Popen([linux_bin] + params,\n"
        "\t\t\t\t   stdin=subprocess.PIPE,\n"
        "\t\t\t\t   stdout=subprocess.PIPE,\n"
        "\t\t\t\t   stderr=subprocess.STDOUT,\n"
        "\t\t\t\t   text=True, errors='backslashreplace',\n"
        "\t\t\t\t   env=env)\n"
    )
    text = text[:begin] + block + text[finish:]
    kernel_py.write_text(text, encoding="utf-8")

    lib_src = src_root / "lib"
    lib_dest = overlay / "lib"
    _symlink_children(lib_src, lib_dest, skip={"kunit"})
    kunit_src = lib_src / "kunit"
    kunit_dest = lib_dest / "kunit"
    _symlink_children(kunit_src, kunit_dest, skip={"Makefile"})
    makefile = (kunit_src / "Makefile").read_text(encoding="utf-8")
    if "mf_cdev_generation_test.o" not in makefile:
        makefile += "\nobj-$(CONFIG_KUNIT) += mf_cdev_generation_test.o\n"
    (kunit_dest / "Makefile").write_text(makefile, encoding="utf-8")
    shutil.copy2(
        repo_root / "kernel" / "tests" / "kunit" / "mf_cdev_generation_test.c",
        kunit_dest / "mf_cdev_generation_test.c",
    )
    shutil.copy2(
        repo_root / "kernel" / "core" / "mf_cdev_generation.h",
        kunit_dest / "mf_cdev_generation.h",
    )


def _output_has_generation(text: str) -> bool:
    return "mf_cdev_generation" in text


def main() -> int:
    src_root = _source_root()
    if src_root is None:
        return _skip("METAFLUX_LINUX_SRC is not set or missing Makefile/kunit.py")

    repo_root = Path(__file__).resolve().parent.parent
    kunitconfig = repo_root / "kernel" / "tests" / "kunit" / "kunitconfig"
    if not kunitconfig.exists():
        return _skip(f"repo kunitconfig not found at {kunitconfig}")
    test_c = repo_root / "kernel" / "tests" / "kunit" / "mf_cdev_generation_test.c"
    test_h = repo_root / "kernel" / "core" / "mf_cdev_generation.h"
    if not test_c.exists() or not test_h.exists():
        return _skip("generation KUnit sources are missing from the repository")

    kunit_py = src_root / "tools" / "testing" / "kunit" / "kunit.py"
    with tempfile.TemporaryDirectory(prefix="metaflux-kunit-") as workdir:
        overlay = Path(workdir) / "src"
        build_dir = Path(workdir) / "build"
        overlay.mkdir()
        build_dir.mkdir()
        _prepare_overlay(src_root, overlay, repo_root)
        env = os.environ.copy()
        gcc = shutil.which("gcc")
        if gcc:
            probe = subprocess.run(
                [gcc, "-print-file-name=libc.so.6"],
                capture_output=True,
                text=True,
                check=False,
            )
            libc = Path(probe.stdout.strip())
            if probe.returncode == 0 and libc.is_file():
                glibc_lib = libc.parent
                # Consumed only by the patched UML start(): the kernel
                # process needs the Nix glibc on LD_LIBRARY_PATH, while the
                # kunit.py/make build environment must stay clean.
                env["METAFLUX_UML_LIB"] = str(glibc_lib)
        # PIE load bases overflow UML's exec-shield memory accounting
        # ("Too few physical memory"), so build the UML kernel non-PIE.
        env["NIX_HARDENING_ENABLE"] = "fortify stackprotector relro bindnow"
        result = subprocess.run(
            [
                sys.executable,
                "-B",
                str(overlay / "tools" / "testing" / "kunit" / "kunit.py"),
                "run",
                "--build_dir",
                str(build_dir),
                "--kunitconfig",
                str(kunitconfig),
                "--arch",
                "um",
                "--timeout",
                "300",
            ],
            cwd=str(overlay),
            capture_output=True,
            text=True,
            env=env,
        )
        combined = (result.stdout or "") + (result.stderr or "")
        if result.stdout:
            sys.stdout.write(result.stdout)
        if result.stderr:
            sys.stderr.write(result.stderr)
        if result.returncode != 0:
            return 1 if result.returncode != 77 else 77
        if not _output_has_generation(combined):
            log_path = build_dir / "test.log"
            if log_path.exists():
                log_text = log_path.read_text(encoding="utf-8", errors="replace")
                sys.stdout.write(log_text)
                combined += log_text
            if not _output_has_generation(combined):
                print(
                    "ERROR: no mf_cdev_generation KUnit cases found in output. "
                    "In-tree UML KUnit without this suite is not a pass.",
                    file=sys.stderr,
                )
                return 1
        return 0


if __name__ == "__main__":
    sys.exit(main())
