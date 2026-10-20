# desc: Build a read-only inventory of NetAnomaly assets and recognizable archives
# input: [asset_root, Root of resources, gamedata, or a staged patch]

"""IX-Ray Level Editor plugin for inspecting NetAnomaly asset trees."""

from __future__ import annotations

import argparse
import json
import os
import struct
from collections import Counter, defaultdict
from datetime import datetime, timezone
from pathlib import Path

LZPACK_HEADER = struct.Struct("<8sIIQ16s8s")
LZPACK_MAGIC = b"LZPACK1\0"
LZPACK_BLOCK_SIZE = 65536
LZPACK_TAG_SIZE = 16
CFS_MAGIC = 666


def is_junction(path: Path) -> bool:
    check = getattr(path, "is_junction", None)
    return bool(check and check())


def archive_info(path: Path) -> dict[str, object] | None:
    """Identify archive envelopes from a small prefix; never decrypt or unpack."""
    try:
        with path.open("rb") as stream:
            prefix = stream.read(LZPACK_HEADER.size)
        if prefix.startswith(LZPACK_MAGIC):
            if len(prefix) != LZPACK_HEADER.size:
                return {"kind": "LZPACK1", "header": "truncated", "structurally_valid": False}
            magic, version, block_size, plain_size, _salt, _nonce = LZPACK_HEADER.unpack(prefix)
            expected_size = (
                LZPACK_HEADER.size
                + plain_size
                + ((plain_size + LZPACK_BLOCK_SIZE - 1) // LZPACK_BLOCK_SIZE) * LZPACK_TAG_SIZE
            )
            actual_size = path.stat().st_size
            valid = (
                magic == LZPACK_MAGIC
                and version == 1
                and block_size == LZPACK_BLOCK_SIZE
                and 0 < plain_size < 4 * 1024**3
                and actual_size == expected_size
            )
            return {
                "kind": "LZPACK1",
                "version": version,
                "block_size": block_size,
                "protected_payload_bytes": plain_size,
                "archive_bytes": actual_size,
                "structurally_valid": valid,
                "integrity_verified": False,
                "note": "Keyless structural inspection only; authentication was not checked.",
            }

        if len(prefix) >= 8 and struct.unpack_from("<I", prefix)[0] == CFS_MAGIC:
            header_size = struct.unpack_from("<I", prefix, 4)[0]
            return {
                "kind": "X-Ray DB/CFS-like",
                "recognizable_header": True,
                "header_bytes": header_size,
                "note": "Header signature only; archive contents were not validated.",
            }
    except OSError as error:
        return {"kind": "unreadable", "error": str(error)}
    return None


def is_archive_candidate(path: Path) -> bool:
    suffix = path.suffix.lower()
    return (
        suffix == ".db"
        or (suffix.startswith(".db") and suffix[3:].isdigit())
        or suffix == ".xdb"
    )


def collect_inventory(root: Path) -> dict[str, object]:
    by_extension: dict[str, dict[str, int]] = defaultdict(lambda: {"files": 0, "bytes": 0})
    role_files: Counter[str] = Counter()
    largest: list[dict[str, object]] = []
    archives: list[dict[str, object]] = []
    issues: list[str] = []
    file_count = 0
    total_bytes = 0
    root_parts = [part.casefold() for part in root.parts]
    root_role = "other"
    if root_parts and root_parts[-1] == "resources":
        root_role = "resource archive/tree"
    elif root_parts and root_parts[-1] == "gamedata":
        root_role = "shared gamedata"
    elif len(root_parts) >= 2 and root_parts[-2:] == ["client", "scripts"]:
        root_role = "client scripts"
    elif len(root_parts) >= 2 and root_parts[-2:] == ["server", "scripts"]:
        root_role = "server scripts"

    def on_walk_error(error: OSError) -> None:
        issues.append(str(error))

    for current, directories, filenames in os.walk(root, followlinks=False, onerror=on_walk_error):
        current_path = Path(current)
        kept_directories = []
        for name in sorted(directories):
            child = current_path / name
            if child.is_symlink() or is_junction(child):
                issues.append(f"Skipped link/junction directory: {child.relative_to(root).as_posix()}")
            else:
                kept_directories.append(name)
        directories[:] = kept_directories

        for filename in sorted(filenames):
            path = current_path / filename
            relative = path.relative_to(root).as_posix()
            try:
                if path.is_symlink() or is_junction(path) or not path.is_file():
                    issues.append(f"Skipped link or non-regular file: {relative}")
                    continue
                size = path.stat().st_size
            except OSError as error:
                issues.append(f"{relative}: {error}")
                continue

            normalized = relative.replace("/", "\\").casefold()
            if normalized.startswith("gamedata\\"):
                role = "shared gamedata"
            elif normalized.startswith("client\\scripts\\"):
                role = "client scripts"
            elif normalized.startswith("server\\scripts\\"):
                role = "server scripts"
            elif normalized.startswith("resources\\"):
                role = "resource archive/tree"
            else:
                role = root_role
            role_files[role] += 1

            extension = path.suffix.lower() or "[no extension]"
            by_extension[extension]["files"] += 1
            by_extension[extension]["bytes"] += size
            file_count += 1
            total_bytes += size
            largest.append({"path": relative, "bytes": size})
            if is_archive_candidate(path) or filename.casefold().startswith("99_lz_patch_"):
                info = archive_info(path)
                if info is not None:
                    archives.append({"path": relative, **info})

    largest.sort(key=lambda item: (-int(item["bytes"]), str(item["path"])))
    return {
        "format": "netanomaly-asset-inventory/v2",
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "asset_root": str(root),
        "files": file_count,
        "total_bytes": total_bytes,
        "by_game_role": [{"role": role, "files": count} for role, count in sorted(role_files.items())],
        "by_extension": [
            {"extension": extension, **values}
            for extension, values in sorted(by_extension.items())
        ],
        "largest_files": largest[:30],
        "recognized_archives": archives,
        "scan_issues": issues,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-asset_root", required=True, help="Asset directory to inspect")
    args = parser.parse_args()
    root = Path(args.asset_root).expanduser()
    if not root.exists() or not root.is_dir() or root.is_symlink() or is_junction(root):
        parser.error("asset_root must be a regular directory, not a link or junction")
    root = root.resolve()

    report = collect_inventory(root)
    report_directory = Path.home() / "NetAnomalySDKReports"
    report_directory.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    report_path = report_directory / f"asset-inventory-{stamp}.json"
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(
        f"Scanned {report['files']} files ({report['total_bytes']} bytes); "
        f"recognized {len(report['recognized_archives'])} archive(s). Report: {report_path}"
    )
    if report["scan_issues"]:
        print(f"Scan issues: {len(report['scan_issues'])}; see scan_issues in the JSON report.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
