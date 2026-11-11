Import("env")
from pathlib import Path
from io import BytesIO
from PIL import Image

project = Path(env.subst("$PROJECT_DIR"))
assets = project / "assets"
target = project / "include" / "splash_jpg.h"

def read_asset(filename):
    source = assets / filename
    parts = sorted(assets.glob(filename + ".part*"))
    if parts:
        payload = b"".join(part.read_bytes() for part in parts)
        print(f"Assembled {filename} from {len(parts)} binary parts ({len(payload)} bytes)")
        return payload
    if not source.exists():
        raise RuntimeError(f"Missing JPEG asset: {source}")
    return source.read_bytes()

def validate_jpeg(payload, filename, expected_size=None):
    if not payload.startswith(bytes((0xFF, 0xD8))) or not payload.endswith(bytes((0xFF, 0xD9))):
        raise RuntimeError(f"{filename} is not a valid JPEG stream")
    with Image.open(BytesIO(payload)) as image:
        if image.format != "JPEG":
            raise RuntimeError(f"{filename} must be JPEG, got {image.format}")
        if expected_size and image.size != expected_size:
            raise RuntimeError(f"{filename} must be {expected_size[0]}x{expected_size[1]}, got {image.size}")
        image.verify()
    return payload

def emit_array(lines, symbol, payload):
    lines.append(f"static const uint8_t {symbol}[] PROGMEM = {{")
    for offset in range(0, len(payload), 16):
        lines.append("  " + ", ".join(f"0x{byte:02X}" for byte in payload[offset:offset + 16]) + ",")
    lines.append("};")

# Approved Olympic Bear splash, 320x240 landscape.
splash = validate_jpeg(read_asset("splash_v61.jpg"), "splash_v61.jpg", (320, 240))
splash_lines = ["#pragma once", "#include <Arduino.h>"]
emit_array(splash_lines, "splash_jpg", splash)
target.write_text("\n".join(splash_lines) + "\n", encoding="utf-8")
print(f"Embedded approved V61 splash: {len(splash)} bytes -> {target}")

# Existing small image assets are retained; the dashboard uses a single larger photo.
ui_assets = [
    ("ui_earth_jpg", "earth_mini.jpg"),
    ("ui_iss_jpg", "iss_mini.jpg"),
    ("ui_hero_jpg", "hero_v61.jpg"),
]
ui_lines = ["#pragma once", "#include <Arduino.h>"]
for symbol, filename in ui_assets:
    payload = validate_jpeg(read_asset(filename), filename)
    emit_array(ui_lines, symbol, payload)
ui_header = project / "include" / "ui_assets.h"
ui_header.write_text("\n".join(ui_lines) + "\n", encoding="utf-8")
print(f"Embedded {len(ui_assets)} UI JPEG assets including V61 photographic hero -> {ui_header}")
