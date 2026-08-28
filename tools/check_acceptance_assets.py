#!/usr/bin/env python3
"""Check Task 15 acceptance asset manifest and optional external asset skips."""

from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path
from typing import Any


REQUIRED_COVERAGE = {
    "shader_ir",
    "codegen",
    "runtime",
    "usd_importer",
    "render",
    "editor",
    "blender_authoring",
}


def repo_root_from_script() -> Path:
    return Path(__file__).resolve().parents[1]


def load_manifest(path: Path) -> dict[str, Any]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise ValueError(f"{path}: invalid JSON: {exc}") from exc
    if not isinstance(data, dict):
        raise ValueError(f"{path}: manifest root must be an object")
    return data


def resolve_repo_path(root: Path, raw_path: str) -> Path:
    path = Path(raw_path)
    if path.is_absolute():
        return path
    return root / path


def check_required_file(root: Path, raw_path: str, errors: list[str]) -> None:
    path = resolve_repo_path(root, raw_path)
    if not path.exists():
        errors.append(f"missing required file: {raw_path}")


def check_golden_usd(root: Path, manifest: dict[str, Any], errors: list[str]) -> set[str]:
    coverage_seen: set[str] = set()
    entries = manifest.get("golden_usd")
    if not isinstance(entries, list) or not entries:
        errors.append("manifest must contain a non-empty golden_usd array")
        return coverage_seen

    ids: set[str] = set()
    for index, entry in enumerate(entries):
        if not isinstance(entry, dict):
            errors.append(f"golden_usd[{index}] must be an object")
            continue

        asset_id = entry.get("id")
        raw_path = entry.get("path")
        if not isinstance(asset_id, str) or not asset_id:
            errors.append(f"golden_usd[{index}] missing id")
        elif asset_id in ids:
            errors.append(f"duplicate golden_usd id: {asset_id}")
        else:
            ids.add(asset_id)

        if not isinstance(raw_path, str) or not raw_path:
            errors.append(f"golden_usd[{asset_id or index}] missing path")
        else:
            check_required_file(root, raw_path, errors)
            if Path(raw_path).suffix.lower() not in {".usd", ".usda", ".usdc"}:
                errors.append(f"golden_usd[{asset_id or index}] is not a USD file: {raw_path}")

        coverage = entry.get("coverage")
        if not isinstance(coverage, list) or not coverage:
            errors.append(f"golden_usd[{asset_id or index}] missing coverage")
        else:
            for item in coverage:
                if isinstance(item, str):
                    coverage_seen.add(item)
                else:
                    errors.append(f"golden_usd[{asset_id or index}] has non-string coverage entry")

        for dep in entry.get("dependencies", []):
            if isinstance(dep, str):
                check_required_file(root, dep, errors)
            else:
                errors.append(f"golden_usd[{asset_id or index}] has non-string dependency")

    return coverage_seen


def check_coverage_requirements(manifest: dict[str, Any], golden_coverage: set[str], errors: list[str]) -> None:
    requirements = manifest.get("coverage_requirements")
    if not isinstance(requirements, list):
        errors.append("manifest must contain coverage_requirements array")
        return

    requirement_ids: set[str] = set()
    for index, entry in enumerate(requirements):
        if not isinstance(entry, dict):
            errors.append(f"coverage_requirements[{index}] must be an object")
            continue
        req_id = entry.get("id")
        if not isinstance(req_id, str) or not req_id:
            errors.append(f"coverage_requirements[{index}] missing id")
            continue
        requirement_ids.add(req_id)
        for field in ("owner", "expected", "command"):
            if not isinstance(entry.get(field), str) or not entry[field]:
                errors.append(f"coverage_requirements[{req_id}] missing {field}")

    missing_requirements = REQUIRED_COVERAGE - requirement_ids
    for req_id in sorted(missing_requirements):
        errors.append(f"missing coverage requirement: {req_id}")

    missing_golden_links = {"usd_importer", "render"} - golden_coverage
    for req_id in sorted(missing_golden_links):
        errors.append(f"golden_usd entries do not link required coverage: {req_id}")


def check_optional_assets(root: Path, manifest: dict[str, Any], errors: list[str]) -> list[str]:
    skips: list[str] = []
    optional_assets = manifest.get("optional_external_assets", [])
    if not isinstance(optional_assets, list):
        errors.append("optional_external_assets must be an array when present")
        return skips

    for index, entry in enumerate(optional_assets):
        if not isinstance(entry, dict):
            errors.append(f"optional_external_assets[{index}] must be an object")
            continue
        asset_id = entry.get("id", f"optional-{index}")
        raw_path = entry.get("path")
        env_name = entry.get("env")
        skip_reason = entry.get("skip_reason")
        if not isinstance(raw_path, str) or not raw_path:
            errors.append(f"optional_external_assets[{asset_id}] missing path")
            continue
        if not isinstance(skip_reason, str) or not skip_reason:
            errors.append(f"optional_external_assets[{asset_id}] missing skip_reason")
        env_path = os.environ.get(env_name, "") if isinstance(env_name, str) and env_name else ""
        candidates = [resolve_repo_path(root, raw_path)]
        if env_path:
            candidates.insert(0, Path(env_path))
        if any(candidate.exists() for candidate in candidates):
            continue
        skips.append(f"skip optional {asset_id}: {skip_reason}")

    return skips


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--manifest",
        type=Path,
        default=Path("assets/test/golden_usd_manifest.json"),
        help="Manifest path relative to repository root.",
    )
    args = parser.parse_args()

    root = repo_root_from_script()
    manifest_path = resolve_repo_path(root, str(args.manifest))
    errors: list[str] = []

    if not manifest_path.exists():
        print(f"error: manifest does not exist: {args.manifest}", file=sys.stderr)
        return 2

    try:
        manifest = load_manifest(manifest_path)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    golden_coverage = check_golden_usd(root, manifest, errors)
    check_coverage_requirements(manifest, golden_coverage, errors)
    skips = check_optional_assets(root, manifest, errors)

    for skip in skips:
        print(skip)

    if errors:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
        return 1

    print(f"ok: {args.manifest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
