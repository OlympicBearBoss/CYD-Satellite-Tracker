Import("env")
from pathlib import Path\nfrom PIL import Image

project = Path(env.subst("$PROJECT_DIR"))
source = project / "assets" / "splash.jpg"
target = project / "include" / "splash_jpg.h"

if not source.exists():
    raise RuntimeError(f"Missing JPEG splash asset: {source}")

with Image.open(source) as image:\n    if image.size != (320, 240):\n        raise RuntimeError(f"Splash must be 320x240, got {image.size}")\n    image.verify()\n\ndata = source.read_bytes()
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
