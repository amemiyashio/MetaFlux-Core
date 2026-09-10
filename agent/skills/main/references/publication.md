# Exact Publication

`../transport.json` pins canonical origin, one external SSH identity and public
fingerprint, and `refs/heads/main`. Shared `../scripts/push_repository.py`
retains configure, check, check --remote-access, exact push, and dry-run modes.
Identity inspection uses metadata and the public key only. Never read private
key bytes, change the fetch remote, force, push tags, or broaden the refspec.

Maintenance, accepted Batch transitions, and Epoch activation automatically
publish their guarded commit unless the user requested local-only work. A
worker's ordinary candidate goes to Batch instead. Diagnostic requests remain
read-only unless configuration repair or publication was requested.

Check Git/OpenSSH inside Nix, configure only the allowed repository-local
transport fields, and push the exact full committed OID. Return both the
published revision and observed remote main revision. Equality proves exact
publication; a differing remote tip needs a governed object fetch and ancestry
proof before reporting already-published. If the remote is ahead, require the
application's matching updated execution context before dispatch. Divergence
preserves the local candidate and reports the exact transport diagnostic.

An interrupted or failed push never causes another acceptance or commit.
Retry the same revision only after the responsible prerequisite changes, or
after a read-only check proves a lost response actually completed publication.
