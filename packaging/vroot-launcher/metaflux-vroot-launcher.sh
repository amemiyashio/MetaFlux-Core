#!/usr/bin/env bash
# metaflux-vroot-launcher: namespace-isolated experimental vroot presentation helper.
#
# Depends only on the experimental metaflux-vroot package metadata. Never
# selects or requires milestone-0.1.1.0 metaflux-vpci-dkms. Does not create
# vendor nodes, hide vendor devices, or enter launch/copy steady state.

set -euo pipefail

usage() {
  cat <<'EOF'
usage: metaflux-vroot-launcher COMMAND [args]

Commands:
  describe-package METADATA_JSON
      Validate package-metadata.json and print the frozen presentation identity.
  plan-namespace METADATA_JSON MOUNT_ROOT
      Emit a namespace plan that binds only MetaFlux canonical node contracts.
  assert-no-vpci-dependency METADATA_JSON
      Fail if metadata lists metaflux-vpci packages as dependencies.
  help
      Show this help text.

The launcher never loads kernel modules, never creates /dev/nvidia*, and never
starts compute work through metaflux_vroot.ko.
EOF
}

fail() {
  printf 'metaflux-vroot-launcher: %s\n' "$*" >&2
  exit 1
}

require_file() {
  local path=$1
  [[ -f "$path" ]] || fail "missing file: $path"
}

python_metadata_check() {
  local metadata=$1
  local mode=$2
  local mount_root=${3:-}
  PYTHONPATH= python3 - "$metadata" "$mode" "$mount_root" <<'PY'
import json
import sys
from pathlib import Path

metadata_path = Path(sys.argv[1])
mode = sys.argv[2]
mount_root = sys.argv[3]

try:
    document = json.loads(metadata_path.read_text(encoding="utf-8"))
except (OSError, json.JSONDecodeError) as error:
    raise SystemExit(f"invalid metadata: {error}") from error

if document.get("id") != "metaflux-vroot-dkms":
    raise SystemExit("metadata id must be metaflux-vroot-dkms")
if document.get("kind") != "experimental-vroot-dkms-source":
    raise SystemExit("metadata kind must be experimental-vroot-dkms-source")
if document.get("autoinstall") is not False:
    raise SystemExit("experimental vroot must keep AUTOINSTALL disabled")
if document.get("depends_on") not in ([], None):
    raise SystemExit("vroot package must declare no package dependencies")

forbidden = set(document.get("forbidden_dependencies") or [])
required_forbidden = {"metaflux-vpci-dkms", "metaflux-vpci"}
if not required_forbidden.issubset(forbidden):
    raise SystemExit("forbidden_dependencies must include metaflux-vpci artifacts")

policy = document.get("policy") or {}
if policy.get("presentation_only") is not True:
    raise SystemExit("policy.presentation_only must be true")
if policy.get("launch_path") != "forbidden":
    raise SystemExit("policy.launch_path must remain forbidden")
if policy.get("vendor_matching") is not False:
    raise SystemExit("policy.vendor_matching must be false")

nodes = policy.get("canonical_nodes") or []
if "/dev/metafluxctl" not in nodes:
    raise SystemExit("canonical_nodes must retain /dev/metafluxctl")

profile = document.get("profile") or {}
if profile.get("vendor_id") != 0x4D46 or profile.get("device_id") != 0x0001:
    raise SystemExit("profile identity is not the CI Type-0 lock")
if profile.get("class_code") != 0x120000:
    raise SystemExit("profile class_code is not 0x120000")

if mode == "describe":
    print(json.dumps({
        "package": document["id"],
        "version": document["package_version"],
        "module": document["module"],
        "identity": {
            "vendor_id": profile["vendor_id"],
            "device_id": profile["device_id"],
            "class_code": profile["class_code"],
        },
        "autoinstall": document["autoinstall"],
        "depends_on": document.get("depends_on") or [],
        "forbidden_dependencies": sorted(forbidden),
        "canonical_nodes": nodes,
        "launch_path": policy["launch_path"],
    }, indent=2, sort_keys=True))
elif mode == "assert-no-vpci":
    print("no-vpci-dependency: ok")
elif mode == "plan-namespace":
    if not mount_root or not mount_root.startswith("/"):
        raise SystemExit("MOUNT_ROOT must be an absolute path")
    lowered = mount_root.lower()
    if "nvidia" in lowered:
        raise SystemExit("MOUNT_ROOT must not target vendor nvidia paths")
    plan = {
        "mount_root": mount_root,
        "bind_only": [
            {"source": "/dev/metafluxctl", "target": f"{mount_root}/dev/metafluxctl"},
            {"source": "/dev/metaflux0", "target": f"{mount_root}/dev/metaflux0"},
        ],
        "forbidden_targets": [
            "/dev/nvidia0",
            "/dev/nvidiactl",
            "/dev/nvidia-uvm",
            "/dev/nvidia-uvm-tools",
        ],
        "module_load": "operator-explicit",
        "compute_entry": "forbidden-through-vroot",
    }
    print(json.dumps(plan, indent=2, sort_keys=True))
else:
    raise SystemExit(f"unknown mode: {mode}")
PY
}

main() {
  local command=${1:-help}
  case "$command" in
    help|-h|--help)
      usage
      ;;
    describe-package)
      [[ $# -eq 2 ]] || fail "describe-package requires METADATA_JSON"
      require_file "$2"
      python_metadata_check "$2" describe
      ;;
    assert-no-vpci-dependency)
      [[ $# -eq 2 ]] || fail "assert-no-vpci-dependency requires METADATA_JSON"
      require_file "$2"
      python_metadata_check "$2" assert-no-vpci
      ;;
    plan-namespace)
      [[ $# -eq 3 ]] || fail "plan-namespace requires METADATA_JSON MOUNT_ROOT"
      require_file "$2"
      python_metadata_check "$2" plan-namespace "$3"
      ;;
    *)
      usage
      fail "unknown command: $command"
      ;;
  esac
}

main "$@"
