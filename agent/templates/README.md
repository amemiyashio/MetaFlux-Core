# Record Templates

Required shapes for every record type. Keep fields and status vocabularies
stable; changing a template is a governance change, not an edit.

| Template | Instantiated at |
| --- | --- |
| [milestone.md](milestone.md) | `agent/plan/M<delivery>-slug/plan.md` |
| [work-item.md](work-item.md) | `agent/plan/M<delivery>-slug/work/W<delivery>-slug.md` |
| [experience.md](experience.md) | `agent/experience/ENNNN-slug.md` |
| [semantic-change.md](semantic-change.md) | `agent/semantic-changes/SCNNNN-slug.md` |
| [checkpoint.md](checkpoint.md) | `agent/progress/checkpoints/YYYY/PYYYYMMDD-NNN-slug.md` |
| [session-summary.md](session-summary.md) | `agent/sessions/YYYY/MM/S<delivery>-YYYYMMDD-NNN-slug/summary.md` |

Sessions are scaffolded by `tools/new-session.py`; the other records are
authored from these templates directly.
