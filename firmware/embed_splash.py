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
    """Render a more realistic cinematic spacecraft illustration for the 320x240 CYD."""
    from PIL import Image, ImageDraw, ImageFont, ImageFilter
    scale = 4
    W, H = 320 * scale, 240 * scale

    # Deep-space gradient with a softly illuminated Earth limb.
    image = Image.new("RGB", (W, H))
    px = image.load()
    for y in range(H):
        t = y / max(1, H - 1)
        for x in range(W):
            glow = max(0.0, 1.0 - (((x-W*0.52)/(W*0.78))**2 + ((y-H*0.72)/(H*0.72))**2))
            px[x, y] = (int(2 + 5*glow), int(8 + 18*glow), int(20 + 38*glow))
    d = ImageDraw.Draw(image)

    def xy(points):
        return tuple(int(v * scale) for v in points)
    def line(points, fill, width=1):
        d.line(xy(points), fill=fill, width=max(1, width * scale), joint="curve")
    def rect(box, fill, outline=None, width=1):
        d.rectangle(xy(box), fill=fill, outline=outline, width=max(1, width * scale))
    def ellipse(box, fill, outline=None, width=1):
        d.ellipse(xy(box), fill=fill, outline=outline, width=max(1, width * scale))
    def poly(points, fill, outline=None, width=1):
        d.polygon([xy(p) for p in points], fill=fill, outline=outline)
        if outline and width > 1:
            d.line([xy(p) for p in points + [points[0]]], fill=outline, width=width*scale, joint="curve")

    # Fine stars, restrained lens glints and a curved blue atmospheric horizon.
    import random
    rng = random.Random(2432)
    for _ in range(105):
        x, y = rng.randrange(4, 316), rng.randrange(5, 184)
        r = rng.choice((1, 1, 1, 2))
        c = rng.choice(((75, 125, 164), (105, 157, 190), (170, 202, 222)))
        ellipse((x-r, y-r, x+r, y+r), c)
    d.arc(xy((-78, 126, 392, 342)), 190, 350, fill=(28, 102, 158), width=7*scale)
    d.arc(xy((-64, 131, 384, 333)), 190, 350, fill=(64, 157, 206), width=2*scale)
    # Blue atmospheric haze, soft rather than a hard blueprint orbit.
    haze = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    hd = ImageDraw.Draw(haze)
    hd.ellipse(xy((-45, 141, 365, 355)), fill=(18, 89, 156, 38))
    haze = haze.filter(ImageFilter.GaussianBlur(18*scale))
    image = Image.alpha_composite(image.convert("RGBA"), haze).convert("RGB")
    d = ImageDraw.Draw(image)

    # Solar arrays: dark glass, individual cells, metallic rails and highlights.
    for x0, x1 in ((18, 104), (216, 302)):
        poly([(x0,82),(x1,82),(x1-2,130),(x0+2,130)], (7, 24, 48), (111, 156, 187), 1)
        for xx in range(x0+4, x1-2, 10):
            line((xx,84,xx-1,128), (32, 83, 127), 1)
        for yy in (95,108,121):
            line((x0+2,yy,x1-2,yy), (35, 91, 132), 1)
        line((x0+5,85,x1-5,85), (122, 174, 207), 1)
        line((x0+5,127,x1-5,127), (18, 52, 83), 1)
    # Structural booms and articulated joints.
    line((101,105,123,105), (184, 196, 199), 3)
    line((197,105,219,105), (184, 196, 199), 3)
    for cx in (106, 214):
        ellipse((cx-5,99,cx+5,111), (71, 91, 104), (201, 216, 220), 1)
        ellipse((cx-2,102,cx+2,108), (14, 38, 57), (139, 171, 184), 1)

    # Spacecraft bus with shaded metal faces, bevels, seams and gold thermal blanket.
    poly([(119,72),(158,53),(202,73),(162,94)], (92, 112, 123), (207, 222, 228), 2)
    poly([(119,72),(162,94),(162,151),(119,128)], (45, 65, 79), (161, 185, 195), 2)
    poly([(162,94),(202,73),(202,129),(162,151)], (29, 47, 61), (171, 196, 204), 2)
    poly([(124,75),(158,59),(194,75),(161,89)], (142, 155, 153), (218, 221, 207), 1)
    # Thermal foil panels on the side face.
    for yy in (101,112,123):
        line((166,yy,197,yy-15), (139, 112, 58), 2)
        line((166,yy+2,197,yy-13), (75, 62, 45), 1)
    for xx in (127,137,147):
        line((xx,79,xx,125), (103, 132, 145), 1)
    # Dark instrument windows and optical payload.
    rect((135,99,154,119), (7, 20, 31), (182, 197, 200), 1)
    ellipse((139,103,150,115), (10, 35, 55), (155, 199, 216), 2)
    ellipse((142,106,147,111), (99, 175, 204), (222, 239, 241), 1)
    # Antenna dish, reflective rim, feed arm and three struts.
    line((193,80,227,50), (179, 192, 197), 2)
    ellipse((216,26,258,68), (91, 111, 121), (226, 237, 238), 2)
    ellipse((221,31,253,63), (29, 57, 76), (131, 181, 201), 1)
    ellipse((228,38,246,56), (11, 29, 46), (172, 206, 218), 1)
    ellipse((234,44,240,50), (185, 207, 214), (235, 243, 243), 1)
    for a, b in (((224,35),(236,46)),((250,38),(238,46)),((236,62),(237,49))):
        line((a[0],a[1],b[0],b[1]), (179, 203, 211), 1)
    # Lower optical sensor, cable runs, brackets and small fasteners.
    line((162,151,162,169), (194, 207, 210), 3)
    rect((151,164,173,183), (23, 42, 55), (183, 203, 209), 1)
    ellipse((156,168,168,180), (5, 18, 30), (105, 188, 216), 2)
    ellipse((160,172,164,176), (116, 190, 217))
    line((121,86,99,68,82,68), (148, 173, 182), 1)
    ellipse((77,63,87,73), (22, 45, 61), (166, 206, 218), 1)
    line((201,126,225,145,246,145), (143, 166, 176), 1)
    ellipse((243,141,251,149), (17, 43, 59), (157, 199, 211), 1)
    for x,y in ((121,74),(158,56),(199,75),(121,127),(162,148),(199,127)):
        ellipse((x-1.5,y-1.5,x+1.5,y+1.5), (218, 229, 228), (70, 88, 97), 1)

    # Subtle engineering overlay and framing retain the original visual identity.
    for x in range(12, 320, 32):
        line((x,12,x,181), (12, 36, 57), 1)
    for y in range(20, 184, 32):
        line((10,y,310,y), (12, 36, 57), 1)
    rect((5,5,314,234), None, (38, 91, 126), 1)
    for x,y,sx,sy in ((11,11,1,1),(309,11,-1,1),(11,228,1,-1),(309,228,-1,-1)):
        line((x,y,x+6*sx,y), (73, 162, 202), 1)
        line((x,y,x,y+6*sy), (73, 162, 202), 1)

    # Preserve the exact approved title, with no additional labels.
    rect((13,190,307,222), (29, 91, 128), (3, 12, 26), 1)
    line((13,187,307,187), (48, 128, 169), 1)
    try:
        font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 17*scale)
    except OSError:
        font = ImageFont.load_default()
    title = "CYD Satellite Tracker"
    box = d.textbbox((0,0), title, font=font)
    tw = box[2]-box[0]
    d.text(((W-tw)//2,195*scale-box[1]), title, font=font, fill=(232, 246, 255))
    output = BytesIO()
    image.resize((320,240), Image.Resampling.LANCZOS).save(output, format="JPEG", quality=96, subsampling=0, optimize=True)
    return output.getvalue()

# Approved CYD spacecraft-render splash, 320x240 landscape.
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
