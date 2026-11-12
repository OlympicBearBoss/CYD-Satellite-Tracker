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
    """Render a detailed 4x engineering drawing and downsample for the CYD."""
    from PIL import ImageDraw, ImageFont
    scale = 4
    W, H = 320 * scale, 240 * scale
    image = Image.new("RGB", (W, H), (3, 14, 31))
    d = ImageDraw.Draw(image)
    def xy(points):
        return tuple(int(v * scale) for v in points)
    def line(points, fill, width=1):
        d.line(xy(points), fill=fill, width=max(1, width * scale), joint="curve")
    def rect(box, outline, fill=None, width=1):
        d.rectangle(xy(box), outline=outline, fill=fill, width=max(1, width * scale))
    def ellipse(box, outline, fill=None, width=1):
        d.ellipse(xy(box), outline=outline, fill=fill, width=max(1, width * scale))
    def polygon(points, outline, fill=None, width=1):
        d.polygon([xy(p) for p in points], outline=outline, fill=fill)
        if width > 1 and outline:
            d.line([xy(p) for p in points + [points[0]]], fill=outline, width=width * scale, joint="curve")

    bg_grid = (10, 38, 66)
    grid = (17, 57, 91)
    cyan = (61, 184, 226)
    bright = (160, 222, 246)
    pale = (213, 241, 252)
    muted = (37, 111, 157)
    # Blueprint grid, frame, corner registration marks.
    for x in range(8, 320, 8):
        line((x, 0, x, 240), grid if x % 32 == 0 else bg_grid)
    for y in range(8, 240, 8):
        line((0, y, 320, y), grid if y % 32 == 0 else bg_grid)
    rect((5, 5, 314, 234), muted, width=1)
    rect((9, 9, 310, 230), (12, 48, 78), width=1)
    for x, y, sx, sy in ((13,13,1,1),(307,13,-1,1),(13,227,1,-1),(307,227,-1,-1)):
        line((x,y,x+7*sx,y),cyan,1); line((x,y,x,y+7*sy),cyan,1)

    # Faint orbital construction arcs behind the spacecraft.
    ellipse((48, 20, 275, 250), (13, 61, 96), width=1)
    d.arc(xy((34, 7, 286, 247)), 198, 342, fill=(25, 91, 132), width=2*scale)
    d.arc(xy((55, 25, 264, 220)), 18, 163, fill=(18, 70, 109), width=1*scale)
    # Solar arrays with individual photovoltaic cells, bus bars and support rails.
    for x0, x1 in ((24, 103), (217, 296)):
        rect((x0, 86, x1, 129), cyan, (5, 34, 59), 1)
        rect((x0+3, 89, x1-3, 126), muted, None, 1)
        for xx in range(x0+13, x1-3, 13):
            line((xx, 90, xx, 125), (44, 126, 174), 1)
        for yy in (101, 113):
            line((x0+3, yy, x1-3, yy), (35, 104, 151), 1)
        line((x0+5, 92, x1-5, 92), (111, 202, 235), 1)
    line((103, 106, 119, 106), pale, 2)
    line((201, 106, 217, 106), pale, 2)
    # Hinges and articulated booms.
    for cx in (108, 212):
        ellipse((cx-5, 100, cx+5, 112), bright, (8, 44, 70), 1)
        ellipse((cx-2, 103, cx+2, 109), cyan, None, 1)
    # Central bus in isometric projection with three distinct faces.
    polygon([(119,75),(158,57),(201,76),(161,96)], bright, (9, 48, 76), 2)
    polygon([(119,75),(161,96),(161,153),(119,132)], cyan, (5, 29, 52), 2)
    polygon([(161,96),(201,76),(201,132),(161,153)], bright, (7, 38, 63), 2)
    # Equipment bay seams, radiator panels, latches and fasteners.
    for x in (128, 138, 148):
        line((x, 82, x, 127), (43, 116, 157), 1)
    for y in (105, 117, 129):
        line((166, y, 195, y-15), (50, 130, 174), 1)
    rect((135, 100, 154, 120), cyan, (4, 24, 44), 1)
    ellipse((140, 104, 149, 114), bright, None, 1)
    for x, y in ((121,77),(158,60),(198,78),(121,130),(161,150),(198,130)):
        ellipse((x-1.2,y-1.2,x+1.2,y+1.2), pale, pale, 1)
    # High-gain parabolic dish, feed horn, support struts and RF wavefronts.
    line((194, 81, 229, 51), bright, 1)
    ellipse((219, 28, 254, 63), bright, (7, 37, 62), 1)
    ellipse((224, 33, 249, 58), cyan, None, 1)
    ellipse((231, 40, 242, 51), muted, None, 1)
    ellipse((235, 44, 238, 47), pale, pale, 1)
    line((236, 45, 229, 51), pale, 1)
    d.arc(xy((212, 19, 265, 72)), 210, 330, fill=(50, 139, 184), width=1*scale)
    d.arc(xy((204, 11, 273, 80)), 210, 330, fill=(25, 94, 141), width=1*scale)
    # Star tracker / optical instrument and lower sensor boom.
    line((161, 153, 161, 169), bright, 2)
    rect((151, 164, 171, 181), cyan, (5, 31, 52), 1)
    ellipse((156, 168, 166, 178), bright, None, 1)
    line((119, 87, 100, 69, 82, 69), muted, 1)
    ellipse((78, 65, 86, 73), cyan, None, 1)
    line((201, 127, 225, 145, 246, 145), muted, 1)
    ellipse((243, 142, 249, 148), cyan, None, 1)
    # Dimension lines, extension marks and small engineering callouts (no extra text).
    line((24, 76, 103, 76), muted, 1)
    line((24, 72, 24, 80), cyan, 1); line((103, 72, 103, 80), cyan, 1)
    line((217, 137, 296, 137), muted, 1)
    line((217, 133, 217, 141), cyan, 1); line((296, 133, 296, 141), cyan, 1)
    line((207, 88, 207, 129), muted, 1)
    line((203, 88, 211, 88), cyan, 1); line((203, 129, 211, 129), cyan, 1)
    # Restrained title plate; exact original wording retained.
    rect((13, 190, 307, 222), (14, 57, 91), (3, 14, 31), 1)
    line((13, 187, 307, 187), muted, 1)
    try:
        font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 17 * scale)
    except OSError:
        font = ImageFont.load_default()
    title = "CYD Satellite Tracker"
    box = d.textbbox((0, 0), title, font=font)
    tw, th = box[2] - box[0], box[3] - box[1]
    d.text(((W-tw)//2, 195*scale-box[1]), title, font=font, fill=(222, 244, 255))
    # Crisp 320x240 landscape output.
    return image.resize((320, 240), Image.Resampling.LANCZOS)

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
