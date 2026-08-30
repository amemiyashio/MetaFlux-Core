# Notes

The user selected three binding design choices before implementation:

- trigger immediately after a collaborator delivers durable changes, with a
  checkpoint-time fallback;
- automatically repair compatible gaps only when the current integration
  session owns the exact change set, while another active session retains
  source ownership and receives transient guidance;
- provide a read-only, JSON-only inventory helper that separates committed,
  staged, unstaged, and untracked layers without storing diffs or inferring
  ownership from authors, paths, processes, or timestamps.

The durable slug is `converge-project-changes`. This is an additive workflow,
not a replacement of established record meaning or authority, so D0025/SC
migration authority is not required. No open-decision row is resolved.
