"""Add texture coordinates for assets/texture/guy.png to assets/model/guy.obj, in place.

OwlCAD exports the guy without UVs, so this maps each face to its region of the atlas by
which body part it belongs to and which way it faces, projecting it flat onto that region.
Re-run it after re-exporting the model:

    python3 tools/guy_uvs.py assets/model/guy.obj

Atlas regions are in pixels, inclusive, in the 16x16 guy.png.
"""
import sys

ATLAS_W, ATLAS_H = 16, 16
BODY_FRONT, BODY_SIDE, BODY_BACK, BODY_BOTTOM = (0, 0, 6, 7), (7, 0, 8, 7), (9, 0, 15, 7), (0, 8, 6, 10)
ARM_SIDE, LEG_SIDE, SHOE = (10, 8, 12, 15), (13, 8, 15, 15), (8, 12, 10, 15)
HEAD_FRONT, HEAD_REST = (0, 12, 3, 15), (4, 12, 7, 15)
INSET = 0.02  # px: keep nearest sampling off the neighbouring region at the edges


def part_of(pts):
    """Body part of a face, from its corners (OBJ space: Z up, front -Y, units mm)."""
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


def normal(pts):
    """Newell's method: works for any planar polygon, either winding."""
    n = [0.0, 0.0, 0.0]
    for i, a in enumerate(pts):
        b = pts[(i + 1) % len(pts)]
        n[0] += (a[1] - b[1]) * (a[2] + b[2])
        n[1] += (a[2] - b[2]) * (a[0] + b[0])
        n[2] += (a[0] - b[0]) * (a[1] + b[1])
    return n


# Seen from outside, which model axes run right (u) and down (v) across the image, per
# facing direction: (axis, sign) pairs, axis 0 = x, 1 = y, 2 = z.
VIEW = {
    (1, -1): ((0, 1), (2, -1)),   # front (faces -Y)
    (1, 1): ((0, -1), (2, -1)),   # back
    (0, -1): ((1, -1), (2, -1)),  # -X side
    (0, 1): ((1, 1), (2, -1)),    # +X side
    (2, -1): ((0, 1), (1, 1)),    # bottom, front of the model at the top
    (2, 1): ((0, 1), (1, -1)),    # top, front of the model at the bottom
}


def region_and_axes(part, facing):
    u_axis, v_axis = VIEW[facing]
    if part in ('arm', 'leg'):  # long strips: down the image is down the limb
        return (ARM_SIDE if part == 'arm' else LEG_SIDE), u_axis, (2, -1)
    if part == 'shoe':
        return SHOE, u_axis, v_axis
    if part == 'head':
        return (HEAD_FRONT if facing == (1, -1) else HEAD_REST), u_axis, v_axis
    return {(1, -1): BODY_FRONT, (1, 1): BODY_BACK, (0, -1): BODY_SIDE, (0, 1): BODY_SIDE}.get(facing, BODY_BOTTOM), u_axis, v_axis


def main():
    path = sys.argv[1]
    lines = open(path, encoding='utf-8').read().splitlines()
    verts = [tuple(map(float, l.split()[1:4])) for l in lines if l.startswith('v ')]
    faces = [[int(p.split('/')[0]) - 1 for p in l.split()[1:]] for l in lines if l.startswith('f ')]

    # Bounds per part, with left and right limbs separate (they're mirror images).
    def key(part, pts):
        side = (sum(p[0] for p in pts) > 0) if part in ('arm', 'leg', 'shoe') else None
        return part, side
    bounds = {}
    for f in faces:
        pts = [verts[i] for i in f]
        k = key(part_of(pts), pts)
        lo, hi = bounds.get(k, ([1e9] * 3, [-1e9] * 3))
        bounds[k] = ([min(lo[a], *(p[a] for p in pts)) for a in range(3)],
                     [max(hi[a], *(p[a] for p in pts)) for a in range(3)])

    texcoords, face_uvs = [], []
    for f in faces:
        pts = [verts[i] for i in f]
        part = part_of(pts)
        lo, hi = bounds[key(part, pts)]
        n = normal(pts)
        axis = max(range(3), key=lambda a: abs(n[a]))
        (x0, y0, x1, y1), u_axis, v_axis = region_and_axes(part, (axis, 1 if n[axis] > 0 else -1))
        uvs = []
        for p in pts:
            t = []
            for a, s in (u_axis, v_axis):
                f01 = (p[a] - lo[a]) / ((hi[a] - lo[a]) or 1)
                t.append(f01 if s > 0 else 1 - f01)
            px = x0 + INSET + t[0] * (x1 + 1 - x0 - 2 * INSET)
            py = y0 + INSET + t[1] * (y1 + 1 - y0 - 2 * INSET)
            uv = (round(px / ATLAS_W, 5), round(1 - py / ATLAS_H, 5))  # OBJ v runs up
            if uv not in texcoords:
                texcoords.append(uv)
            uvs.append(texcoords.index(uv) + 1)
        face_uvs.append(uvs)

    out = [l for l in lines if not l.startswith(('vt ', 'f '))]
    out += ['vt %g %g' % uv for uv in texcoords]
    out += ['f ' + ' '.join(f'{i + 1}/{t}' for i, t in zip(f, uvs)) for f, uvs in zip(faces, face_uvs)]
    open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
    print(f'{path}: {len(faces)} faces, {len(texcoords)} texture coordinates')


main()
