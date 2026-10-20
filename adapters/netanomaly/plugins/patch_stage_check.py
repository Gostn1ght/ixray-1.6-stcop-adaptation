# desc: Check a NetAnomaly patch staging tree against the lzpack input policy
# input: [stage_root, Root containing gamedata, client/scripts, and server/scripts]

"""Read-only staging validator; it does not build archives or access keys."""

from __future__ import annotations

import argparse
import os
from pathlib import Path

MAX_VOLUME = 850 * 1024 * 1024
ALLOWED_PREFIXES = ("gamedata\\", "client\\scripts\\", "server\\scripts\\")
BLOCKED_PARTS = {"appdata", ".git", "_build"}
BLOCKED_SUFFIXES = {".pdb", ".key", ".secret"}


def is_junction(path: Path) -> bool:
    check = getattr(path, "is_junction", None)
    return bool(check and check())


def validate_stage(root: Path) -> tuple[list[str], dict[str, int]]:
    errors: list[str] = []
    role_bytes = {"client": 0, "server": 0}
    seen_paths: set[str] = set()
    file_count = 0

    def on_walk_error(error: OSError) -> None:
        errors.append(f"Cannot scan directory: {error}")

    for current, directories, filenames in os.walk(root, followlinks=False, onerror=on_walk_error):
        current_path = Path(current)
        kept_directories = []
        for name in sorted(directories):
            child = current_path / name
            if child.is_symlink() or is_junction(child):
                errors.append(f"Link/junction is forbidden: {child.relative_to(root).as_posix()}")
            else:
                kept_directories.append(name)
        directories[:] = kept_directories

        for filename in sorted(filenames):
            path = current_path / filename
            relative = path.relative_to(root).as_posix()
            normalized = relative.replace("/", "\\").lower()
            try:
                if path.is_symlink() or is_junction(path) or not path.is_file():
                    errors.append(f"Link or non-regular file is forbidden: {relative}")
                    continue
                size = path.stat().st_size
            except OSError as error:
                errors.append(f"Cannot inspect {relative}: {error}")
                continue

            file_count += 1
            if normalized in seen_paths:
                errors.append(f"Duplicate case-insensitive staging path: {relative}")
            seen_paths.add(normalized)
            if not normalized.startswith(ALLOWED_PREFIXES):
                errors.append(f"Unsupported staged path (use gamedata/, client/scripts/, or server/scripts/): {relative}")
            try:
                encoded_length = len(normalized.encode("cp1251"))
                if encoded_length > 240:
                    errors.append(f"Archive path exceeds the 240-byte CP1251 limit: {relative}")
            except UnicodeEncodeError:
                errors.append(f"Archive path cannot be encoded as CP1251: {relative}")

            parts = normalized.split("\\")
            if any(part in BLOCKED_PARTS for part in parts):
                errors.append(f"Private/state/build directory is forbidden: {relative}")
            if path.suffix.casefold() in BLOCKED_SUFFIXES:
                errors.append(f"Private/debug file extension is forbidden: {relative}")
            if any(part.startswith("netcoop_debug_") for part in parts):
                errors.append(f"Private debug probe is forbidden: {relative}")

            if normalized.startswith("gamedata\\"):
                role_bytes["client"] += size
                role_bytes["server"] += size
            elif normalized.startswith("client\\scripts\\"):
                role_bytes["client"] += size
            elif normalized.startswith("server\\scripts\\"):
                role_bytes["server"] += size

    if file_count == 0:
        errors.append("Staging tree contains no regular files.")
    for role, total in role_bytes.items():
        if total > MAX_VOLUME:
            errors.append(f"{role.title()} patch input is {total} bytes; the lzpack limit is 850 MiB.")
    return errors, role_bytes


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-stage_root", required=True, help="Patch staging directory to inspect")
    args = parser.parse_args()
    root = Path(args.stage_root).expanduser()
    if not root.exists() or not root.is_dir() or root.is_symlink() or is_junction(root):
        parser.error("stage_root must be a regular directory, not a link or junction")
    root = root.resolve()

    errors, sizes = validate_stage(root)
    print(f"Role totals: client={sizes['client']} bytes; server={sizes['server']} bytes.")
    if errors:
        for error in errors:
            print(f"FAIL: {error}")
        return 1
    print("Patch staging tree matches the lzpack input policy.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
