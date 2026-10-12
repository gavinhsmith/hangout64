/* Place loader. Input may come from anywhere (eventually the network), so every length,
 * count, index and value is checked before use; on any error the place is freed and
 * left empty. Format: see tools/placec.py. */
#include <stdlib.h>
#include <string.h>
#include "world.h"

#define PART_BYTES 30
#define FLOOR_TRIS (FLOOR_TILES * FLOOR_TILES * 2)
#define BOX_TRIS 10

static unsigned rd16(const uint8_t *p)
{
    return p[0] | p[1] << 8;
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

void place_free(place *pl)
{
    for (int i = 0; i < pl->mesh_count; i++) {
        free((void *)pl->meshes[i].verts);
        free((void *)pl->meshes[i].colors);
        free((void *)pl->meshes[i].uvs);
    }
    free(pl->meshes);
    for (int i = 0; i < pl->texture_count; i++)
        free((void *)pl->textures[i].pixels);
    free(pl->textures);
    free(pl->parts);
    memset(pl, 0, sizeof *pl);
}

static int load_texture(texture *t, const uint8_t *d, uint32_t len)
{
    if (len < 4)
        return PLACE_BAD_CHUNK;
    if (d[0] < 3 || d[0] > 5 || d[1] < 3 || d[1] > 5) /* TEXTURE_MIN..TEXTURE_MAX */
        return PLACE_BAD_VALUE;
    int w = 1 << d[0], h = 1 << d[1];
    uint32_t bytes = (uint32_t)(w * h / 2);
    if (len != 4 + 48 + bytes)
        return PLACE_BAD_CHUNK;
    uint8_t *pixels = malloc(bytes);
    t->pixels = pixels; /* set before checking, so place_free cleans up either way */
    if (!pixels)
        return PLACE_NO_MEMORY;
    t->w = w;
    t->h = h;
    memcpy(t->palette, d + 4, 48);
    memcpy(pixels, d + 52, bytes);
    return PLACE_OK;
}

#define MESH_HEADER 8
#define MESH_VERT 10 /* x, y, z, u, v */
#define MESH_TRI 9   /* a, b, c, r, g, b */

/* Indexed triangles in the file, expanded to the 3-corners-per-triangle mesh the
 * renderers draw. */
static int load_mesh(mesh *m, const uint8_t *d, uint32_t len)
{
    if (len < MESH_HEADER)
        return PLACE_BAD_CHUNK;
    uint32_t nv = rd16(d), nt = rd16(d + 2);
    m->texture = (int16_t)rd16(d + 4); /* checked against the texture count in check_place() */
    if (nv == 0 || nt == 0 || len != MESH_HEADER + nv * MESH_VERT + nt * MESH_TRI)
        return PLACE_BAD_CHUNK;
    if (nt > PLACE_MAX_TRIS)
        return PLACE_OVER_BUDGET;

    int16_t (*verts)[3] = malloc(nt * 3 * sizeof *verts);
    uint8_t (*colors)[3] = malloc(nt * sizeof *colors);
    int16_t (*uvs)[2] = m->texture >= 0 ? malloc(nt * 3 * sizeof *uvs) : NULL;
    m->verts = verts; /* set before checking, so place_free cleans up either way */
    m->colors = colors;
    m->uvs = uvs;
    m->tris = (int)nt;
    if (!verts || !colors || (m->texture >= 0 && !uvs))
        return PLACE_NO_MEMORY;

    const uint8_t *vd = d + MESH_HEADER, *td = vd + nv * MESH_VERT;
    for (uint32_t t = 0; t < nt; t++, td += MESH_TRI) {
        for (int c = 0; c < 3; c++) {
            uint32_t i = rd16(td + 2 * c);
            if (i >= nv)
                return PLACE_BAD_INDEX;
            const uint8_t *v = vd + i * MESH_VERT;
            for (int a = 0; a < 3; a++)
                verts[t * 3 + c][a] = (int16_t)rd16(v + 2 * a);
            if (uvs)
                for (int a = 0; a < 2; a++)
                    uvs[t * 3 + c][a] = (int16_t)rd16(v + 6 + 2 * a);
        }
        memcpy(colors[t], td + 6, 3);
    }
    return PLACE_OK;
}

static int in_range(int32_t v, int32_t lo, int32_t hi)
{
    return v >= lo && v <= hi;
}

static int load_parts(place *pl, const uint8_t *d, uint32_t len)
{
    if (len < 2)
        return PLACE_BAD_CHUNK;
    uint32_t n = rd16(d);
    if (len != 2 + n * PART_BYTES)
        return PLACE_BAD_CHUNK;
    if (n > PLACE_MAX_PARTS)
        return PLACE_OVER_BUDGET;
    pl->parts = calloc(n ? n : 1, sizeof *pl->parts);
    if (!pl->parts)
        return PLACE_NO_MEMORY;
    pl->part_count = (int)n;

    for (uint32_t i = 0; i < n; i++) {
        const uint8_t *r = d + 2 + i * PART_BYTES;
        part *p = &pl->parts[i];
        int32_t v[6];
        for (int k = 0; k < 6; k++)
            v[k] = (int32_t)rd32(r + 4 * k);
        for (int k = 0; k < 3; k++)
            if (!in_range(v[k], -PLACE_MAX_COORD, PLACE_MAX_COORD) || !in_range(v[3 + k], 1, PLACE_MAX_COORD))
                return PLACE_BAD_VALUE;
        p->x = v[0], p->y = v[1], p->z = v[2];
        p->sx = v[3], p->sy = v[4], p->sz = v[5];
        memcpy(p->color, r + 24, 3);
        p->mesh = (int16_t)rd16(r + 28);
    }
    return PLACE_OK;
}

/* Cross-chunk checks: texture and mesh references, and the triangle budget for what gets
 * drawn. */
static int check_place(const place *pl)
{
    for (int i = 0; i < pl->mesh_count; i++)
        if (pl->meshes[i].texture < -1 || pl->meshes[i].texture >= pl->texture_count)
            return PLACE_BAD_INDEX;
    long tris = FLOOR_TRIS;
    for (int i = 0; i < pl->part_count; i++) {
        int m = pl->parts[i].mesh;
        if (m < -1 || m >= pl->mesh_count)
            return PLACE_BAD_INDEX;
        tris += m < 0 ? BOX_TRIS : pl->meshes[m].tris;
    }
    return tris > PLACE_MAX_TRIS ? PLACE_OVER_BUDGET : PLACE_OK;
}

int place_load(place *pl, const uint8_t *data, size_t size)
{
    memset(pl, 0, sizeof *pl);
    if (size < 6 || memcmp(data, "HG64", 4) != 0)
        return PLACE_BAD_MAGIC;
    if (rd16(data + 4) != PLACE_VERSION)
        return PLACE_BAD_VERSION;

    int err = PLACE_OK, have_parts = 0;
    size_t off = 6;
    while (off < size && err == PLACE_OK) {
        if (size - off < 8) {
            err = PLACE_TRUNCATED;
            break;
        }
        const uint8_t *tag = data + off;
        uint32_t len = rd32(data + off + 4);
        off += 8;
        if (len > size - off) {
            err = PLACE_TRUNCATED;
            break;
        }
        const uint8_t *d = data + off;
        off += len;

        if (memcmp(tag, "TEXR", 4) == 0) {
            if (pl->texture_count == PLACE_MAX_TEXTURES) {
                err = PLACE_OVER_BUDGET;
                break;
            }
            texture *grown = realloc(pl->textures, (pl->texture_count + 1) * sizeof *grown);
            if (!grown) {
                err = PLACE_NO_MEMORY;
                break;
            }
            pl->textures = grown;
            memset(&pl->textures[pl->texture_count], 0, sizeof *grown);
            err = load_texture(&pl->textures[pl->texture_count++], d, len);
        } else if (memcmp(tag, "MESH", 4) == 0) {
            if (pl->mesh_count == PLACE_MAX_MESHES) {
                err = PLACE_OVER_BUDGET;
                break;
            }
            mesh *grown = realloc(pl->meshes, (pl->mesh_count + 1) * sizeof *grown);
            if (!grown) {
                err = PLACE_NO_MEMORY;
                break;
            }
            pl->meshes = grown;
            memset(&pl->meshes[pl->mesh_count], 0, sizeof *grown);
            err = load_mesh(&pl->meshes[pl->mesh_count++], d, len);
        } else if (memcmp(tag, "PART", 4) == 0) {
            err = have_parts++ ? PLACE_BAD_CHUNK : load_parts(pl, d, len);
        }
        /* Unknown chunks are skipped, so places with newer additions still load. */
    }
    if (err == PLACE_OK && !have_parts)
        err = PLACE_BAD_CHUNK;
    if (err == PLACE_OK)
        err = check_place(pl);
    if (err != PLACE_OK)
        place_free(pl);
    return err;
}
