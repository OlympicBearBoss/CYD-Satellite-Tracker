Import("env")
from pathlib import Path
from PIL import Image

project = Path(env.subst("$PROJECT_DIR"))
source = project / "assets" / "splash.jpg"
target = project / "include" / "splash_jpg.h"

if not source.exists():
    raise RuntimeError(f"Missing JPEG splash asset: {source}")

with Image.open(source) as image:
    if image.format != "JPEG":
        raise RuntimeError(f"Splash must be JPEG, got {image.format}")
    if image.size != (320, 240):
        raise RuntimeError(f"Splash must be 320x240, got {image.size}")
    image.verify()

data = source.read_bytes()
if not data.startswith(bytes((0xFF, 0xD8))) or not data.endswith(bytes((0xFF, 0xD9))):
    raise RuntimeError("Splash asset is not a valid JPEG stream")

lines = []
for offset in range(0, len(data), 16):
    chunk = data[offset:offset + 16]
    lines.append("  " + ", ".join(f"0x{byte:02X}" for byte in chunk) + ",")
header = (
    "#pragma once\n#include <Arduino.h>\n"
    "static const uint8_t splash_jpg[] PROGMEM = {\n"
    + "\n".join(lines)
    + "\n};\n"
)
target.write_text(header, encoding="utf-8")
print(f"Embedded splash JPEG: {len(data)} bytes -> {target}")

# Additional photographic UI layers are embedded into flash as JPEG byte arrays.
ui_assets = [
    ("ui_earth_jpg", "earth_mini.jpg"),
    ("ui_iss_jpg", "iss_mini.jpg"),
]
ui_lines = ["#pragma once", "#include <Arduino.h>"]
for symbol, filename in ui_assets:
    asset = project / "assets" / filename
    with Image.open(asset) as image:
        if image.format != "JPEG":
            raise RuntimeError(f"{filename} must be JPEG")
        image.verify()
    payload = asset.read_bytes()
    if not payload.startswith(bytes((0xFF, 0xD8))) or not payload.endswith(bytes((0xFF, 0xD9))):
        raise RuntimeError(f"{filename} is not a valid JPEG stream")
    ui_lines.append(f"static const uint8_t {symbol}[] PROGMEM = {{")
    for offset in range(0, len(payload), 16):
        ui_lines.append("  " + ", ".join(f"0x{byte:02X}" for byte in payload[offset:offset + 16]) + ",")
    ui_lines.append("};")
ui_header = project / "include" / "ui_assets.h"
ui_header.write_text("\\n".join(ui_lines) + "\\n", encoding="utf-8")
print(f"Embedded {len(ui_assets)} UI JPEG assets -> {ui_header}")
