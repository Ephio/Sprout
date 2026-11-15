/* SPDX-License-Identifier: MPL-2.0
 * Pocket Scion scenes: the garden band (240 x 144) of the eight pieces, drawn in place of the scale ring. Each one
 * answers the Scion's notes (en_sc_count / en_sc_glow / en_sc_note: a new note on a channel is an event):
 *
 *   0 Garden      a flowering Japanese cherry in a garden: the blossom of a branch flares and a petal falls from the
 *                 tree and a ripple spreads on the pond for every note
 *   1 Mycelium    five fungal networks, one for each MIDI channel: a note makes its network grow and flash
 *   2 Flora       a Monstera that grows with the notes: a leaf unfurls, the leaf of the channel stirs, dust rises
 *   3 Earth       the globe turning (faster with the tempo); a note is a drop of rain, a ripple on the ground
 *   4 Posidonia   a seagrass meadow under the sea, swaying with the waves; broken sunbeams from above. A note stirs
 *                 the grass, lights a beam and lets go a bubble or a fish
 *   5 Night       a river bank by moonlight, reeds in the wind. A note is a gust and fireflies rise; ripples
 *   6 Lighthouse  a storm on a northern coast, the lighthouse's beam sweeping. A note is a wave breaking, a flash
 *                 of lightning now and then
 *   7 Shoreline   a sunset from the sea wall: two lovers with their backs to us, an ice cream van at the side. A note
 *                 glints on the sea and lights a bulb of the van, a gull crosses, a heart rises now and then
 *
 * Part of the unity build: included by ui.c, after its drawing helpers and before draw_main. */
#include "land.h"

#define SCN_H MAIN_H

/* the five voices' dots at the bottom: lit as their notes arrive (the same on every scene) */
static const uint16_t SC_DOT[EN_NSCION] = {RGB(238, 110, 100), RGB(238, 170, 80), RGB(150, 214, 110), RGB(96, 190, 230),
                                           RGB(186, 130, 236)};

static float sc_hf(uint32_t i)                  /* a stable pseudo-random number 0..1 from an integer */
{
    i ^= i >> 16;
    i *= 0x7FEB352Du;
    i ^= i >> 15;
    i *= 0x846CA68Bu;
    i ^= i >> 16;
    return (float)(i & 0xFFFFu) * (1.0f / 65536.0f);
}
static uint32_t sc_rs = 0x2545F491u;
static float sc_rand(void)                      /* a running one */
{
    sc_rs ^= sc_rs << 13;
    sc_rs ^= sc_rs >> 17;
    sc_rs ^= sc_rs << 5;
    return (float)(sc_rs & 0xFFFFu) * (1.0f / 65536.0f);
}

static struct {
    uint8_t seen[EN_NSCION];                    /* en_sc_count as last drawn */
    uint8_t fresh[EN_NSCION];                   /* notes since then, at most 3 */
    uint8_t note[EN_NSCION];
    float glow[EN_NSCION];
    float grow[EN_NSCION];                      /* Mycelium: how far each network has grown, 0.3 .. 1 */
    float gmo;                                  /* Flora: how far the Monstera has grown, 0.2 .. 1 */
    float flash, boltx;                         /* Lighthouse: the lightning's light, and where it struck */
    float dt, now, amb;                         /* the frame's seconds; a clock; the ambient spawn's timer */
    uint32_t last_ms, cnt;                      /* (cnt: the notes of the scene, for what happens every so often) */
} sc;

static void sc_begin(void)                      /* the frame: its time, and what the Scion played since the last */
{
    uint32_t ms = plat_ms();
    float dt = (float)(ms - sc.last_ms) * 0.001f;
    int i;
    sc.last_ms = ms;
    sc.dt = dt > 0.25f ? 0.25f : dt;
    sc.now += sc.dt;
    if (sc.now > 4096.0f)
        sc.now -= 4096.0f;
    for (i = 0; i < EN_NSCION; i++) {
        uint8_t c = en_sc_count[i], n = (uint8_t)(c - sc.seen[i]);
        sc.seen[i] = c;
        sc.fresh[i] = n > 3u ? 3u : n;
        sc.cnt += sc.fresh[i];
        sc.glow[i] = en_sc_glow[i];
        sc.note[i] = en_sc_note[i];
    }
}

static float sc_gsum(void)                      /* how much is playing, 0..1 */
{
    float s = sc.glow[0] + sc.glow[1] + sc.glow[2] + sc.glow[3] + sc.glow[4];
    return s > 1.0f ? 1.0f : s;
}

static void sc_dots(void)
{
    int i;
    for (i = 0; i < EN_NSCION; i++) {
        int32_t x = 120 + (i - 2) * 12, y = SCN_H - 9;
        cv_disc(x, y, 4, RGB(24, 24, 28));
        cv_disc(x, y, 3, mix(RGB(86, 86, 92), SC_DOT[i], q16(sc.glow[i])));
    }
}

static int32_t ftoi(float v) { return (int32_t)(v < 0.0f ? v - 0.5f : v + 0.5f); }

/* a line, one pixel wide, blended a/16 (px_blend clips) */
static void ln(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c, uint32_t a)
{
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int32_t sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx - dy, e2, n = 0;
    for (;;) {
        px_blend(x0, y0, c, a);
        if ((x0 == x1 && y0 == y1) || ++n > 400)
            break;
        e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}
static void ln_w(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t w, uint16_t c)   /* w pixels thick, opaque */
{
    int32_t k, ax = x1 > x0 ? x1 - x0 : x0 - x1, ay = y1 > y0 ? y1 - y0 : y0 - y1;
    for (k = 0; k < w; k++)
        if (ax > ay)
            ln(x0, y0 + k, x1, y1 + k, c, 16);
        else
            ln(x0 + k, y0, x1 + k, y1, c, 16);
}

/* a filled ellipse, by rows */
static void ell(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint16_t c)
{
    int32_t j;
    for (j = -ry; j <= ry; j++) {
        int32_t hw = ftoi((float)rx * fm_sqrtf(1.0f - (float)(j * j) / (float)(ry * ry + 1)));
        cv_rect(cx - hw, cy + j, 2 * hw + 1, 1, c);
    }
}

/* a filled polygon (even-odd, by scanlines): n <= 24 points */
static void poly_fill(const float *px, const float *py, int n, uint16_t c)
{
    float ymin = py[0], ymax = py[0];
    int i, y;
    for (i = 1; i < n; i++) {
        if (py[i] < ymin)
            ymin = py[i];
        if (py[i] > ymax)
            ymax = py[i];
    }
    for (y = (int)ymin; y <= (int)ymax; y++) {
        float xs[12], yy = (float)y + 0.5f;
        int m = 0, a, b;
        for (i = 0; i < n && m < 12; i++) {
            int j = (i + 1) % n;
            float y0 = py[i], y1 = py[j];
            if ((y0 <= yy && y1 > yy) || (y1 <= yy && y0 > yy))
                xs[m++] = px[i] + (yy - y0) * (px[j] - px[i]) / (y1 - y0);
        }
        for (a = 1; a < m; a++) {               /* sorted, by insertion */
            float v = xs[a];
            for (b = a - 1; b >= 0 && xs[b] > v; b--)
                xs[b + 1] = xs[b];
            xs[b + 1] = v;
        }
        for (a = 0; a + 1 < m; a += 2)
            cv_rect(ftoi(xs[a]), y, ftoi(xs[a + 1]) - ftoi(xs[a]) + 1, 1, c);
    }
}

/* falling and floating things (petals, bubbles, fish, fireflies, spray, gulls, hearts): a few at a time, shared by
 * the scenes (one is on at a time). life in seconds; they end at the ground (y), off the sides or off the top */
#define SC_NP 40
static struct {
    float x, y, vx, vy, ph, life;
    uint8_t kind, tone;
} scp[SC_NP];
static void scp_add(float x, float y, float vx, float vy, float life, uint8_t kind, uint8_t tone)
{
    int i;
    for (i = 0; i < SC_NP; i++)
        if (scp[i].life <= 0.0f) {
            scp[i].x = x;
            scp[i].y = y;
            scp[i].vx = vx;
            scp[i].vy = vy;
            scp[i].ph = sc_rand() * FM_TWO_PI;
            scp[i].life = life;
            scp[i].kind = kind;
            scp[i].tone = tone;
            return;
        }
}
static void scp_step(float ground, float sway, float grav)   /* move them; sway: the drift to and fro, grav: px/s^2 */
{
    int i;
    for (i = 0; i < SC_NP; i++) {
        if (scp[i].life <= 0.0f)
            continue;
        scp[i].ph += sc.dt * 2.4f;
        scp[i].vy += grav * sc.dt;
        scp[i].x += (scp[i].vx + sway * fm_sinf(scp[i].ph)) * sc.dt;
        scp[i].y += scp[i].vy * sc.dt;
        scp[i].life -= sc.dt;
        if (scp[i].y > ground || scp[i].y < -8.0f || scp[i].x < -12.0f || scp[i].x > 252.0f)
            scp[i].life = 0.0f;
    }
}

#define SC_NR 8
static struct {
    float x, y, age;
    uint8_t on;
} scr[SC_NR];
static void scr_add(float x, float y)
{
    int i, k = 0;
    for (i = 0; i < SC_NR; i++) {
        if (!scr[i].on) {
            k = i;
            break;
        }
        if (scr[i].age > scr[k].age)
            k = i;
    }
    scr[k].x = x;
    scr[k].y = y;
    scr[k].age = 0.0f;
    scr[k].on = 1;
}
/* rings spreading and fading: flat squashes them (a pond seen at a slant), maxr is how far they go, speed in px/s */
static void scr_draw(float flat, float maxr, float speed, uint16_t c)
{
    int i, k;
    for (i = 0; i < SC_NR; i++) {
        float r;
        uint32_t a;
        if (!scr[i].on)
            continue;
        scr[i].age += sc.dt;
        r = scr[i].age * speed;
        if (r >= maxr) {
            scr[i].on = 0;
            continue;
        }
        a = (uint32_t)(14.0f * (1.0f - r / maxr)) + 1u;
        for (k = 0; k < 28; k++) {
            float t = (float)k * (FM_TWO_PI / 28.0f);
            px_blend(ftoi(scr[i].x + fm_cosf(t) * r), ftoi(scr[i].y + fm_sinf(t) * r * flat), c, a);
        }
    }
}

/* ------------------------------------------------------------- GARDEN --- */
#define GD_MAX 5
static const uint16_t GD_PINK[3] = {RGB(250, 168, 188), RGB(255, 204, 216), RGB(238, 134, 168)};

/* a cloud of blossom at the end of a branch; flares (towards white) with the note of the channel its branch is tied to */
static void gd_blossom(float x, float y, uint32_t id, int small)
{
    int k, f = q16(sc.glow[id % 5u]) / 2, s = (int)(id % 3u);
    if (!small)
        cv_disc(ftoi(x), ftoi(y), 5, mix(RGB(242, 182, 202), RGB(255, 236, 242), f));
    for (k = 0; k < (small ? 3 : 5); k++) {
        float bx = x + (sc_hf(id * 11u + (uint32_t)k * 3u) - 0.5f) * 11.0f;
        float by = y + (sc_hf(id * 13u + (uint32_t)k * 5u) - 0.5f) * 9.0f;
        cv_disc(ftoi(bx), ftoi(by), 2 + (k & 1), mix(GD_PINK[(s + k) % 3], RGB(255, 255, 255), f));
    }
}

/* a limb forks into two or three shorter, thinner ones, GD_MAX times; the blossom is on the last, and a little on the
 * one before */
static void gd_branch(float x, float y, float ang, float len, int d, uint32_t id)
{
    float a = ang + (sc_hf(id) - 0.5f) * 0.8f + 0.03f * (float)d * fm_sinf(sc.now * 0.7f + (float)(id & 7u));
    float x1 = x + fm_cosf(a) * len, y1 = y + fm_sinf(a) * len;
    ln_w(ftoi(x), ftoi(y), ftoi(x1), ftoi(y1), d < 3 ? 3 : d < 5 ? 2 : 1, d < 4 ? RGB(78, 52, 46) : RGB(100, 70, 60));
    if (d < GD_MAX) {
        float s = 0.4f + 0.4f * sc_hf(id * 7u + 3u);
        gd_branch(x1, y1, a - s, len * 0.76f, d + 1, id * 3u + 1u);
        gd_branch(x1, y1, a + s, len * 0.76f, d + 1, id * 3u + 2u);
        if (sc_hf(id * 5u + 1u) > 0.45f)
            gd_branch(x1, y1, a + 0.05f, len * 0.7f, d + 1, id * 3u + 3u);
        if (d == GD_MAX - 1)
            gd_blossom((x + x1) * 0.5f, (y + y1) * 0.5f, id + 99u, 1);
    } else {
        gd_blossom(x1, y1, id, 0);
    }
}

static void scene_garden(void)
{
    static const uint16_t PETAL[3] = {RGB(255, 208, 220), RGB(255, 238, 242), RGB(246, 170, 192)};
    const int32_t PCX = 158, PCY = 122, PRX = 56, PRY = 12;
    int32_t i, j, x;
    int c;
    for (i = 0; i < 12; i++)                                            /* the sky, pink to cream */
        cv_rect(0, i * 8, 240, 8, mix(RGB(246, 214, 224), RGB(255, 244, 226), i * 16 / 11));
    cv_disc(196, 40, 6, RGB(255, 246, 214));                            /* a pale sun */
    for (x = 0; x < 240; x += 2) {                                      /* the far hills, then the near */
        int32_t h = 14 + ftoi(8.0f * fm_sinf((float)x * 0.021f + 1.0f) + 5.0f * fm_sinf((float)x * 0.053f + 0.4f));
        int32_t g = 10 + ftoi(6.0f * fm_sinf((float)x * 0.033f + 2.2f) + 4.0f * fm_sinf((float)x * 0.071f));
        cv_rect(x, 82 - h, 2, h + 20, RGB(212, 196, 214));
        cv_rect(x, 96 - g, 2, g + 10, RGB(168, 178, 150));
    }
    cv_rect(0, 100, 240, SCN_H - 100, RGB(108, 142, 88));               /* the moss */
    cv_rect(0, 128, 240, SCN_H - 128, RGB(94, 128, 78));
    for (j = -PRY; j <= PRY; j++) {                                     /* the pond */
        int32_t hw = ftoi((float)PRX * fm_sqrtf(1.0f - (float)(j * j) / (float)(PRY * PRY)));
        cv_rect(PCX - hw, PCY + j, 2 * hw, 1, mix(RGB(156, 188, 198), RGB(214, 226, 226), (j + PRY) * 5 / PRY));
    }
    cv_disc(PCX - 62, PCY + 9, 3, RGB(150, 146, 140));                  /* stepping stones */
    cv_disc(PCX - 44, PCY + 15, 3, RGB(158, 154, 148));
    cv_disc(PCX + 4, PCY + 17, 3, RGB(150, 146, 140));
    for (i = 0; i < 9; i++)                                             /* petals on the pond, drifting */
        disc((float)PCX + fm_sinf(sc.now * 0.12f + (float)i * 1.7f) * 44.0f, (float)PCY + (sc_hf((uint32_t)i + 40u) - 0.5f) * 16.0f, 1.3f, RGB(255, 220, 230));
    for (i = 0; i < 46; i++)                                            /* and on the ground, under the tree */
        px_blend(14 + (int32_t)(sc_hf((uint32_t)i * 3u) * 100.0f), 104 + (int32_t)(sc_hf((uint32_t)i * 3u + 1u) * 38.0f), RGB(250, 190, 206), 12u);
    cv_rect(199, 107, 12, 3, RGB(140, 136, 130));                       /* a stone lantern */
    cv_rect(203, 98, 4, 9, RGB(156, 152, 146));
    cv_rect(200, 90, 10, 8, RGB(140, 136, 130));
    cv_rect(202, 92, 6, 4, RGB(255, 214, 120));
    cv_rect(197, 86, 16, 4, RGB(112, 108, 104));
    cv_rect(203, 83, 4, 3, RGB(126, 122, 118));
    ln_w(38, SCN_H, 44, 122, 7, RGB(74, 50, 44));                       /* the tree: a trunk with a lean ... */
    ln_w(44, 122, 54, 102, 6, RGB(78, 52, 46));
    ln_w(54, 102, 60, 84, 5, RGB(78, 52, 46));
    gd_branch(60.0f, 84.0f, -2.5f, 30.0f, 2, 1u);                       /* ... and its limbs, forking */
    gd_branch(60.0f, 84.0f, -1.9f, 34.0f, 2, 2u);
    gd_branch(60.0f, 84.0f, -1.45f, 32.0f, 2, 3u);
    gd_branch(60.0f, 84.0f, -1.0f, 32.0f, 2, 4u);
    gd_branch(60.0f, 84.0f, -0.45f, 30.0f, 2, 5u);
    gd_branch(50.0f, 112.0f, -2.9f, 26.0f, 2, 6u);
    gd_branch(50.0f, 112.0f, -0.2f, 36.0f, 2, 7u);
    for (c = 0; c < EN_NSCION; c++)                                     /* a note: petals, and a ripple on the pond */
        if (sc.fresh[c]) {
            int n;
            for (n = 0; n < 3 * sc.fresh[c]; n++)
                scp_add(10.0f + 130.0f * sc_rand(), 26.0f + 60.0f * sc_rand(), 3.0f + 4.0f * sc_rand(), 13.0f + 10.0f * sc_rand(), 12.0f, 0, (uint8_t)(sc_rand() * 2.99f));
            scr_add((float)(PCX - 44 + (sc.note[c] * 7 % 12) * 8), (float)(PCY + (c - 2) * 3));
        }
    sc.amb -= sc.dt;
    if (sc.amb <= 0.0f) {                                               /* and now and then one by itself */
        sc.amb = 0.6f + sc_rand();
        scp_add(10.0f + 130.0f * sc_rand(), 26.0f + 60.0f * sc_rand(), 3.0f + 4.0f * sc_rand(), 12.0f + 8.0f * sc_rand(), 12.0f, 0, (uint8_t)(sc_rand() * 2.99f));
    }
    scr_draw(0.32f, 26.0f, 14.0f, RGB(255, 255, 255));
    scp_step(138.0f, 9.0f, 0.0f);
    for (i = 0; i < SC_NP; i++)
        if (scp[i].life > 0.0f)
            disc(scp[i].x, scp[i].y, 1.6f, PETAL[scp[i].tone]);
}

/* ----------------------------------------------------------- MYCELIUM --- */
#define MYC_DEPTH 4
static const int16_t MYC_O[EN_NSCION][2] = {{34, 96}, {86, 38}, {128, 108}, {176, 46}, {208, 100}};
static const uint16_t MYC_COL[EN_NSCION] = {RGB(236, 226, 196), RGB(150, 226, 200), RGB(240, 172, 192), RGB(238, 198, 122),
                                            RGB(196, 172, 238)};

/* a hypha: it forks into two (now and then three) shorter ones; g says how far the network has grown, from the
 * root out, so a young one is a few strands and an old one a mesh; bright (0..8) is its flash */
static void myc_branch(float x, float y, float ang, float len, int depth, uint32_t id, float g, uint16_t col, int bright)
{
    float vis = fm_clampf(g * (float)(MYC_DEPTH + 1) - (float)depth, 0.0f, 1.0f);
    float a = ang + (sc_hf(id) - 0.5f) * 0.9f + 0.05f * (float)depth * fm_sinf(sc.now * 0.5f + (float)(id & 15u));
    float x1, y1;
    uint16_t c = mix(col, RGB(255, 255, 255), bright > 0 && depth >= 1 ? bright : 0);
    if (vis <= 0.0f)
        return;
    x1 = x + fm_cosf(a) * len * vis;
    y1 = y + fm_sinf(a) * len * vis;
    ln(ftoi(x), ftoi(y), ftoi(x1), ftoi(y1), c, (uint32_t)(16 - 2 * depth));
    if (vis >= 1.0f && depth < MYC_DEPTH) {
        float s = 0.35f + 0.45f * sc_hf(id * 7u + 3u);
        myc_branch(x1, y1, a - s, len * 0.74f, depth + 1, id * 3u + 1u, g, col, bright);
        myc_branch(x1, y1, a + s, len * 0.74f, depth + 1, id * 3u + 2u, g, col, bright);
        if (sc_hf(id * 5u + 1u) > 0.72f)
            myc_branch(x1, y1, a + 0.1f, len * 0.6f, depth + 1, id * 3u + 3u, g, col, bright);
    } else {
        cv_rect(ftoi(x1), ftoi(y1), 2, 2, c);                           /* a tip */
    }
}

static void scene_mycelium(void)
{
    int c, i;
    cv_rect(0, 0, 240, SCN_H, RGB(10, 14, 20));
    for (i = 0; i < 36; i++) {                                          /* spores drifting up */
        float pos = sc_hf((uint32_t)i * 7u) * (float)SCN_H - sc.now * (2.0f + 4.0f * sc_hf((uint32_t)i));
        float y = pos - fm_floorf(pos / (float)SCN_H) * (float)SCN_H;
        uint32_t a = (uint32_t)(3.0f + 5.0f * fm_fabsf(fm_sinf(sc.now * 0.9f + (float)i)));
        px_blend(ftoi(sc_hf((uint32_t)i * 5u) * 240.0f), ftoi(y), RGB(190, 210, 200), a);
    }
    for (c = 0; c < EN_NSCION; c++) {                                   /* a note feeds its network; it slowly lets go */
        sc.grow[c] += 0.2f * (float)sc.fresh[c] - 0.02f * sc.dt;
        sc.grow[c] = fm_clampf(sc.grow[c], 0.3f, 1.0f);
    }
    for (c = 0; c < EN_NSCION; c++) {
        float ox = (float)MYC_O[c][0], oy = (float)MYC_O[c][1], a0 = sc_hf((uint32_t)c * 11u) * FM_TWO_PI;
        int k, br = q16(sc.glow[c]) / 2;
        for (k = 0; k < 3; k++)
            myc_branch(ox, oy, a0 + (float)k * 2.09f, 22.0f, 0, (uint32_t)(c * 3 + k + 1) * 977u, sc.grow[c], MYC_COL[c], br);
        cv_disc(ftoi(ox), ftoi(oy), 3, mix(MYC_COL[c], RGB(255, 255, 255), br));
    }
}

/* -------------------------------------------------------------- FLORA --- */
/* A Monstera in a pot by a window. Every note makes it grow: more leaves unfurl, a taller stem; the leaf of the
 * note's channel (a leaf belongs to channel i % 5) stirs and brightens, and dust rises in the light. With no notes it
 * slowly shrinks back to two leaves. */
#define MO_N 9
static const float MO_P[9][2] = {{0.0f, 0.0f},   {-0.10f, 0.12f}, {-0.06f, 0.30f}, {0.08f, 0.46f}, {0.30f, 0.54f},
                                 {0.55f, 0.50f}, {0.78f, 0.36f},  {0.95f, 0.14f},  {1.0f, 0.0f}};   /* half a leaf: (along, across) */

static float monstera_half_width(float u)                      /* the leaf's half-width at u, 0.08 .. 1 */
{
    int i;
    for (i = 4; i < 9; i++)
        if (u <= MO_P[i][0] || i == 8) {
            float u0 = MO_P[i - 1][0], u1 = MO_P[i][0], t = u1 > u0 ? (u - u0) / (u1 - u0) : 0.0f;
            t = fm_clampf(t, 0.0f, 1.0f);
            return MO_P[i - 1][1] + (MO_P[i][1] - MO_P[i - 1][1]) * t;
        }
    return 0.0f;
}

/* the blade as a polygon, from its base (bx, by) along angle a, len long; k < 1 shrinks it about its middle */
static void mo_poly(float bx, float by, float a, float len, float k, uint16_t c)
{
    float px[16], py[16], ca = fm_cosf(a), sa = fm_sinf(a);
    int i, n = 0;
    for (i = 0; i < 9; i++) {
        float u = 0.5f + (MO_P[i][0] - 0.5f) * k, v = MO_P[i][1] * k;
        px[n] = bx + (u * ca - v * sa) * len;
        py[n++] = by + (u * sa + v * ca) * len;
    }
    for (i = 7; i >= 1; i--) {
        float u = 0.5f + (MO_P[i][0] - 0.5f) * k, v = -MO_P[i][1] * k;
        px[n] = bx + (u * ca - v * sa) * len;
        py[n++] = by + (u * sa + v * ca) * len;
    }
    poly_fill(px, py, n, c);
}

static void mo_leaf(float bx, float by, float a, float len, uint16_t body, uint16_t edge, uint16_t rib, uint16_t wall)
{
    float ca = fm_cosf(a), sa = fm_sinf(a);
    int s, k;
    if (len < 3.0f)
        return;
    mo_poly(bx, by, a, len, 1.0f, edge);
    mo_poly(bx, by, a, len, 0.86f, body);
    ln(ftoi(bx), ftoi(by), ftoi(bx + ca * len * 0.96f), ftoi(by + sa * len * 0.96f), rib, 14);
    for (s = -1; s <= 1; s += 2)                /* the cuts of a Monstera: from the edge towards the rib */
        for (k = 0; k < 3; k++) {
            float u = 0.34f + 0.22f * (float)k, w = monstera_half_width(u) * len, u2 = (u + 0.08f) * len, v2 = 0.2f * len * (float)s;
            float ex = bx + ca * u * len - sa * w * (float)s, ey = by + sa * u * len + ca * w * (float)s;
            int32_t ix = ftoi(bx + ca * u2 - sa * v2), iy = ftoi(by + sa * u2 + ca * v2);
            ln(ftoi(ex), ftoi(ey), ix, iy, wall, 16);
        }
}

static void scene_flora(void)
{
    const uint16_t WALL = RGB(214, 224, 200);
    float g, h, gs = sc_gsum(), sumf = 0.0f, stx[7], sty[7];
    int i, c, k;
    for (c = 0; c < EN_NSCION; c++)
        sumf += (float)sc.fresh[c];
    sc.gmo = fm_clampf(sc.gmo + 0.05f * sumf - 0.005f * sc.dt, 0.2f, 1.0f);
    g = sc.gmo;
    h = 14.0f + 64.0f * g;                      /* the stem's height */
    cv_rect(0, 0, 240, 112, WALL);              /* the room: wall, wainscot, floor, window */
    cv_rect(0, 0, 240, 6, mix(WALL, RGB(150, 160, 140), 4));
    cv_rect(0, 90, 240, 22, RGB(196, 208, 182));
    cv_rect(0, 90, 240, 2, RGB(176, 190, 160));
    cv_rect(0, 112, 240, SCN_H - 112, RGB(156, 108, 72));
    for (i = 0; i < 4; i++)
        cv_rect(0, 114 + i * 8 + i * i, 240, 1, RGB(128, 86, 56));
    for (i = 0; i < 12; i++)
        cv_rect(10 + i * 21 + (i & 1) * 9, 114 + (i % 4) * 8 + (i % 4) * (i % 4), 1, 9 + (i % 4) * 2, RGB(128, 86, 56));
    cv_rect(168, 12, 56, 60, RGB(240, 236, 220));                       /* the window: its light grows with the playing */
    cv_rect(172, 16, 22, 26, mix(RGB(176, 214, 238), RGB(255, 252, 232), q16(gs) / 2));
    cv_rect(198, 16, 22, 26, mix(RGB(176, 214, 238), RGB(255, 252, 232), q16(gs) / 2));
    cv_rect(172, 46, 22, 22, mix(RGB(168, 208, 234), RGB(255, 252, 232), q16(gs) / 2));
    cv_rect(198, 46, 22, 22, mix(RGB(168, 208, 234), RGB(255, 252, 232), q16(gs) / 2));
    {
        const float lx[4] = {150.0f, 206.0f, 226.0f, 128.0f}, ly[4] = {114.0f, 114.0f, 140.0f, 140.0f};
        poly_fill(lx, ly, 4, mix(RGB(168, 120, 82), RGB(204, 160, 110), 3 + q16(gs) / 3));
    }
    ell(120, 143, 26, 3, RGB(120, 80, 52));                             /* the pot */
    {
        const float pxs[4] = {104.0f, 136.0f, 132.0f, 108.0f}, pys[4] = {118.0f, 118.0f, 142.0f, 142.0f};
        poly_fill(pxs, pys, 4, RGB(190, 98, 58));
    }
    cv_rect(101, 114, 38, 6, RGB(208, 114, 70));
    ell(120, 114, 16, 3, RGB(74, 50, 38));
    for (k = 0; k < 7; k++) {                                           /* the stem: a curve, thick */
        float t = (float)k / 6.0f;
        stx[k] = 120.0f + 5.0f * fm_sinf(t * 2.4f + 0.4f) + 1.5f * fm_sinf(sc.now * 0.9f + t * 3.0f) * t;
        sty[k] = 113.0f - h * t;
    }
    for (k = 1; k < 7; k++)
        ln_w(ftoi(stx[k - 1]), ftoi(sty[k - 1]), ftoi(stx[k]), ftoi(sty[k]), 3, RGB(84, 134, 72));
    for (i = 0; i < MO_N; i++) {                                        /* its leaves, the oldest below */
        float s = fm_clampf(g * (float)MO_N - (float)i * 0.85f, 0.0f, 1.0f), f = ((float)i + 0.6f) / ((float)MO_N + 0.6f);
        float hf = sc_hf((uint32_t)i * 7u + 3u), side = (i & 1) ? 1.0f : -1.0f, gl = sc.glow[i % 5];
        float t = f * 6.0f, nx, ny, pa, pl, bx, by, a, len;
        int ki = (int)t, fl;
        uint16_t body, edge, rib;
        if (s <= 0.0f)
            continue;
        if (ki > 5)
            ki = 5;
        nx = stx[ki] + (stx[ki + 1] - stx[ki]) * (t - (float)ki);
        ny = sty[ki] + (sty[ki + 1] - sty[ki]) * (t - (float)ki);
        pa = -1.5708f + side * (0.55f + 0.5f * hf);
        pl = (6.0f + 16.0f * hf) * s;
        bx = nx + fm_cosf(pa) * pl;
        by = ny + fm_sinf(pa) * pl;
        a = pa + side * 0.25f + gl * 0.35f * fm_sinf(sc.now * 13.0f + (float)i) + 0.04f * fm_sinf(sc.now * 0.8f + (float)i);
        len = (40.0f - 2.2f * (float)i + 8.0f * hf) * s;
        fl = q16(gl) / 3;
        body = mix(mix(RGB(34, 112, 66), RGB(92, 172, 98), i * 16 / MO_N), RGB(170, 232, 150), fl);
        edge = mix(RGB(22, 78, 50), RGB(70, 140, 80), fl);
        rib = mix(RGB(150, 204, 130), RGB(235, 255, 220), fl);
        ln_w(ftoi(nx), ftoi(ny), ftoi(bx), ftoi(by), 2, RGB(70, 124, 64));
        mo_leaf(bx, by, a, len, body, edge, rib, WALL);
    }
    for (c = 0; c < EN_NSCION; c++)                                     /* a note: dust rising in the light */
        if (sc.fresh[c]) {
            int n;
            for (n = 0; n < 3 * sc.fresh[c]; n++)
                scp_add(100.0f + 80.0f * sc_rand(), 100.0f + 20.0f * sc_rand(), (sc_rand() - 0.5f) * 6.0f, -(8.0f + 10.0f * sc_rand()), 3.0f, 9, 0);
        }
    scp_step(200.0f, 5.0f, 0.0f);
    for (i = 0; i < SC_NP; i++)
        if (scp[i].life > 0.0f) {
            uint32_t a = scp[i].life > 1.0f ? 12u : (uint32_t)(scp[i].life * 12.0f);
            px_blend(ftoi(scp[i].x), ftoi(scp[i].y), RGB(255, 250, 214), a);
            px_blend(ftoi(scp[i].x) + 1, ftoi(scp[i].y), RGB(255, 250, 214), a / 2u);
        }
}

/* -------------------------------------------------------------- EARTH --- */
static void scene_earth(void)
{
    const int32_t GX = 120, GY = 72, GR = 54;
    float lon0 = -sc.now * (float)proj.par[P_TEMPO] * (1.0f / 60.0f) * (1.0f / 90.0f);   /* turns: east, once in 90 s at 60 */
    float glow = 0.0f;
    int32_t dy, dx, i, c;
    lon0 -= fm_floorf(lon0);
    cv_rect(0, 0, 240, SCN_H, RGB(6, 8, 18));
    for (i = 0; i < 48; i++) {                                          /* stars */
        uint32_t a = (uint32_t)(4.0f + 8.0f * fm_fabsf(fm_sinf(sc.now * (0.5f + sc_hf((uint32_t)i)) + (float)i)));
        px_blend(ftoi(sc_hf((uint32_t)i * 3u) * 240.0f), ftoi(sc_hf((uint32_t)i * 3u + 1u) * (float)SCN_H), RGB(220, 226, 255), a);
    }
    for (c = 0; c < EN_NSCION; c++)
        glow = fm_maxf(glow, sc.glow[c]);
    for (i = 0; i < 90; i++) {                                          /* the air, brighter when a note comes */
        float t = (float)i * (FM_TWO_PI / 90.0f), cs = fm_cosf(t), sn = fm_sinf(t);
        px_blend(ftoi((float)GX + cs * (float)(GR + 1)), ftoi((float)GY + sn * (float)(GR + 1)), RGB(130, 190, 255), 6u + (uint32_t)(glow * 6.0f));
        px_blend(ftoi((float)GX + cs * (float)(GR + 2)), ftoi((float)GY + sn * (float)(GR + 2)), RGB(130, 190, 255), 3u + (uint32_t)(glow * 4.0f));
        px_blend(ftoi((float)GX + cs * (float)(GR + 3)), ftoi((float)GY + sn * (float)(GR + 3)), RGB(130, 190, 255), 1u + (uint32_t)(glow * 2.0f));
    }
    for (dy = -GR; dy <= GR; dy++) {                                      /* the globe, by inverse projection */
        float yn = (float)dy * (1.0f / (float)GR), cl = fm_sqrtf(1.0f - yn * yn), lat;
        int32_t hw = ftoi(cl * (float)GR), row;
        float lt = turns(-yn, cl);                                      /* the latitude, in turns, +north */
        lat = (lt > 0.5f ? lt - 1.0f : lt);
        row = (int32_t)((0.25f - lat) * 72.0f);
        if (row < 0)
            row = 0;
        if (row > LAND_H - 1)
            row = LAND_H - 1;
        for (dx = -hw; dx <= hw; dx++) {
            float xn = (float)dx * (1.0f / (float)GR), zz = 1.0f - xn * xn - yn * yn, z, rel, u, light;
            int32_t col;
            uint16_t base;
            int land, ice;
            if (zz <= 0.0f)
                continue;
            z = fm_sqrtf(zz);
            rel = turns(xn, z);                                         /* east of the middle, in turns */
            if (rel > 0.5f)
                rel -= 1.0f;
            u = lon0 + rel + 0.5f;
            u -= fm_floorf(u);
            col = (int32_t)(u * (float)LAND_W);
            if (col > LAND_W - 1)
                col = LAND_W - 1;
            land = (int)(LAND[row][col >> 5] >> (col & 31) & 1u);
            ice = lat > 0.19f || lat < -0.19f;
            if (ice)
                base = RGB(236, 242, 250);
            else if (land)
                base = (lat > 0.04f || lat < -0.04f) && (lat < 0.09f && lat > -0.09f) ? RGB(184, 156, 90)
                       : (lat > 0.15f || lat < -0.15f) ? RGB(120, 140, 110) : RGB(78, 150, 78);
            else
                base = RGB(30, 94, 172);
            light = fm_clampf(0.15f + 1.0f * (-0.35f * xn - 0.35f * yn + 0.87f * z), 0.12f, 1.0f);
            px_blend(GX + dx, GY + dy, mix(RGB(3, 5, 12), base, q16(light)), 16u);
        }
    }
    for (c = 0; c < EN_NSCION; c++)                                     /* a note: rain on the ground */
        if (sc.fresh[c]) {
            float t = (float)(sc.note[c] % 12) * (FM_TWO_PI / 12.0f) + (float)c * 1.3f, r = (0.15f + 0.6f * sc_rand()) * (float)GR;
            scr_add((float)GX + fm_cosf(t) * r, (float)GY + fm_sinf(t) * r * 0.9f);
        }
    scr_draw(0.85f, 20.0f, 20.0f, RGB(214, 238, 255));
}

/* ---------------------------------------------------------- POSIDONIA --- */
static void scene_posidonia(void)
{
    int i, j, c, r;
    float gs = sc_gsum();
    for (i = 0; i < 12; i++)                                            /* the water, light above and dark below */
        cv_rect(0, i * 12, 240, 12, mix(RGB(54, 170, 184), RGB(6, 40, 72), i * 16 / 11));
    for (r = 0; r < 7; r++) {                                           /* the sun's beams, in pieces: they come and go */
        float x0 = 14.0f + (float)r * 36.0f + 10.0f * fm_sinf(sc.now * 0.15f + (float)r), gl = sc.glow[r % 5];
        for (j = 0; j < 8; j++) {
            float y0 = (float)j * 17.0f, y1 = y0 + 12.0f;
            float f = 0.5f + 0.5f * fm_sinf(sc.now * 0.5f + (float)j * 1.9f + (float)r * 2.3f);
            int k;
            if (f < 0.3f)
                continue;
            for (k = -2 - j / 3; k <= 2 + j / 3; k++)                   /* wider the deeper they go; a note lights its beam */
                ln(ftoi(x0 + 0.45f * y0) + k, ftoi(y0), ftoi(x0 + 0.45f * y1) + k, ftoi(y1), RGB(232, 252, 224), (uint32_t)(2.5f + 3.5f * f + 7.0f * gl));
        }
    }
    for (i = 0; i < 36; i++) {                                          /* plankton, drifting */
        float x = sc_hf((uint32_t)i * 5u) * 240.0f + fm_sinf(sc.now * 0.2f + (float)i) * 8.0f;
        float p = sc_hf((uint32_t)i * 7u) * 140.0f - sc.now * (1.0f + 2.0f * sc_hf((uint32_t)i));
        px_blend(ftoi(x), ftoi(p - fm_floorf(p / 140.0f) * 140.0f), RGB(210, 240, 230), 4u + (uint32_t)(i & 3));
    }
    cv_rect(0, 136, 240, 8, RGB(176, 152, 104));                        /* the sand */
    cv_rect(0, 140, 240, 4, RGB(150, 128, 88));
    for (i = 0; i < 40; i++)
        px_blend((int32_t)(sc_hf((uint32_t)i * 3u) * 240.0f), 136 + (int32_t)(sc_hf((uint32_t)i * 3u + 1u) * 8.0f), RGB(214, 192, 144), 10u);
    for (i = 0; i < 68; i++) {                                          /* the meadow: ribbons of seagrass, in the swell */
        float bx = sc_hf((uint32_t)i * 3u + 1u) * 250.0f - 5.0f, by = 132.0f + sc_hf((uint32_t)i * 3u + 2u) * 10.0f;
        float hgt = 34.0f + 48.0f * sc_hf((uint32_t)i * 3u + 3u), dk = sc_hf((uint32_t)i * 5u + 7u);
        float amp = (1.0f + 1.8f * sc.glow[i % 5]) * (0.7f + 0.6f * sc_hf((uint32_t)i + 55u)), px = bx, py = by;
        uint16_t c0 = mix(RGB(10, 60, 70), RGB(20, 92, 58), (int)(dk * 16.0f)), c1 = mix(RGB(34, 110, 110), RGB(110, 186, 96), (int)(dk * 16.0f));
        for (j = 1; j <= 6; j++) {
            float t = (float)j * (1.0f / 6.0f);
            float x = bx + amp * 13.0f * fm_sinf(sc.now * 1.1f + sc_hf((uint32_t)i) * FM_TWO_PI + t * 2.2f) * t * t + (sc_hf((uint32_t)i + 9u) - 0.5f) * 14.0f * t;
            float y = by - hgt * t;
            ln_w(ftoi(px), ftoi(py), ftoi(x), ftoi(y), 2, mix(c0, c1, (int)(t * 16.0f)));
            px = x;
            py = y;
        }
    }
    for (c = 0; c < EN_NSCION; c++)                                     /* a note: bubbles, and now and then a fish */
        if (sc.fresh[c]) {
            int n;
            for (n = 0; n < 3 * sc.fresh[c]; n++)
                scp_add(20.0f + 200.0f * sc_rand(), 128.0f, 0.0f, -(16.0f + 12.0f * sc_rand()), 7.0f, 0, (uint8_t)(sc_rand() * 2.99f));
            if (sc.note[c] % 2 == 0) {
                int dir = (c & 1) ? -1 : 1;
                scp_add(dir > 0 ? -8.0f : 248.0f, 30.0f + 70.0f * sc_rand(), (float)dir * (22.0f + 12.0f * sc_rand()), 0.0f, 12.0f, 3, (uint8_t)(sc_rand() * 2.99f));
            }
        }
    scp_step(200.0f, 4.0f, 0.0f);
    for (i = 0; i < SC_NP; i++) {
        if (scp[i].life <= 0.0f)
            continue;
        if (scp[i].kind == 0) {                                         /* a bubble: a ring with a glint */
            float rr = 1.6f + (float)scp[i].tone;
            for (j = 0; j < 8; j++)
                px_blend(ftoi(scp[i].x + fm_cosf((float)j * 0.785f) * rr), ftoi(scp[i].y + fm_sinf((float)j * 0.785f) * rr), RGB(220, 248, 255), 12u);
            px_blend(ftoi(scp[i].x - rr * 0.4f), ftoi(scp[i].y - rr * 0.4f), RGB(255, 255, 255), 16u);
        } else {                                                        /* a fish: a body and a tail, towards where it goes */
            float d = scp[i].vx > 0.0f ? 1.0f : -1.0f, wag = fm_sinf(sc.now * 9.0f + scp[i].ph) * 1.5f;
            uint16_t col = scp[i].tone == 0 ? RGB(214, 228, 232) : scp[i].tone == 1 ? RGB(240, 150, 70) : RGB(110, 172, 224);
            ell(ftoi(scp[i].x), ftoi(scp[i].y), 4, 2, col);
            ln(ftoi(scp[i].x - d * 4.0f), ftoi(scp[i].y), ftoi(scp[i].x - d * 8.0f), ftoi(scp[i].y - 3.0f + wag), col, 16);
            ln(ftoi(scp[i].x - d * 4.0f), ftoi(scp[i].y), ftoi(scp[i].x - d * 8.0f), ftoi(scp[i].y + 3.0f + wag), col, 16);
            px_blend(ftoi(scp[i].x + d * 2.0f), ftoi(scp[i].y) - 1, RGB(20, 30, 40), 16u);
        }
    }
    (void)gs;
}

/* -------------------------------------------------------------- NIGHT --- */
static void scene_night(void)
{
    int i, j, c;
    int32_t x;
    float gs = sc_gsum(), wind;
    for (i = 0; i < 8; i++)
        cv_rect(0, i * 12, 240, 12, mix(RGB(6, 10, 30), RGB(42, 62, 110), i * 16 / 7));
    for (i = 0; i < 44; i++)                                            /* stars */
        px_blend(ftoi(sc_hf((uint32_t)i * 3u) * 240.0f), ftoi(sc_hf((uint32_t)i * 3u + 1u) * 70.0f), RGB(230, 236, 255),
                 (uint32_t)(4.0f + 10.0f * fm_fabsf(fm_sinf(sc.now * (0.4f + sc_hf((uint32_t)i)) + (float)i))));
    for (i = 0; i < 80; i++) {                                          /* the moon and its halo */
        float t = (float)i * (FM_TWO_PI / 80.0f);
        px_blend(ftoi(190.0f + fm_cosf(t) * 15.0f), ftoi(32.0f + fm_sinf(t) * 15.0f), RGB(200, 214, 255), 5u);
        px_blend(ftoi(190.0f + fm_cosf(t) * 19.0f), ftoi(32.0f + fm_sinf(t) * 19.0f), RGB(200, 214, 255), 3u);
        px_blend(ftoi(190.0f + fm_cosf(t) * 24.0f), ftoi(32.0f + fm_sinf(t) * 24.0f), RGB(200, 214, 255), 2u);
    }
    disc(190.0f, 32.0f, 11.0f, RGB(244, 240, 212));
    disc(186.0f, 29.0f, 2.2f, RGB(222, 218, 190));
    disc(194.0f, 36.0f, 1.8f, RGB(226, 222, 194));
    for (x = 0; x < 240; x += 2) {                                      /* the far bank: hills, trees */
        float h = 8.0f + 6.0f * fm_sinf((float)x * 0.03f + 0.5f) + 5.0f * fm_maxf(0.0f, fm_sinf((float)x * 0.21f + 1.0f)) + 4.0f * sc_hf((uint32_t)x / 6u);
        cv_rect(x, 92 - ftoi(h), 2, ftoi(h) + 2, RGB(12, 22, 38));
    }
    for (i = 0; i < 6; i++)                                             /* the river, darker down */
        cv_rect(0, 92 + i * 9, 240, 9, mix(RGB(18, 34, 66), RGB(8, 16, 34), i * 16 / 5));
    for (j = 94; j < 138; j += 2) {                                     /* the moon on the water */
        float w = 5.0f + 5.0f * fm_sinf((float)j * 0.45f + sc.now * 1.3f), cx = 190.0f + 3.0f * fm_sinf((float)j * 0.3f + sc.now);
        cv_rect(ftoi(cx - w), j, ftoi(2.0f * w), 1, mix(RGB(20, 38, 70), RGB(200, 204, 170), 10 - (j - 94) / 6 + q16(gs) / 6));
    }
    scr_draw(0.28f, 26.0f, 11.0f, RGB(190, 206, 236));
    for (x = 0; x < 240; x += 4)                                        /* the bank, in front */
        cv_rect(x, 129 + ftoi(3.0f * fm_sinf((float)x * 0.2f)), 4, 20, RGB(8, 14, 20));
    wind = 0.55f * fm_sinf(sc.now * 0.7f) + 0.35f * fm_sinf(sc.now * 1.9f + 1.0f) + 0.8f * fm_maxf(0.0f, fm_sinf(sc.now * 0.31f)) + 1.2f * gs;
    for (i = 0; i < 54; i++) {                                          /* the reeds, bent by the wind and by the notes */
        float bx = i < 27 ? sc_hf((uint32_t)i * 3u + 1u) * 76.0f : 164.0f + sc_hf((uint32_t)i * 3u + 1u) * 80.0f;
        float hgt = 42.0f + 52.0f * sc_hf((uint32_t)i * 3u + 2u), ph = sc_hf((uint32_t)i * 3u + 3u) * FM_TWO_PI, px = bx, py = 140.0f;
        uint16_t c0 = RGB(10, 26, 30), c1 = mix(RGB(34, 62, 60), RGB(70, 108, 96), (int)(sc_hf((uint32_t)i + 70u) * 16.0f));
        for (j = 1; j <= 6; j++) {
            float t = (float)j * (1.0f / 6.0f);
            float xx = bx + (wind * 0.9f + 0.25f * fm_sinf(sc.now * 2.2f + ph)) * 16.0f * t * t * (0.8f + 0.4f * sc_hf((uint32_t)i + 5u));
            float yy = 140.0f - hgt * t;
            ln_w(ftoi(px), ftoi(py), ftoi(xx), ftoi(yy), 2, mix(c0, c1, (int)(t * 16.0f)));
            if (j == 5 && sc_hf((uint32_t)i + 31u) > 0.6f)              /* a cattail */
                ln_w(ftoi(xx), ftoi(yy) - 1, ftoi(xx + (xx - px) * 0.3f), ftoi(yy - 8.0f), 3, RGB(58, 38, 28));
            px = xx;
            py = yy;
        }
    }
    for (c = 0; c < EN_NSCION; c++)                                     /* a note: ripples, fireflies rise */
        if (sc.fresh[c]) {
            int n;
            scr_add(20.0f + 200.0f * sc_rand(), 100.0f + 30.0f * sc_rand());
            for (n = 0; n < 3 * sc.fresh[c]; n++)
                scp_add(sc_rand() > 0.5f ? 4.0f + 70.0f * sc_rand() : 170.0f + 70.0f * sc_rand(), 126.0f, (sc_rand() - 0.5f) * 8.0f, -(6.0f + 5.0f * sc_rand()), 6.0f + 3.0f * sc_rand(), 4, 0);
        }
    sc.amb -= sc.dt;
    if (sc.amb <= 0.0f) {                                               /* and now and then a few by themselves */
        int n;
        sc.amb = 2.5f + 3.5f * sc_rand();
        for (n = 0; n < 2; n++)
            scp_add(sc_rand() > 0.5f ? 4.0f + 70.0f * sc_rand() : 170.0f + 70.0f * sc_rand(), 126.0f, (sc_rand() - 0.5f) * 8.0f, -(6.0f + 5.0f * sc_rand()), 7.0f, 4, 0);
    }
    scp_step(200.0f, 14.0f, 0.0f);
    for (i = 0; i < SC_NP; i++)
        if (scp[i].life > 0.0f) {                                       /* a firefly: it blinks, with a halo */
            float b = 0.5f + 0.5f * fm_sinf(scp[i].ph * 2.5f), fade = scp[i].life < 1.5f ? scp[i].life * (1.0f / 1.5f) : 1.0f;
            uint32_t a = (uint32_t)(b * fade * 7.0f) + 1u;
            for (j = 0; j < 8; j++)
                px_blend(ftoi(scp[i].x + fm_cosf((float)j * 0.785f) * 2.6f), ftoi(scp[i].y + fm_sinf((float)j * 0.785f) * 2.6f), RGB(200, 250, 110), a / 2u + 1u);
            disc(scp[i].x, scp[i].y, 1.2f, mix(RGB(70, 90, 40), RGB(226, 255, 130), (int)(b * fade * 16.0f)));
        }
}

/* ---------------------------------------------------------- LIGHTHOUSE --- */
static void scene_lighthouse(void)
{
    static const float RX[10] = {0, 0, 14, 22, 34, 46, 58, 70, 84, 90}, RY[10] = {144, 92, 88, 96, 90, 100, 108, 118, 126, 144};
    static const float CXS[12] = {132, 140, 150, 158, 170, 178, 188, 206, 220, 240, 240, 132}, CYS[12] = {144, 122, 112, 100, 92, 80, 74, 72, 70, 66, 144, 144};
    float gs = sc_gsum(), th, cb, L, sgn;
    int i, j, c;
    int32_t x;
    if (sc.flash > 0.0f)
        sc.flash -= sc.dt * 2.2f;
    if (sc.flash < 0.0f)
        sc.flash = 0.0f;
    for (c = 0; c < EN_NSCION; c++)                                     /* a note is a wave; now and then a bolt */
        if (sc.fresh[c]) {
            int n;
            if (sc.note[c] % 4 == 0 || sc.fresh[c] > 1) {
                sc.flash = 1.0f;
                sc.boltx = 20.0f + 130.0f * sc_rand();
            }
            for (n = 0; n < 5 * sc.fresh[c]; n++)
                scp_add(70.0f + 70.0f * sc_rand(), 118.0f, (sc_rand() - 0.5f) * 44.0f, -(40.0f + 40.0f * sc_rand()), 1.6f, 5, (uint8_t)(sc_rand() * 2.99f));
        }
    for (i = 0; i < 10; i++)                                            /* the sky, heavy */
        cv_rect(0, i * 10, 240, 10, mix(mix(RGB(26, 32, 42), RGB(70, 78, 90), i * 16 / 9), RGB(210, 220, 240), q16(sc.flash) / 2));
    for (i = 0; i < 7; i++) {                                           /* clouds, driven by the wind */
        float cx = sc_hf((uint32_t)i * 3u) * 300.0f + sc.now * (7.0f + 9.0f * sc_hf((uint32_t)i)), cy = 10.0f + 62.0f * sc_hf((uint32_t)i * 3u + 1u);
        cx = cx - fm_floorf(cx / 300.0f) * 300.0f - 30.0f;
        ell(ftoi(cx), ftoi(cy), 40 + (int32_t)(sc_hf((uint32_t)i + 9u) * 20.0f), 9, mix(RGB(40, 46, 58), RGB(220, 226, 238), q16(sc.flash) / 3));
        ell(ftoi(cx + 14.0f), ftoi(cy - 5.0f), 26, 7, mix(RGB(54, 62, 76), RGB(236, 240, 250), q16(sc.flash) / 3));
    }
    if (sc.flash > 0.4f) {                                              /* the bolt */
        float bx = sc.boltx;
        int32_t by;
        for (by = 0; by < 92; by += 11) {
            float nx = bx + (sc_hf((uint32_t)by + (uint32_t)(sc.boltx * 7.0f)) - 0.5f) * 16.0f;
            ln_w(ftoi(bx), by, ftoi(nx), by + 11, 2, RGB(250, 250, 255));
            bx = nx;
        }
    }
    for (i = 0; i < 10; i++)                                            /* the sea */
        cv_rect(0, 96 + i * 5, 240, 5, mix(RGB(30, 52, 68), RGB(10, 22, 32), i * 16 / 9));
    for (j = 0; j < 8; j++) {                                           /* waves, higher with the playing */
        float y0 = 100.0f + (float)j * 6.0f, am = 2.0f + 3.0f * gs, py = y0 + am * fm_sinf(sc.now * 1.4f + (float)j);
        for (x = 6; x <= 240; x += 6) {
            float y = y0 + am * fm_sinf((float)x * 0.08f + sc.now * (1.4f + 0.1f * (float)j) + (float)j);
            ln((int32_t)(x - 6), ftoi(py), x, ftoi(y), RGB(110, 140, 158), 3u + (uint32_t)j / 3u);
            if (y < y0 - am * 0.8f)
                px_blend(x, ftoi(y) - 1, RGB(240, 248, 255), 12u);
            py = y;
        }
    }
    poly_fill(RX, RY, 10, RGB(24, 26, 30));                             /* the rocks */
    poly_fill(CXS, CYS, 12, RGB(30, 32, 36));
    for (i = 0; i < 9; i++) {
        ln(ftoi(RX[i]), ftoi(RY[i]), ftoi(RX[i + 1]), ftoi(RY[i + 1]), RGB(60, 66, 72), 16);
        if (i < 9 && i + 1 < 11)
            ln(ftoi(CXS[i]), ftoi(CYS[i]), ftoi(CXS[i + 1]), ftoi(CYS[i + 1]), RGB(62, 68, 74), 16);
    }
    for (i = 0; i < 4; i++) {                                           /* the tower: white and red, tapering */
        float y0 = 26.0f + (float)i * 11.75f, y1 = y0 + 11.75f;
        float l0 = 200.0f - (6.0f + 3.0f * (y0 - 26.0f) / 47.0f), l1 = 200.0f - (6.0f + 3.0f * (y1 - 26.0f) / 47.0f);
        float tx[4], ty[4];
        tx[0] = l0; ty[0] = y0;
        tx[1] = 400.0f - l0; ty[1] = y0;
        tx[2] = 400.0f - l1; ty[2] = y1;
        tx[3] = l1; ty[3] = y1;
        poly_fill(tx, ty, 4, (i & 1) ? RGB(196, 48, 44) : RGB(232, 232, 226));
    }
    cv_rect(192, 23, 16, 3, RGB(40, 40, 44));                           /* the gallery, the lantern, the roof */
    cv_rect(194, 13, 12, 10, mix(RGB(255, 216, 120), RGB(255, 250, 200), 4 + q16(gs) / 4));
    cv_rect(199, 13, 2, 10, RGB(60, 52, 40));
    {
        const float rx[3] = {192.0f, 208.0f, 200.0f}, ry[3] = {13.0f, 13.0f, 5.0f};
        poly_fill(rx, ry, 3, RGB(170, 40, 40));
    }
    cb = fm_cosf(sc.now * 0.9f);                                        /* the beam sweeping: a wedge, long when it points sideways */
    sgn = cb > 0.0f ? 1.0f : -1.0f;
    L = 120.0f * fm_fabsf(cb);
    for (i = 0; i < 90; i++) {                                          /* the light's halo, warm: it makes everything familiar */
        float t = (float)i * (FM_TWO_PI / 90.0f);
        px_blend(ftoi(200.0f + fm_cosf(t) * 14.0f), ftoi(18.0f + fm_sinf(t) * 14.0f), RGB(255, 226, 150), 5u);
        px_blend(ftoi(200.0f + fm_cosf(t) * 22.0f), ftoi(18.0f + fm_sinf(t) * 22.0f), RGB(255, 226, 150), 3u);
        px_blend(ftoi(200.0f + fm_cosf(t) * 34.0f), ftoi(18.0f + fm_sinf(t) * 34.0f), RGB(255, 226, 150), 1u);
    }
    for (j = 4; j < (int)L; j++) {
        float hh = 1.5f + (float)j * 0.1f;
        uint32_t a = (uint32_t)(8.0f * (1.0f - (float)j / L)) + 1u;
        int k;
        for (k = -(int)hh; k <= (int)hh; k++)
            px_blend(ftoi(200.0f + sgn * (float)j), 18 + k + (int)((float)j * 0.04f), RGB(255, 236, 170), a);
    }
    for (i = 0; i < 56; i++) {                                          /* the rain, slanting */
        float x0 = sc_hf((uint32_t)i * 3u) * 260.0f + sc.now * 70.0f, y0 = sc_hf((uint32_t)i * 3u + 1u) * 150.0f + sc.now * 230.0f * (0.7f + 0.6f * sc_hf((uint32_t)i + 17u));
        int32_t rx = ftoi(x0 - fm_floorf(x0 / 260.0f) * 260.0f) - 10, ry = ftoi(y0 - fm_floorf(y0 / 150.0f) * 150.0f);
        ln(rx, ry, rx - 3, ry + 7, RGB(160, 180, 200), 5u);
    }
    scp_step(124.0f, 0.0f, 110.0f);
    for (i = 0; i < SC_NP; i++)                                         /* spray off the rocks */
        if (scp[i].life > 0.0f)
            disc(scp[i].x, scp[i].y, 1.0f + 0.4f * (float)scp[i].tone, RGB(236, 244, 252));
    (void)th;
}

/* ------------------------------------------------------------ SHORELINE --- */
static void sh_heart(int32_t x, int32_t y, uint16_t c)
{
    cv_rect(x - 2, y, 2, 1, c);
    cv_rect(x + 1, y, 2, 1, c);
    cv_rect(x - 2, y + 1, 5, 1, c);
    cv_rect(x - 1, y + 2, 3, 1, c);
    cv_rect(x, y + 3, 1, 1, c);
}

static void scene_shoreline(void)
{
    static const uint16_t SKY[6] = {RGB(70, 52, 120), RGB(126, 66, 128), RGB(190, 84, 118), RGB(236, 120, 96), RGB(250, 160, 96), RGB(255, 206, 128)};
    float gs = sc_gsum(), sunY = 80.0f + 4.0f * fm_sinf(sc.now * 0.02f);
    int i, j, c;
    for (i = 0; i < 12; i++)                                            /* the sky, violet to gold at the horizon */
        cv_rect(0, i * 7, 240, 7, mix(SKY[i * 5 / 11], SKY[i * 5 / 11 + (i < 11 ? 1 : 0)], (i * 5 % 11) * 16 / 11));
    for (i = 0; i < 3; i++)                                             /* thin clouds */
        ell(40 + i * 70 + ftoi(fm_sinf(sc.now * 0.03f + (float)i) * 10.0f), 30 + i * 11, 34, 2, RGB(200, 100, 120));
    for (i = 0; i < 90; i++) {                                          /* the sun and its glow */
        float t = (float)i * (FM_TWO_PI / 90.0f);
        px_blend(ftoi(92.0f + fm_cosf(t) * 17.0f), ftoi(sunY + fm_sinf(t) * 17.0f), RGB(255, 226, 150), 6u);
        px_blend(ftoi(92.0f + fm_cosf(t) * 24.0f), ftoi(sunY + fm_sinf(t) * 24.0f), RGB(255, 226, 150), 4u);
        px_blend(ftoi(92.0f + fm_cosf(t) * 34.0f), ftoi(sunY + fm_sinf(t) * 34.0f), RGB(255, 226, 150), 2u);
    }
    disc(92.0f, sunY, 13.0f, RGB(255, 238, 176));
    for (i = 0; i < 6; i++)                                             /* the sea, gold into violet */
        cv_rect(0, 84 + i * 6, 240, 6, mix(RGB(240, 150, 100), RGB(64, 56, 110), i * 16 / 5));
    for (j = 85; j < 118; j += 2) {                                     /* the sun's glitter on it, brighter with the notes */
        float w = 4.0f + (float)(j - 85) * 0.9f, x0 = 92.0f + 3.0f * fm_sinf((float)j * 0.8f + sc.now * 2.0f);
        for (i = 0; i < 3; i++) {
            float xx = x0 + (sc_hf((uint32_t)(j * 7 + i) + (uint32_t)(sc.now * 3.0f)) - 0.5f) * w * 2.0f;
            cv_rect(ftoi(xx), j, 3 + i, 1, mix(RGB(150, 100, 100), RGB(255, 232, 160), 6 + q16(gs) / 2));
        }
    }
    for (j = 0; j < 4; j++) {                                           /* small waves */
        float y0 = 92.0f + (float)j * 7.0f, py = y0;
        int32_t x;
        for (x = 6; x <= 240; x += 6) {
            float y = y0 + 1.2f * fm_sinf((float)x * 0.1f + sc.now * 1.2f + (float)j * 2.0f);
            ln(x - 6, ftoi(py), x, ftoi(y), RGB(255, 210, 170), 4u);
            py = y;
        }
    }
    cv_rect(0, 116, 240, 12, RGB(230, 196, 150));                       /* the beach and the foam */
    cv_rect(0, 124, 240, 4, RGB(206, 170, 126));
    for (i = 0; i < 240; i += 4)
        cv_rect(i, 115 + ftoi(1.5f * fm_sinf((float)i * 0.15f + sc.now * 0.8f)), 4, 2, RGB(255, 246, 236));
    cv_rect(0, 122, 240, 22, RGB(176, 150, 142));                       /* the wall: its cap, its face */
    cv_rect(0, 122, 240, 4, RGB(206, 182, 170));
    cv_rect(0, 126, 240, 18, RGB(150, 126, 120));
    for (i = 0; i < 12; i++)
        cv_rect(i * 21 + (i & 1) * 9, 126, 1, 18, RGB(128, 106, 102));
    ell(160, 108, 6, 13, RGB(52, 30, 26));                              /* the lovers, from behind: her long hair, ... */
    ell(143, 114, 10, 10, RGB(40, 38, 66));                             /* ... his back and hers */
    ell(158, 115, 9, 9, RGB(96, 48, 72));
    ln_w(146, 108, 156, 108, 4, RGB(40, 38, 66));                       /* his arm round her */
    disc(142.0f, 100.0f, 5.5f, RGB(46, 34, 30));                        /* the heads, leaning together */
    ell(157, 100, 5, 6, RGB(66, 38, 30));
    for (i = 0; i < 6; i++)                                             /* the rim of the sunset on them */
        px_blend(137 + (i & 1), 97 + i, RGB(255, 190, 120), 8u);
    cv_rect(-30, 92, 84, 44, RGB(250, 228, 214));                       /* the ice cream van, at the left, half out of the picture */
    cv_rect(-26, 88, 76, 4, RGB(236, 208, 196));
    cv_rect(-30, 118, 84, 8, RGB(84, 190, 190));
    cv_rect(4, 100, 36, 16, RGB(60, 40, 50));
    cv_rect(6, 102, 32, 12, mix(RGB(255, 214, 150), RGB(255, 246, 210), q16(gs) / 3));
    cv_rect(2, 116, 40, 3, RGB(200, 170, 150));
    cv_rect(44, 98, 10, 20, RGB(170, 214, 232));                        /* the windscreen and a headlight */
    cv_disc(53, 126, 2, RGB(255, 236, 160));
    for (i = 0; i < 8; i++) {                                           /* the awning, striped, with a scalloped edge */
        uint16_t sc2 = (i & 1) ? RGB(255, 252, 250) : RGB(234, 84, 108);
        cv_rect(i * 6 - 2, 94, 6, 5, sc2);
        cv_disc(i * 6 + 1, 99, 3, sc2);
    }
    for (i = 0; i < EN_NSCION; i++)                                     /* five bulbs on it, one for each voice */
        cv_disc(5 + i * 9, 105, 2, mix(RGB(110, 90, 80), RGB(255, 238, 150), q16(sc.glow[i])));
    cv_rect(22, 78, 4, 10, RGB(150, 110, 100));                         /* the cone on the roof */
    {
        const float cx[3] = {16.0f, 32.0f, 24.0f}, cy[3] = {62.0f, 62.0f, 78.0f};
        poly_fill(cx, cy, 3, RGB(222, 170, 100));
    }
    disc(24.0f, 58.0f, 7.0f, mix(RGB(255, 176, 196), RGB(255, 230, 240), q16(gs) / 2));
    disc(24.0f, 51.0f, 6.0f, mix(RGB(255, 248, 238), RGB(255, 255, 255), q16(gs) / 2));
    disc(24.0f, 46.0f, 2.0f, RGB(220, 40, 60));
    cv_disc(6, 140, 6, RGB(28, 28, 34));                                /* the wheels */
    cv_disc(42, 140, 6, RGB(28, 28, 34));
    cv_disc(6, 140, 3, RGB(170, 170, 176));
    cv_disc(42, 140, 3, RGB(170, 170, 176));
    for (c = 0; c < EN_NSCION; c++)                                     /* a note: a glint on the sea, a gull, a heart now and then */
        if (sc.fresh[c]) {
            int n;
            for (n = 0; n < 4; n++)
                scp_add(92.0f + (sc_rand() - 0.5f) * 80.0f, 86.0f + 28.0f * sc_rand(), 0.0f, 0.0f, 0.7f, 6, 0);
            if (sc.note[c] % 3 == 0)
                scp_add(246.0f, 14.0f + 40.0f * sc_rand(), -(20.0f + 10.0f * sc_rand()), 0.0f, 14.0f, 7, 0);
            if (sc.cnt % 5u == 0u)
                scp_add(152.0f, 92.0f, 0.0f, -14.0f, 3.2f, 8, 0);
        }
    scp_step(200.0f, 4.0f, 0.0f);
    for (i = 0; i < SC_NP; i++) {
        int32_t px = ftoi(scp[i].x), py = ftoi(scp[i].y);
        if (scp[i].life <= 0.0f)
            continue;
        if (scp[i].kind == 6) {                                         /* a glint: a small cross of light */
            uint32_t a = (uint32_t)(scp[i].life * 22.0f) + 1u;
            px_blend(px, py, RGB(255, 246, 200), a > 16u ? 16u : a);
            px_blend(px - 1, py, RGB(255, 246, 200), a / 2u);
            px_blend(px + 1, py, RGB(255, 246, 200), a / 2u);
            px_blend(px, py - 1, RGB(255, 246, 200), a / 2u);
            px_blend(px, py + 1, RGB(255, 246, 200), a / 2u);
        } else if (scp[i].kind == 7) {                                  /* a gull: a V, flapping */
            int32_t fl = ftoi(fm_sinf(sc.now * 7.0f + scp[i].ph) * 2.5f);
            ln(px - 5, py - fl, px, py + 1, RGB(250, 246, 240), 16u);
            ln(px, py + 1, px + 5, py - fl, RGB(250, 246, 240), 16u);
        } else {                                                        /* a heart */
            sh_heart(px, py, RGB(255, 110, 140));
        }
    }
}


/* -------------------------------------------------------------- SPLASH --- */
/* The first seconds after switching on: a seedling grows out of the dark, its leaves open, SPROUT fades in. It ends
 * after six seconds, or at the first button, key or note of the Scion. */
#ifndef APP_VERSION
#define APP_VERSION "DEV"
#endif

static void sp_leaf(float bx, float by, float a, float len, uint16_t edge, uint16_t body)   /* a leaf, set out from the stem */
{
    bx += fm_cosf(a) * len * 0.16f;
    by += fm_sinf(a) * len * 0.16f;
    mo_poly(bx, by, a, len, 1.0f, edge);
    mo_poly(bx, by, a, len, 0.86f, body);
}

static void splash_draw(float t)
{
    float g = fm_clampf(t / 2.4f, 0.0f, 1.0f), gl = fm_clampf((t - 0.9f) / 1.5f, 0.0f, 1.0f), g2 = fm_clampf((t - 1.5f) / 1.5f, 0.0f, 1.0f);
    float ta = fm_clampf((t - 1.6f) / 0.9f, 0.0f, 1.0f), tx, ty, sway = fm_sinf(t * 1.3f) * 1.5f * g;
    float sx[7], sy[7];
    int i, k;
    for (i = 0; i < 12; i++)                                            /* the dark, a little green at the bottom */
        cv_rect(0, i * 12, 240, 12, mix(RGB(6, 12, 26), RGB(14, 38, 48), i * 16 / 11));
    for (i = 0; i < 34; i++)                                            /* stars */
        px_blend(ftoi(sc_hf((uint32_t)i * 3u) * 240.0f), ftoi(sc_hf((uint32_t)i * 3u + 1u) * 90.0f), RGB(220, 230, 255), 3u + (uint32_t)(i & 7));
    for (i = 0; i < 26; i++) {                                          /* spores rising from the soil */
        float p = sc_hf((uint32_t)i * 7u) * 130.0f - t * (5.0f + 9.0f * sc_hf((uint32_t)i)), y = p - fm_floorf(p / 130.0f) * 130.0f + 6.0f;
        px_blend(ftoi(70.0f + sc_hf((uint32_t)i * 5u) * 100.0f + fm_sinf(t * 0.8f + (float)i) * 5.0f), ftoi(y), RGB(170, 240, 200), (uint32_t)(2.0f + 6.0f * ta) + (uint32_t)(i & 1));
    }
    ell(120, 144, 78, 13, RGB(44, 32, 30));                             /* the soil */
    ell(120, 141, 62, 8, RGB(66, 48, 40));
    for (i = 0; i < 14; i++)
        px_blend(70 + ftoi(sc_hf((uint32_t)i * 9u) * 100.0f), 134 + ftoi(sc_hf((uint32_t)i * 9u + 1u) * 9.0f), RGB(98, 74, 60), 14u);
    for (k = 0; k < 7; k++) {                                           /* the stem: a curve, growing */
        float u = (float)k / 6.0f;
        sx[k] = 120.0f + sway * u + 3.0f * fm_sinf(u * 2.6f);
        sy[k] = 136.0f - 52.0f * g * u;
    }
    tx = sx[6];
    ty = sy[6];
    for (i = 0; i < 90; i++) {                                          /* a soft light round the tip */
        float a = (float)i * (FM_TWO_PI / 90.0f);
        px_blend(ftoi(tx + fm_cosf(a) * 12.0f), ftoi(ty + fm_sinf(a) * 12.0f), RGB(130, 235, 190), (uint32_t)(5.0f * gl) + 1u);
        px_blend(ftoi(tx + fm_cosf(a) * 20.0f), ftoi(ty + fm_sinf(a) * 20.0f), RGB(130, 235, 190), (uint32_t)(3.0f * gl) + 0u);
        px_blend(ftoi(tx + fm_cosf(a) * 30.0f), ftoi(ty + fm_sinf(a) * 30.0f), RGB(130, 235, 190), (uint32_t)(2.0f * gl));
    }
    for (k = 1; k < 7; k++)
        ln_w(ftoi(sx[k - 1]), ftoi(sy[k - 1]), ftoi(sx[k]), ftoi(sy[k]), 3, RGB(98, 190, 108));
    if (g2 > 0.0f) {                                                    /* the lower pair, smaller */
        sp_leaf(sx[3], sy[3], -3.05f + sway * 0.02f, 26.0f * g2, RGB(34, 96, 66), RGB(86, 170, 98));
        sp_leaf(sx[3], sy[3], -0.10f + sway * 0.02f, 26.0f * g2, RGB(34, 96, 66), RGB(86, 170, 98));
    }
    if (gl > 0.0f) {                                                    /* the top pair, opening */
        sp_leaf(tx, ty, -2.80f - 0.10f * fm_sinf(t * 1.3f), 32.0f * gl, RGB(40, 112, 74), RGB(120, 214, 128));
        sp_leaf(tx, ty, -0.34f + 0.10f * fm_sinf(t * 1.3f), 32.0f * gl, RGB(40, 112, 74), RGB(120, 214, 128));
    }
    cv_disc(ftoi(tx), ftoi(ty), 2, RGB(170, 245, 190));                 /* the bud */
    if (ta > 0.0f) {                                                    /* the name, fading in */
        const uint16_t bg = RGB(10, 20, 34);
        text_c(120, 8, &FONT_L, "SPROUT", mix(bg, RGB(240, 250, 236), q16(ta)));
        text_c(120, 44, &FONT_XS, "A POCKET SCION COMPANION", mix(bg, RGB(150, 214, 200), q16(ta)));
        text_c(120, 128, &FONT_XS, "v" APP_VERSION, mix(bg, RGB(120, 150, 168), q16(ta)));
    }
}

static void scene_draw(int piece)
{
    sc_begin();
    switch (piece) {
    case 0: scene_garden(); break;
    case 1: scene_mycelium(); break;
    case 2: scene_flora(); break;
    case 3: scene_earth(); break;
    case 4: scene_posidonia(); break;
    case 5: scene_night(); break;
    case 6: scene_lighthouse(); break;
    default: scene_shoreline(); break;
    }
    sc_dots();
}
