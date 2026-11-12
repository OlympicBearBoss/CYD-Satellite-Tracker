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

# Build the approved 320x240 engineering-blueprint splash locally at build time.
# No status icons, timers, buttons, radar or telemetry are placed on this screen.
def build_engineering_splash():
    from PIL import ImageDraw, ImageFont
    image = Image.new("RGB", (320, 240), (4, 15, 39))
    d = ImageDraw.Draw(image)
    grid, minor = (12, 43, 78), (8, 29, 57)
    bright, cyan, pale = (34, 112, 166), (67, 190, 225), (150, 205, 225)
    for x in range(8, 320, 8):
        d.line((x, 0, x, 239), fill=grid if x % 32 == 0 else minor, width=1)
    for y in range(8, 240, 8):
        d.line((0, y, 319, y), fill=grid if y % 32 == 0 else minor, width=1)
    d.rectangle((5, 5, 314, 234), outline=bright, width=1)
    # Earth limb and orbit behind the satellite.
    d.ellipse((158, 119, 366, 327), outline=(21, 88, 137), width=2)
    d.arc((145, 103, 365, 323), 190, 340, fill=cyan, width=2)
    d.arc((165, 125, 350, 310), 198, 332, fill=(25, 92, 144), width=1)
    # Satellite bus, solar panels and segmented photovoltaic cells.
    d.polygon([(118,77),(148,65),(170,78),(165,111),(137,123),(114,108)],
              outline=pale, fill=(7,35,67))
    d.line([(118,77),(148,65),(170,78),(165,111),(137,123),(114,108),(118,77)], fill=cyan, width=2)
    d.rectangle((130,83,156,105), outline=pale, width=2)
    d.ellipse((137,88,149,100), outline=cyan, width=1)
    d.line((143,88,143,100), fill=pale, width=1)
    d.line((137,94,149,94), fill=pale, width=1)
    for x0,x1 in ((43,107),(177,241)):
        d.rectangle((x0,82,x1,110), outline=cyan, width=2)
        for xx in range(x0+13,x1,13):
            d.line((xx,84,xx,108), fill=bright, width=1)
        d.line((x0+2,96,x1-2,96), fill=bright, width=1)
    d.line((107,96,114,96), fill=pale, width=2)
    d.line((170,96,177,96), fill=pale, width=2)
    # Directional antenna and dish.
    d.line((143,65,143,42), fill=pale, width=2)
    d.ellipse((134,30,152,48), outline=cyan, width=2)
    d.line((143,39,143,25), fill=pale, width=1)
    d.arc((123,17,163,57), 205, 335, fill=bright, width=1)
    # Dimension ticks and fine engineering leader lines (without labels).
    d.line((43,72,241,72), fill=bright, width=1)
    d.line((43,68,43,76), fill=bright, width=1)
    d.line((241,68,241,76), fill=bright, width=1)
    d.line((253,82,253,110), fill=bright, width=1)
    d.line((249,82,257,82), fill=bright, width=1)
    d.line((249,110,257,110), fill=bright, width=1)
    d.line((118,77,92,51,64,51), fill=bright, width=1)
    d.ellipse((61,48,67,54), outline=cyan, width=1)
    d.line((165,111,194,137,229,137), fill=bright, width=1)
    d.ellipse((226,134,232,140), outline=cyan, width=1)
    # The only text on the splash.
    try:
        font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 17)
    except OSError:
        font = ImageFont.load_default()
    d.rectangle((13,184,307,221), fill=(4,15,39))
    d.line((13,181,307,181), fill=bright, width=1)
    d.text((18,191), "CYD Satellite Tracker", font=font, fill=(222,242,250))
    for x,y,sx,sy in ((10,10,1,1),(309,10,-1,1),(10,229,1,-1),(309,229,-1,-1)):
        d.line((x,y,x+8*sx,y), fill=cyan, width=1)
        d.line((x,y,x,y+8*sy), fill=cyan, width=1)
    output = BytesIO()
    image.save(output, format="JPEG", quality=91, optimize=True)
    return output.getvalue()

# Approved CYD engineering-blueprint splash, 320x240 landscape.
splash = validate_jpeg(build_engineering_splash(), "generated engineering splash", (320, 240))
splash_lines = ["#pragma once", "#include <Arduino.h>"]
emit_array(splash_lines, "splash_jpg", splash)
target.write_text("\n".join(splash_lines) + "\n", encoding="utf-8")
print(f"Embedded generated engineering-blueprint splash: {len(splash)} bytes -> {target}")

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
print(f"Embedded {len(ui_assets)} UI JPEG assets including V62 approved splash -> {ui_header}")
