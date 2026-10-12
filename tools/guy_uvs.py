"""Add texture coordinates for the guy's atlas (assets/texture/guy_base.png + guy_coat.png)
to assets/model/guy.obj, in place.

OwlCAD exports the guy without UVs, so this works them out. Each flat panel of the model
(connected triangles of one body part lying in one plane, e.g. the front of the body) is
stretched over the atlas region for that part and the way it faces: at every height, the
panel's left edge maps to the region's left edge and its right edge to the right edge, so
the texture follows the shape (the body's front narrows toward the neck, and so does its
texture). "Up" is world up, except along the arms, where it runs from hand to shoulder.
Re-run it after re-exporting the model:

    python3 tools/guy_uvs.py assets/model/guy.obj

Atlas regions are in pixels, inclusive, in the 16x16 atlas.
"""
import sys

ATLAS_W, ATLAS_H = 16, 16
BODY_FRONT, BODY_SIDE, BODY_BACK, BODY_BOTTOM = (0, 0, 6, 7), (7, 0, 8, 7), (9, 0, 15, 7), (0, 8, 6, 10)
ARM_SIDE, LEG_SIDE, SHOE = (10, 8, 12, 15), (13, 8, 15, 15), (8, 12, 9, 15)
HEAD_FRONT, HEAD_REST = (0, 12, 3, 15), (4, 12, 7, 15)
INSET = 0.02  # px: keep nearest sampling off the neighbouring region at the edges


def sub(a, b): return [a[i] - b[i] for i in range(3)]
def dot(a, b): return sum(a[i] * b[i] for i in range(3))
def cross(a, b): return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]


def norm(a):
    length = dot(a, a) ** 0.5
    return [c / length for c in a] if length > 1e-9 else [0.0, 0.0, 0.0]


def part_of(pts):
    """Body part of a triangle, from its corners (OBJ space: Z up, front -Y, units mm)."""
    xs, ys, zs = zip(*pts)
    if min(zs) >= 24 - 1e-3:
        return 'head'
    if any(abs(x) > 9 for x in xs) or any(abs(z - 24) < 1e-3 and 1.6 < abs(x) < 2.3 and abs(y) < 0.3 for x, y, z in pts):
        return 'arm'  # reaches the hand, or the arm's top ring at the neck
    if max(zs) <= 3 + 1e-3:
        return 'shoe'
    if max(zs) <= 10 + 1e-3 and min(zs) < 10 - 1e-3:
        return 'leg'
    return 'body'


def region(part, facing):
    """Atlas region for a panel of `part` whose outward normal is mostly along `facing`."""
    if part == 'arm':
        return ARM_SIDE
    if part == 'leg':
        return LEG_SIDE
    if part == 'shoe':
        return SHOE
    if part == 'head':
        return HEAD_FRONT if facing == (1, -1) else HEAD_REST
    return {(1, -1): BODY_FRONT, (1, 1): BODY_BACK, (0, -1): BODY_SIDE, (0, 1): BODY_SIDE}.get(facing, BODY_BOTTOM)


def panels(verts, tris, parts):
    """Group triangles into flat panels: same part, sharing an edge, in the same plane."""
    normals = [norm(cross(sub(verts[t[1]], verts[t[0]]), sub(verts[t[2]], verts[t[0]]))) for t in tris]
    parent = list(range(len(tris)))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i
    edges = {}
    for i, t in enumerate(tris):
        for k in range(3):
            edges.setdefault(frozenset((t[k], t[(k + 1) % 3])), []).append(i)
    for shared in edges.values():
        for a in shared:
            for b in shared:
                same_plane = abs(dot(normals[a], normals[b])) > 0.999 and \
                    abs(dot(normals[a], sub(verts[tris[b][0]], verts[tris[a][0]]))) < 1e-3
                if a < b and parts[a] == parts[b] and same_plane:
                    parent[find(a)] = find(b)
    groups = {}
    for i in range(len(tris)):
        groups.setdefault(find(i), []).append(i)
    return list(groups.values())


def main():
    path = sys.argv[1]
    lines = open(path, encoding='utf-8').read().splitlines()
    verts = [tuple(map(float, l.split()[1:4])) for l in lines if l.startswith('v ')]
    tris = []
    for l in lines:
        if l.startswith('f '):
            idx = [int(p.split('/')[0]) - 1 for p in l.split()[1:]]
            tris += [(idx[0], idx[i], idx[i + 1]) for i in range(1, len(idx) - 1)]
    parts = [part_of([verts[i] for i in t]) for t in tris]

    # Centre of each part (left/right limbs separately), to tell outward from inward.
    def side_key(i):
        x = sum(verts[k][0] for k in tris[i]) / 3
        return parts[i], (x > 0) if parts[i] in ('arm', 'leg', 'shoe') else None
    centres = {}
    for i in range(len(tris)):
        centres.setdefault(side_key(i), set()).update(tris[i])
    centres = {k: [sum(verts[v][a] for v in vs) / len(vs) for a in range(3)] for k, vs in centres.items()}

    tri_uvs = [None] * len(tris)
    for panel in panels(verts, tris, parts):
        part = parts[panel[0]]
        n = [0.0, 0.0, 0.0]
        for i in panel:
            t = tris[i]
            c = cross(sub(verts[t[1]], verts[t[0]]), sub(verts[t[2]], verts[t[0]]))
            n = [n[a] + c[a] for a in range(3)]
        n = norm(n)
        pts = {v for i in panel for v in tris[i]}
        mid = [sum(verts[v][a] for v in pts) / len(pts) for a in range(3)]
        if dot(n, sub(mid, centres[side_key(panel[0])])) < 0:
            n = [-c for c in n]  # make it point outward
        axis = max(range(3), key=lambda a: abs(n[a]))
        x0, y0, x1, y1 = region(part, (axis, 1 if n[axis] > 0 else -1))

        # "Up" on the panel: along the arm for arms, else world up; horizontal panels use
        # the model's front at the top for bottoms, at the bottom for tops.
        if part == 'arm':
            up = norm([-(1 if mid[0] > 0 else -1) * 7.5, 0.0, 10.8])  # hand -> shoulder
        else:
            up = [0.0, 0.0, 1.0]
        up = norm(sub(up, [c * dot(up, n) for c in n]))
        if up == [0.0, 0.0, 0.0]:
            up = [0.0, -1.0, 0.0] if n[2] < 0 else [0.0, 1.0, 0.0]
        right = cross(up, n)

        coords = {v: (dot(verts[v], right), dot(verts[v], up)) for v in pts}
        b_lo, b_hi = min(c[1] for c in coords.values()), max(c[1] for c in coords.values())

        def across(b):
            """The panel's extent across (min, max) at height b, from its triangles' edges."""
            hits = []
            for i in panel:
                t = tris[i]
                for k in range(3):
                    (pa, pb), (qa, qb) = coords[t[k]], coords[t[(k + 1) % 3]]
                    if min(pb, qb) - 1e-6 <= b <= max(pb, qb) + 1e-6:
                        f = 0.5 if abs(qb - pb) < 1e-9 else (b - pb) / (qb - pb)
                        hits += [pa, qa] if abs(qb - pb) < 1e-9 else [pa + f * (qa - pa)]
            return min(hits), max(hits)

        for i in panel:
            uvs = []
            for v in tris[i]:
                a, b = coords[v]
                lo, hi = across(b)
                s = 0.5 if hi - lo < 1e-6 else (a - lo) / (hi - lo)
                t = 0.5 if b_hi - b_lo < 1e-6 else (b_hi - b) / (b_hi - b_lo)
                px = x0 + INSET + s * (x1 + 1 - x0 - 2 * INSET)
                py = y0 + INSET + t * (y1 + 1 - y0 - 2 * INSET)
                uvs.append((round(px / ATLAS_W, 5), round(1 - py / ATLAS_H, 5)))  # OBJ v runs up
            tri_uvs[i] = uvs

    texcoords, index = [], {}
    face_lines = []
    for t, uvs in zip(tris, tri_uvs):
        refs = []
        for v, uv in zip(t, uvs):
            if uv not in index:
                index[uv] = len(texcoords) + 1
                texcoords.append(uv)
            refs.append(f'{v + 1}/{index[uv]}')
        face_lines.append('f ' + ' '.join(refs))
    out = [l for l in lines if not l.startswith(('vt ', 'f '))]
    out += ['vt %g %g' % uv for uv in texcoords] + face_lines
    open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
    print(f'{path}: {len(tris)} triangles in {len(panels(verts, tris, parts))} panels, {len(texcoords)} texture coordinates')


main()
