#include "ui_gpu.h"

#include <pspdisplay.h>
#include <pspge.h>
#include <pspgu.h>
#include <pspkernel.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

/* Embedded raw RGBA via psp-objcopy -I binary. */
extern unsigned char _binary_assets_ui_atlas_rgba_start[];
extern unsigned char _binary_assets_ui_font_rgba_start[];
extern unsigned char _binary_assets_ui_labels_rgba_start[];

#define BUF_W 512
#define SCR_W 480
#define SCR_H 272

static unsigned int __attribute__((aligned(16))) g_list[262144];
static UiGpuTex g_atlas;
static UiGpuTex g_font;
static UiGpuTex g_labels;
static int g_ready;
static int g_list_started;

typedef struct {
    int sx, sy, sw, sh;
} AtlasRect;

static const AtlasRect ATLAS_UV[UI_ATLAS_COUNT] = {
    {0, 0, 96, 96},
    {96, 0, 128, 64},
    {0, 96, 32, 32},
    {32, 96, 32, 32},
    {64, 96, 32, 32},
    {96, 96, 32, 32},
    {128, 96, 32, 32},
    {0, 160, 256, 32},
    {0, 192, 64, 24},
    {160, 192, 64, 64},
};

/* labels.rgba layout from tools/gen_ui_assets.py (large chrome strings) */
static const AtlasRect LABEL_UV[UI_LABEL_COUNT] = {
    {120, 0, 150, 32},   /* Now Playing */
    {280, 0, 70, 32},    /* Paused */
    {360, 0, 50, 24},    /* SHUF */
    {420, 0, 40, 24},    /* RPT */
    {0, 0, 110, 36},     /* Music */
    {0, 40, 100, 28},    /* Up Next */
    {120, 40, 90, 24},   /* Loading */
    {220, 40, 150, 28},  /* Now Playing green */
    {400, 40, 70, 24},   /* Saved */
};

/* Per-glyph advance in atlas cell pixels (ink width + pad), filled at init. */
static unsigned char g_font_adv[96];

static void measure_font_advances(void) {
    const u32 *src;
    int idx;
    if (!g_font.ready || !g_font.data) {
        return;
    }
    src = (const u32 *)g_font.data;
    for (idx = 0; idx < 96; idx++) {
        int col = idx % UI_GPU_FONT_COLS;
        int row = idx / UI_GPU_FONT_COLS;
        int sx0 = col * UI_GPU_FONT_CELL;
        int sy0 = row * UI_GPU_FONT_CELL;
        int minx = UI_GPU_FONT_CELL, maxx = -1;
        int y, x;
        for (y = 0; y < UI_GPU_FONT_CELL; y++) {
            for (x = 0; x < UI_GPU_FONT_CELL; x++) {
                u32 sp = src[(sy0 + y) * g_font.stride + (sx0 + x)];
                if (((sp >> 24) & 0xFF) > 24) {
                    if (x < minx) minx = x;
                    if (x > maxx) maxx = x;
                }
            }
        }
        if (maxx < minx) {
            g_font_adv[idx] = (unsigned char)(UI_GPU_FONT_CELL / 3);
        } else {
            int w = maxx - minx + 3;
            if (w < 4) w = 4;
            if (w > UI_GPU_FONT_CELL - 1) w = UI_GPU_FONT_CELL - 1;
            g_font_adv[idx] = (unsigned char)w;
        }
    }
    g_font_adv[0] = 10; /* space */
}

static int pot_ge(int v) {
    int p = 1;
    while (p < v) {
        p <<= 1;
    }
    return p;
}

static void *aligned_rgba(int w, int h) {
    return memalign(16, (size_t)w * (size_t)h * 4u);
}

static void upload_static(UiGpuTex *tex, const unsigned char *src, int w, int h) {
    int tw = pot_ge(w);
    int th = pot_ge(h);
    int y;
    u32 *dst;

    memset(tex, 0, sizeof(*tex));
    tex->data = aligned_rgba(tw, th);
    if (!tex->data) {
        return;
    }
    memset(tex->data, 0, (size_t)tw * (size_t)th * 4u);
    dst = (u32 *)tex->data;
    for (y = 0; y < h; y++) {
        memcpy(dst + y * tw, src + y * w * 4, (size_t)w * 4u);
    }
    sceKernelDcacheWritebackInvalidateRange(tex->data, (unsigned)(tw * th * 4));
    tex->width = tw;
    tex->height = th;
    tex->stride = tw;
    tex->ready = 1;
}

void ui_gpu_init(void) {
    if (g_ready) {
        return;
    }
    upload_static(&g_atlas, _binary_assets_ui_atlas_rgba_start, UI_GPU_ATLAS_W, UI_GPU_ATLAS_H);
    upload_static(&g_font, _binary_assets_ui_font_rgba_start, UI_GPU_FONT_W, UI_GPU_FONT_H);
    upload_static(&g_labels, _binary_assets_ui_labels_rgba_start, UI_GPU_LABELS_W, UI_GPU_LABELS_H);
    measure_font_advances();

    sceGuInit();
    sceGuStart(GU_DIRECT, g_list);
    sceGuDrawBuffer(GU_PSM_8888, (void *)0, BUF_W);
    sceGuDispBuffer(SCR_W, SCR_H, (void *)0x88000, BUF_W);
    sceGuOffset(2048 - (SCR_W / 2), 2048 - (SCR_H / 2));
    sceGuViewport(2048, 2048, SCR_W, SCR_H);
    sceGuScissor(0, 0, SCR_W, SCR_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
    g_ready = 1;
}

void ui_gpu_shutdown(void) {
    if (g_atlas.data) {
        free(g_atlas.data);
    }
    if (g_font.data) {
        free(g_font.data);
    }
    if (g_labels.data) {
        free(g_labels.data);
    }
    memset(&g_atlas, 0, sizeof(g_atlas));
    memset(&g_font, 0, sizeof(g_font));
    memset(&g_labels, 0, sizeof(g_labels));
    if (g_ready) {
        sceGuTerm();
    }
    g_ready = 0;
}

void ui_gpu_reset(void) {
    if (!g_ready) {
        ui_gpu_init();
        return;
    }
    sceGuInit();
    sceGuStart(GU_DIRECT, g_list);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuFinish();
    sceGuSync(0, 0);
}

UiGpuTex *ui_gpu_atlas(void) {
    return &g_atlas;
}

UiGpuTex *ui_gpu_font(void) {
    return &g_font;
}

UiGpuTex *ui_gpu_labels(void) {
    return &g_labels;
}

int ui_gpu_upload_cover(UiGpuTex *tex, const u32 *pixels_96, int track_id) {
    int tw = 128;
    int th = 128;
    int cover_n = 128;
    int y;
    u32 *dst;
    (void)track_id;

    if (!tex || !pixels_96) {
        return -1;
    }
    if (!tex->data) {
        tex->data = aligned_rgba(tw, th);
        if (!tex->data) {
            return -1;
        }
        tex->width = tw;
        tex->height = th;
        tex->stride = tw;
    }
    dst = (u32 *)tex->data;
    memset(dst, 0, (size_t)tw * (size_t)th * 4u);
    for (y = 0; y < cover_n && y < th; y++) {
        memcpy(dst + y * tw, pixels_96 + y * cover_n, (size_t)cover_n * 4u);
    }
    sceKernelDcacheWritebackInvalidateRange(tex->data, (unsigned)(tw * th * 4));
    tex->ready = 1;
    return 0;
}

void ui_gpu_free_tex(UiGpuTex *tex) {
    if (!tex) {
        return;
    }
    if (tex->data) {
        free(tex->data);
    }
    memset(tex, 0, sizeof(*tex));
}

typedef struct {
    float u, v;
    unsigned int color;
    float x, y, z;
} TexVertex;

static void draw_tex_quad(
    const UiGpuTex *tex,
    float su,
    float sv,
    float eu,
    float ev,
    float x,
    float y,
    float w,
    float h,
    u32 color
) {
    TexVertex *v;

    if (!tex || !tex->ready || !tex->data || w <= 0 || h <= 0) {
        return;
    }
    sceGuTexImage(0, tex->width, tex->height, tex->stride, tex->data);
    v = (TexVertex *)sceGuGetMemory(2 * sizeof(TexVertex));
    v[0].u = su;
    v[0].v = sv;
    v[0].color = color;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0;
    v[1].u = eu;
    v[1].v = ev;
    v[1].color = color;
    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0;
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, v);
}

void ui_gpu_present(
    u32 *soft_buf,
    int buf_stride,
    u32 *vram_dst,
    const UiGpuTex *cover,
    int cover_x,
    int cover_y,
    int cover_w,
    int cover_h
) {
    void *dst;

    if (!soft_buf || !vram_dst) {
        return;
    }
    if (!g_ready) {
        ui_gpu_init();
    }

    sceKernelDcacheWritebackInvalidateRange(soft_buf, (unsigned)(buf_stride * SCR_H * 4));

    sceGuStart(GU_DIRECT, g_list);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuScissor(0, 0, SCR_W, SCR_H);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);

    /* VRAM destination is uncached GE pointer; strip uncached bit for CopyImage dest. */
    dst = (void *)(((u32)vram_dst) & ~0x40000000u);
    sceGuCopyImage(GU_PSM_8888, 0, 0, SCR_W, SCR_H, buf_stride, soft_buf, 0, 0, BUF_W, dst);
    sceGuTexSync();

    if (cover && cover->ready && cover_w > 0 && cover_h > 0) {
        /* NEAREST keeps album art sharp when upscaling 128→~152; LINEAR looked mushy. */
        sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
        sceGuTexFilter(GU_NEAREST, GU_NEAREST);
        draw_tex_quad(
            cover,
            0,
            0,
            128.0f,
            128.0f,
            (float)cover_x,
            (float)cover_y,
            (float)cover_w,
            (float)cover_h,
            0xFFFFFFFFu
        );
    }

    sceGuFinish();
    sceGuSync(0, 0);
    (void)g_list_started;
}

static u32 tint_pixel(u32 src, u32 tint) {
    int sr = (int)(src & 0xFF);
    int sg = (int)((src >> 8) & 0xFF);
    int sb = (int)((src >> 16) & 0xFF);
    int sa = (int)((src >> 24) & 0xFF);
    int tr = (int)(tint & 0xFF);
    int tg = (int)((tint >> 8) & 0xFF);
    int tb = (int)((tint >> 16) & 0xFF);
    int ta = (int)((tint >> 24) & 0xFF);
    int a = (sa * ta) / 255;
    if (a <= 0) {
        return 0;
    }
    return ((u32)a << 24) |
           ((u32)((sb * tb) / 255) << 16) |
           ((u32)((sg * tg) / 255) << 8) |
           (u32)((sr * tr) / 255);
}

void ui_gpu_blit_atlas_cpu(u32 *dst, int dst_stride, int region, int x, int y, int w, int h, u32 tint) {
    const AtlasRect *r;
    int dy;
    const u32 *src;

    if (!dst || !g_atlas.ready || region < 0 || region >= UI_ATLAS_COUNT || w <= 0 || h <= 0) {
        return;
    }
    r = &ATLAS_UV[region];
    src = (const u32 *)g_atlas.data;
    for (dy = 0; dy < h; dy++) {
        int dx;
        int sy = r->sy + (dy * r->sh) / h;
        for (dx = 0; dx < w; dx++) {
            int sx = r->sx + (dx * r->sw) / w;
            int px = x + dx;
            int py = y + dy;
            u32 sp;
            u32 dp;
            int sa;
            if (px < 0 || py < 0 || px >= SCR_W || py >= SCR_H) {
                continue;
            }
            sp = tint_pixel(src[sy * g_atlas.stride + sx], tint);
            sa = (int)((sp >> 24) & 0xFF);
            if (sa <= 0) {
                continue;
            }
            if (sa >= 250) {
                dst[py * dst_stride + px] = 0xFF000000u | (sp & 0x00FFFFFFu);
                continue;
            }
            dp = dst[py * dst_stride + px];
            {
                int sr = (int)(sp & 0xFF), sg = (int)((sp >> 8) & 0xFF), sb = (int)((sp >> 16) & 0xFF);
                int dr = (int)(dp & 0xFF), dg = (int)((dp >> 8) & 0xFF), db = (int)((dp >> 16) & 0xFF);
                dr = dr + ((sr - dr) * sa) / 255;
                dg = dg + ((sg - dg) * sa) / 255;
                db = db + ((sb - db) * sa) / 255;
                dst[py * dst_stride + px] = 0xFF000000u | ((u32)db << 16) | ((u32)dg << 8) | (u32)dr;
            }
        }
    }
}

static int font_sample_a_nn(const u32 *src, int stride, int sx0, int sy0, int fx, int fy) {
    /* Nearest sample — bilinear made every glyph look mushy at 480x272. */
    int x, y;
    if (fx < 0) {
        fx = 0;
    }
    if (fy < 0) {
        fy = 0;
    }
    x = (fx + 128) >> 8;
    y = (fy + 128) >> 8;
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (x >= UI_GPU_FONT_CELL) {
        x = UI_GPU_FONT_CELL - 1;
    }
    if (y >= UI_GPU_FONT_CELL) {
        y = UI_GPU_FONT_CELL - 1;
    }
    return (int)((src[(sy0 + y) * stride + (sx0 + x)] >> 24) & 0xFF);
}

void ui_gpu_blit_font_cpu(u32 *dst, int dst_stride, int x, int y, u32 color, const char *s, int size) {
    int cx = x;
    int gh = 12;
    int cell = UI_GPU_FONT_CELL;
    const u32 *src;
    int tracking = 1;
    int sr = (int)(color & 0xFF), sg = (int)((color >> 8) & 0xFF), sb = (int)((color >> 16) & 0xFF);
    u32 solid = 0xFF000000u | ((u32)sb << 16) | ((u32)sg << 8) | (u32)sr;

    if (!dst || !g_font.ready || !s) {
        return;
    }
    /* Integer-friendly heights vs 32px cells: 24=3/4, 16=1/2, 12=3/8. */
    if (size >= 2) {
        gh = 24;
        tracking = 1;
    } else if (size == 1) {
        gh = 16;
        tracking = 1;
    } else {
        gh = 12;
        tracking = 0;
    }
    src = (const u32 *)g_font.data;
    while (*s) {
        unsigned char ch = (unsigned char)*s++;
        int idx;
        int col;
        int row;
        int sx0;
        int sy0;
        int dy;
        int adv_src;
        int adv;
        int gw;
        if (ch < 32 || ch > 127) {
            ch = ' ';
        }
        idx = (int)ch - 32;
        col = idx % UI_GPU_FONT_COLS;
        row = idx / UI_GPU_FONT_COLS;
        sx0 = col * cell;
        sy0 = row * cell;
        adv_src = g_font_adv[idx] ? (int)g_font_adv[idx] : (cell * 2) / 3;
        /* Scale by height; sample only the left ink span of the cell. */
        gw = (adv_src * gh + cell / 2) / cell;
        if (gw < 3) {
            gw = 3;
        }
        adv = gw + tracking;
        for (dy = 0; dy < gh; dy++) {
            int dx;
            int fy = ((dy * 2 + 1) * cell * 128) / gh;
            for (dx = 0; dx < gw; dx++) {
                int fx = (gw > 1) ? ((dx * 2 + 1) * adv_src * 128) / gw : 0;
                int px = cx + dx;
                int py = y + dy;
                int sa;
                if (px < 0 || py < 0 || px >= SCR_W || py >= SCR_H) {
                    continue;
                }
                sa = font_sample_a_nn(src, g_font.stride, sx0, sy0, fx, fy);
                /* Controlled AA: solid core + single soft fringe (premium, not mushy). */
                if (sa < 90) {
                    continue;
                }
                if (sa >= 200) {
                    dst[py * dst_stride + px] = solid;
                } else {
                    u32 dp = dst[py * dst_stride + px];
                    int dr = (int)(dp & 0xFF), dg = (int)((dp >> 8) & 0xFF), db = (int)((dp >> 16) & 0xFF);
                    int a = 90 + ((sa - 90) * 165) / 110;
                    dr = dr + ((sr - dr) * a) / 255;
                    dg = dg + ((sg - dg) * a) / 255;
                    db = db + ((sb - db) * a) / 255;
                    dst[py * dst_stride + px] = 0xFF000000u | ((u32)db << 16) | ((u32)dg << 8) | (u32)dr;
                }
            }
        }
        cx += adv;
    }
}

void ui_gpu_blit_label_cpu(u32 *dst, int dst_stride, int label, int x, int y) {
    const AtlasRect *r;
    int dy;
    const u32 *src;

    if (!dst || !g_labels.ready || label < 0 || label >= UI_LABEL_COUNT) {
        return;
    }
    r = &LABEL_UV[label];
    src = (const u32 *)g_labels.data;
    for (dy = 0; dy < r->sh; dy++) {
        int dx;
        for (dx = 0; dx < r->sw; dx++) {
            int px = x + dx;
            int py = y + dy;
            u32 sp;
            int sa;
            if (px < 0 || py < 0 || px >= SCR_W || py >= SCR_H) {
                continue;
            }
            sp = src[(r->sy + dy) * g_labels.stride + (r->sx + dx)];
            sa = (int)((sp >> 24) & 0xFF);
            /* Alpha only — luminance fallback caused gray mush around glyphs. */
            if (sa < 16) {
                continue;
            }
            if (sa > 220) {
                sa = 255;
            }
            {
                u32 dp = dst[py * dst_stride + px];
                int sr = (int)(sp & 0xFF), sg = (int)((sp >> 8) & 0xFF), sb = (int)((sp >> 16) & 0xFF);
                int dr = (int)(dp & 0xFF), dg = (int)((dp >> 8) & 0xFF), db = (int)((dp >> 16) & 0xFF);
                dr = dr + ((sr - dr) * sa) / 255;
                dg = dg + ((sg - dg) * sa) / 255;
                db = db + ((sb - db) * sa) / 255;
                dst[py * dst_stride + px] = 0xFF000000u | ((u32)db << 16) | ((u32)dg << 8) | (u32)dr;
            }
        }
    }
}
