/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal
 * Copyright (C) 2026 David (davidaparicio2205-dotcom): format 5, the older formats reset to the new recipes */
/* AMB-1 project: one flash object (OBJ_PROJ). A project of another format, or none, gives the
 * defaults. Part of the unity build, after app.h.
 *
 * The pieces (their recipes are the engine's, en_piece): each keeps its own settings as you leave it;
 * Tune and MIDI out are global. */
project_t proj;

const char *piece_name(int i) { return en_piece_name(i); }

static int global_par(int p) { return p == P_TUNE || p == P_MIDI; }

static void recipe(int i, slot_t *s) { en_piece(i, s->par, &s->pool); }

static void take(const slot_t *s)                /* a piece's settings into play, the globals kept */
{
    int p;
    for (p = 0; p < P_NPARAMS; p++)
        if (!global_par(p))
            proj.par[p] = s->par[p];
    proj.pool = s->pool;
}

static void keep(void)                           /* the piece playing, into its slot */
{
    int p;
    for (p = 0; p < P_NPARAMS; p++)
        proj.slot[proj.piece].par[p] = proj.par[p];
    proj.slot[proj.piece].pool = proj.pool;
}

void project_defaults(void)
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    for (i = 0; i < NPIECES; i++)
        recipe(i, &proj.slot[i]);
    for (i = 0; i < P_NPARAMS; i++)
        proj.par[i] = en_param_info(i)->def;
    take(&proj.slot[0]);
    proj.leds = 1;
}

static int pool_ok(uint32_t p)                   /* the root and at least two more notes */
{
    int n = 0, i;
    for (i = 0; i < 12; i++)
        n += (int)(p >> i & 1u);
    return (p & 1u) && !(p >> 12) && n >= 3;
}

static int pars_ok(const int16_t *par)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++) {
        const en_param_t *p = en_param_info(i);
        if (par[i] < p->lo || par[i] > p->hi)
            return 0;
    }
    return 1;
}

static int project_valid(void)                  /* an older format: the recipes again */
{
    int i;
    if (proj.magic != PROJ_MAGIC || !pool_ok(proj.pool) || !pars_ok(proj.par))
        return 0;
    if (proj.format != PROJ_FORMAT || proj.piece >= NPIECES)
        return 0;
    for (i = 0; i < NPIECES; i++)
        if (!pool_ok(proj.slot[i].pool) || !pars_ok(proj.slot[i].par))
            return 0;
    return 1;
}

/* the older formats: 3 (AMB-1 0.9 before the layer pages, 23 settings a piece) and 4 (AMB-1 1.x and this fork
 * before the eras, 28). Their pieces were other pieces: the eras' recipes take their place, and only what belongs to
 * the player is carried over: Tune, MIDI out, the lights and the piece that was playing. (A tweak saved on an old
 * piece would otherwise mask the new recipe of the piece now in its slot.) */
#define V3_NPARAMS 23
#define V4_NPARAMS 28
typedef struct {
    int16_t par[V3_NPARAMS];
    uint16_t pool;
} slot_v3_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[V3_NPARAMS];
    uint16_t pool;
    uint8_t leds, piece, rsv0[2];
    slot_v3_t slot[NPIECES];
    uint8_t rsv[16];
} project_v3_t;
typedef struct {
    int16_t par[V4_NPARAMS];
    uint16_t pool;
} slot_v4_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[V4_NPARAMS];
    uint16_t pool;
    uint8_t leds, piece, rsv0[2];
    slot_v4_t slot[NPIECES];
    uint8_t rsv[16];
} project_v4_t;

static int16_t clampi(int v, int p)
{
    const en_param_t *pi = en_param_info(p);
    return (int16_t)(v < pi->lo ? pi->def : v > pi->hi ? pi->def : v);
}
static void from_old(const int16_t *par, uint8_t leds, uint8_t piece)
{
    project_defaults();
    proj.par[P_TUNE] = clampi(par[P_TUNE], P_TUNE);
    proj.par[P_MIDI] = clampi(par[P_MIDI], P_MIDI);
    proj.leds = leds <= 2u ? leds : 1u;
    proj.piece = piece < NPIECES ? piece : 0u;
    take(&proj.slot[proj.piece]);
}

int project_load(void)
{
    static union {
        project_t now;
        project_v3_t v3;
        project_v4_t v4;
    } buf;
    int n;
    memset(&buf, 0, sizeof buf);
    n = plat_store_load(OBJ_PROJ, &buf, sizeof buf);
    if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 3u) {
        from_old(buf.v3.par, buf.v3.leds, buf.v3.piece);
        return 0;
    }
    if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 4u) {
        from_old(buf.v4.par, buf.v4.leds, buf.v4.piece);
        return 0;
    }
    proj = buf.now;
    if (n >= 8 && project_valid())
        return 0;
    project_defaults();
    return -1;
}

int project_save(void)
{
    keep();
    return plat_store_save(OBJ_PROJ, &proj, sizeof proj);
}

void project_piece(int i)
{
    keep();
    proj.piece = (uint8_t)((i % NPIECES + NPIECES) % NPIECES);
    take(&proj.slot[proj.piece]);
}

void project_piece_reset(void)
{
    slot_t s;
    recipe(proj.piece, &s);
    take(&s);
}

void project_apply(void)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, proj.par[i]);
    en_pool(proj.pool);
}
