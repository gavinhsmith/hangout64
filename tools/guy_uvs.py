"""Add texture coordinates for the guy's atlas (assets/texture/guy_base.png + guy_coat.png)
to assets/model/guy.obj, in place.

OwlCAD exports the guy without UVs, so this works them out per body part:

- Each part has an "up": world up, except the arms, where it runs from hand to shoulder.
  A vertex's height along it picks the row in the part's atlas region.
- Across, a vertex is placed relative to the part's whole cross-section at that height,
  so every side of a part spans its region's full width at every height: the body narrows
  toward the neck and its texture narrows with it, and stripes stay straight. "Right" comes
  from up and the face's outward normal, so nothing is mirrored.
- Flat tops and bottoms (head, body bottom, soles) are projected straight down instead.
- The bottoms of the legs (the trousers' rim around the shoe) and of the arms (the end cap
  at the hand) have no texture: they're moved into the OBJ group "hidden", which the
  loader skips, so you see through them.

The model's winding must be counter-clockwise from outside (OwlCAD's is). Re-run this
after re-exporting the model:

    python3 tools/guy_uvs.py assets/model/guy.obj

Atlas regions are in pixels, inclusive, in the 16x16 atlas.
"""
import sys

ATLAS_W, ATLAS_H = 16, 16
BODY_FRONT, BODY_SIDE, BODY_BACK, BODY_BOTTOM = (0, 0, 6, 7), (7, 0, 8, 7), (9, 0, 15, 7), (0, 8, 6, 10)
ARM_SIDE, LEG_SIDE, SHOE = (10, 8, 12, 15), (13, 8, 15, 15), (8, 12, 9, 15)
HEAD_FRONT, HEAD_REST = (0, 12, 3, 15), (4, 12, 7, 15)
PARTS = ('head', 'body', 'arm', 'leg', 'shoe', 'hidden')
INSET = 0.125  # px: keeps nearest sampling off the neighbouring texel; survives 8.8 rounding


def sub(a, b): return [a[i] - b[i] for i in range(3)]
def dot(a, b): return sum(a[i] * b[i] for i in range(3))
def cross(a, b): return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]


def norm(a):
    length = dot(a, a) ** 0.5
    return [c / length for c in a] if length > 1e-9 else [0.0, 0.0, 0.0]


def split(poly, f):
    """Cut a convex polygon where f (roughly linear along each edge) crosses 0."""
    vals = [f(p) for p in poly]
    if all(v > -1e-4 for v in vals) or all(v < 1e-4 for v in vals):
        return [poly]
    below, above = [], []
    for k, (p, fp) in enumerate(zip(poly, vals)):
        q, fq = poly[(k + 1) % len(poly)], vals[(k + 1) % len(poly)]
        if fp < 1e-4:
            below.append(p)
        if fp > -1e-4:
            above.append(p)
        if (fp < -1e-4 and fq > 1e-4) or (fp > 1e-4 and fq < -1e-4):
            a, b = 0.0, 1.0  # bisect: f isn't exactly linear along the edge
            for _ in range(40):
                m = (a + b) / 2
                if (f([p[i] + m * (q[i] - p[i]) for i in range(3)]) < 0) == (fp < 0):
                    a = m
                else:
                    b = m
            x = tuple(p[i] + a * (q[i] - p[i]) for i in range(3))
            below.append(x)
            above.append(x)
    return [h for h in (below, above) if len(h) >= 3]


def part_of(pts, n):
    """Body part of a triangle, from its corners and outward normal (OBJ space: Z up,
    front -Y, units mm). None for faces that should be hidden."""
    xs, ys, zs = zip(*pts)
    if min(zs) >= 24 - 1e-3:
        return 'head'
    if any(abs(x) > 9 for x in xs) or any(abs(z - 24) < 1e-3 and 1.6 < abs(x) < 2.3 and abs(y) < 0.3 for x, y, z in pts):
        return None if max(zs) < 12.5 else 'arm'  # all near the hand: the arm's end cap
    if max(zs) <= 3 + 1e-3:
        return None if abs(min(zs) - 3) < 1e-3 and n[2] < 0 else 'shoe'  # trouser rim faces down
    if max(zs) <= 10 + 1e-3 and min(zs) < 10 - 1e-3:
        return 'leg'
    return 'body'


def region(part, facing):
    """Atlas region for a face of `part` whose outward normal is mostly along `facing`."""
    if part == 'arm':
        return ARM_SIDE
    if part == 'leg':
        return LEG_SIDE
    if part == 'shoe':
        return SHOE
    if part == 'head':
        return HEAD_FRONT if facing == (1, -1) else HEAD_REST
    return {(1, -1): BODY_FRONT, (1, 1): BODY_BACK, (0, -1): BODY_SIDE, (0, 1): BODY_SIDE}.get(facing, BODY_BOTTOM)


def main():
    path = sys.argv[1]
    lines = open(path, encoding='utf-8').read().splitlines()
    verts = [tuple(map(float, l.split()[1:4])) for l in lines if l.startswith('v ')]
    if any(l.startswith('g ') for l in lines):
        sys.exit(f'{path} already has texture coordinates; run this on a fresh export')
    tris = []
    for l in lines:
        if l.startswith('f '):
            idx = [int(p.split('/')[0]) - 1 for p in l.split()[1:]]
            tris += [(idx[0], idx[i], idx[i + 1]) for i in range(1, len(idx) - 1)]
    normals = [norm(cross(sub(verts[t[1]], verts[t[0]]), sub(verts[t[2]], verts[t[0]]))) for t in tris]
    parts = [part_of([verts[i] for i in t], n) for t, n in zip(tris, normals)]

    # Group each part's triangles (left and right limbs separately) and give it an "up".
    groups = {}
    for i, part in enumerate(parts):
        if part:
            side = sum(verts[k][0] for k in tris[i]) > 0 if part in ('arm', 'leg', 'shoe') else None
            groups.setdefault((part, side), []).append(i)
    ups = {}
    for key, members in groups.items():
        if key[0] == 'arm':
            pts = {v for i in members for v in tris[i]}
            def middle(near):  # of the bounding box: unchanged by the cuts' extra vertices
                ends = [verts[v] for v in pts if near(verts[v][2])]
                return [(min(p[a] for p in ends) + max(p[a] for p in ends)) / 2 for a in range(3)]
            ups[key] = norm(sub(middle(lambda z: z > 21), middle(lambda z: z < 12.5)))
        else:
            ups[key] = [0.0, 0.0, 1.0]
    heights, bounds = {}, {}
    for key, members in groups.items():
        pts = {v for i in members for v in tris[i]}
        hs = [dot(verts[v], ups[key]) for v in pts]
        heights[key] = min(hs), max(hs)
        bounds[key] = [min(verts[v][a] for v in pts) for a in range(3)], [max(verts[v][a] for v in pts) for a in range(3)]

    # Flat panels: the coplanar triangles of one group.
    # Biggest first, matched by distance: small triangles have noisy normals.
    def area(i):
        t = tris[i]
        c = cross(sub(verts[t[1]], verts[t[0]]), sub(verts[t[2]], verts[t[0]]))
        return dot(c, c)
    panels = {}  # (group, normal, offset) -> triangles
    for key, members in groups.items():
        for i in sorted(members, key=area, reverse=True):
            n = normals[i]
            plane = next((pl for pl in panels if pl[0] == key and dot(pl[1], n) > 0.9 and
                          all(abs(dot(pl[1], verts[v]) - pl[2]) < 0.01 for v in tris[i])),
                         (key, tuple(n), dot(n, verts[tris[i][0]])))
            panels.setdefault(plane, []).append(i)
    group_of = {i: key for key, members in groups.items() for i in members}

    pieces = []  # (positions, uvs) per output triangle
    for members in panels.values():
        key = group_of[members[0]]
        part, up, n = key[0], ups[key], normals[members[0]]
        h_lo, h_hi = heights[key]
        axis = max(range(3), key=lambda a: abs(n[a]))
        x0, y0, x1, y1 = region(part, (axis, 1 if n[axis] > 0 else -1))
        w = x1 + 1 - x0
        right = cross(up, n)
        flat = dot(right, right) < 0.05  # perpendicular to up: a top or bottom
        right = norm(right)
        lo3, hi3 = bounds[key]

        def section(h):
            """The panel's extent along `right` at height h along `up`."""
            hits = []
            for i in members:
                t = tris[i]
                for k in range(3):
                    p, q = verts[t[k]], verts[t[(k + 1) % 3]]
                    hp, hq = dot(p, up), dot(q, up)
                    if min(hp, hq) - 1e-4 <= h <= max(hp, hq) + 1e-4:
                        if abs(hq - hp) < 1e-9:
                            hits += [dot(p, right), dot(q, right)]
                        else:
                            f = min(1, max(0, (h - hp) / (hq - hp)))
                            hits.append(dot([p[a] + f * (q[a] - p[a]) for a in range(3)], right))
            return min(hits), max(hits)

        def st(p):
            if flat:  # straight down: front of the model at the top of the region
                s = (p[0] - lo3[0]) / ((hi3[0] - lo3[0]) or 1)
                t = (p[1] - lo3[1]) / ((hi3[1] - lo3[1]) or 1)
                return s, 1 - t if n[2] > 0 else t
            h = dot(p, up)
            lo, hi = section(h)
            return (0.5 if hi - lo < 1e-4 else (dot(p, right) - lo) / (hi - lo)), (h_hi - h) / ((h_hi - h_lo) or 1)

        # The stretch isn't affine, so cut the panel along each texel column's edges: then
        # every column is exactly where it belongs, with no kink along triangle diagonals.
        polys = [[verts[v] for v in tris[i]] for i in members]
        mids = [[(verts[t[k]][a] + verts[t[(k + 1) % 3]][a]) / 2 for a in range(3)] for t in (tris[i] for i in members) for k in range(3)]
        ends = [(verts[t[k]], verts[t[(k + 1) % 3]]) for t in (tris[i] for i in members) for k in range(3)]
        affine = all(abs(st(m)[0] - (st(p)[0] + st(q)[0]) / 2) < 1e-4 for m, (p, q) in zip(mids, ends))
        cut = not flat and not affine
        if cut:
            # Bands between the panel's corner heights have straight sides, so in each band
            # the column edges are straight too.
            for h in sorted({round(dot(verts[v], up), 4) for i in members for v in tris[i]}):
                polys = [half for poly in polys for half in split(poly, lambda p: dot(p, up) - h)]
            for k in range(1, w):
                c = k / w
                polys = [half for poly in polys for half in split(poly, lambda p: st(p)[0] - c)]
        for poly in polys:
            coords = [st(p) for p in poly]
            col = min(w - 1, int(sum(s for s, t in coords) / len(coords) * w)) if cut else None
            uvs = []
            for s, t in coords:
                px = x0 + s * w
                px = min(max(px, x0 + col + INSET), x0 + col + 1 - INSET) if col is not None else                     min(max(px, x0 + INSET), x1 + 1 - INSET)
                py = min(max(y0 + t * (y1 + 1 - y0), y0 + INSET), y1 + 1 - INSET)
                uvs.append((round(px / ATLAS_W, 5), round(1 - py / ATLAS_H, 5)))  # OBJ v runs up
            for j in range(1, len(poly) - 1):
                a, b, c = poly[0], poly[j], poly[j + 1]
                if dot(*[cross(sub(b, a), sub(c, a))] * 2) > 1e-6:  # drop slivers
                    pieces.append((part, (a, b, c), (uvs[0], uvs[j], uvs[j + 1])))

    positions, pos_index, texcoords, index = [], {}, [], {}

    def ref(table, lookup, value):
        if value not in lookup:
            lookup[value] = len(table) + 1
            table.append(value)
        return lookup[value]
    for p in verts:  # keep the file's vertices (and numbering) first
        ref(positions, pos_index, tuple(round(c, 5) for c in p))
    shown = []
    for part in PARTS[:-1]:
        faces = ['f ' + ' '.join(f'{ref(positions, pos_index, tuple(round(c, 5) for c in p))}/{ref(texcoords, index, uv)}'
                                 for p, uv in zip(*piece[1:])) for piece in pieces if piece[0] == part]
        shown += [f'g {part}'] + faces if faces else []
    hidden = ['f ' + ' '.join(str(v + 1) for v in t) for i, t in enumerate(tris) if not parts[i]]

    out = [l for l in lines if not l.startswith(('v ', 'vt ', 'f ', 'g '))]
    out += ['v %.5f %.5f %.5f' % p for p in positions] + ['vt %g %g' % uv for uv in texcoords] + shown
    out += ['g hidden'] + hidden if hidden else []
    open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
    print(f'{path}: {len(pieces)} textured triangles, {len(hidden)} hidden')


main()
