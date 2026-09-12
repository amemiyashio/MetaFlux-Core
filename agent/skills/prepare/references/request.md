# Bounded Request

Run `main.py inspect`, then `begin --request-json JSON` through the clean Nix
entry. Inline JSON avoids a temporary-input write before preparation.

Request schema 1 contains:
- `schema_version: 1`, `kind`: maintenance, iteration, batch or epoch.
- `objective`: concrete outcome preserving user intent.
- `base_revision`: full current HEAD.
- `allowed_paths`: repository-relative exact files or trailing-slash subtrees.
- `checks`: the formal plan; see [$verify](../../verify/SKILL.md) skill.
- `publication`: auto, or local when requested by the user.
- Optional `skills`: additional required owners, supplementing path ownership.
- Iteration/Batch `assignment`: exact epoch, batch, iteration and lane from the
  application; it must match Goal and the current execution context.
- Epoch `confirmation`: quote the explicit governance request or already
  approved unchanged proposal. A JSON field never manufactures authorization.

Batch requests omit `checks`; [$batch](../../batch/SKILL.md) skill owns its fixed
final metadata plan and separate integration checks. For read-only work call
inspect directly; `begin {"kind":"read-only"}` also preserves an active operation.

Begin captures existing edits. Immediately load preparation rules and enter
prepared before further edits; then load implementation rules. Paths must stay
within the repository, outside Git and ignored Agent state. Necessary omitted
companions use the recovery skill's rescope with renewed preparation/review.
