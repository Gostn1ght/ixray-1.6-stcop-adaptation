# desc: Create a read-only inventory of a NetAnomaly asset directory
# input: [asset_root, Path to the NetAnomaly asset directory]

"""IX-Ray Level Editor Python plugin: inspect assets without changing them."""

from __future__ import annotations

import argparse
import json
import os
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path


def collect_inventory(root: Path) -> dict[str, object]:
    """Collect file counts and sizes without following symlinks or reading contents."""
    by_extension: dict[str, dict[str, int]] = defaultdict(lambda: {"files": 0, "bytes": 0})
    largest: list[dict[str, object]] = []
    errors: list[str] = []
    files_seen = 0
    bytes_seen = 0

    for current, directories, filenames in os.walk(root, followlinks=False):
        current_path = Path(current)
        directories[:] = sorted(
            name for name in directories if not (current_path / name).is_symlink()
        )

        for filename in filenames:
            path = current_path / filename
            try:
                if path.is_symlink() or not path.is_file():
                    continue
                size = path.stat().st_size
            except OSError as error:
                errors.append(f"{path.relative_to(root).as_posix()}: {error}")
                continue

            extension = path.suffix.lower() or "[no extension]"
            by_extension[extension]["files"] += 1
            by_extension[extension]["bytes"] += size
            files_seen += 1
            bytes_seen += size
            largest.append(
                {"path": path.relative_to(root).as_posix(), "bytes": size}
            )

    largest.sort(key=lambda item: (-int(item["bytes"]), str(item["path"])))
    extension_rows = [
        {"extension": extension, **values}
        for extension, values in sorted(by_extension.items())
    ]

    return {
        "format": "netanomaly-asset-inventory/v1",
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "asset_root": str(root),
        "files": files_seen,
        "total_bytes": bytes_seen,
        "by_extension": extension_rows,
        "largest_files": largest[:30],
        "scan_errors": errors,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-asset_root", required=True, help="Asset directory to inspect")
    arguments = parser.parse_args()

    root = Path(arguments.asset_root).expanduser().resolve()
    if not root.exists() or not root.is_dir():
        parser.error("asset_root must be an existing directory")

    report = collect_inventory(root)
    report_directory = Path.home() / "NetAnomalySDKReports"
    report_directory.mkdir(parents=True, exist_ok=True)
    report_path = report_directory / "asset-inventory.json"
    temporary_path = report_path.with_suffix(".json.tmp")
    temporary_path.write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    temporary_path.replace(report_path)

    print(
        f"Scanned {report['files']} files ({report['total_bytes']} bytes). "
        f"Report: {report_path}"
    )
    if report["scan_errors"]:
        print(f"Could not inspect {len(report['scan_errors'])} file(s); see scan_errors.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
