"""Load a PNG as a 16-colour texture for core's `texture` struct. Standard library only, so
it runs in the console build images.

The image is padded (bottom/right, with its first colour) up to a power-of-two size, since
the consoles need one; texture coordinates made for the original image must be scaled by
(scale_u, scale_v) to hit the same pixels.
"""
import struct
import sys
import zlib

SIZES = (8, 16, 32)  # core/world.h TEXTURE_MIN..TEXTURE_MAX


def read_png(path):
    """Returns ((w, h), pixel(x, y) -> (r, g, b, a)) for a non-interlaced PNG: greyscale or
    palette at 1-8 bits, or RGB, grey + alpha or RGBA at 8 bits."""
    data = open(path, 'rb').read()
    if data[:8] != bytes.fromhex('89504e470d0a1a0a'):
        sys.exit(f'{path}: not a PNG')
    pos, idat, plte, trns, kind = 8, b'', b'', b'', None
    while kind != b'IEND':
        n, kind = struct.unpack('>I4s', data[pos:pos + 8])
        body, pos = data[pos + 8:pos + 8 + n], pos + 12 + n
        if kind == b'IHDR':
            w, h, depth, ctype, _, _, interlace = struct.unpack('>IIBBBBB', body)
        elif kind == b'PLTE':
            plte = body
        elif kind == b'tRNS':
            trns = body
        elif kind == b'IDAT':
            idat += body
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ctype]
    if interlace or depth > 8 or (depth < 8 and ctype not in (0, 3)):
        sys.exit(f'{path}: interlaced or 16-bit PNGs are not supported')
    bpp = max(1, channels * depth // 8)  # bytes to the left pixel, for the filters
    stride = (w * channels * depth + 7) // 8
    raw, prev, rows = zlib.decompress(idat), bytearray(stride), []
    for y in range(h):
        kind, row = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = row[i - bpp] if i >= bpp else 0
            b, c = prev[i], prev[i - bpp] if i >= bpp else 0
            if kind == 1:
                row[i] = (row[i] + a) & 255
            elif kind == 2:
                row[i] = (row[i] + b) & 255
            elif kind == 3:
                row[i] = (row[i] + (a + b) // 2) & 255
            elif kind == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                row[i] = (row[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(row)
        prev = row

    def sample(x, y, ch):  # channel ch of pixel (x, y), as stored
        bit = (x * channels + ch) * depth
        return rows[y][bit // 8] >> (8 - depth - bit % 8) & ((1 << depth) - 1)

    def pixel(x, y):
        if ctype == 3:
            i = sample(x, y, 0)
            return (*plte[i * 3:i * 3 + 3], trns[i] if i < len(trns) else 255)
        v = [sample(x, y, ch) * 255 // ((1 << depth) - 1) for ch in range(channels)]
        if ctype == 0:
            opaque = not trns or v[0] != struct.unpack('>H', trns)[0] * 255 // ((1 << depth) - 1)
            return (v[0], v[0], v[0], 255 if opaque else 0)
        if ctype == 2:
            return (*v, 255 if not trns or tuple(v) != struct.unpack('>HHH', trns) else 0)
        return (v[0], v[0], v[0], v[1]) if ctype == 4 else tuple(v)
    return (w, h), pixel


def load_texture(path, coat_path=None):
    """Returns (w, h, palette, pixels, scale_u, scale_v, coat_first): padded size, 16
    (r, g, b), 4-bit pixel bytes (rows top to bottom, low nibble = left pixel), UV scale
    factors, and the first palette index of the coat's shades (16 without a coat).

    With a coat, its opaque pixels are layered over the image; its colours (greys, to be
    tinted at runtime) get the last palette entries so they can be swapped."""
    (iw, ih), img = read_png(path)
    (cw, ch), coat = read_png(coat_path) if coat_path else ((iw, ih), None)
    if (cw, ch) != (iw, ih):
        sys.exit(f'{coat_path}: must be the same size as {path}')
    w = next((s for s in SIZES if s >= iw), None)
    h = next((s for s in SIZES if s >= ih), None)
    if w is None or h is None:
        sys.exit(f'{path}: {iw}x{ih} is over the {SIZES[-1]} px limit')

    def opaque(im):
        return {} if im is None else {(x, y): im(x, y)[:3] for y in range(ih) for x in range(iw) if im(x, y)[3]}
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
