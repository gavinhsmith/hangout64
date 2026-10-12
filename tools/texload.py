"""Load a PNG as a 16-colour texture for core's `texture` struct. Needs Pillow.

The image is padded (bottom/right, with its first colour) up to a power-of-two size, since
the consoles need one; texture coordinates made for the original image must be scaled by
(scale_u, scale_v) to hit the same pixels.
"""
import sys

from PIL import Image

SIZES = (8, 16, 32)  # core/world.h TEXTURE_MIN..TEXTURE_MAX


def load_texture(path, coat_path=None):
    """Returns (w, h, palette, pixels, scale_u, scale_v, coat_first): padded size, 16
    (r, g, b), 4-bit pixel bytes (rows top to bottom, low nibble = left pixel), UV scale
    factors, and the first palette index of the coat's shades (16 without a coat).

    With a coat, its opaque pixels are layered over the image; its colours (greys, to be
    tinted at runtime) get the last palette entries so they can be swapped."""
    img = Image.open(path).convert('RGBA')
    coat = Image.open(coat_path).convert('RGBA') if coat_path else None
    if coat and coat.size != img.size:
        sys.exit(f'{coat_path}: must be the same size as {path}')
    iw, ih = img.size
    w = next((s for s in SIZES if s >= iw), None)
    h = next((s for s in SIZES if s >= ih), None)
    if w is None or h is None:
        sys.exit(f'{path}: {iw}x{ih} is over the {SIZES[-1]} px limit')

    def opaque(im):
        return {} if im is None else {(x, y): im.getpixel((x, y))[:3] for y in range(ih) for x in range(iw) if im.getpixel((x, y))[3]}
    base_px, coat_px = opaque(img), opaque(coat)
    coat_colors = sorted(set(coat_px.values()))
    base_colors = sorted(set(c for p, c in base_px.items() if p not in coat_px)) or [(0, 0, 0)]
    if len(base_colors) + len(coat_colors) > 16:
        sys.exit(f'{path}: more than 16 colours (with the coat)')
    coat_first = 16 - len(coat_colors)
    palette = base_colors + [(0, 0, 0)] * (coat_first - len(base_colors)) + coat_colors
    index = {c: i for i, c in enumerate(base_colors)}
    index.update({('coat', c): coat_first + i for i, c in enumerate(coat_colors)})

    def at(x, y):
        if (x, y) in coat_px:
            return index[('coat', coat_px[(x, y)])]
        return index[base_px[(x, y)]] if (x, y) in base_px else 0  # transparent / padding
    px = [at(x, y) for y in range(h) for x in range(w)]
    pixels = bytes(px[i] | px[i + 1] << 4 for i in range(0, len(px), 2))
    return w, h, palette, pixels, iw / w, ih / h, coat_first
