---
name: push-repository
description: Configure, validate, or push an exact committed MetaFlux revision to its canonical GitHub repository through the governed SSH key. Use after a published Epoch activation or automatic acceptance commit is on disk, or when the user or application asks about repository push transport or explicitly requests a push. Do not use for ordinary Iteration commits, release packaging, or branch allocation.
---

# Push Repository

Own the external Git transport boundary after `start-work`. An ordinary
Iteration commit does not imply permission to push. After a published Epoch
activation commit, or an automatic acceptance commit that updated `goal.json`
lane or Batch state, is on disk, the governing or accepting parent must invoke
this skill with that commit's full object ID. Configure or inspect transport
when requested. Other remote mutations still require an explicit user or
application request and one full commit object ID.

Do not treat an ordinary Iteration commit, an unreviewed subagent diff, a dirty
worktree, or a conversation-inferred `HEAD` as authorization to push. Coding
subagents never push.

## Canonical Transport

[`transport.json`](transport.json) is the machine-readable authority:

- remote `origin` is `git@github.com:amemiyashio/MetaFlux-Core.git`;
- the private/public key pair resolves from the repository as
  `../../keys/github-ssh-key{,.pub}`;
- the public fingerprint must match the pinned ED25519 fingerprint;
- the only destination is `refs/heads/main`;
- SSH uses only that identity, batch mode, strict known-host verification, and
  no user SSH configuration.

The private key remains external host state. Never read it as text, copy it,
stage it, print it, place it in an environment variable, or persist its bytes.
Only validate its type and permission bits and pass its absolute path to SSH.
The public key fingerprint is non-secret and may be reported.

Git and OpenSSH must resolve from the Git-aware `nix develop .` environment.
The locked nixpkgs input fixes their current versions; Nix only supplies the
tools. Git retains remote, ref, object, and push semantics.

## Configure And Check

Run the helper from the repository root:

```sh
nix develop . --command python3 -B \
  agent/skills/push-repository/scripts/push_repository.py configure
nix develop . --command python3 -B \
  agent/skills/push-repository/scripts/push_repository.py check
```

`configure` first validates the existing fetch URL, Nix tools, key metadata,
and public fingerprint. It then writes only these repository-local Git values:

- `remote.origin.pushurl`;
- `core.sshCommand` with the current Nix OpenSSH executable and canonical key;
- `ssh.variant=ssh`;
- `push.default=nothing`.

It does not change the fetch URL, global SSH configuration, user Git config,
branches, upstreams, credentials, or tracked files. Re-run it after a Nix
revision changes the OpenSSH store path. `check --remote-access` may perform a
read-only authenticated `ls-remote`; ordinary `check` is local-only.

## Push One Revision

Require the full commit object ID. After Epoch publication or Batch
integration, that ID is the just-committed object resolved immediately with
`git rev-parse HEAD` in the governance or integration context. For any other
push, the user or application supplies the OID. Never invent a short hash or
infer a revision from a dirty tree. The helper resolves and verifies that
object, revalidates the configured transport, and pushes exactly
`COMMIT:refs/heads/main` without force:

```sh
nix develop . --command python3 -B \
  agent/skills/push-repository/scripts/push_repository.py push \
  --revision FULL_COMMIT_OID
```

Use `--dry-run` only when remote validation without mutation was requested.
Never infer a revision from dirty files, an ambient branch, another worktree,
or a conversation. Never allocate another local or remote branch, add a remote,
force a non-fast-forward update, push tags, broaden the refspec, or retry
unchanged network/authentication evidence. An explicit first push may initialize
the one policy-fixed `refs/heads/main`; this is not permission to choose another
destination. Preserve raw Git/SSH output.

## Task Stops

Use the `start-work` diagnostic contract. Important classifications are:

- `git-publish.nix-tool-invalid`: `current-agent / fix-and-retry`; restore Git
  or OpenSSH to the repository Nix environment.
- `git-publish.remote-mismatch`: `user-or-application / preserve-and-report`;
  leave remotes and revisions unchanged until the canonical target is supplied.
- `git-publish.key-invalid`: `host-operator / stop-and-report`; restore the
  external key pair, private permissions, or pinned public fingerprint.
- `git-publish.local-config-mismatch`: `current-agent / fix-and-retry`; run the
  bounded `configure` mode, then repeat the check.
- `git-publish.revision-invalid`: `user-or-application / preserve-and-report`;
  supply one full existing commit object ID.
- `git-publish.remote-command-failed`: `user-or-application /
  preserve-and-report`; retain the local revision and remote output, then retry
  only after authentication, host trust, connectivity, or remote state changes.

None of these diagnostics authorizes branch/worktree/clone/task/thread
creation, force push, credential prompting, privilege escalation, or cleanup.

## Verification

```sh
nix develop . --command git --version
nix develop . --command ssh -V
nix develop . --command python3 -B \
  agent/skills/push-repository/scripts/test_push_repository.py
nix develop . --command python3 -B tools/check-agent-state.py .
```
