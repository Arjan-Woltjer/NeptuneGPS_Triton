"""Draws the kilverbak's VT pictures and writes them as VT3 PictureGraphic data.

Side view of a tractor driving right with a kilverbak (levelling bucket) behind it: rough ground
ahead of the bucket, the levelled surface behind it, and the reference plane as a dashed line at
the slope. Three pictures, one per position of the blade against the plane -- on it, above it
(the valves will lower the bucket: arrow down), below it (arrow up) -- plus the green / red status
icons (the plough's) and the 16 x 16 monochrome app-switcher icon. Pixel art drawn here in code,
as the plough's, so it can be changed and regenerated (NeptuneGPS_Triton#291):

    py tools/generate_vt_images.py                     # writes lib/.../isobus/VTImages.generated.cpp
    py tools/generate_vt_images.py --preview out.png   # also a 4x preview of all of them

Colours are indices into the ISO 11783-6 standard 8-bit palette (16 + 36 r + 6 g + b, each 0..5 in
steps of 0x33). The colour pictures are run-length encoded (count, colour) pairs, runs never
crossing a row; the icon is 1 bit per pixel, 8 pixels per byte, most significant bit first.
"""
import argparse
import os

W, H = 120, 76


def cube(r, g, b):
    return 16 + 36 * r + 6 * g + b


BLACK = 0
WHITE = 1
SKY = cube(3, 4, 5)
SOIL = cube(3, 2, 1)
SOIL_DARK = cube(2, 1, 0)
LEVELLED = cube(4, 3, 1)       # the scraped surface behind the bucket, a little lighter
TRACTOR = cube(0, 1, 4)        # blue body, as the plough's
TRACTOR_DARK = cube(0, 0, 2)
WINDOW = cube(4, 5, 5)
TYRE = cube(1, 1, 1)
RIM = cube(4, 4, 4)
STEEL = cube(1, 1, 1)
BUCKET = cube(5, 4, 0)         # yellow box
BUCKET_DARK = cube(3, 2, 0)
BLADE = cube(2, 2, 2)
PLANE = cube(5, 5, 0)          # the reference line
ARROW = WHITE

# The reference plane: a straight line rising to the right (the slope, exaggerated).
PLANE_LEFT_Y, PLANE_RIGHT_Y = 54.0, 46.0
BUCKET_X0, BUCKET_X1 = 22, 56   # the bucket's box, left to right


def plane_y(x):
    return PLANE_LEFT_Y + (PLANE_RIGHT_Y - PLANE_LEFT_Y) * x / (W - 1)


def draw(offset):
    """The side view with the blade `offset` pixels below the plane (negative: above it)."""
    px = [[SKY] * W for _ in range(H)]

    def rect(x0, y0, x1, y1, colour):
        for y in range(max(0, int(y0)), min(H, int(y1) + 1)):
            for x in range(max(0, int(x0)), min(W, int(x1) + 1)):
                px[y][x] = colour

    def disc(cx, cy, r, colour):
        for y in range(H):
            for x in range(W):
                if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                    px[y][x] = colour

    def line(x0, y0, x1, y1, colour, half_width=1.0):
        steps = int(max(abs(x1 - x0), abs(y1 - y0)) * 2) + 1
        for i in range(steps + 1):
            t = i / steps
            x, y = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
            r = int(half_width + 0.5)
            for dy in range(-r, r + 1):
                for dx in range(-r, r + 1):
                    if dx * dx + dy * dy <= half_width * half_width + 0.25:
                        xi, yi = int(round(x + dx)), int(round(y + dy))
                        if 0 <= xi < W and 0 <= yi < H:
                            px[yi][xi] = colour

    # Ground: rough ahead of the bucket (right), levelled to the plane behind it (left).
    def ground_y(x):
        if x <= BUCKET_X0:
            return plane_y(x)
        bump = (3, 5, 2, 6, 4, 1, 5, 3)[(x // 7) % 8] - 3
        return plane_y(x) - 2 + bump * 0.8
    for x in range(W):
        top = int(round(ground_y(x)))
        for y in range(max(0, top), H):
            if x <= BUCKET_X0 and y == top:
                px[y][x] = LEVELLED
            else:
                px[y][x] = SOIL_DARK if (y - top) > 6 and (x + y) % 5 == 0 else SOIL

    # Tractor, driving right: rear wheel, front wheel, body, cab.
    rear_cx, rear_cy, rear_r = 88, int(ground_y(88)) - 11, 11
    front_cx, front_cy, front_r = 111, int(ground_y(111)) - 7, 7
    rect(84, rear_cy - 12, 116, rear_cy - 3, TRACTOR)               # body and bonnet
    rect(116, rear_cy - 10, 118, rear_cy - 5, TRACTOR_DARK)         # grille
    rect(78, rear_cy - 30, 96, rear_cy - 12, TRACTOR)               # cab
    rect(81, rear_cy - 27, 93, rear_cy - 15, WINDOW)                # side window
    rect(78, rear_cy - 31, 97, rear_cy - 30, TRACTOR_DARK)          # roof
    disc(rear_cx, rear_cy, rear_r, TYRE)
    disc(rear_cx, rear_cy, 5, RIM)
    disc(front_cx, front_cy, front_r, TYRE)
    disc(front_cx, front_cy, 3, RIM)

    # The kilverbak: an open box with its blade at the bottom, on a small rear wheel, and the
    # drawbar to the tractor's hitch. `offset` moves the whole bucket against the plane.
    blade_y = plane_y((BUCKET_X0 + BUCKET_X1) / 2) + offset
    top_y = blade_y - 14
    hitch_x, hitch_y = 77, rear_cy + 2
    line(BUCKET_X1, top_y + 5, hitch_x, hitch_y, STEEL, 1.0)        # drawbar
    rect(hitch_x - 1, hitch_y - 1, hitch_x + 1, hitch_y + 1, STEEL)
    disc(BUCKET_X0 - 3, blade_y - 4, 4, TYRE)                       # rear wheel
    disc(BUCKET_X0 - 3, blade_y - 4, 1.5, RIM)
    rect(BUCKET_X0, top_y, BUCKET_X1, blade_y - 2, BUCKET)          # box side
    rect(BUCKET_X0, top_y, BUCKET_X1, top_y + 1, BUCKET_DARK)       # rim
    rect(BUCKET_X0, top_y, BUCKET_X0 + 1, blade_y - 2, BUCKET_DARK)
    rect(BUCKET_X1 - 1, top_y, BUCKET_X1, blade_y - 2, BUCKET_DARK)
    rect(BUCKET_X0 + 2, blade_y - 1, BUCKET_X1 - 2, blade_y, BLADE) # blade

    # The reference plane over everything, dashed, so it shows through the bucket.
    for x in range(W):
        if (x // 4) % 2 == 0:
            y = int(round(plane_y(x)))
            if 0 <= y < H:
                px[y][x] = PLANE

    # Which way the valves will move the bucket: down when it is above the plane, up when below.
    if offset != 0:
        ax = BUCKET_X1 + 7
        tip, tail = (blade_y + 1, top_y) if offset < 0 else (top_y - 4, blade_y - 3)
        line(ax, tail, ax, tip, ARROW, 1.2)
        d = 1 if tip > tail else -1
        for i in range(6):
            rect(ax - i, tip - d * i, ax + i, tip - d * i, ARROW)
    return px


ICON = 14                     # status icons: ICON x ICON, the plough's
OK_GREEN = cube(0, 4, 0)
OK_RIM = cube(0, 2, 0)
WARN_RED = cube(5, 0, 0)
WARN_RIM = cube(3, 0, 0)


def draw_dot():
    """Status OK: a green dot on the mask's black background."""
    px = [[BLACK] * ICON for _ in range(ICON)]
    c = (ICON - 1) / 2
    for y in range(ICON):
        for x in range(ICON):
            d2 = (x - c) ** 2 + (y - c) ** 2
            if d2 <= 6.4 ** 2:
                px[y][x] = OK_RIM if d2 > 5.2 ** 2 else OK_GREEN
    return px


def draw_triangle():
    """Status not OK: a red triangle pointing up, on black."""
    px = [[BLACK] * ICON for _ in range(ICON)]
    top, bottom = 1, ICON - 2
    for y in range(top, bottom + 1):
        half = (y - top) * (ICON - 2) / (2 * (bottom - top))
        c = (ICON - 1) / 2
        for x in range(ICON):
            if abs(x - c) <= half + 0.5:
                edge = abs(x - c) > half - 0.7 or y == bottom
                px[y][x] = WARN_RIM if edge else WARN_RED
    return px


ICON_MONO = 16


def draw_app_icon():
    """The app-switcher icon, 16 x 16 monochrome: the bucket over a level line. 1 = white."""
    rows = [
        "................",
        "................",
        "................",
        "..#..........#..",
        "..#..........#..",
        "..#..........#..",
        "..#..........###",
        "..#..........#..",
        "..#..........#..",
        "..############..",
        "...##########...",
        "................",
        "#.#.#.#.#.#.#.#.",
        "................",
        "################",
        "................",
    ]
    return [[1 if c == "#" else 0 for c in row] for row in rows]


def pack_mono(px):
    out = []
    for row in px:
        for x in range(0, len(row), 8):
            byte = 0
            for bit in range(8):
                if x + bit < len(row) and row[x + bit]:
                    byte |= 0x80 >> bit
            out.append(byte)
    return bytes(out)


def rle(px):
    """ISO 11783-6 8-bit run-length encoding: (count, colour) pairs, runs of 1..255 within a row."""
    out = []
    for row in px:
        x = 0
        while x < len(row):
            colour, n = row[x], 1
            while x + n < len(row) and row[x + n] == colour and n < 255:
                n += 1
            out += [n, colour]
            x += n
    return bytes(out)


def palette_rgb(index):
    basic = [(0, 0, 0), (255, 255, 255), (0, 153, 0), (0, 153, 153), (153, 0, 0), (153, 0, 153),
             (153, 153, 0), (204, 204, 204), (153, 153, 153), (0, 0, 255), (0, 255, 0), (0, 255, 255),
             (255, 0, 0), (255, 0, 255), (255, 255, 0), (0, 0, 153)]
    if index < 16:
        return basic[index]
    i = index - 16
    return (i // 36 * 0x33, (i // 6) % 6 * 0x33, i % 6 * 0x33)


def write_preview(path, pictures):
    from PIL import Image, ImageDraw
    scale, gap = 4, 16
    width = sum(len(px[0]) * scale + gap for _, px in pictures) + gap
    img = Image.new("RGB", (width, H * scale + 2 * gap + 12), (40, 40, 40))
    draw = ImageDraw.Draw(img)
    ox = gap
    for name, px in pictures:
        for y in range(len(px)):
            for x in range(len(px[0])):
                draw.rectangle([ox + x * scale, gap + y * scale, ox + x * scale + scale - 1, gap + y * scale + scale - 1],
                               fill=palette_rgb(px[y][x]))
        draw.text((ox, gap + H * scale + 4), name, fill=(255, 255, 255))
        ox += len(px[0]) * scale + gap
    img.save(path)


def write_cpp(path, pictures, icon):
    lines = ["// GENERATED by tools/generate_vt_images.py -- do not edit; change the script and regenerate.",
             "// ISO 11783-6 PictureGraphic data: the colour pictures in the 8-bit standard palette,",
             "// run-length encoded; the app-switcher icon monochrome, 1 bit per pixel.",
             '#include "VTImages.hpp"', "", "namespace triton", "{", ""]
    for name, px in pictures:
        data = rle(px)
        lines.append(f"const uint8_t {name}[{len(data)}] PROGMEM = {{")
        for i in range(0, len(data), 16):
            lines.append("    " + ", ".join(f"0x{b:02X}" for b in data[i:i + 16]) + ",")
        lines.append("};")
        lines.append(f"const uint32_t {name}Size = sizeof({name});")
        lines.append("")
    data = pack_mono(icon)
    lines.append(f"const uint8_t kScraperIconImage[{len(data)}] PROGMEM = {{")
    for i in range(0, len(data), 16):
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in data[i:i + 16]) + ",")
    lines.append("};")
    lines.append("const uint32_t kScraperIconImageSize = sizeof(kScraperIconImage);")
    lines.append("")
    lines.append(f"const uint16_t kScraperImageWidth = {W};")
    lines.append(f"const uint16_t kScraperImageHeight = {H};")
    lines.append(f"const uint16_t kStatusIconSize = {ICON};")
    lines.append(f"const uint16_t kScraperIconSize = {ICON_MONO};")
    lines.append("")
    lines.append("}  // namespace triton")
    with open(path, "w", encoding="ascii", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    return {name: len(rle(px)) for name, px in pictures}


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview")
    args = ap.parse_args()
    on, above, below = draw(0), draw(-7), draw(6)
    pictures = [("kScraperOnPlaneImage", on), ("kScraperAbovePlaneImage", above),
                ("kScraperBelowPlaneImage", below),
                ("kStatusOkImage", draw_dot()), ("kStatusWarnImage", draw_triangle())]
    icon = draw_app_icon()
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, "..", "lib", "KilverbakbesturingCore", "src", "isobus", "VTImages.generated.cpp")
    sizes = write_cpp(out, pictures, icon)
    print("wrote", os.path.normpath(out), sizes, f"({W}x{H}), icon {len(pack_mono(icon))} B")
    if args.preview:
        icon_px = [[WHITE if v else BLACK for v in row] for row in icon]
        write_preview(args.preview, [("on the plane", on), ("above: lower", above), ("below: raise", below),
                                     ("OK", draw_dot()), ("not OK", draw_triangle()), ("icon", icon_px)])
        print("preview", args.preview)
