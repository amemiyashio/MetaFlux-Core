# Record Templates

Required shapes for every record type. Keep fields and status vocabularies
stable; changing a template is a governance change, not an edit.

| Template | Instantiated at |
| --- | --- |
| [milestone.md](milestone.md) | `agent/plan/MNNNN-slug/plan.md` |
| [work-item.md](work-item.md) | `agent/plan/MNNNN-slug/work/WNN-slug.md` |
| [experience.md](experience.md) | `agent/experience/ENNNN-slug.md` |
| [checkpoint.md](checkpoint.md) | `agent/progress/checkpoints/YYYY/PYYYYMMDD-NNN-slug.md` |
| [session-summary.md](session-summary.md) | `agent/sessions/YYYY/MM/SYYYYMMDD-NNN-slug/summary.md` |

Sessions are scaffolded by `tools/new-session.py`; the other records are
authored from these templates directly.
