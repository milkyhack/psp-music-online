#include "ui_coverflow.h"
#include "ppsspp_qa_demo.h"

#include "cf_trace.h"
#include "paths.h"
#include "theme.h"
#include "ui.h"
#include "ui_font.h"
#include "ui_gfx.h"
#include "ui_gpu.h"

#include <math.h>
#include <malloc.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <pspgu.h>
#include <pspgum.h>
#include <string.h>

#define CF_SCR_W 480
#define CF_SCR_H 272
#define CF_BUF_W 512
#define CF_FRAME_BYTES (CF_BUF_W * CF_SCR_H * 4)
#define CF_QUAD_Z 2.0f
#define CF_LIST_WORDS 65536

typedef struct {
    float u, v;
    unsigned int color;
    float x, y, z;
} CfVertex;

typedef struct {
    int track_id;
    unsigned tick;
    UiGpuTex main;
    UiGpuTex reflection;
} CfCoverSlot;

static unsigned int __attribute__((aligned(16))) g_cf_list[CF_LIST_WORDS];

static int g_cf_active = 0;
static int g_cf_count = 0;
static int g_cf_cursor = 0;
static const int *g_cf_ids = NULL;
static const UiCover *g_cf_hero = NULL;
static float g_cf_scroll = 0.0f;
static float g_cf_target = 0.0f;
static unsigned g_cf_tick = 0;
static int g_cf_use_3d = 1;
static int g_cf_safe_checked = 0;
static int g_cf_3d_failed = 0;
static CfCoverSlot g_cf_tex[UI_COVERFLOW_TEX_CACHE];
static UiGpuTex g_cf_fallback;
static UiGpuTex g_cf_fallback_refl;

static float cf_rel_pos(int i) {
    float center;
    float rel;
    float half;

    center = g_cf_scroll / (float)UI_COVERFLOW_DISTANCE;
    rel = center - (float)i;
    if (g_cf_count > 2) {
        half = (float)g_cf_count * 0.5f;
        while (rel > half) {
            rel -= (float)g_cf_count;
        }
        while (rel < -half) {
            rel += (float)g_cf_count;
        }
    }
    return rel * (float)UI_COVERFLOW_DISTANCE * 0.9f;
}

static int cf_safe_mode_file(void) {
    char path[280];
    SceUID fd;

    paths_join(path, sizeof(path), "data/cf_safe.txt");
    fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd >= 0) {
        sceIoClose(fd);
        return 1;
    }
    return 0;
}

static void cf_pick_render_mode(void) {
    if (!g_cf_safe_checked) {
        g_cf_safe_checked = 1;
        if (cf_safe_mode_file()) {
            g_cf_use_3d = 0;
            g_cf_3d_failed = 1;
            cf_trace_step("safe_file_cpu");
        }
    }
    if (g_cf_3d_failed) {
        g_cf_use_3d = 0;
    }
    cf_trace_mode_3d(g_cf_use_3d);
}

int ui_coverflow_wants_3d(void) {
    cf_pick_render_mode();
    return g_cf_use_3d;
}

static const UiCover *cf_cover_at(int i) {
    const UiCover *cover = NULL;

    if (g_cf_hero && i == g_cf_cursor) {
        return g_cf_hero;
    }
    if (g_cf_count == 1 && g_cf_hero) {
        return g_cf_hero;
    }
    if (g_cf_ids) {
        int off;
        cover = ui_image_cover_for(g_cf_ids[i]);
        if (cover && cover->ready) {
            return cover;
        }
        /* Keep side cards populated: use nearest ready cover as fallback. */
        for (off = 1; off < g_cf_count && off <= 3; off++) {
            int idx_l = i - off;
            int idx_r = i + off;
            if (idx_l < 0) {
                idx_l += g_cf_count;
            }
            if (idx_r >= g_cf_count) {
                idx_r -= g_cf_count;
            }
            cover = ui_image_cover_for(g_cf_ids[idx_l]);
            if (cover && cover->ready) {
                return cover;
            }
            cover = ui_image_cover_for(g_cf_ids[idx_r]);
            if (cover && cover->ready) {
                return cover;
            }
        }
    }
    if (g_cf_hero && g_cf_hero->ready) {
        return g_cf_hero;
    }
    return NULL;
}

static void cf_draw_cpu_placeholder(int cx, int cy, int sz, int tilt, const PlayerTheme *th) {
    int tx = cx - sz / 2 + tilt / 2;
    int ty = cy;
    u32 rim = th ? th->chrome_hi : UI_RGB(180, 180, 200);
    u32 well = th ? th->chrome_lo : UI_RGB(40, 40, 56);
    u32 acc = th ? th->accent : UI_RGB(255, 120, 200);

    if (sz < 48) {
        sz = 48;
    }
    ui_gfx_round_fill(tx, ty, sz, sz, 6, well);
    ui_gfx_hairline_rect(tx, ty, sz, sz, rim, 100);
    ui_gfx_fill_alpha(tx + 6, ty + 6, sz - 12, sz - 12, acc, 35);
    ui_gfx_ring(tx + sz / 2, ty + sz / 2, sz / 5, 2, acc);
    ui_gfx_circle_fill(tx + sz / 2, ty + sz / 2, 2, acc);
}

void ui_coverflow_draw_covers_cpu(void) {
    const PlayerTheme *th = theme_active();
    int i;
    int drawn = 0;

    if (!g_cf_active || g_cf_count <= 0) {
        return;
    }
    cf_trace_step("cpu_draw");

    for (i = 0; i < g_cf_count && drawn < 10; i++) {
        float pos;
        int cx;
        int cy;
        int sz;
        int tilt;
        const UiCover *cover;

        pos = cf_rel_pos(i);
        if (pos <= -8.0f || pos >= 8.0f) {
            continue;
        }

        cx = 240 + (int)(pos * 36.0f);
        sz = 118 - (int)(fabsf(pos) * 9.0f);
        if (sz < 48) {
            sz = 48;
        }
        cy = 92 - (int)(fabsf(pos) * 3.0f);
        tilt = (int)(pos * 10.0f);

        cover = cf_cover_at(i);
        if (cover && cover->ready) {
            ui_image_draw_cover_trapezoid(
                cover,
                cy,
                sz,
                cx - sz / 2 + tilt,
                cx + sz / 2 + tilt,
                cx - sz / 2 - tilt / 2,
                cx + sz / 2 - tilt / 2,
                255
            );

            if (!ppsspp_qa_demo_active()) {
                int refl_h = sz / 3;
                ui_image_draw_cover_trapezoid(
                    cover,
                    cy + sz + 8,
                    refl_h,
                    cx - sz / 2 + tilt,
                    cx + sz / 2 + tilt,
                    cx - sz / 2 - tilt,
                    cx + sz / 2 - tilt,
                    70
                );
            }
        } else {
            cf_draw_cpu_placeholder(cx, cy, sz, tilt, th);
        }
        drawn++;
    }
    cf_trace_val("cpu_drawn", drawn);
}

void ui_coverflow_on_present_done(void) {
    cf_trace_flush_file();
}

static u32 cf_sample_cover(const UiCover *cover, int x, int y) {
    int sw = cover->width > 0 ? cover->width : UI_COVER_SIZE;
    int sh = cover->height > 0 ? cover->height : UI_COVER_SIZE;
    int sx = x * sw / UI_COVERFLOW_TEX;
    int sy = y * sh / UI_COVERFLOW_TEX;

    if (sx < 0) {
        sx = 0;
    }
    if (sy < 0) {
        sy = 0;
    }
    if (sx >= sw) {
        sx = sw - 1;
    }
    if (sy >= sh) {
        sy = sh - 1;
    }
    return cover->pixels[sy * UI_COVER_SIZE + sx];
}

static void cf_downsample_main(const UiCover *cover, u32 *out) {
    int y;
    int x;

    for (y = 0; y < UI_COVERFLOW_TEX; y++) {
        for (x = 0; x < UI_COVERFLOW_TEX; x++) {
            out[y * UI_COVERFLOW_TEX + x] = cf_sample_cover(cover, x, y);
        }
    }
}

static void cf_make_reflection(const u32 *main_px, u32 *refl_px) {
    int y;
    int x;

    memcpy(refl_px, main_px, (size_t)UI_COVERFLOW_TEX * UI_COVERFLOW_TEX * 4u);
    for (y = UI_COVERFLOW_TEX / 2; y < UI_COVERFLOW_TEX; y++) {
        int fade = 255 - ((y - UI_COVERFLOW_TEX / 2) * 255 / (UI_COVERFLOW_TEX / 2));
        for (x = 0; x < UI_COVERFLOW_TEX; x++) {
            u32 p = refl_px[y * UI_COVERFLOW_TEX + x];
            int a = ((p >> 24) & 0xFF) * fade / 255;
            refl_px[y * UI_COVERFLOW_TEX + x] = (p & 0x00FFFFFFu) | ((u32)a << 24);
        }
    }
}

static int cf_upload_rgba(UiGpuTex *tex, const u32 *pixels) {
    if (!tex->data) {
        tex->data = memalign(16, (size_t)UI_COVERFLOW_TEX * UI_COVERFLOW_TEX * 4u);
        if (!tex->data) {
            return -1;
        }
        tex->width = UI_COVERFLOW_TEX;
        tex->height = UI_COVERFLOW_TEX;
        tex->stride = UI_COVERFLOW_TEX;
    }
    memcpy(tex->data, pixels, (size_t)UI_COVERFLOW_TEX * UI_COVERFLOW_TEX * 4u);
    sceKernelDcacheWritebackInvalidateRange(
        tex->data,
        (unsigned)(UI_COVERFLOW_TEX * UI_COVERFLOW_TEX * 4)
    );
    tex->content_w = UI_COVERFLOW_TEX;
    tex->content_h = UI_COVERFLOW_TEX;
    tex->ready = 1;
    return 0;
}

static CfCoverSlot *cf_tex_slot_for(int track_id) {
    int i;
    int oldest = 0;
    unsigned oldest_tick = 0xFFFFFFFFu;

    if (track_id == 0) {
        return NULL;
    }
    for (i = 0; i < UI_COVERFLOW_TEX_CACHE; i++) {
        if (g_cf_tex[i].track_id == track_id && g_cf_tex[i].main.ready) {
            g_cf_tex[i].tick = ++g_cf_tick;
            return &g_cf_tex[i];
        }
    }
    for (i = 0; i < UI_COVERFLOW_TEX_CACHE; i++) {
        if (g_cf_tex[i].track_id <= 0 || !g_cf_tex[i].main.ready) {
            memset(&g_cf_tex[i], 0, sizeof(g_cf_tex[i]));
            g_cf_tex[i].track_id = track_id;
            g_cf_tex[i].tick = ++g_cf_tick;
            return &g_cf_tex[i];
        }
        if (g_cf_tex[i].tick < oldest_tick) {
            oldest_tick = g_cf_tex[i].tick;
            oldest = i;
        }
    }
    ui_gpu_free_tex(&g_cf_tex[oldest].main);
    ui_gpu_free_tex(&g_cf_tex[oldest].reflection);
    memset(&g_cf_tex[oldest], 0, sizeof(g_cf_tex[oldest]));
    g_cf_tex[oldest].track_id = track_id;
    g_cf_tex[oldest].tick = ++g_cf_tick;
    return &g_cf_tex[oldest];
}

static void cf_init_fallback(void) {
    const UiGpuTex *atlas;
    u32 main_px[UI_COVERFLOW_TEX * UI_COVERFLOW_TEX];
    u32 refl_px[UI_COVERFLOW_TEX * UI_COVERFLOW_TEX];
    int y;
    int x;

    if (g_cf_fallback.ready) {
        return;
    }
    atlas = ui_gpu_atlas();
    if (!atlas || !atlas->ready || !atlas->data) {
        return;
    }
    memset(main_px, 0, sizeof(main_px));
    for (y = 0; y < UI_COVERFLOW_TEX; y++) {
        for (x = 0; x < UI_COVERFLOW_TEX; x++) {
            int sx = x * 96 / UI_COVERFLOW_TEX;
            int sy = y * 96 / UI_COVERFLOW_TEX;
            main_px[y * UI_COVERFLOW_TEX + x] = ((const u32 *)atlas->data)[sy * atlas->stride + sx];
        }
    }
    cf_make_reflection(main_px, refl_px);
    cf_upload_rgba(&g_cf_fallback, main_px);
    cf_upload_rgba(&g_cf_fallback_refl, refl_px);
}

static const UiGpuTex *cf_cover_pair(const UiCover *cover, const UiGpuTex **reflection_out) {
    u32 main_px[UI_COVERFLOW_TEX * UI_COVERFLOW_TEX];
    u32 refl_px[UI_COVERFLOW_TEX * UI_COVERFLOW_TEX];
    CfCoverSlot *slot;

    if (reflection_out) {
        *reflection_out = NULL;
    }
    if (!cover || !cover->ready) {
        cf_init_fallback();
        if (reflection_out && g_cf_fallback_refl.ready) {
            *reflection_out = &g_cf_fallback_refl;
        }
        return g_cf_fallback.ready ? &g_cf_fallback : NULL;
    }

    slot = cf_tex_slot_for(cover->track_id);
    if (!slot) {
        cf_init_fallback();
        if (reflection_out && g_cf_fallback_refl.ready) {
            *reflection_out = &g_cf_fallback_refl;
        }
        return g_cf_fallback.ready ? &g_cf_fallback : NULL;
    }
    if (!slot->main.ready) {
        cf_downsample_main(cover, main_px);
        cf_make_reflection(main_px, refl_px);
        if (cf_upload_rgba(&slot->main, main_px) < 0 ||
            cf_upload_rgba(&slot->reflection, refl_px) < 0) {
            cf_init_fallback();
            if (reflection_out && g_cf_fallback_refl.ready) {
                *reflection_out = &g_cf_fallback_refl;
            }
            return g_cf_fallback.ready ? &g_cf_fallback : NULL;
        }
    }
    if (reflection_out) {
        *reflection_out = slot->reflection.ready ? &slot->reflection : NULL;
    }
    return slot->main.ready ? &slot->main : NULL;
}

static void cf_quad_verts(CfVertex *v, float dis, int reflection) {
    unsigned col = reflection ? 0x80FFFFFFu : 0xFFFFFFFFu;
    const float u0 = 0.0f;
    const float u1 = 1.0f;
    const float um = 0.5f;
    const float v0 = 0.0f;
    const float v1 = 1.0f;
    const float vm = 0.5f;

    if (reflection) {
        v[0].u = u0;
        v[0].v = vm;
        v[0].color = col;
        v[0].x = 0.5f;
        v[0].y = 0.0f;
        v[0].z = dis;
        v[1].u = u0;
        v[1].v = v1;
        v[1].color = col;
        v[1].x = 0.5f;
        v[1].y = -0.5f;
        v[1].z = dis;
        v[2].u = um;
        v[2].v = vm;
        v[2].color = col;
        v[2].x = 0.0f;
        v[2].y = 0.0f;
        v[2].z = dis;

        v[3].u = u0;
        v[3].v = v1;
        v[3].color = col;
        v[3].x = 0.5f;
        v[3].y = -0.5f;
        v[3].z = dis;
        v[4].u = u1;
        v[4].v = v1;
        v[4].color = col;
        v[4].x = -0.5f;
        v[4].y = -0.5f;
        v[4].z = dis;
        v[5].u = um;
        v[5].v = vm;
        v[5].color = col;
        v[5].x = 0.0f;
        v[5].y = 0.0f;
        v[5].z = dis;
        return;
    }

    v[0].u = u0;
    v[0].v = v0;
    v[0].color = col;
    v[0].x = -0.5f;
    v[0].y = 0.5f;
    v[0].z = dis;
    v[1].u = u1;
    v[1].v = v0;
    v[1].color = col;
    v[1].x = 0.5f;
    v[1].y = 0.5f;
    v[1].z = dis;
    v[2].u = um;
    v[2].v = vm;
    v[2].color = col;
    v[2].x = 0.0f;
    v[2].y = 0.0f;
    v[2].z = dis;

    v[3].u = u1;
    v[3].v = v0;
    v[3].color = col;
    v[3].x = 0.5f;
    v[3].y = 0.5f;
    v[3].z = dis;
    v[4].u = u1;
    v[4].v = v1;
    v[4].color = col;
    v[4].x = 0.5f;
    v[4].y = -0.5f;
    v[4].z = dis;
    v[5].u = um;
    v[5].v = vm;
    v[5].color = col;
    v[5].x = 0.0f;
    v[5].y = 0.0f;
    v[5].z = dis;

    v[6].u = u1;
    v[6].v = v1;
    v[6].color = col;
    v[6].x = 0.5f;
    v[6].y = -0.5f;
    v[6].z = dis;
    v[7].u = u0;
    v[7].v = v1;
    v[7].color = col;
    v[7].x = -0.5f;
    v[7].y = -0.5f;
    v[7].z = dis;
    v[8].u = um;
    v[8].v = vm;
    v[8].color = col;
    v[8].x = 0.0f;
    v[8].y = 0.0f;
    v[8].z = dis;

    v[9].u = u0;
    v[9].v = v1;
    v[9].color = col;
    v[9].x = -0.5f;
    v[9].y = -0.5f;
    v[9].z = dis;
    v[10].u = u0;
    v[10].v = v0;
    v[10].color = col;
    v[10].x = -0.5f;
    v[10].y = 0.5f;
    v[10].z = dis;
    v[11].u = um;
    v[11].v = vm;
    v[11].color = col;
    v[11].x = 0.0f;
    v[11].y = 0.0f;
    v[11].z = dis;
}

static void cf_draw_quad_3d(
    const UiGpuTex *tex,
    float px,
    float py,
    float pz,
    float rot_y,
    float rot_z,
    int reflection
) {
    CfVertex *verts;
    int n;
    ScePspFVector3 t;
    ScePspFVector3 r;

    if (!tex || !tex->ready || !tex->data) {
        return;
    }

    sceGuTexImage(0, tex->width, tex->height, tex->stride, tex->data);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuTexSync();

    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    t.x = px;
    t.y = py;
    t.z = pz;
    r.x = 0.0f;
    r.y = rot_y;
    r.z = rot_z;
    sceGumTranslate(&t);
    sceGumRotateXYZ(&r);

    verts = (CfVertex *)sceGuGetMemory(12 * sizeof(CfVertex));
    if (!verts) {
        g_cf_3d_failed = 1;
        cf_trace_step("gu_nomem");
        return;
    }
    cf_quad_verts(verts, CF_QUAD_Z, reflection);
    n = reflection ? 6 : 12;

    sceGumDrawArray(
        GU_TRIANGLES,
        GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
        n,
        0,
        verts
    );
}

/* Mirrors Coverflow v2.5 start_gu() — projection/view only; color buffer kept. */
static void cf_gu_begin(void) {
    sceGumMatrixMode(GU_TEXTURE);
    sceGumLoadIdentity();
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);

    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumPerspective(75.0f, 16.0f / 9.0f, 0.5f, 1000.0f);
    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    sceGumMatrixMode(GU_MODEL);
}

static void cf_gu_end(void) {
    sceGuDisable(GU_DEPTH_TEST);
}

void ui_coverflow_reset(void) {
    g_cf_active = 0;
    g_cf_count = 0;
    g_cf_cursor = 0;
    g_cf_ids = NULL;
    g_cf_hero = NULL;
}

void ui_coverflow_prepare_hero(const UiCover *hero_cover) {
    g_cf_active = 1;
    g_cf_count = 1;
    g_cf_cursor = 0;
    g_cf_ids = NULL;
    g_cf_hero = hero_cover;
    g_cf_scroll = 0.0f;
    g_cf_target = 0.0f;
}

void ui_coverflow_set_center(const UiCover *hero_cover) {
    g_cf_hero = hero_cover;
}

void ui_coverflow_prepare(int cursor, int count, const int *track_ids) {
    if (count <= 0) {
        ui_coverflow_reset();
        return;
    }
    g_cf_active = 1;
    g_cf_count = count;
    g_cf_cursor = cursor;
    g_cf_ids = track_ids;
    g_cf_hero = NULL;
    g_cf_target = (float)cursor * UI_COVERFLOW_DISTANCE;
    if (g_cf_scroll < 0.0f || g_cf_scroll > (float)(count - 1) * UI_COVERFLOW_DISTANCE) {
        g_cf_scroll = g_cf_target;
    }
}

int ui_coverflow_is_active(void) {
    return g_cf_active && g_cf_count > 0;
}

void ui_coverflow_animate(void) {
    if (!g_cf_active || g_cf_count <= 0) {
        return;
    }
    g_cf_target = (float)g_cf_cursor * UI_COVERFLOW_DISTANCE;
    if (g_cf_scroll < g_cf_target) {
        g_cf_scroll += 0.35f;
        if (g_cf_scroll > g_cf_target) {
            g_cf_scroll = g_cf_target;
        }
    } else if (g_cf_scroll > g_cf_target) {
        g_cf_scroll -= 0.35f;
        if (g_cf_scroll < g_cf_target) {
            g_cf_scroll = g_cf_target;
        }
    }
}

void ui_coverflow_draw_stage(const PlayerTheme *th, const char *title, int playing, int footer_h) {
    int floor_y = 196 - (footer_h ? 4 : 0);
    u32 top = th ? th->header : UI_RGB(40, 20, 90);
    u32 bot = th ? th->bg : UI_RGB(10, 16, 48);
    u32 floor_l = th ? th->accent : UI_RGB(255, 90, 180);
    u32 floor_r = th ? th->seek : UI_RGB(80, 210, 255);

    cf_trace_val("cf_stage", g_cf_count);

    ui_gfx_grad_v(0, 0, CF_SCR_W, CF_SCR_H, top, bot);
    ui_gfx_grad_h(0, floor_y - 10, CF_SCR_W, 18, floor_l, floor_r);
    ui_gfx_grad_v(0, floor_y - 28, CF_SCR_W, 40, bot, th ? th->chrome_lo : UI_RGB(20, 30, 70));
    ui_gfx_fill_alpha(0, floor_y, CF_SCR_W, 8, th ? th->chrome_hi : UI_RGB(255, 220, 120), 90);
    ui_gfx_fill_alpha(0, floor_y + 6, CF_SCR_W, 28, th ? th->chrome_lo : UI_RGB(30, 40, 90), 120);
    ui_gfx_hline(0, floor_y + 1, CF_SCR_W, th ? th->seek : UI_RGB(120, 240, 255));

    ui_gfx_fill_alpha(0, 0, CF_SCR_W, 28, top, 210);
    if (title && title[0]) {
        ui_font_text_clip(16, 8, 360, th ? th->text : UI_RGB(255, 255, 255), title, UI_FONT_MD);
    }
    if (ppsspp_qa_demo_active()) {
        ui_font_text(16, 22, th ? th->muted : UI_RGB(160, 160, 160), "Cover Flow", UI_FONT_SM);
        ui_font_text(438, 10, th ? th->text : UI_RGB(255, 255, 255), "12:34", UI_FONT_SM);
    }
    if (playing) {
        ui_font_text(400, 10, th ? th->accent : UI_RGB(255, 200, 80), "PLAY", UI_FONT_SM);
    }
}

void ui_coverflow_draw_label(const PlayerTheme *th, const char *name, int footer_h) {
    int y = 204 - (footer_h ? 4 : 0);
    int lane_h = ppsspp_qa_demo_active() ? 42 : 24;
    const char *track;
    const char *artist;
    const char *album;

    /* Protect readability: keep a dedicated text lane over the stage. */
    ui_gfx_fill_alpha(0, y - 3, CF_SCR_W, lane_h, th->bg, 210);
    ui_gfx_hline(0, y - 3, CF_SCR_W, th->chrome_hi);

    if (ppsspp_qa_demo_active()) {
        track = ppsspp_qa_demo_cf_track();
        artist = ppsspp_qa_demo_cf_artist();
        album = ppsspp_qa_demo_cf_album();
        ui_font_text_clip(24, y, 432, th->text, track, UI_FONT_MD);
        ui_font_text_clip(24, y + 20, 220, th->muted, artist, UI_FONT_SM);
        ui_font_text_clip(24, y + 36, 220, th->muted, album, UI_FONT_SM);
        ui_font_text(24, 252, th->muted, "L/R Albums", UI_FONT_SM);
        ui_font_text(320, 252, th->muted, "X Open   O Back", UI_FONT_SM);
        return;
    }

    ui_font_text_clip(24, y, 432, th->text, name && name[0] ? name : "-", UI_FONT_MD);
    ui_font_text(24, y + 18, th->muted, "Analog/D-Pad L/R  X open  O back", UI_FONT_SM);
}

void ui_coverflow_render_gu(int vram_page, int buf_stride) {
    int i;
    int drawn = 0;
    void *draw_ofs;

    if (!g_cf_active || g_cf_count <= 0) {
        return;
    }

    cf_trace_val("gu_render", vram_page);
    cf_init_fallback();

    draw_ofs = (void *)(vram_page ? CF_FRAME_BYTES : 0);

    sceGuStart(GU_DIRECT, g_cf_list);
    sceGuDrawBuffer(GU_PSM_8888, draw_ofs, buf_stride);
    sceGuDispBuffer(
        CF_SCR_W,
        CF_SCR_H,
        (void *)(vram_page ? 0 : CF_FRAME_BYTES),
        buf_stride
    );
    sceGuDepthBuffer((void *)(CF_FRAME_BYTES * 2), buf_stride);
    sceGuOffset(2048 - (CF_SCR_W / 2), 2048 - (CF_SCR_H / 2));
    sceGuViewport(2048, 2048, CF_SCR_W, CF_SCR_H);
    sceGuDepthRange(0xc350, 0x2710);
    /* Keep 3D cards inside stage: never overlap header/label/footer text. */
    sceGuScissor(0, 30, CF_SCR_W, 182);
    sceGuEnable(GU_SCISSOR_TEST);

    cf_gu_begin();

    for (i = 0; i < g_cf_count && drawn < 12; i++) {
        float pos;
        float rot;
        float z;
        const UiCover *cover;
        const UiGpuTex *tex;
        const UiGpuTex *refl;

        pos = cf_rel_pos(i);
        if (pos <= -8.0f || pos >= 8.0f) {
            continue;
        }

        cover = cf_cover_at(i);
        tex = cf_cover_pair(cover, &refl);
        if (!tex) {
            continue;
        }

        rot = -pos / 4.2f;
        z = UI_COVERFLOW_ZAXIS - fabsf(pos) / 18.0f;

        cf_draw_quad_3d(tex, pos, 0.44f, z, rot, 0.0f, 0);
        cf_draw_quad_3d(refl ? refl : tex, pos, -0.95f, z, rot, 180.0f, 1);
        drawn++;
        if (g_cf_3d_failed) {
            break;
        }
    }

    cf_gu_end();
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    cf_trace_val("gu_drawn", drawn);
}
