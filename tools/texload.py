"""Load a PNG as a 16-colour texture for core's `texture` struct. Needs Pillow.

The image is padded (bottom/right, with its first colour) up to a power-of-two size, since
the consoles need one; texture coordinates made for the original image must be scaled by
(scale_u, scale_v) to hit the same pixels.
"""
import sys

from PIL import Image

SIZES = (8, 16, 32)  # core/world.h TEXTURE_MIN..TEXTURE_MAX


def load_texture(path):
    """Returns (w, h, palette, pixels, scale_u, scale_v): padded size, 16 (r, g, b), 4-bit
    pixel bytes (rows top to bottom, low nibble = left pixel), UV scale factors."""
    img = Image.open(path).convert('RGB')
    iw, ih = img.size
    w = next((s for s in SIZES if s >= iw), None)
    h = next((s for s in SIZES if s >= ih), None)
    if w is None or h is None:
        sys.exit(f'{path}: {iw}x{ih} is over the {SIZES[-1]} px limit')
    colors = img.getcolors(16)
    if colors is None:
        sys.exit(f'{path}: more than 16 colours')
    palette = [c for _, c in colors] + [(0, 0, 0)] * (16 - len(colors))
    index = {c: i for i, c in enumerate(palette[:len(colors)])}
    px = [index[img.getpixel((x, y))] if x < iw and y < ih else 0 for y in range(h) for x in range(w)]
    pixels = bytes(px[i] | px[i + 1] << 4 for i in range(0, len(px), 2))
    return w, h, palette, pixels, iw / w, ih / h
