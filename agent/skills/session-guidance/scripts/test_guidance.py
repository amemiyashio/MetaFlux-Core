#!/usr/bin/env python3
"""Behavioral self-tests for the session-guidance CLI."""

from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).with_name("guidance.py").resolve()
SESSION_ID = "S0100-20260830-001-guidance-test"


class GuidanceCliTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(prefix="metaflux-guidance-test-")
        self.repo = Path(self.temporary.name)
        self.session = (
            self.repo / "agent" / "sessions" / "2026" / "08" / SESSION_ID
        )
        self.session.mkdir(parents=True)
        self.write_session("in_progress", None)
        (self.session / "events.jsonl").write_text("", encoding="utf-8")

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def write_session(self, status: str, ended_at: str | None) -> None:
        document = {
            "schema_version": 1,
            "id": SESSION_ID,
            "delivery": "0.1.0.0",
            "status": status,
            "ended_at": ended_at,
            "event_log": "events.jsonl",
        }
        (self.session / "session.json").write_text(
            json.dumps(document) + "\n", encoding="utf-8"
        )

    def run_cli(self, *arguments: str, success: bool = True) -> subprocess.CompletedProcess[str]:
        result = subprocess.run(
            [sys.executable, str(SCRIPT), "--repo", str(self.repo), *arguments],
            text=True,
            capture_output=True,
            check=False,
        )
        if success and result.returncode != 0:
            self.fail(f"CLI failed ({result.returncode}): {result.stderr}")
        if not success and result.returncode == 0:
            self.fail(f"CLI unexpectedly passed: {result.stdout}")
        return result

    def create_complete(self, slug: str = "cache-review", *extra: str) -> Path:
        result = self.run_cli(
            "create",
            "--session",
            SESSION_ID,
            "--slug",
            slug,
            "--author",
            "A002",
            "--role",
            "compiler reviewer",
            "--scope",
            "cache publication",
            "--direction",
            "Retain the epoch in the cache key.",
            "--evidence",
            "The cache test demonstrates cross-epoch rejection.",
            "--constraints",
            "D0018 remains fixed.",
            "--verification",
            "Run the cache regression test.",
            *extra,
        )
        return self.repo / result.stdout.strip()

    def append_disposition(
        self,
        guidance_id: str,
        disposition: str,
        **extra: str,
    ) -> None:
        events_path = self.session / "events.jsonl"
        sequence = len(events_path.read_text(encoding="utf-8").splitlines()) + 1
        event = {
            "schema_version": 1,
            "seq": sequence,
            "timestamp": "2026-08-30",
            "type": "work_note",
            "actor": "A001",
            "content": "Verified and handled specialist direction.",
            "guidance_id": guidance_id,
            "disposition": disposition,
            **extra,
        }
        with events_path.open("a", encoding="utf-8") as stream:
            stream.write(json.dumps(event) + "\n")

    def write_semantic_change_handoff(
        self,
        guidance_id: str,
        *,
        sc_id: str = "SC0001",
    ) -> None:
        semantic_changes = self.repo / "agent" / "semantic-changes"
        semantic_changes.mkdir(parents=True, exist_ok=True)
        (semantic_changes / f"{sc_id}-fixture.md").write_text(
            "# Fixture semantic change\n\n"
            "## Active-session handoff\n\n"
            "| Session | Guidance | Status | Outcome |\n"
            "| --- | --- | --- | --- |\n"
            f"| `{SESSION_ID}` | `{guidance_id}` | Published | Fixture handoff |\n\n"
            "## Evidence preservation\n\nFixture.\n",
            encoding="utf-8",
        )

    def test_complete_lifecycle_and_patch_cleanup(self) -> None:
        source_patch = self.repo / "candidate.patch"
        source_patch.write_text("diff --git a/a b/a\n", encoding="utf-8")
        draft = self.create_complete("cache-review", "--patch", str(source_patch))
        self.assertEqual(draft.name, "G001-cache-review.draft.md")
        attachment = draft.with_name("G001-cache-review.patch")
        self.assertTrue(attachment.is_file())
        self.assertEqual(list(draft.parent.glob(".*.tmp")), [])

        published = self.run_cli("publish", str(draft))
        ready = self.repo / published.stdout.strip()
        self.assertEqual(ready.name, "G001-cache-review.ready.md")

        listed = self.run_cli("list", "--session", SESSION_ID, "--json")
        entries = json.loads(listed.stdout)
        self.assertEqual(entries[0]["guidance_id"], "G001")
        self.assertEqual(entries[0]["state"], "ready")

        claimed = self.run_cli("claim", str(ready))
        processing = self.repo / claimed.stdout.strip()
        self.assertEqual(processing.name, "G001-cache-review.processing.md")
        self.append_disposition("G001", "adapted")
        resolved = self.run_cli(
            "resolve", str(processing), "--disposition", "adapted"
        )
        self.assertIn("resolved G001 as adapted", resolved.stdout)
        self.assertFalse(processing.exists())
        self.assertFalse(attachment.exists())

    def test_terminal_session_is_rejected(self) -> None:
        self.write_session("complete", "2026-08-30")
        result = self.run_cli(
            "create",
            "--session",
            SESSION_ID,
            "--slug",
            "late-guidance",
            "--author",
            "A002",
            "--role",
            "reviewer",
            "--scope",
            "closed work",
            success=False,
        )
        self.assertIn("target session is not active", result.stderr)

    def test_session_delivery_must_match_scoped_id(self) -> None:
        document = json.loads((self.session / "session.json").read_text(encoding="utf-8"))
        document["delivery"] = "0.1.0.1"
        (self.session / "session.json").write_text(
            json.dumps(document) + "\n", encoding="utf-8"
        )
        result = self.run_cli(
            "list", "--session", SESSION_ID, "--state", "all", success=False
        )
        self.assertIn("delivery does not match its session ID", result.stderr)

    def test_conflicting_packet_numbers_are_rejected(self) -> None:
        first = self.create_complete()
        conflict = first.with_name("G001-other.ready.md")
        conflict.write_text(first.read_text(encoding="utf-8"), encoding="utf-8")
        result = self.run_cli(
            "list", "--session", SESSION_ID, "--state", "all", success=False
        )
        self.assertIn("conflicting guidance packet IDs", result.stderr)

    def test_no_material_discard_needs_no_event(self) -> None:
        draft = self.create_complete("duplicate-guidance")
        result = self.run_cli(
            "resolve",
            str(draft),
            "--no-material",
            "--reason",
            "duplicate of current source evidence",
        )
        self.assertIn("discarded G001 with no material outcome", result.stdout)
        self.assertFalse(draft.exists())

    def test_no_material_id_is_reused_without_a_durable_reference(self) -> None:
        draft = self.create_complete("duplicate-guidance")
        self.run_cli(
            "resolve",
            str(draft),
            "--no-material",
            "--reason",
            "duplicate of current source evidence",
        )
        replacement = self.create_complete("replacement-guidance")
        self.assertEqual(replacement.name, "G001-replacement-guidance.draft.md")

    def test_semantic_change_handoff_reserves_cleaned_id(self) -> None:
        self.write_semantic_change_handoff("G001")
        draft = self.create_complete("semantic-successor")
        self.assertEqual(draft.name, "G002-semantic-successor.draft.md")

    def test_semantic_change_handoff_and_events_allocate_g004(self) -> None:
        self.append_disposition("G001", "adopted")
        self.append_disposition("G002", "adapted")
        self.write_semantic_change_handoff("G003")
        draft = self.create_complete("completion-successor")
        self.assertEqual(draft.name, "G004-completion-successor.draft.md")

    def test_material_resolution_requires_matching_event(self) -> None:
        draft = self.create_complete()
        ready = self.repo / self.run_cli("publish", str(draft)).stdout.strip()
        processing = self.repo / self.run_cli("claim", str(ready)).stdout.strip()
        missing = self.run_cli(
            "resolve",
            str(processing),
            "--disposition",
            "adopted",
            success=False,
        )
        self.assertIn("has no 'G001' disposition 'adopted'", missing.stderr)
        self.append_disposition("G001", "deferred")
        incomplete = self.run_cli(
            "resolve",
            str(processing),
            "--disposition",
            "deferred",
            success=False,
        )
        self.assertIn("deferred guidance requires deferred_to", incomplete.stderr)

    def test_created_at_requires_utc_offset(self) -> None:
        draft = self.create_complete()
        content = draft.read_text(encoding="utf-8")
        content = content.replace(
            next(line for line in content.splitlines() if line.startswith("created_at:")),
            'created_at: "2026-08-30T12:00:00"',
        )
        draft.write_text(content, encoding="utf-8")
        result = self.run_cli("publish", str(draft), success=False)
        self.assertIn("created_at must include a UTC offset", result.stderr)

    def test_supersedes_must_name_lower_number(self) -> None:
        draft = self.create_complete()
        content = draft.read_text(encoding="utf-8").replace(
            "supersedes: []", 'supersedes: ["G002"]'
        )
        draft.write_text(content, encoding="utf-8")
        result = self.run_cli("publish", str(draft), success=False)
        self.assertIn("only lower-numbered IDs", result.stderr)

    def test_resolution_recovers_after_attachment_was_removed(self) -> None:
        source_patch = self.repo / "candidate.patch"
        source_patch.write_text("diff --git a/a b/a\n", encoding="utf-8")
        draft = self.create_complete("retry-cleanup", "--patch", str(source_patch))
        ready = self.repo / self.run_cli("publish", str(draft)).stdout.strip()
        processing = self.repo / self.run_cli("claim", str(ready)).stdout.strip()
        processing.with_name("G001-retry-cleanup.patch").unlink()
        self.append_disposition("G001", "adopted")
        self.run_cli("resolve", str(processing), "--disposition", "adopted")
        self.assertFalse(processing.exists())

    def test_duplicate_disposition_events_are_rejected(self) -> None:
        draft = self.create_complete()
        ready = self.repo / self.run_cli("publish", str(draft)).stdout.strip()
        processing = self.repo / self.run_cli("claim", str(ready)).stdout.strip()
        self.append_disposition("G001", "adopted")
        self.append_disposition("G001", "adopted")
        result = self.run_cli(
            "resolve", str(processing), "--disposition", "adopted", success=False
        )
        self.assertIn("duplicate disposition for G001", result.stderr)
        self.assertTrue(processing.exists())

    def test_material_resolution_rejects_malformed_event_log(self) -> None:
        draft = self.create_complete()
        ready = self.repo / self.run_cli("publish", str(draft)).stdout.strip()
        processing = self.repo / self.run_cli("claim", str(ready)).stdout.strip()
        malformed = {
            "schema_version": 1,
            "seq": 1,
            "timestamp": "2026-08-30",
            "type": "work_note",
            "content": "Missing actor.",
            "guidance_id": "G001",
            "disposition": "adopted",
        }
        events_path = self.session / "events.jsonl"
        events_path.write_text(json.dumps(malformed) + "\n", encoding="utf-8")
        missing_actor = self.run_cli(
            "resolve", str(processing), "--disposition", "adopted", success=False
        )
        self.assertIn("missing fields: actor", missing_actor.stderr)
        self.assertTrue(processing.exists())

        malformed["actor"] = "A001"
        malformed["seq"] = 2
        events_path.write_text(json.dumps(malformed) + "\n", encoding="utf-8")
        sequence_gap = self.run_cli(
            "resolve", str(processing), "--disposition", "adopted", success=False
        )
        self.assertIn("expected contiguous seq 1, got 2", sequence_gap.stderr)
        self.assertTrue(processing.exists())

    def test_material_resolution_rejects_symlink_event_log(self) -> None:
        draft = self.create_complete()
        ready = self.repo / self.run_cli("publish", str(draft)).stdout.strip()
        processing = self.repo / self.run_cli("claim", str(ready)).stdout.strip()
        self.append_disposition("G001", "adopted")

        events_path = self.session / "events.jsonl"
        external = self.repo / "external-events.jsonl"
        external.write_bytes(events_path.read_bytes())
        events_path.unlink()
        events_path.symlink_to(external)

        result = self.run_cli(
            "resolve", str(processing), "--disposition", "adopted", success=False
        )
        self.assertIn("event_log escapes its session directory", result.stderr)
        self.assertTrue(processing.exists())

    def test_material_resolution_rejects_escaping_output_parent(self) -> None:
        draft = self.create_complete()
        ready = self.repo / self.run_cli("publish", str(draft)).stdout.strip()
        processing = self.repo / self.run_cli("claim", str(ready)).stdout.strip()

        external_outputs = self.repo / "external-outputs"
        external_outputs.mkdir()
        payload = b"verified output\n"
        (external_outputs / "0001.txt").write_bytes(payload)
        (self.session / "outputs").symlink_to(external_outputs, target_is_directory=True)
        event = {
            "schema_version": 1,
            "seq": 1,
            "timestamp": "2026-08-30",
            "type": "work_note",
            "actor": "A001",
            "output_ref": {
                "path": "outputs/0001.txt",
                "bytes": len(payload),
                "sha256": hashlib.sha256(payload).hexdigest(),
            },
            "guidance_id": "G001",
            "disposition": "adopted",
        }
        (self.session / "events.jsonl").write_text(
            json.dumps(event) + "\n", encoding="utf-8"
        )

        result = self.run_cli(
            "resolve", str(processing), "--disposition", "adopted", success=False
        )
        self.assertIn("output_ref.path escapes its session directory", result.stderr)
        self.assertTrue(processing.exists())

    def test_session_json_symlink_is_rejected(self) -> None:
        draft = self.create_complete()
        session_path = self.session / "session.json"
        external = self.repo / "external-session.json"
        external.write_bytes(session_path.read_bytes())
        session_path.unlink()
        session_path.symlink_to(external)

        result = self.run_cli(
            "resolve",
            str(draft),
            "--no-material",
            "--reason",
            "duplicate",
            success=False,
        )
        self.assertIn("session.json must be a regular non-symlink file", result.stderr)
        self.assertTrue(draft.exists())

    def test_no_material_rejects_existing_disposition(self) -> None:
        draft = self.create_complete("already-handled")
        self.append_disposition("G001", "rejected")
        result = self.run_cli(
            "resolve",
            str(draft),
            "--no-material",
            "--reason",
            "looks obsolete",
            success=False,
        )
        self.assertIn("already has material disposition 'rejected'", result.stderr)
        self.assertTrue(draft.exists())

    def test_recover_interrupted_create_residue(self) -> None:
        guidance_dir = self.session / "guidance"
        guidance_dir.mkdir()
        packet_temp = guidance_dir / ".G001-cache-review.draft.md.tmp"
        packet_temp.write_text("truncated packet", encoding="utf-8")
        orphan_patch = guidance_dir / "G001-cache-review.patch"
        orphan_patch.write_text("complete candidate patch\n", encoding="utf-8")
        result = self.run_cli(
            "create",
            "--session",
            SESSION_ID,
            "--slug",
            "cache-review",
            "--author",
            "A002",
            "--role",
            "reviewer",
            "--scope",
            "cache",
            success=False,
        )
        self.assertIn("create residue requires explicit recovery", result.stderr)
        self.assertTrue(packet_temp.exists())
        self.assertTrue(orphan_patch.exists())

        recovered = self.run_cli(
            "recover",
            "--session",
            SESSION_ID,
            "--guidance-id",
            "G001",
            "--slug",
            "cache-review",
            "--reason",
            "interrupted before draft publication",
        )
        self.assertIn("recovered G001-cache-review", recovered.stdout)
        self.assertFalse(packet_temp.exists())
        self.assertFalse(orphan_patch.exists())
        retry = self.create_complete()
        self.assertEqual(retry.name, "G001-cache-review.draft.md")

    def test_recover_malformed_draft_but_refuses_valid_draft(self) -> None:
        guidance_dir = self.session / "guidance"
        guidance_dir.mkdir()
        malformed = guidance_dir / "G001-broken.draft.md"
        malformed.write_text("---\nguidance_id:", encoding="utf-8")
        patch = guidance_dir / "G001-broken.patch"
        patch.write_text("candidate\n", encoding="utf-8")
        recovered = self.run_cli(
            "recover",
            "--session",
            SESSION_ID,
            "--guidance-id",
            "G001",
            "--slug",
            "broken",
            "--reason",
            "hard interruption left malformed final draft",
        )
        self.assertIn("G001-broken.draft.md", recovered.stdout)
        self.assertFalse(malformed.exists())
        self.assertFalse(patch.exists())

        valid = self.create_complete("valid-draft")
        refused = self.run_cli(
            "recover",
            "--session",
            SESSION_ID,
            "--guidance-id",
            "G001",
            "--slug",
            "valid-draft",
            "--reason",
            "must not bypass normal discard",
            success=False,
        )
        self.assertIn("structurally valid draft", refused.stderr)
        self.assertTrue(valid.exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
