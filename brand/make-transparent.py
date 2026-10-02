"""Give the Red Warden Multistream logo a real transparent background.

The original renders were RGB: the rounded tile sat on an opaque white square, so
white corners showed everywhere (Explorer, installer, taskbar). This rebuilds an
RGBA master with an analytic, anti-aliased rounded-rect alpha, then regenerates
every PNG size and logo.ico from it.

    python brand\\make-transparent.py
"""
import math
import os

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "logo-1024.png")  # actually 1254 px, RGB
SS = 4  # supersampling for the mask edge


def is_dark(p):
    return sum(p[:3]) < 3 * 60


def corner_radius(im):
    """Walk the diagonal from the top-left corner until we hit the dark tile."""
    w, _ = im.size
    for d in range(w // 2):
        if is_dark(im.getpixel((d, d))):
            break
    # A circle of radius r tangent to both edges meets the diagonal at r(1 - 1/sqrt2).
    return d / (1 - 1 / math.sqrt(2))


def rounded_mask(size, r):
    big = size * SS
    rr = r * SS
    m = Image.new("L", (big, big), 0)
    px = m.load()
    for y in range(big):
        for x in range(big):
            cx = min(max(x + 0.5, rr), big - rr)
            cy = min(max(y + 0.5, rr), big - rr)
            if (x + 0.5 - cx) ** 2 + (y + 0.5 - cy) ** 2 <= rr * rr:
                px[x, y] = 255
    return m.resize((size, size), Image.LANCZOS)


def main():
    src = Image.open(SRC).convert("RGB")
    w, h = src.size
    assert w == h, "expected a square master"
    r = corner_radius(src)
    print(f"master {w}px, corner radius ~{r:.1f}px")

    # Work at 1024 so the file name is finally true.
    base = src.resize((1024, 1024), Image.LANCZOS)
    r1024 = r * 1024 / w
    mask = rounded_mask(1024, r1024)

    # Pixels on the curved edge were blended with white in the original; paint
    # them with the tile's own colour so no white fringe survives under the alpha.
    tile = base.getpixel((512, 6))
    rgba = base.convert("RGBA")
    px = rgba.load()
    mp = mask.load()
    for y in range(1024):
        for x in range(1024):
            a = mp[x, y]
            if a < 255:
                px[x, y] = (tile[0], tile[1], tile[2], a)
    rgba.save(os.path.join(HERE, "logo-1024.png"))

    for s in (256, 128, 64, 32, 16):
        rgba.resize((s, s), Image.LANCZOS).save(os.path.join(HERE, f"logo-{s}.png"))
    rgba.save(os.path.join(HERE, "logo.ico"),
              sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])

    # Check: corners transparent, centre opaque, no white anywhere it's visible.
    for s in (1024, 256, 16):
        im = Image.open(os.path.join(HERE, f"logo-{s}.png"))
        c, mid = im.getpixel((0, 0)), im.getpixel((s // 2, s // 2))
        print(f"logo-{s}.png {im.mode} corner alpha={c[3]} centre alpha={mid[3]}")
    print("ico sizes:", sorted(Image.open(os.path.join(HERE, "logo.ico")).info.get("sizes")))


if __name__ == "__main__":
    main()
