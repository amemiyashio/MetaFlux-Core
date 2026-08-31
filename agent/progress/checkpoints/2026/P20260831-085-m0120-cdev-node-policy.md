---
id: P20260831-085
status: Recorded
captured: 2026-08-31
milestone: M0120
workstream: W0123
branch: main
git_revision: 6839fa9e9eec2cd57fe43e813b29cda7b2cfaa96
workspace: canonical MetaFlux cdev udev/node policy
---

# M0120 W0123 Cdev Node Policy Checkpoint

## Outcome

The Daemon component now installs one canonical `70-metaflux.rules` policy for
the existing kernel-owned MetaFlux cdev nodes. It matches only the `misc`
nodes `metafluxctl` and `metaflux[0-9]*`, assigns group `metaflux` and mode
`0660`, and creates no nodes or vendor aliases. The complete-package payload,
tar lifecycle metadata, and removal checks require and remove this exact rule.

## Verification evidence

| Gate | Result |
|---|---|
| Vulkan configure/build and CTest | Passed: `.#vulkan-runtime`, 91/91 |
| Generic release configure/build | Passed: Ubuntu 20.04 target SDK; highest referenced glibc symbol `GLIBC_2.29` |
| Daemon install payload | Passed: rule installed at `/usr/lib/udev/rules.d/70-metaflux.rules`; both exact rules matched |
| Python and records checks | Passed: `py_compile` for packaging/release scripts and `check-agent-records.py` |
| Diff checks | Passed: `git diff --check` |
| Content identity | `6839fa9`; Agent Harness (codex) is Author and Committer |

## Boundary

This checkpoint covers package-owned policy for existing cdev nodes. It does
not create or rename nodes, add NVIDIA or namespace aliases, qualify a live
udev daemon or kernel module, run native NixOS VM packaging, or close the
W0123 concurrent three-transport/fault and lifecycle ABI-freeze gates.

## Cleanup

- Removed: none; no repository-local build output or source snapshot was added.
- Retained: external build trees under the existing build owner and foreign
  guidance packets in the active S01322/S01323 inboxes.

## roast

### light roasts

- Canonical cdev udev policy -> `packaging/common/70-metaflux.rules`
  (`6839fa9`, exact install and payload assertions)

### medium roasts

- W0123 node policy boundary ->
  `agent/plan/M0120-vpci-lifecycle/work/W0123-core-qualification.md` (P085)

### dark roasts

- none.

## session-only

- none.

## Handoff

Resume W0123 from P085 and `6839fa9`. The next bounded unit is the live
multi-transport reset/remove/add and fault qualification; preserve the kernel
node authority and keep W0124 namespace/NVIDIA aliases out of this policy.
