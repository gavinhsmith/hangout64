"""Load an OBJ (triangles or polygons, Z-up, front facing -Y) as an indexed mesh.

The model is turned Y-up with its front facing -Z, centred on X/Z, feet at y = 0 and
scaled to `height` units. Positions are 8.8 fixed point (256 = 1 unit). Each triangle
gets `rgb` shaded by how much it faces a fixed light, so the shape reads without
runtime lighting.
"""

LIGHT = (0.4, 0.8, 0.45)  # roughly from above, front-right


def load_obj(path, height, rgb):
    """Returns (verts, tris, colors): fixed-point (x, y, z), (a, b, c) indices, (r, g, b)."""
    verts, tris = [], []
    for line in open(path, encoding='utf-8'):
        parts = line.split()
        if parts and parts[0] == 'v':
            x, y, z = map(float, parts[1:4])
            verts.append((-x, z, y))  # Z-up, front -Y  ->  Y-up, front -Z (a proper rotation)
        elif parts and parts[0] == 'f':
            idx = [int(p.split('/')[0]) - 1 for p in parts[1:]]
            tris += [(idx[0], idx[i], idx[i + 1]) for i in range(1, len(idx) - 1)]

    lo = [min(v[i] for v in verts) for i in range(3)]
    hi = [max(v[i] for v in verts) for i in range(3)]
    scale = height / (hi[1] - lo[1])
    centre = ((lo[0] + hi[0]) / 2, lo[1], (lo[2] + hi[2]) / 2)
    fixed = [tuple(round((v[i] - centre[i]) * scale * 256) for i in range(3)) for v in verts]
    colors = [tuple(min(255, round(c * _shade(verts, t))) for c in rgb) for t in tris]
    return fixed, tris, colors


def _shade(verts, t):
    a, b, c = (verts[i] for i in t)
    u = [b[i] - a[i] for i in range(3)]
    w = [c[i] - a[i] for i in range(3)]
    n = (u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0])
    length = sum(k * k for k in n) ** 0.5 or 1
    light = sum(n[i] * LIGHT[i] for i in range(3)) / length / sum(k * k for k in LIGHT) ** 0.5
    return 0.55 + 0.45 * abs(light)  # abs: don't depend on the file's winding order
