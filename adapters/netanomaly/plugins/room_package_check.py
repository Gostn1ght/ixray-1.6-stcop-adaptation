# desc: Validate NetAnomaly NCRM room geometry and optional camera/pick sidecars
# input: [room_file, Path to personal_room.room]

"""Read-only validator for the custom NetAnomaly personal-room file family."""

from __future__ import annotations

import argparse
import math
import struct
from pathlib import Path

VERTEX = struct.Struct("<3f3fI2f")
CAMERA_HEADER = struct.Struct("<4sI")
PICK_HEADER = struct.Struct("<4sII")


def is_junction(path: Path) -> bool:
    check = getattr(path, "is_junction", None)
    return bool(check and check())


def validate_room(path: Path) -> list[str]:
    errors: list[str] = []
    try:
        data = path.read_bytes()
    except OSError as error:
        return [f"Cannot read room file: {error}"]
    if len(data) < 12 or data[:4] != b"NCRM":
        return ["Room header must be NCRM followed by version and material count."]
    version, material_count = struct.unpack_from("<II", data, 4)
    if version != 2:
        errors.append(f"Unsupported NCRM version {version}; runtime expects version 2.")
    if not 1 <= material_count <= 128:
        errors.append(f"Material count {material_count} is outside runtime range 1..128.")
        return errors

    offset = 12
    total_vertices = 0
    for index in range(material_count):
        # The runtime reads at most 255 bytes into a 256-byte buffer, including
        # the terminator, so the NUL must be found within those first 255 bytes.
        nul = data.find(b"\0", offset, min(len(data), offset + 255))
        if nul < 0:
            errors.append(f"Material {index}: missing or overlong CP1251 NUL-terminated texture name.")
            return errors
        texture_bytes = data[offset:nul]
        if not texture_bytes:
            errors.append(f"Material {index}: texture name is empty.")
        try:
            texture_bytes.decode("cp1251")
        except UnicodeDecodeError:
            errors.append(f"Material {index}: texture name is not valid CP1251.")
        offset = nul + 1
        if offset + 4 > len(data):
            errors.append(f"Material {index}: missing vertex count.")
            return errors
        count = struct.unpack_from("<I", data, offset)[0]
        offset += 4
        if count == 0 or count % 3 != 0 or count > 300000:
            errors.append(f"Material {index}: vertex count {count} violates runtime limits.")
            return errors
        byte_count = count * VERTEX.size
        if offset + byte_count > len(data):
            errors.append(f"Material {index}: vertex data is truncated.")
            return errors
        # Runtime checks the record size, while finite checks catch NaN/Inf geometry
        # that would otherwise poison the renderer. The color is an opaque uint32.
        for vertex_index in range(count):
            values = VERTEX.unpack_from(data, offset + vertex_index * VERTEX.size)
            if not all(math.isfinite(value) for value in values[:3] + values[3:6] + values[7:9]):
                errors.append(f"Material {index}: non-finite position, normal, or UV at vertex {vertex_index}.")
                break
        offset += byte_count
        total_vertices += count

    if offset != len(data):
        errors.append(f"Room file has {len(data) - offset} unexpected trailing byte(s).")
    if version == 2 and not errors:
        print(f"NCRM v2: {material_count} material(s), {total_vertices} vertices; valid.")
    return errors


def validate_camera(path: Path) -> list[str]:
    try:
        data = path.read_bytes()
    except OSError as error:
        return [f"Cannot read camera sidecar: {error}"]
    if len(data) < CAMERA_HEADER.size:
        return ["Camera sidecar is truncated before its header."]
    magic, version = CAMERA_HEADER.unpack_from(data)
    expected_counts = {1: 27, 2: 42, 3: 43}
    float_count = expected_counts.get(version)
    if magic != b"NCRC" or float_count is None or len(data) != 8 + float_count * 4:
        return ["Camera sidecar must be NCRC v1/v2/v3 with the matching exact file length."]
    values = struct.unpack_from(f"<{float_count}f", data, 8)
    if any(not math.isfinite(value) or abs(value) > 1000.0 for value in values):
        return ["Camera sidecar contains non-finite or out-of-range coordinates."]
    camera_count = 4 if version == 1 else 6
    for index in range(camera_count):
        eye = values[index * 6 : index * 6 + 3]
        target = values[index * 6 + 3 : index * 6 + 6]
        distance_squared = sum((a - b) ** 2 for a, b in zip(eye, target))
        if distance_squared < 0.01:
            return [f"Camera view {index} eye and target are too close (squared distance < 0.01)."]
    if version == 3 and abs(values[-1]) > 10.0:
        return ["Camera v3 seat heading must be within -10..10 radians."]
    print(f"NCRC v{version}: {camera_count} camera view(s); valid.")
    return []


def validate_pick(path: Path) -> list[str]:
    try:
        data = path.read_bytes()
    except OSError as error:
        return [f"Cannot read pick sidecar: {error}"]
    if len(data) != 12 + 5 * 6 * 4:
        return ["Pick sidecar must be exactly 132 bytes (header plus five AABBs)."]
    magic, version, count = PICK_HEADER.unpack_from(data)
    if magic != b"NCRP" or version != 1 or count != 5:
        return ["Pick sidecar header must be NCRP v1 with exactly five boxes."]
    values = struct.unpack_from("<30f", data, 12)
    if any(not math.isfinite(value) or abs(value) > 1000.0 for value in values):
        return ["Pick sidecar contains non-finite or out-of-range coordinates."]
    print("NCRP v1: five pick boxes; valid.")
    return []


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-room_file", required=True, help="Path to personal_room.room")
    args = parser.parse_args()
    room = Path(args.room_file).expanduser()
    if not room.is_file() or room.is_symlink():
        parser.error("room_file must be an existing regular file")

    errors = validate_room(room)
    camera = room.with_suffix(".camera")
    pick = room.with_suffix(".pick")
    if camera.exists() or camera.is_symlink():
        if camera.is_symlink() or is_junction(camera):
            errors.append("Camera sidecar must not be a symlink or junction.")
        else:
            errors.extend(validate_camera(camera))
    else:
        print("NOTE: camera sidecar is absent; the game will use built-in camera defaults.")
    if pick.exists() or pick.is_symlink():
        if pick.is_symlink() or is_junction(pick):
            errors.append("Pick sidecar must not be a symlink or junction.")
        else:
            errors.extend(validate_pick(pick))
    else:
        print("NOTE: pick sidecar is absent; room interaction boxes will be unavailable.")

    if errors:
        for error in errors:
            print(f"FAIL: {error}")
        return 1
    print("NetAnomaly room package check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
