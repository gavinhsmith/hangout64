/* Host-side checks for core. Run with `make -C core test`. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "world.h"

static place pl;

/* Run `frames` frames with player 0 holding `buttons`, looking along `yaw`. */
static world run(world w, unsigned buttons, int yaw, int frames)
{
    input in[MAX_PLAYERS] = {{buttons, yaw}};
    while (frames--)
        w = world_update(w, in);
    return w;
}

static camera follow(camera c, unsigned buttons, const player *p, int frames)
{
    while (frames--)
        c = camera_update(c, buttons, p);
    return c;
}

/* Little-endian writers and a chunk finder for hand-made and corrupted place files. */
static void wr16(uint8_t *p, unsigned v) { p[0] = v & 0xff; p[1] = v >> 8; }
static void wr32(uint8_t *p, uint32_t v) { wr16(p, v & 0xffff); wr16(p + 2, v >> 16); }

static size_t find_chunk(const uint8_t *d, size_t size, const char *tag)
{
    for (size_t off = 6; off + 8 <= size;) {
        uint32_t len = d[off + 4] | d[off + 5] << 8 | d[off + 6] << 16 | (uint32_t)d[off + 7] << 24;
        if (memcmp(d + off, tag, 4) == 0)
            return off + 8;
        off += 8 + len;
    }
    return 0;
}

static uint8_t *copy_default(size_t extra)
{
    uint8_t *b = malloc(default_place_size + extra);
    memcpy(b, default_place, default_place_size);
    return b;
}

static int load(const uint8_t *d, size_t size)
{
    place p;
    int err = place_load(&p, d, size);
    assert(err != PLACE_OK || p.part_count > 0);
    assert(err == PLACE_OK || (p.parts == NULL && p.meshes == NULL && p.part_count == 0));
    place_free(&p);
    return err;
}

static void test_place_loader(void)
{
    /* The default place: 11 parts; the statue (mesh 0, untextured) and three crates
     * (mesh 1, textured with texture 0, a 32x32 crate). */
    assert(place_load(&pl, default_place, default_place_size) == PLACE_OK);
    assert(pl.part_count == 11 && pl.mesh_count == 2 && pl.texture_count == 1);
    assert(pl.parts[1].y == 300 && pl.parts[1].sy == 600 && pl.parts[1].mesh == -1);
    assert(pl.parts[7].mesh == 0 && pl.parts[7].color[0] == 230 && pl.parts[8].mesh == 1);
    const mesh *statue = &pl.meshes[0], *crate = &pl.meshes[1];
    assert(statue->tris == guy_mesh.tris && statue->texture == NULL && statue->uvs == NULL);
    assert(crate->tris == 12 && crate->texture == &pl.textures[0] && crate->uvs != NULL);
    for (int i = 0; i < crate->tris * 3; i++) /* each face maps the whole texture */
        for (int a = 0; a < 2; a++)
            assert(crate->uvs[i][a] == 0 || crate->uvs[i][a] == MESH_ONE);
    const texture *tex = &pl.textures[0];
    assert(tex->w == 32 && tex->h == 32);
    const uint8_t *corner = tex->palette[texel(tex, 0, 0)], *nail = tex->palette[texel(tex, 2, 2)];
    assert(corner[0] == 70 && corner[1] == 42 && corner[2] == 20);  /* dark outer edge */
    assert(nail[0] == 210 && nail[1] == 200 && nail[2] == 180);     /* a nail */

    /* Every truncation is rejected cleanly. */
    for (size_t n = 0; n < default_place_size; n++)
        assert(load(default_place, n) != PLACE_OK);

    uint8_t *b = copy_default(16);
    b[0] = 'X';
    assert(load(b, default_place_size) == PLACE_BAD_MAGIC);
    memcpy(b, default_place, default_place_size);
    wr16(b + 4, PLACE_VERSION + 1);
    assert(load(b, default_place_size) == PLACE_BAD_VERSION);

    /* Unknown chunks are skipped; a second PART chunk is an error. */
    memcpy(b, default_place, default_place_size);
    memcpy(b + default_place_size, "XTRA", 4);
    wr32(b + default_place_size + 4, 8);
    assert(load(b, default_place_size + 16) == PLACE_OK);
    free(b);
    size_t parts_at = find_chunk(default_place, default_place_size, "PART") - 8;
    size_t parts_len = default_place_size - parts_at;
    b = copy_default(parts_len);
    memcpy(b + default_place_size, default_place + parts_at, parts_len);
    assert(load(b, default_place_size + parts_len) == PLACE_BAD_CHUNK);
    free(b);

    /* Bad references and values. */
    b = copy_default(0);
    size_t m = find_chunk(b, default_place_size, "MESH");
    unsigned nv = b[m] | b[m + 1] << 8;
    wr16(b + m + 8 + nv * 10, nv); /* first triangle's first corner */
    assert(load(b, default_place_size) == PLACE_BAD_INDEX);
    memcpy(b, default_place, default_place_size);
    wr16(b + m + 4, 1); /* statue mesh -> texture 1, which doesn't exist */
    assert(load(b, default_place_size) == PLACE_BAD_INDEX);
    memcpy(b, default_place, default_place_size);
    b[find_chunk(b, default_place_size, "TEXR")] = 6; /* 64 px wide: over TEXTURE_MAX */
    assert(load(b, default_place_size) == PLACE_BAD_VALUE);
    memcpy(b, default_place, default_place_size);
    size_t p = find_chunk(b, default_place_size, "PART");
    wr16(b + p + 2 + 7 * 30 + 28, 2); /* statue -> mesh 2, which doesn't exist */
    assert(load(b, default_place_size) == PLACE_BAD_INDEX);
    memcpy(b, default_place, default_place_size);
    wr32(b + p + 2 + 12, 0); /* part 0 with zero width */
    assert(load(b, default_place_size) == PLACE_BAD_VALUE);
    free(b);

    /* Over the triangle budget: 200 boxes is 2000 triangles plus the floor. */
    size_t size = 6 + 8 + 2 + 200 * 30;
    b = calloc(1, size);
    memcpy(b, "HG64", 4);
    wr16(b + 4, PLACE_VERSION);
    memcpy(b + 6, "PART", 4);
    wr32(b + 10, 2 + 200 * 30);
    wr16(b + 14, 200);
    for (int i = 0; i < 200; i++)
        for (int k = 3; k < 6; k++)
            wr32(b + 16 + i * 30 + 4 * k, 100), wr16(b + 16 + i * 30 + 28, 0xffff);
    assert(load(b, size) == PLACE_OVER_BUDGET);
    wr16(b + 14, 100), wr32(b + 10, 2 + 100 * 30); /* 100 boxes fit */
    assert(load(b, 6 + 8 + 2 + 100 * 30) == PLACE_OK);
    free(b);
}

int main(void)
{
    test_place_loader();
    assert(isin(0) == 0 && isin(90) == 4096 && isin(270) == -4096 && icos(0) == 4096);
    assert(abs(isin(30) - 2048) < 20 && abs(isin(-30) + 2048) < 20);

    world w = world_init(&pl, 1);
    const player *p0 = &w.players[0];
    assert(w.player_count == 1 && p0->x == 0 && p0->z == 0);

    /* Movement is relative to the viewer's camera yaw: at 0, forward is -Z, right is +X. */
    world f = run(w, MOVE_FWD, 0, 10);
    assert(f.players[0].z == -600 && f.players[0].x == 0);
    world r = run(w, MOVE_RIGHT, 0, 10);
    assert(r.players[0].x == 600 && r.players[0].z == 0);
    world side = run(w, MOVE_FWD, 900, 10); /* camera orbited to the +X side */
    assert(side.players[0].x == -600 && abs(side.players[0].z) <= 10);

    /* Diagonals move at the same speed (within rounding), not sqrt(2) faster. */
    world d = run(w, MOVE_FWD | MOVE_RIGHT, 0, 10);
    int dist2 = d.players[0].x * d.players[0].x + d.players[0].z * d.players[0].z;
    assert(d.players[0].x > 0 && d.players[0].z < 0 && dist2 > 580 * 580 && dist2 <= 600 * 600);

    /* Jump: up about 1.2 units, back down, no jumping again mid-air. */
    world j = run(w, JUMP, 0, 1);
    assert(j.players[0].y > 0 && !j.players[0].on_ground);
    int peak = 0;
    for (int i = 0; i < 100; i++) {
        j = run(j, 0, 0, 1);
        peak = j.players[0].y > peak ? j.players[0].y : peak;
    }
    assert(peak > 1100 && peak < 1300 && j.players[0].y == 0 && j.players[0].on_ground);
    world air = run(w, JUMP, 0, 10);
    assert(run(air, JUMP, 0, 1).players[0].vy < air.players[0].vy);

    /* Collision: the wall (x -4.5..-3.5) stops you flush against it; step 1 (z -5..-3,
     * 0.6 high) is too tall to walk up, so you stop flush; jumping gets you on top; you
     * land on tops. */
    world wall = run(w, MOVE_LEFT, 0, 200);
    assert(wall.players[0].x == -3500 + PLAYER_RADIUS);
    world step = run(w, MOVE_FWD, 0, 200);
    assert(step.players[0].z == -3000 + PLAYER_RADIUS && step.players[0].y == 0);

    /* The curb (z 3.5..4.5, 0.3 high) is low enough to walk up and off again. */
    world curb = run(w, MOVE_BACK, 0, 65); /* z = 3.9: on top of it */
    assert(curb.players[0].y == 300 && curb.players[0].on_ground);
    curb = run(curb, MOVE_BACK, 0, 40);   /* z = 6.3: walked off the far side */
    assert(curb.players[0].y == 0 && curb.players[0].z == 6300);
    world high = w; /* only walks up when grounded */
    high.players[0].z = 3100; /* front edge 3.45: one step reaches into the curb */
    high.players[0].y = 150;
    high.players[0].on_ground = 0;
    high.players[0].vy = 50;
    high = run(high, MOVE_BACK, 0, 1);
    assert(high.players[0].z == 3500 - PLAYER_RADIUS);
    world up = run(step, MOVE_FWD | JUMP, 0, 25);
    up = run(up, 0, 0, 60);
    assert(up.players[0].on_ground && up.players[0].y == 600);
    world drop = w;
    drop.players[0].z = -4000;
    drop.players[0].y = 3000;
    drop = run(drop, 0, 0, 100);
    assert(drop.players[0].y == 600 && drop.players[0].on_ground);
    for (int i = 0; i < 4; i++) { /* standing on a part: grounded every frame, not every other */
        drop = run(drop, 0, 0, 1);
        assert(drop.players[0].on_ground && drop.players[0].y == 600);
    }

    /* Players move independently from their own inputs. */
    world two = world_init(&pl, 2);
    input in[MAX_PLAYERS] = {{MOVE_RIGHT, 0}, {MOVE_FWD, 0}};
    for (int i = 0; i < 10; i++)
        two = world_update(two, in);
    assert(two.players[0].x == 600 && two.players[0].z == 0);
    assert(two.players[1].x == 1500 && two.players[1].z == -600);

    /* Players turn (the short way) to face where they move, and keep facing it. */
    assert(run(w, MOVE_RIGHT, 0, 100).players[0].heading == 2700);
    assert(run(w, MOVE_BACK, 0, 100).players[0].heading == 1800);
    assert(run(w, MOVE_LEFT | MOVE_FWD, 0, 100).players[0].heading == 450);
    assert(run(r, 0, 0, 100).players[0].heading == 2700);
    world turn = w;
    turn.players[0].heading = 100;
    turn.players[0].target_heading = 3500;
    assert(run(turn, 0, 0, 1).players[0].heading < 100);
    assert(run(turn, 0, 0, 100).players[0].heading == 3500);

    /* The camera follows with a lag, then settles exactly; orbit/zoom ease and clamp. */
    camera c = follow(camera_init(), 0, &f.players[0], 1);
    assert(c.now.z > f.players[0].z && c.now.z < 0);
    c = follow(c, 0, &f.players[0], 200);
    assert(c.now.z == f.players[0].z && c.now.x == f.players[0].x);
    camera o = follow(camera_init(), CAM_RIGHT, p0, 1);
    assert(o.target.yaw == 20 && o.now.yaw > 0 && o.now.yaw < 20);
    o = follow(o, CAM_UP | CAM_IN, p0, 300);
    assert(o.now.pitch == CAM_PITCH_MAX && o.now.dist == CAM_DIST_MIN);

    /* Standing on a part, the camera settles at its height; a jump from there only
     * raises the camera by half the jump. */
    camera onstep = follow(camera_init(), 0, &drop.players[0], 300);
    assert(onstep.now.y == 600);
    world hop = run(drop, JUMP, 0, 10);
    onstep = camera_update(onstep, 0, &hop.players[0]);
    assert(onstep.target.y == 600 + (hop.players[0].y - 600) / 2);

    /* The avatar is textured with its 16x16 atlas, and every texture coordinate is on it. */
    assert(guy_mesh.texture == &guy_texture && guy_texture.w == 16 && guy_texture.h == 16);
    for (int i = 0; i < guy_mesh.tris * 3; i++)
        for (int a = 0; a < 2; a++)
            assert(guy_mesh.uvs[i][a] >= 0 && guy_mesh.uvs[i][a] <= MESH_ONE);

    /* Model: feet on y = 0, about 2.4 units tall. */
    int lo = 0, hi = 0;
    for (int i = 0; i < guy_mesh.tris * 3; i++) {
        lo = guy_mesh.verts[i][1] < lo ? guy_mesh.verts[i][1] : lo;
        hi = guy_mesh.verts[i][1] > hi ? guy_mesh.verts[i][1] : hi;
    }
    assert(lo == 0 && abs(hi - 24 * MESH_ONE / 10) <= 1);

    /* Level mesh: checkered floor at FLOOR_Y, then 10 triangles per box part (not the
     * 4 mesh parts), its top face first, drawn at the part's top. */
    int floor_tris = FLOOR_TILES * FLOOR_TILES * 2;
    assert(level_mesh.tris == floor_tris + 10 * (pl.part_count - 4));
    for (int i = 0; i < floor_tris * 3; i++)
        assert(level_mesh.verts[i][1] == FLOOR_Y * MESH_ONE);
    assert(level_mesh.colors[0][0] != level_mesh.colors[2][0]);
    const part *s1 = &pl.parts[1];
    assert(level_mesh.verts[(floor_tris + 10) * 3][1] == (FLOOR_Y * 1000 + s1->y + s1->sy / 2) * MESH_ONE / 1000);

    /* Mesh parts collide as their box: the statue (x -7.8..-6.2, z -7.4..-6.6). */
    world near = w;
    near.players[0].x = -7000;
    near.players[0].z = -5000;
    near = run(near, MOVE_FWD, 0, 100);
    assert(near.players[0].z == -6600 + PLAYER_RADIUS);

    place_free(&pl);
    puts("core: ok");
    return EXIT_SUCCESS;
}
