"""Draws the plough control's VT pictures and writes them as VT3 PictureGraphic data.

Top view of a tractor driving up the screen with a mounted plough trailing diagonally behind it:
unploughed stubble on one side, turned furrows on the other. Two pictures, one per ploughing side;
"R" is the exact mirror of "L". Pixel art drawn here in code, so it can be changed and regenerated:

    py tools/generate_vt_images.py            # writes lib/.../isobus/VTImages.generated.cpp
    py tools/generate_vt_images.py --preview out.png   # also a 4x preview of both

Colours are indices into the ISO 11783-6 standard 8-bit palette (16 + 36 r + 6 g + b, each 0..5 in
steps of 0x33). The data is run-length encoded (count, colour) pairs, runs never crossing a row.
"""
import argparse
import os

W, H = 120, 76


def cube(r, g, b):
    return 16 + 36 * r + 6 * g + b


BLACK = 0
WHITE = 1
STUBBLE = cube(4, 5, 2)        # light yellow-green
STUBBLE_ROW = cube(3, 4, 1)    # crop rows
FURROW_DARK = cube(2, 1, 0)
FURROW_LIGHT = cube(3, 2, 1)
TRACTOR = cube(0, 1, 4)        # blue body
TRACTOR_DARK = cube(0, 0, 2)
WINDOW = cube(4, 5, 5)
TYRE = cube(1, 1, 1)
STEEL = cube(1, 1, 1)
MOULDBOARD = cube(4, 4, 4)
SHARE = cube(2, 2, 2)


def draw_left():
    """The "L" picture: plough trailing to the left, turned soil to the right."""
    px = [[STUBBLE] * W for _ in range(H)]
    cx = 70                      # tractor centre line
    hitch_y = 40
    end_x, end_y = cx - 36, H - 2  # last plough body

    def beam_x(y):
        return cx + (end_x - cx) * (y - hitch_y) / (end_y - hitch_y)

    # Field: stubble with crop rows; furrows right of the tractor, and right of the plough behind it.
    for y in range(H):
        boundary = cx + 13 if y < hitch_y else beam_x(y) + 2
        for x in range(W):
            if x >= boundary:
                px[y][x] = FURROW_DARK if (x // 4) % 2 == 0 else FURROW_LIGHT
            elif x % 6 == 0:
                px[y][x] = STUBBLE_ROW

    def rect(x0, y0, x1, y1, colour):
        for y in range(max(0, y0), min(H, y1 + 1)):
            for x in range(max(0, x0), min(W, x1 + 1)):
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

    # Plough: a steel beam from the hitch to the last body, four mouldboards on its right side.
    line(cx, hitch_y, end_x, end_y, STEEL, 1.2)
    for f in (0.28, 0.5, 0.72, 0.94):
        bx = cx + (end_x - cx) * f
        by = hitch_y + (end_y - hitch_y) * f
        line(bx + 1, by - 1, bx + 9, by + 2, MOULDBOARD, 1.3)   # mouldboard
        line(bx, by, bx + 3, by + 2, SHARE, 0.8)              # share point

    # Tractor, driving up: rear and front wheels, bonnet, cab with roof window, hitch.
    rect(cx - 14, 22, cx - 9, 37, TYRE)
    rect(cx + 9, 22, cx + 14, 37, TYRE)
    for y in range(23, 37, 3):                                # tread
        rect(cx - 14, y, cx - 9, y, BLACK)
        rect(cx + 9, y, cx + 14, y, BLACK)
    rect(cx - 11, 5, cx - 8, 14, TYRE)
    rect(cx + 8, 5, cx + 11, 14, TYRE)
    rect(cx - 5, 2, cx + 5, 20, TRACTOR)                      # bonnet
    rect(cx - 5, 2, cx + 5, 3, TRACTOR_DARK)                  # grille
    rect(cx - 8, 18, cx + 8, 35, TRACTOR)                     # cab
    rect(cx - 6, 21, cx + 6, 31, WINDOW)                      # roof window
    rect(cx - 6, 21, cx + 6, 21, TRACTOR_DARK)
    rect(cx - 2, 36, cx + 2, hitch_y, STEEL)                  # three-point hitch
    return px


ICON = 14                     # status icons: ICON x ICON
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


def mirror(px):
    return [list(reversed(row)) for row in px]


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
    img = Image.new("RGB", (len(pictures) * (W * scale + gap) + gap, H * scale + 2 * gap + 12), (40, 40, 40))
    draw = ImageDraw.Draw(img)
    for n, (name, px) in enumerate(pictures):
        ox = gap + n * (W * scale + gap)
        for y in range(len(px)):
            for x in range(len(px[0])):
                draw.rectangle([ox + x * scale, gap + y * scale, ox + x * scale + scale - 1, gap + y * scale + scale - 1],
                               fill=palette_rgb(px[y][x]))
        draw.text((ox, gap + H * scale + 4), name, fill=(255, 255, 255))
    img.save(path)


def write_cpp(path, pictures):
    lines = ["// GENERATED by tools/generate_vt_images.py -- do not edit; change the script and regenerate.",
             "// ISO 11783-6 PictureGraphic data, 8-bit standard palette, run-length encoded.",
             '#include "VTImages.hpp"', "", "namespace triton", "{", ""]
    for name, px in pictures:
        data = rle(px)
        lines.append(f"const uint8_t {name}[{len(data)}] PROGMEM = {{")
        for i in range(0, len(data), 16):
            lines.append("    " + ", ".join(f"0x{b:02X}" for b in data[i:i + 16]) + ",")
        lines.append("};")
        lines.append(f"const uint32_t {name}Size = sizeof({name});")
        lines.append("")
    lines.append(f"const uint16_t kPloughImageWidth = {W};")
    lines.append(f"const uint16_t kPloughImageHeight = {H};")
    lines.append(f"const uint16_t kStatusIconSize = {ICON};")
    lines.append("")
    lines.append("}  // namespace triton")
    with open(path, "w", encoding="ascii", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    return {name: len(rle(px)) for name, px in pictures}


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview")
    args = ap.parse_args()
    left = draw_left()
    pictures = [("kPloughLeftImage", left), ("kPloughRightImage", mirror(left)),
                ("kStatusOkImage", draw_dot()), ("kStatusWarnImage", draw_triangle())]
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, "..", "lib", "PloegbesturingCore", "src", "isobus", "VTImages.generated.cpp")
    sizes = write_cpp(out, pictures)
    print("wrote", os.path.normpath(out), sizes, f"({W}x{H})")
    if args.preview:
        write_preview(args.preview, [("L: plough left, furrows right", left), ("R: mirror", mirror(left)),
                                     ("OK", draw_dot()), ("not OK", draw_triangle())])
        print("preview", args.preview)
