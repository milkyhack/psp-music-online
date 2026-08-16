#include "ui.h"
#include "ui_gfx.h"
#include "ui_font.h"
#include "ui_gpu.h"
#include "ui_image.h"
#include "theme.h"
#include "player.h"

#include <pspdisplay.h>
#include <pspge.h>
#include <pspkernel.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#define BUF_WIDTH 512
#define FRAME_SIZE (BUF_WIDTH * UI_SCREEN_H * 4)

static u32 *g_draw = NULL;
static u32 *g_vram[2] = {NULL, NULL};
static int g_vram_page = 0;
static int g_eq_levels[16];
static int g_eq_peaks[16];
static unsigned g_eq_tick;
static int g_eq_stepped;
#ifdef DEBUG_HUD
static char g_dbg1[96];
static char g_dbg2[96];
#endif
static int g_loading;

static void ui_font_text_marquee(int x, int y, int max_w, u32 color, const char *s, int size);
static int skin_is_neon(const PlayerTheme *th);

static const int EQ_FLAT[11] = {12,12,12,12,12,12,12,12,12,12,12};
static const int EQ_BASS[11] = {20,18,14,10,10,10,10,10,10,10,10};
static const int EQ_ROCK[11] = {18,16,10,8,12,14,16,18,18,16,14};
static const int EQ_POP[11] = {12,14,16,14,12,14,16,16,14,12,12};
static const int EQ_VOCAL[11] = {8,8,10,14,18,20,18,14,10,8,8};

static void clear_buffer(u32 *buf, u32 color) {
    int i;
    for (i = 0; i < BUF_WIDTH * UI_SCREEN_H; i++) {
        buf[i] = color;
    }
}

static void blit_to_vram(u32 *dst_uncached) {
    memcpy(dst_uncached, g_draw, FRAME_SIZE);
}

void ui_init(void) {
    int i;
    u8 *edram = (u8 *)sceGeEdramGetAddr();

    if (!g_draw) {
        g_draw = (u32 *)memalign(64, FRAME_SIZE);
    }
    if (!g_draw) {
        g_draw = (u32 *)edram;
    }

    g_vram[0] = (u32 *)(0x40000000u | (u32)edram);
    g_vram[1] = (u32 *)(0x40000000u | (u32)(edram + FRAME_SIZE));
    g_vram_page = 0;

    ui_gfx_bind(g_draw);
    ui_font_init();
    ui_gpu_init();

    sceDisplaySetMode(0, UI_SCREEN_W, UI_SCREEN_H);
    clear_buffer(g_draw, UI_COL_BG);
    blit_to_vram(g_vram[0]);
    for (i = 0; i < 16; i++) {
        g_eq_levels[i] = 4;
        g_eq_peaks[i] = 4;
    }
    sceDisplayWaitVblankStart();
    sceDisplaySetFrameBuf(g_vram[0], BUF_WIDTH, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_IMMEDIATE);
}

void ui_begin(void) {
    g_eq_stepped = 0;
    ui_gfx_bind(g_draw);
    /* Prevent stale GPU cover overlay from previous screen/frame. */
    ui_image_clear_present();
}

void ui_end(void) {
    int next = g_vram_page ^ 1;
    int cx, cy, cw, ch;
    const UiGpuTex *cover_tex;

    sceDisplayWaitVblankStart();
    ui_image_get_present_rect(&cx, &cy, &cw, &ch);
    cover_tex = (cw > 0 && ch > 0) ? ui_image_cover_gpu() : NULL;
    /* GPU path: DMA soft frame into VRAM + linear-filtered cover overlay. */
    ui_gpu_present(g_draw, BUF_WIDTH, g_vram[next], cover_tex, cx, cy, cw, ch);
    sceDisplaySetFrameBuf(g_vram[next], BUF_WIDTH, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
    g_vram_page = next;
}

void ui_clear(u32 color) {
    ui_gfx_clear(color);
}

void ui_fill(int x, int y, int w, int h, u32 color) {
    ui_gfx_fill(x, y, w, h, color);
}

void ui_text(int x, int y, u32 color, const char *s) {
    ui_font_text(x, y, color, s, UI_FONT_SM);
}

void ui_text_clip(int x, int y, int max_w, u32 color, const char *s) {
    ui_font_text_clip(x, y, max_w, color, s, UI_FONT_SM);
}

/* -------- EQ sim / viz helpers -------- */
static void ui_eq_step(int playing) {
    int i;
    if (g_eq_stepped) {
        return;
    }
    g_eq_stepped = 1;
    g_eq_tick++;
    for (i = 0; i < 16; i++) {
        int target;
        if (!playing) {
            target = 3;
        } else {
            unsigned seed = g_eq_tick * 1103515245u + 12345u + (unsigned)i * 9973u;
            int wave = (int)((seed >> 16) & 63);
            int bass = (i < 3) ? 14 : ((i > 12) ? 8 : 0);
            int pulse = ((int)(g_eq_tick + (unsigned)i * 3) & 15);
            if (pulse > 8) {
                pulse = 16 - pulse;
            }
            target = 8 + (wave % 40) + bass + pulse;
            if (target > 48) {
                target = 48;
            }
        }
        if (target > g_eq_levels[i]) {
            g_eq_levels[i] += (target - g_eq_levels[i] + 1) / 2;
        } else {
            g_eq_levels[i] -= (g_eq_levels[i] - target + 2) / 3;
        }
        if (g_eq_levels[i] < 2) {
            g_eq_levels[i] = 2;
        }
        if (g_eq_levels[i] > g_eq_peaks[i]) {
            g_eq_peaks[i] = g_eq_levels[i];
        } else if ((g_eq_tick & 2) == 0 && g_eq_peaks[i] > g_eq_levels[i]) {
            g_eq_peaks[i]--;
        }
    }
}

static void viz_soft_bars(int x, int y, int w, int h, int playing, const PlayerTheme *th, int rounded) {
    int i, n = 16;
    int bw = (w - 4) / n;
    ui_eq_step(playing);
    ui_gfx_inset(x, y, w, h, th->lcd, th->chrome_hi, th->chrome_lo, rounded ? 3 : 0);
    for (i = 0; i < n; i++) {
        int lv = g_eq_levels[i];
        int bh = 2 + (lv * (h - 8)) / 48;
        int peak = 2 + (g_eq_peaks[i] * (h - 8)) / 48;
        u32 col;
        int bx = x + 2 + i * bw;
        if (bh > (h * 4) / 5) {
            col = th->accent;
        } else if (bh > (h * 2) / 3) {
            col = th->motif;
        } else {
            col = th->lcd_fg;
        }
        if (rounded) {
            ui_gfx_round_fill(bx, y + h - 2 - bh, bw - 2, bh, 2, col);
        } else {
            ui_gfx_fill(bx, y + h - 2 - bh, bw - 2, bh, col);
        }
        if (playing) {
            ui_gfx_fill(bx, y + h - 2 - peak, bw - 2, 1, th->text);
        }
    }
}

static void viz_scope(int x, int y, int w, int h, int playing, const PlayerTheme *th) {
    int i;
    int mid = y + h / 2;
    ui_eq_step(playing);
    ui_gfx_inset(x, y, w, h, th->lcd, th->chrome_hi, th->chrome_lo, 3);
    ui_gfx_fill(x + 2, mid, w - 4, 1, th->lcd_dim);
    for (i = 0; i < w - 4; i++) {
        int idx = (i + (int)g_eq_tick) % 16;
        int amp = playing ? ((g_eq_levels[idx] * (h / 2 - 3)) / 48) : 2;
        int yy = mid - amp + ((i & 2) ? amp / 3 : 0);
        ui_gfx_fill(x + 2 + i, yy, 1, 2, th->lcd_fg);
    }
}

static void viz_led_bar(int x, int y, int w, int h, int playing, const PlayerTheme *th) {
    int i, rows = h / 5;
    int cols = w / 10;
    ui_eq_step(playing);
    ui_gfx_round_fill(x, y, w, h, 3, th->chrome_lo);
    for (i = 0; i < cols && i < 16; i++) {
        int lit = playing ? (g_eq_levels[i] * rows) / 48 : 1;
        int r;
        for (r = 0; r < rows; r++) {
            u32 c = th->bar_bg;
            if (r < lit) {
                c = (r > rows - 3) ? th->accent : ((r > rows / 2) ? th->seek : th->lcd_fg);
            }
            ui_gfx_round_fill(x + 2 + i * 10, y + h - 4 - r * 5, 8, 3, 1, c);
        }
    }
}

static void viz_vu(int x, int y, int w, int h, int level, const char *tag, const PlayerTheme *th) {
    int pivot_x = x + w / 2;
    int pivot_y = y + h - 5;
    int tip_x, tip_y, lv = level, i;
    if (lv < 0) {
        lv = 0;
    }
    if (lv > 48) {
        lv = 48;
    }
    ui_gfx_inset(x, y, w, h, th->lcd, th->chrome_hi, th->chrome_lo, 3);
    ui_gfx_fill(x + 2, y + 2, w - 4, h - 4, UI_RGB(18, 12, 8));
    for (i = 0; i < 5; i++) {
        int tx = x + 6 + (i * (w - 12)) / 4;
        ui_gfx_fill(tx, y + 4, 1, 4, th->lcd_dim);
    }
    tip_x = x + 4 + (lv * (w - 8)) / 48;
    tip_y = y + 8 + (((lv < 24) ? (24 - lv) : (lv - 24)) * (h / 3)) / 24;
    ui_gfx_line(pivot_x, pivot_y, tip_x, tip_y, th->lcd_fg);
    ui_gfx_circle_fill(pivot_x, pivot_y, 3, th->chrome_hi);
    if (tag) {
        ui_font_text(x + 4, y + h - 12, th->muted, tag, UI_FONT_SM);
    }
}

static void viz_draw(int x, int y, int w, int h, int playing, const PlayerTheme *th) {
    switch (th->viz) {
        case VIZ_SCOPE:
            viz_scope(x, y, w, h, playing, th);
            break;
        case VIZ_LED_BAR:
            viz_led_bar(x, y, w, h, playing, th);
            break;
        case VIZ_ANALOG_VU:
        case VIZ_CASSETTE_METERS: {
            int l = playing ? (10 + g_eq_levels[0] + g_eq_levels[1] / 2) : 2;
            int r = playing ? (10 + g_eq_levels[8] + g_eq_levels[9] / 2) : 2;
            int mw = (w - 4) / 2;
            ui_eq_step(playing);
            if (th->viz == VIZ_ANALOG_VU) {
                viz_vu(x, y, w, h, playing ? (20 + g_eq_levels[0]) : 4, "VU", th);
            } else {
                viz_vu(x, y, mw, h, l, "L", th);
                viz_vu(x + mw + 4, y, mw, h, r, "R", th);
            }
            break;
        }
        case VIZ_CRT_SCAN:
            viz_scope(x, y, w, h, playing, th);
            ui_gfx_fill_alpha(x, y + (int)(g_eq_tick % (unsigned)(h > 1 ? h : 1)), w, 1, th->motif, 80);
            break;
        case VIZ_CD_RING: {
            int cx = x + w / 2, cy = y + h / 2, r = (h < w ? h : w) / 2 - 2, i;
            ui_eq_step(playing);
            ui_gfx_inset(x, y, w, h, th->lcd, th->chrome_hi, th->chrome_lo, 4);
            for (i = 0; i < 6; i++) {
                int rr = 4 + (playing ? (g_eq_levels[i] * (r - 4)) / 48 : 2);
                ui_gfx_ring(cx, cy, rr, 2, (i & 1) ? th->motif : th->lcd_fg);
            }
            break;
        }
        case VIZ_MATRIX_RAIN: {
            int col, max_cols = 14, stride;
            static const char glyphs[] = "01ABCDEF";
            ui_eq_step(playing);
            ui_gfx_fill(x, y, w, h, th->lcd);
            stride = w / max_cols;
            if (stride < 12) {
                stride = 12;
                max_cols = w / stride;
            }
            for (col = 0; col < max_cols; col++) {
                int head = (int)((g_eq_tick * 2 + (unsigned)col * 19) % (unsigned)(h / 10 + 6));
                int row;
                for (row = 0; row < h / 10; row++) {
                    int dist = head - row;
                    u32 c = th->lcd_dim;
                    if (dist < 0 || dist > 3) {
                        continue;
                    }
                    if (dist == 0) {
                        c = th->text;
                    } else if (dist < 2) {
                        c = th->lcd_fg;
                    }
                    ui_font_glyph(x + col * stride, y + row * 10, c, glyphs[(col + row + (int)g_eq_tick) & 7], UI_FONT_SM);
                }
            }
            break;
        }
        case VIZ_CYBER_GRID:
        case VIZ_DC_ORANGE:
        case VIZ_ARCADE_NEON:
        case VIZ_XMB_WAVE:
        case VIZ_MD_WAVE:
        case VIZ_SOFT_SPEC:
        case VIZ_DOS_BARS:
        case VIZ_WINAMP_SPEC:
        default:
            viz_soft_bars(x, y, w, h, playing, th, 1);
            break;
    }
}

static void np_draw_progress(int x, int y, int w, int h, int elapsed_ms, int duration_ms, const PlayerTheme *th) {
    int seek_w = 0;
    int knob;
    int neon = skin_is_neon(th);
    (void)h;
    if (duration_ms > 0) {
        seek_w = (int)(((long)w * elapsed_ms) / duration_ms);
        if (seek_w < 0) seek_w = 0;
        if (seek_w > w) seek_w = w;
    }
    if (neon) {
        ui_gfx_fill(x, y + 1, w, 3, th->chrome_lo);
        if (seek_w > 0) {
            ui_gfx_fill(x, y + 1, seek_w, 3, th->accent);
        }
        knob = x + seek_w;
        if (knob < x) knob = x;
        if (knob > x + w) knob = x + w;
        ui_gfx_circle_fill(knob, y + 2, 5, th->accent);
        return;
    }
    /* Inset track with hairline — feels like device chrome, not a flat CSS bar. */
    ui_gfx_round_fill(x, y, w, 5, 2, th->chrome_lo);
    ui_gfx_hairline_rect(x, y, w, 5, th->chrome_hi, 40);
    if (seek_w > 0) {
        ui_gfx_round_fill(x, y, seek_w, 5, 2, th->accent);
        ui_gfx_fill_alpha(x, y, seek_w, 1, th->text, 50);
    }
    knob = x + seek_w;
    if (knob < x) knob = x;
    if (knob > x + w) knob = x + w;
    ui_gfx_circle_fill_alpha(knob, y + 2, 8, th->accent, 70);
    ui_gfx_circle_fill_alpha(knob, y + 2, 6, th->text, 120);
    ui_gfx_circle_fill(knob, y + 2, 4, th->accent);
}

static void np_soft_icon(int cx, int cy, int r, int icon, int lit, const PlayerTheme *th) {
    int atlas_region = UI_ATLAS_ICON_PLAY;
    int iw = (r >= 24) ? 30 : 20;
    int neon = skin_is_neon(th);
    if (neon) {
        if (lit) {
            ui_gfx_ring(cx, cy, r, 3, th->accent);
            ui_font_icon(cx - 10, cy - 10, 20, icon, th->accent);
        } else {
            ui_gfx_ring(cx, cy, r, 2, th->chrome_hi);
            ui_font_icon(cx - 8, cy - 8, 16, icon, th->text);
        }
        return;
    }
    if (lit) {
        /* Hero play — bloom + solid accent disc + dark glyph */
        ui_gfx_bloom(cx, cy, r + 10, th->accent, 45);
        ui_gfx_circle_fill(cx, cy, r, th->accent);
        ui_gfx_circle_fill_alpha(cx, cy - r / 3, r - 4, th->text, 35);
        if (icon == UI_ICON_PAUSE) {
            ui_gpu_blit_atlas_cpu(
                g_draw, BUF_WIDTH, UI_ATLAS_ICON_PAUSE, cx - iw / 2, cy - iw / 2, iw, iw, 0xFF08080Au
            );
        } else {
            ui_gpu_blit_atlas_cpu(
                g_draw, BUF_WIDTH, UI_ATLAS_ICON_PLAY, cx - iw / 2 + 1, cy - iw / 2, iw, iw, 0xFF08080Au
            );
        }
        return;
    }
    /* Secondary wells — recessed, not twin clones of Play */
    ui_gfx_circle_fill(cx, cy, r, th->chrome_lo);
    ui_gfx_hairline_rect(cx - r, cy - r, r * 2, r * 2, th->chrome_hi, 50);
    ui_gfx_circle_fill_alpha(cx, cy, r - 1, th->panel, 220);
    switch (icon) {
        case UI_ICON_PREV: atlas_region = UI_ATLAS_ICON_PREV; break;
        case UI_ICON_NEXT: atlas_region = UI_ATLAS_ICON_NEXT; break;
        case UI_ICON_PAUSE: atlas_region = UI_ATLAS_ICON_PAUSE; break;
        default: atlas_region = UI_ATLAS_ICON_PLAY; break;
    }
    ui_gpu_blit_atlas_cpu(g_draw, BUF_WIDTH, atlas_region, cx - iw / 2, cy - iw / 2, iw, iw, 0xFFE8E8F0u);
}

static void np_draw_transport(int y, const UiNowPlaying *np, const PlayerTheme *th) {
    int playing = np->playing && !np->paused;
    int cy = (y > 0) ? y : 246;
    np_soft_icon(156, cy, 18, UI_ICON_PREV, 0, th);
    np_soft_icon(240, cy - 4, 28, playing ? UI_ICON_PAUSE : UI_ICON_PLAY, 1, th);
    np_soft_icon(324, cy, 18, UI_ICON_NEXT, 0, th);
}

static void np_draw_status_pills(const UiNowPlaying *np, const PlayerTheme *th) {
    int x = 196;
    int y = 96;
    if (np->shuffle) {
        ui_gfx_round_fill(x, y, 44, 14, 4, th->card);
        ui_gfx_hairline_rect(x, y, 44, 14, th->accent, 90);
        ui_font_text(x + 6, y + 2, th->accent, "SHUF", UI_FONT_SM);
        x += 50;
    }
    if (np->repeat) {
        ui_gfx_round_fill(x, y, 36, 14, 4, th->card);
        ui_gfx_hairline_rect(x, y, 36, 14, th->accent, 90);
        ui_font_text(x + 6, y + 2, th->accent, "RPT", UI_FONT_SM);
        x += 42;
    }
    {
        char vbuf[24];
        snprintf(vbuf, sizeof(vbuf), "VOL %d%%", np->volume);
        ui_font_text(380, y + 2, th->muted, vbuf, UI_FONT_SM);
    }
}

static void np_draw_download(int x, int y, int w, const UiNowPlaying *np, const PlayerTheme *th) {
    char buf[64];
    int pct = np->dl_percent;
    float mb;
    float total_mb;
    /* Explicit download (§48) — not stream buffering */
    if (np->dl_state == 5 /* DL_COMPLETE */ || np->offline_saved) {
        ui_gfx_round_fill(x, y, 72, 12, 3, th->card);
        ui_font_text(x + 6, y + 1, th->success, "FLAC OK", UI_FONT_SM);
        if (np->dl_total > 0) {
            snprintf(buf, sizeof(buf), "%.1f MB lossless", np->dl_total / (1024.0f * 1024.0f));
            ui_font_text(x + 80, y + 1, th->muted, buf, UI_FONT_SM);
        }
        return;
    }
    if (np->dl_state == 1 || np->dl_state == 2 || np->dl_state == 3 || np->dl_state == 6) {
        /* RUNNING / PAUSED / VERIFYING / INCOMPLETE */
        if (np->dl_name && np->dl_name[0]) {
            ui_font_text_clip(x, y - 10, w + 80, th->muted, np->dl_name, UI_FONT_SM);
        }
        if (np->dl_state == 3) {
            ui_font_text(x, y + 6, th->warning ? th->warning : th->seek, "Verifying...", UI_FONT_SM);
            return;
        }
        ui_gfx_round_fill(x, y, w, 3, 2, th->chrome_lo);
        if (pct > 0) {
            if (pct > 100) pct = 100;
            ui_gfx_round_fill(x, y, (w * pct) / 100, 3, 2, th->accent);
        }
        mb = np->dl_bytes / (1024.0f * 1024.0f);
        total_mb = np->dl_total > 0 ? np->dl_total / (1024.0f * 1024.0f) : 0.0f;
        if (np->dl_state == 2) {
            snprintf(buf, sizeof(buf), "Paused %d%%", pct);
        } else if (np->dl_state == 6) {
            snprintf(buf, sizeof(buf), "Incomplete %d%%", pct);
        } else if (total_mb > 0.01f) {
            snprintf(
                buf,
                sizeof(buf),
                "%d%%  %.1f/%.1f MB  %.1f MB/s",
                pct,
                mb,
                total_mb,
                np->dl_speed_bps / (1024.0f * 1024.0f)
            );
        } else {
            snprintf(buf, sizeof(buf), "Downloading %d%%", pct);
        }
        ui_font_text_clip(x, y + 6, w + 120, th->muted, buf, UI_FONT_SM);
        return;
    }
    if (np->dl_state == 7 /* ERROR */) {
        ui_font_text_clip(x, y, w + 80, th->danger, np->status ? np->status : "Download error", UI_FONT_SM);
        return;
    }
    /* Stream buffer progress (online prebuffer) */
    if (np->buffering || (np->dl_total > 0 && np->dl_bytes > 0 && np->dl_bytes < np->dl_total && !np->playing)) {
        if (np->dl_total > 0) {
            pct = (np->dl_bytes * 100) / np->dl_total;
            if (pct > 100) pct = 100;
        } else {
            pct = 0;
        }
        ui_gfx_round_fill(x, y, w, 3, 2, th->chrome_lo);
        if (pct > 0) {
            ui_gfx_round_fill(x, y, (w * pct) / 100, 3, 2, th->accent);
        }
        snprintf(buf, sizeof(buf), "Loading %d%%", pct);
        ui_font_text(x, y + 6, th->muted, buf, UI_FONT_SM);
        return;
    }
    if (np->status && np->status[0] && strcmp(np->status, "Playing...") != 0) {
        ui_font_text_clip(x, y, w, th->muted, np->status, UI_FONT_SM);
    }
}

static void np_draw_cover_hero(const UiNowPlaying *np, const PlayerTheme *th) {
    int has_cover = np->cover && np->cover->ready;
    /* Soft accent atmosphere behind art */
    ui_gfx_bloom(100, 100, 94, th->accent, 32);
    ui_gfx_bloom(100, 100, 70, th->accent, 18);
    /* Drop shadow for depth */
    ui_gfx_round_fill(24, 28, 168, 168, 12, UI_RGB(0, 0, 0));
    ui_gfx_fill_alpha(20, 24, 172, 172, UI_RGB(0, 0, 0), 90);
    /* Outer frame / elevated well */
    ui_gfx_round_fill(16, 16, 168, 168, 12, th->chrome_lo);
    ui_gfx_hairline_rect(16, 16, 168, 168, th->chrome_hi, 90);
    ui_gfx_round_fill(20, 20, 160, 160, 11, th->surface_elevated ? th->surface_elevated : th->card);
    ui_gfx_round_fill(22, 22, 156, 156, 10, th->card);
    if (has_cover) {
        /* CPU blit — GPU overlay often left the hero frame empty on device. */
        ui_image_draw_cover_ex(24, 24, 152, 152, np->cover, 0);
    } else {
        ui_gpu_blit_atlas_cpu(g_draw, BUF_WIDTH, UI_ATLAS_FALLBACK, 24, 24, 152, 152, 0xFFFFFFFFu);
    }
    /* Inner rim + bottom reflection strip */
    ui_gfx_hairline_rect(24, 24, 152, 152, th->text, 36);
    ui_gfx_hairline_rect(25, 25, 150, 150, th->accent, 22);
    ui_gfx_fill_alpha(28, 160, 144, 10, th->text, 22);
}

/* Real queue list — every visible row is a real track */
static void np_draw_queue(
    int x, int y, int w, int h, const UiNowPlaying *np, const PlayerTheme *th, int embedded
) {
    int row_h = 18;
    int visible = h / row_h;
    int start = 0;
    int i;
    int cursor = np->queue_focus ? np->queue_cursor : np->queue_index;

    if (!np->queue_titles || np->queue_count <= 0) {
        ui_gfx_inset(x, y, w, h, th->lcd, th->chrome_hi, th->chrome_lo, 3);
        ui_font_text(x + 8, y + h / 2 - 4, th->muted, "No tracks", UI_FONT_SM);
        return;
    }
    if (visible < 1) {
        visible = 1;
    }
    if (cursor < 0) {
        cursor = 0;
    }
    if (cursor >= start + visible) {
        start = cursor - visible + 1;
    }
    if (cursor < start) {
        start = cursor;
    }
    if (start < 0) {
        start = 0;
    }

    if (!embedded) {
        ui_gfx_fill_alpha(0, 0, UI_SCREEN_W, UI_SCREEN_H, UI_RGB(0, 0, 0), 160);
        ui_gfx_glass(x - 12, y - 28, w + 24, h + 48, 8, th->panel, th->chrome_hi, 240);
        ui_gfx_fill_alpha(x - 4, y - 20, 40, 2, th->accent, 200);
        ui_font_text(x, y - 18, th->text, "Up Next", UI_FONT_MD);
    } else {
        ui_gfx_glass(x, y, w, h, 6, th->lcd, th->chrome_hi, 230);
    }

    for (i = 0; i < visible; i++) {
        int idx = start + i;
        int ry = y + i * row_h;
        char num[12];
        if (idx >= np->queue_count) {
            break;
        }
        if (idx == cursor) {
            if (th->cursor_style == CURSOR_GLOW) {
                ui_gfx_round_fill_alpha(x + 2, ry + 1, w - 4, row_h - 2, 3, th->accent, 180);
            } else {
                ui_gfx_round_fill(x + 2, ry + 1, w - 4, row_h - 2, 2, th->card);
                ui_gfx_fill(x + 2, ry + 1, 3, row_h - 2, th->accent);
            }
        } else if (idx == np->queue_index) {
            ui_gfx_fill(x + 2, ry + 1, 3, row_h - 2, th->seek);
        }
        snprintf(num, sizeof(num), "%d.", idx + 1);
        ui_font_text(x + 8, ry + 4, th->muted, num, UI_FONT_SM);
        ui_font_text_clip(x + 32, ry + 4, w - 40, (idx == cursor) ? th->text : th->muted,
                          np->queue_titles[idx] ? np->queue_titles[idx] : "-", UI_FONT_SM);
    }
}

static const int *eq_preset_bands(int preset) {
    switch (preset) {
        case 1: return EQ_BASS;
        case 2: return EQ_ROCK;
        case 3: return EQ_POP;
        case 4: return EQ_VOCAL;
        default: return EQ_FLAT;
    }
}

static void ui_draw_eq_panel(const UiNowPlaying *np) {
    const PlayerTheme *th = theme_active();
    int i;
    int cursor = np->eq_cursor;

    if (cursor < 0) {
        cursor = np->eq_preset;
    }
    if (cursor >= PLAYER_EQ_COUNT) {
        cursor = PLAYER_EQ_COUNT - 1;
    }

    ui_clear(th->bg);
    ui_gfx_grad_v(0, 0, UI_SCREEN_W, UI_SCREEN_H, th->header, th->bg);
    ui_gfx_glass(0, 0, UI_SCREEN_W, 40, 0, th->panel, th->chrome_hi, 245);
    ui_gfx_fill_alpha(24, 38, 56, 2, th->accent, 200);
    ui_font_text(24, 11, th->text, "Equalizer", UI_FONT_MD);
    ui_font_text(140, 14, th->muted, "sound profile", UI_FONT_SM);

    ui_gfx_glass(12, 48, 200, 176, 8, th->panel, th->chrome_hi, 230);
    for (i = 0; i < PLAYER_EQ_COUNT; i++) {
        int y = 56 + i * 32;
        const char *name = player_eq_preset_name(i);
        int sel = (i == cursor);
        int applied = (i == np->eq_preset);
        if (sel) {
            ui_gfx_round_fill(20, y, 184, 26, 5, th->accent);
            ui_gfx_bloom(40, y + 13, 20, th->accent, 40);
        } else if (applied) {
            ui_gfx_round_fill(20, y, 184, 26, 5, th->card);
            ui_gfx_fill(20, y + 4, 3, 18, th->seek);
        }
        ui_font_text(36, y + 7, sel ? UI_RGB(8, 8, 10) : th->muted, name, UI_FONT_MD);
        if (applied && !sel) {
            ui_gfx_circle_fill(188, y + 13, 3, th->accent);
        }
    }

    {
        const int *bands = eq_preset_bands(cursor);
        ui_gfx_glass(228, 48, 240, 176, 8, th->lcd, th->chrome_hi, 230);
        ui_font_text(240, 58, th->muted, "BAND RESPONSE", UI_FONT_SM);
        for (i = 0; i < 10; i++) {
            int bx = 248 + i * 20;
            int bh = (bands[i + 1] * 100) / 24;
            ui_gfx_round_fill(bx, 78, 14, 110, 3, th->chrome_lo);
            ui_gfx_round_fill(bx, 78 + (110 - bh), 14, bh, 3, th->accent);
            ui_gfx_fill_alpha(bx, 78 + (110 - bh), 14, 2, th->text, 60);
        }
        ui_font_text(240, 200, th->text, player_eq_preset_name(cursor), UI_FONT_MD);
    }
}

/* -------- 15 compositions -------- */
/*
 * One production layout shared by all 15 color/material presets.
 * Covers use GPU-filtered textures; chrome comes from the UI atlas.
 */
/* Bitmap font has no Cyrillic — transliterate UTF-8 Russian to Latin. */
static int np_utf8_cp(const unsigned char *p, unsigned *cp, int *adv) {
    if (!p || !p[0]) {
        return 0;
    }
    if (p[0] < 0x80) {
        *cp = p[0];
        *adv = 1;
        return 1;
    }
    if ((p[0] & 0xE0) == 0xC0 && p[1]) {
        *cp = ((unsigned)(p[0] & 0x1F) << 6) | (unsigned)(p[1] & 0x3F);
        *adv = 2;
        return 1;
    }
    if ((p[0] & 0xF0) == 0xE0 && p[1] && p[2]) {
        *cp = ((unsigned)(p[0] & 0x0F) << 12) | ((unsigned)(p[1] & 0x3F) << 6) |
              (unsigned)(p[2] & 0x3F);
        *adv = 3;
        return 1;
    }
    *adv = 1;
    *cp = '?';
    return 1;
}

static const char *np_cyr_to_lat(unsigned cp) {
    switch (cp) {
        case 0x0410: case 0x0430: return "A";
        case 0x0411: case 0x0431: return "B";
        case 0x0412: case 0x0432: return "V";
        case 0x0413: case 0x0433: return "G";
        case 0x0414: case 0x0434: return "D";
        case 0x0415: case 0x0435: return "E";
        case 0x0416: case 0x0436: return "Zh";
        case 0x0417: case 0x0437: return "Z";
        case 0x0418: case 0x0438: return "I";
        case 0x0419: case 0x0439: return "Y";
        case 0x041A: case 0x043A: return "K";
        case 0x041B: case 0x043B: return "L";
        case 0x041C: case 0x043C: return "M";
        case 0x041D: case 0x043D: return "N";
        case 0x041E: case 0x043E: return "O";
        case 0x041F: case 0x043F: return "P";
        case 0x0420: case 0x0440: return "R";
        case 0x0421: case 0x0441: return "S";
        case 0x0422: case 0x0442: return "T";
        case 0x0423: case 0x0443: return "U";
        case 0x0424: case 0x0444: return "F";
        case 0x0425: case 0x0445: return "Kh";
        case 0x0426: case 0x0446: return "Ts";
        case 0x0427: case 0x0447: return "Ch";
        case 0x0428: case 0x0448: return "Sh";
        case 0x0429: case 0x0449: return "Sch";
        case 0x042A: case 0x044A: return "";
        case 0x042B: case 0x044B: return "Y";
        case 0x042C: case 0x044C: return "";
        case 0x042D: case 0x044D: return "E";
        case 0x042E: case 0x044E: return "Yu";
        case 0x042F: case 0x044F: return "Ya";
        case 0x0401: case 0x0451: return "Yo";
        default: return NULL;
    }
}

static void np_ascii(char *dst, int dst_sz, const char *src) {
    int di = 0;
    const unsigned char *p;
    if (!dst || dst_sz <= 0) {
        return;
    }
    dst[0] = '\0';
    if (!src) {
        return;
    }
    p = (const unsigned char *)src;
    while (*p && di < dst_sz - 1) {
        unsigned cp = 0;
        int adv = 1;
        const char *rep;
        if (!np_utf8_cp(p, &cp, &adv)) {
            break;
        }
        p += adv;
        if (cp < 128) {
            if (cp >= 32) {
                dst[di++] = (char)cp;
            }
            continue;
        }
        rep = np_cyr_to_lat(cp);
        if (rep) {
            while (*rep && di < dst_sz - 1) {
                dst[di++] = *rep++;
            }
        }
    }
    dst[di] = '\0';
    if (strcmp(dst, "Media.localized") == 0 || strcmp(dst, "Unknown Album") == 0 ||
        strcmp(dst, "Unknown Artist") == 0) {
        dst[0] = '\0';
    }
}

static void ui_font_text_marquee(int x, int y, int max_w, u32 color, const char *s, int size);

static void np_comp_modern(const UiNowPlaying *np, const PlayerTheme *th) {
    char elbuf[12];
    char durbuf[12];
    char title[96];
    char artist[96];
    char album[96];
    char meta[80];
    int el, dur;
    u32 artist_col;
    const char *fmt;

    np_ascii(title, sizeof(title), np->title);
    np_ascii(artist, sizeof(artist), np->artist);
    np_ascii(album, sizeof(album), np->album);
    if (!title[0] || title[0] == ' ') {
        snprintf(title, sizeof(title), "Unknown Track");
    }
    if (!artist[0] || artist[0] == ' ') {
        snprintf(artist, sizeof(artist), "Unknown Artist");
    }

    /* Atmosphere */
    ui_clear(th->bg);
    if (skin_is_neon(th)) {
        ui_gfx_fill(0, 0, UI_SCREEN_W, UI_SCREEN_H, th->bg);
        ui_font_icon(12, 8, 14, UI_ICON_NOTE, th->text);
        ui_font_text(30, 10, th->text, "Now Playing", UI_FONT_MD);
        ui_font_icon(400, 8, 14, UI_ICON_SPEAKER, th->muted);
        ui_font_icon(424, 8, 14, UI_ICON_BATTERY, th->accent);
    } else {
        ui_gfx_grad_v(0, 0, UI_SCREEN_W, UI_SCREEN_H, th->header, th->bg);
        ui_gfx_fill_alpha(0, 0, UI_SCREEN_W, 1, th->chrome_hi, 60);
        ui_gfx_fill_alpha(0, 1, UI_SCREEN_W, 1, th->accent, 35);
    }

    np_draw_cover_hero(np, th);

    /* Right column: title → artist → album → mode pills (no time/format here) */
    ui_font_text_marquee(196, 24, 268, th->text, title, UI_FONT_LG);
    artist_col = ui_gfx_lerp(th->text, th->muted, 90);
    if (skin_is_neon(th)) {
        ui_font_icon(196, 48, 12, UI_ICON_USER, th->muted);
        ui_font_text_clip(214, 50, 250, artist_col, artist, UI_FONT_MD);
        if (album[0]) {
            ui_font_icon(196, 70, 12, UI_ICON_DISC, th->muted);
            ui_font_text_clip(214, 72, 200, th->muted, album, UI_FONT_SM);
        }
    } else {
        ui_font_text_clip(196, 50, 268, artist_col, artist, UI_FONT_MD);
        if (album[0]) {
            ui_font_text_clip(196, 72, 200, th->muted, album, UI_FONT_SM);
        }
    }
    if (np->rating > 0) {
        char stars[8];
        int si;
        for (si = 0; si < 5; si++) {
            stars[si] = (si < np->rating) ? '*' : '-';
        }
        stars[5] = '\0';
        ui_font_text(404, 72, th->accent, stars, UI_FONT_SM);
    }
    np_draw_status_pills(np, th);
    viz_draw(196, 128, 268, 40, np->playing && !np->paused, th);

    /* One format line under cover only — never overlaps the scrubber */
    fmt = np->format_name && np->format_name[0] ? np->format_name : "—";
    if (np->buffer_state == 7 /* NETWORK_LOST */ ||
        (np->buffering && np->buffered_sec < 2.0f && np->dl_bytes > 0 && !np->playing)) {
        snprintf(meta, sizeof(meta), "Reconnecting...");
        ui_font_text_clip(20, 186, 160, th->warning ? th->warning : th->seek, meta, UI_FONT_SM);
    } else if (np->lossless) {
        snprintf(
            meta,
            sizeof(meta),
            "%s %s %dk/%db",
            np->online ? "ON" : "OFF",
            fmt,
            np->sample_khz > 0 ? np->sample_khz : 44,
            np->bit_depth > 0 ? np->bit_depth : 16
        );
        ui_font_text_clip(20, 186, 168, th->muted, meta, UI_FONT_SM);
    } else {
        if (np->bitrate_kbps > 0) {
            snprintf(
                meta,
                sizeof(meta),
                "%s %s %dk",
                np->online ? "ON" : "OFF",
                fmt,
                np->bitrate_kbps
            );
        } else {
            snprintf(meta, sizeof(meta), "%s %s", np->online ? "ON" : "OFF", fmt);
        }
        ui_font_text_clip(20, 186, 168, th->muted, meta, UI_FONT_SM);
    }
    if (np->buffered_sec > 0.5f && np->online) {
        char bufsec[24];
        snprintf(bufsec, sizeof(bufsec), "buf %.0fs", np->buffered_sec);
        ui_font_text_clip(196, 186, 100, th->lcd_dim, bufsec, UI_FONT_SM);
    }

    /* Scrubber band: elapsed | bar | duration — exclusive lane */
    el = np->elapsed_ms / 1000;
    dur = np->duration_ms / 1000;
    if (el < 0) el = 0;
    if (dur < 0) dur = 0;
    snprintf(elbuf, sizeof(elbuf), "%d:%02d", el / 60, el % 60);
    snprintf(durbuf, sizeof(durbuf), "%d:%02d", dur / 60, dur % 60);
    ui_font_text(20, 204, th->text, elbuf, UI_FONT_SM);
    ui_font_text(430, 204, th->muted, durbuf, UI_FONT_SM);
    np_draw_progress(58, 208, 360, 5, np->elapsed_ms, np->duration_ms, th);

    np_draw_download(20, 222, 160, np, th);
    np_draw_transport(232, np, th);

    if (np->queue_focus) {
        np_draw_queue(60, 20, 360, 200, np, th, 0);
    }
}

static void np_draw_skin(const UiNowPlaying *np, const PlayerTheme *th) {
    ui_eq_step(np->playing && !np->paused);
    np_comp_modern(np, th);
}

void ui_set_loading(int on) {
    g_loading = on ? 1 : 0;
}

static void ui_font_text_marquee(int x, int y, int max_w, u32 color, const char *s, int size) {
    int w;
    int skip;
    int len;
    unsigned t;
    if (!s || !s[0]) {
        return;
    }
    w = ui_font_text_w(s, size);
    if (w <= max_w) {
        ui_font_text_clip(x, y, max_w, color, s, size);
        return;
    }
    len = (int)strlen(s);
    t = sceKernelGetSystemTimeLow() / 180000u;
    skip = (int)(t % (unsigned)(len + 4));
    if (skip >= len) {
        skip = 0;
    }
    ui_font_text_clip(x, y, max_w, color, s + skip, size);
}

static int skin_is_neon(const PlayerTheme *th) {
    return th && th->chrome_style == CHROME_NEON;
}

void ui_draw_footer_hints(const char *x_label, const char *o_label) {
    const PlayerTheme *th = theme_active();
    int y = UI_SCREEN_H - 22;
    u32 line = skin_is_neon(th) ? th->accent : th->chrome_lo;
    ui_gfx_fill(0, y - 4, UI_SCREEN_W, 1, line);
    if (skin_is_neon(th)) {
        ui_gfx_fill_alpha(0, y - 4, UI_SCREEN_W, 1, th->accent, 180);
    }
    ui_font_icon(16, y - 1, 14, UI_ICON_BTN_X, th->text);
    ui_font_text(34, y + 1, th->text, x_label ? x_label : "Select", UI_FONT_SM);
    ui_font_icon(UI_SCREEN_W - 90, y - 1, 14, UI_ICON_BTN_O, th->text);
    ui_font_text(UI_SCREEN_W - 72, y + 1, th->text, o_label ? o_label : "Back", UI_FONT_SM);
}

void ui_draw_header(const char *title, int playing) {
    const PlayerTheme *th = theme_active();
    int neon = skin_is_neon(th);
    if (neon) {
        ui_gfx_fill(0, 0, UI_SCREEN_W, 36, th->bg);
        ui_font_text_clip(16, 8, UI_SCREEN_W - 80, th->text, title ? title : "Music", UI_FONT_LG);
        ui_gfx_fill(0, 34, UI_SCREEN_W, 2, th->accent);
        if (playing) {
            ui_gfx_circle_fill(456, 18, 4, th->accent);
        }
        return;
    }
    ui_gfx_fill(0, 0, UI_SCREEN_W, 40, th->panel);
    ui_gfx_fill_alpha(0, 0, UI_SCREEN_W, 1, th->chrome_hi, 90);
    ui_gfx_fill(0, 39, UI_SCREEN_W, 1, th->chrome_lo);
    ui_gfx_fill(24, 37, 56, 3, th->accent);
    ui_font_text_clip(24, 10, UI_SCREEN_W - 80, th->text, title ? title : "Music", UI_FONT_LG);
    if (g_loading) {
        unsigned t = sceKernelGetSystemTimeLow() / 90000u;
        int phase = (int)(t % 5);
        ui_gfx_fill(UI_SCREEN_W - 52, 34, 6 + phase * 6, 3, th->accent);
    }
    if (playing) {
        ui_gfx_circle_fill_alpha(456, 20, 7, th->accent, 60);
        ui_gfx_circle_fill(456, 20, 4, th->accent);
    } else {
        ui_gfx_circle_fill(456, 20, 4, th->muted);
    }
}

void ui_draw_list(
    const char *title,
    const char **labels,
    const char **rights,
    int count,
    int cursor,
    int playing
) {
    ui_draw_library_ex(title, labels, rights, NULL, NULL, count, cursor, playing, NULL, 0);
}

static void ui_draw_mini_player(const UiMiniPlayer *mini, const PlayerTheme *th) {
    char tbuf[24];
    int y = UI_SCREEN_H - 48;
    const UiCover *cover;
    char title[96];
    char artist[96];
    int i;
    int seek_w = 0;
    int neon = skin_is_neon(th);

    if (!mini || !mini->now_title || !mini->now_title[0]) {
        return;
    }
    for (i = 0; i < 95 && mini->now_title[i]; i++) {
        unsigned char c = (unsigned char)mini->now_title[i];
        title[i] = (c >= 32 && c < 127) ? (char)c : ' ';
    }
    title[i] = '\0';
    artist[0] = '\0';
    if (mini->now_artist) {
        for (i = 0; i < 95 && mini->now_artist[i]; i++) {
            unsigned char c = (unsigned char)mini->now_artist[i];
            artist[i] = (c >= 32 && c < 127) ? (char)c : ' ';
        }
        artist[i] = '\0';
    }

    if (neon) {
        ui_gfx_fill(0, y, UI_SCREEN_W, 48, th->header);
        ui_gfx_fill(0, y, 4, 48, th->accent);
        cover = ui_image_cover_for(mini->track_id);
        ui_gfx_fill(12, y + 8, 32, 32, th->chrome_lo);
        if (cover && cover->ready) {
            ui_image_draw_cover(12, y + 8, 32, 32, cover);
        } else {
            ui_gpu_blit_atlas_cpu(g_draw, BUF_WIDTH, UI_ATLAS_FALLBACK, 12, y + 8, 32, 32, 0xFFFFFFFFu);
        }
        ui_font_text_clip(54, y + 10, 300, th->text, title, UI_FONT_MD);
        ui_font_text_clip(54, y + 28, 280, th->muted, artist, UI_FONT_SM);
        /* Outline play circle like library mockup */
        ui_gfx_ring(452, y + 24, 14, 2, th->accent);
        ui_font_icon(444, y + 16, 16, mini->paused ? UI_ICON_PLAY : UI_ICON_PAUSE, th->accent);
        return;
    }

    ui_gfx_glass(0, y, UI_SCREEN_W, 48, 0, th->header, th->chrome_hi, 235);
    ui_gfx_fill_alpha(0, y, UI_SCREEN_W, 1, th->accent, 160);
    cover = ui_image_cover_for(mini->track_id);
    ui_gfx_round_fill(8, y + 6, 36, 36, 6, th->chrome_lo);
    if (cover && cover->ready) {
        ui_image_draw_cover(10, y + 8, 32, 32, cover);
    } else {
        ui_gpu_blit_atlas_cpu(g_draw, BUF_WIDTH, UI_ATLAS_FALLBACK, 10, y + 8, 32, 32, 0xFFFFFFFFu);
    }
    ui_gfx_hairline_rect(10, y + 8, 32, 32, th->chrome_hi, 50);
    ui_font_text_clip(54, y + 8, 230, th->text, title, UI_FONT_MD);
    ui_font_text_clip(54, y + 26, 200, th->muted, artist, UI_FONT_SM);
    ui_font_text(270, y + 26, th->lcd_dim, "Tri=player", UI_FONT_SM);
    if (mini->duration_ms > 0) {
        seek_w = (int)(((long)160 * mini->elapsed_ms) / mini->duration_ms);
        if (seek_w < 0) seek_w = 0;
        if (seek_w > 160) seek_w = 160;
    }
    ui_gfx_round_fill(300, y + 32, 160, 3, 2, th->chrome_lo);
    if (seek_w > 0) {
        ui_gfx_round_fill(300, y + 32, seek_w, 3, 2, th->accent);
    }
    {
        int el = mini->elapsed_ms / 1000;
        int dur = mini->duration_ms / 1000;
        if (el < 0) el = 0;
        if (dur < 0) dur = 0;
        snprintf(tbuf, sizeof(tbuf), "%d:%02d/%d:%02d", el / 60, el % 60, dur / 60, dur % 60);
        ui_font_text(300, y + 10, th->muted, tbuf, UI_FONT_SM);
    }
    ui_gfx_circle_fill(452, y + 24, 15, th->accent);
    ui_gfx_bloom(452, y + 24, 18, th->accent, 40);
    ui_gpu_blit_atlas_cpu(
        g_draw, BUF_WIDTH,
        mini->paused ? UI_ATLAS_ICON_PLAY : UI_ATLAS_ICON_PAUSE,
        440, y + 12, 24, 24, 0xFF0A0A10u
    );
}

void ui_draw_library(
    const char *title,
    const char **labels,
    const char **rights,
    const int *track_ids,
    int count,
    int cursor,
    int playing,
    const UiMiniPlayer *mini
) {
    ui_draw_library_ex(title, labels, rights, NULL, track_ids, count, cursor, playing, mini, 0);
}

void ui_draw_library_ex(
    const char *title,
    const char **labels,
    const char **rights,
    const int *icons,
    const int *track_ids,
    int count,
    int cursor,
    int playing,
    const UiMiniPlayer *mini,
    int show_footer
) {
    const PlayerTheme *th = theme_active();
    int i;
    int neon = skin_is_neon(th);
    int row_h = neon ? 34 : 32;
    int top = neon ? 42 : 40;
    int footer_h = 0;
    int visible;
    int start = 0;
    int show_thumbs = (track_ids != NULL);
    int has_icons = (icons != NULL);

    if (mini && mini->now_title && mini->now_title[0]) {
        footer_h = 48;
    } else if (show_footer && neon) {
        footer_h = 26;
    }

    ui_clear(th->bg);
    if (!neon) {
        ui_gfx_grad_v(0, 0, UI_SCREEN_W, UI_SCREEN_H, th->header, th->bg);
    }
    ui_draw_header(title, playing);

    visible = (UI_SCREEN_H - top - footer_h) / row_h;
    if (visible < 1) visible = 1;
    if (cursor >= start + visible) start = cursor - visible + 1;
    if (cursor < start) start = cursor;
    if (start < 0) start = 0;

    for (i = 0; i < visible; i++) {
        int idx = start + i;
        int y = top + i * row_h;
        int text_x = show_thumbs ? 56 : (has_icons ? 44 : 28);
        char label[96];
        int n = 0;
        u32 rights_col;
        if (idx >= count) break;
        if (idx == cursor) {
            if (neon) {
                /* Flush-left gray bar + neon rail like home.png */
                ui_gfx_fill(0, y, UI_SCREEN_W, row_h, th->card);
                ui_gfx_fill(0, y, 4, row_h, th->accent);
            } else {
                ui_gfx_round_fill(6, y + 1, UI_SCREEN_W - 12, row_h - 2, 6, th->card);
                ui_gfx_fill(6, y + 3, 4, row_h - 6, th->accent);
                ui_gfx_fill_alpha(10, y + 3, 8, row_h - 6, th->accent, 55);
                ui_gfx_hairline_rect(6, y + 1, UI_SCREEN_W - 12, row_h - 2, th->chrome_hi, 70);
            }
        } else if (!neon) {
            ui_gfx_fill_alpha(28, y + row_h - 1, UI_SCREEN_W - 56, 1, th->chrome_lo, 110);
        } else {
            ui_gfx_fill_alpha(16, y + row_h - 1, UI_SCREEN_W - 32, 1, th->chrome_lo, 80);
        }
        if (has_icons && icons[idx] >= 0) {
            ui_font_icon(14, y + (row_h - 16) / 2, 16, icons[idx], th->accent);
        }
        if (show_thumbs) {
            const UiCover *c = ui_image_cover_for(track_ids[idx]);
            ui_gfx_round_fill(22, y + 4, 24, 24, neon ? 2 : 5, th->chrome_lo);
            if (c && c->ready) {
                ui_image_draw_cover(23, y + 5, 22, 22, c);
            } else {
                ui_gpu_blit_atlas_cpu(g_draw, BUF_WIDTH, UI_ATLAS_FALLBACK, 23, y + 5, 22, 22, 0xFFFFFFFFu);
            }
        }
        if (labels && labels[idx]) {
            while (labels[idx][n] && n < 95) {
                unsigned char c = (unsigned char)labels[idx][n];
                label[n] = (c >= 32 && c < 127) ? (char)c : ' ';
                n++;
            }
        }
        label[n] = '\0';
        if (idx == cursor) {
            ui_font_text_marquee(text_x, y + 8, UI_SCREEN_W - text_x - 176,
                          th->text, label, UI_FONT_MD);
        } else {
            ui_font_text_clip(text_x, y + 8, UI_SCREEN_W - text_x - 176,
                          neon ? th->text : th->muted, label, UI_FONT_MD);
        }
        if (rights && rights[idx] && rights[idx][0]) {
            rights_col = (neon || idx == cursor) ? th->accent : th->muted;
            ui_font_text_clip(UI_SCREEN_W - 176, y + 9, 168, rights_col, rights[idx], UI_FONT_SM);
        }
    }

    if (mini && mini->now_title && mini->now_title[0]) {
        ui_draw_mini_player(mini, th);
    } else if (show_footer && neon) {
        ui_draw_footer_hints("Select", "Back");
    }
}

void ui_draw_now_playing(const UiNowPlaying *np) {
    const PlayerTheme *th;
    if (!np) {
        return;
    }
    if (np->show_eq) {
        ui_draw_eq_panel(np);
        return;
    }
    th = skin_active();
    np_draw_skin(np, th);
}

static void ui_draw_np_preview_box(int x, int y, int w, int h, int skin_id) {
    const PlayerTheme *th = skin_get(skin_id);
    ui_gfx_panel(x, y, w, h, th->chrome, th->chrome_hi, th->chrome_lo, 4);
    ui_gfx_inset(x + 4, y + 4, w - 8, h - 22, th->lcd, th->chrome_hi, th->chrome_lo, 3);
    viz_draw(x + 8, y + 10, w - 16, h - 36, 1, th);
    ui_font_text_clip(x + 4, y + h - 14, w - 8, th->muted, skin_name(skin_id), UI_FONT_SM);
}

void ui_draw_appearance(
    int focus_themes,
    int theme_cursor,
    int layout_cursor,
    const char *title_hint,
    const char *artist_hint,
    int playing
) {
    const PlayerTheme *th = theme_active();
    int i;
    char line[64];
    int cursor = theme_cursor;
    int visible = 8;
    int start = 0;
    (void)focus_themes;
    (void)layout_cursor;
    (void)title_hint;
    (void)artist_hint;

    ui_clear(th->bg);
    ui_gfx_grad_v(0, 0, UI_SCREEN_W, UI_SCREEN_H, th->header, th->bg);
    ui_draw_header("Themes", playing);

    if (cursor >= start + visible) {
        start = cursor - visible + 1;
    }
    if (cursor < start) {
        start = cursor;
    }
    if (start < 0) {
        start = 0;
    }
    if (start > SKIN_COUNT - visible) {
        start = SKIN_COUNT - visible;
    }
    if (start < 0) {
        start = 0;
    }

    ui_gfx_glass(8, 48, 220, 176, 8, th->panel, th->chrome_hi, 230);
    for (i = 0; i < visible; i++) {
        int idx = start + i;
        int y = 56 + i * 20;
        u32 col = (idx == cursor) ? th->text : th->muted;
        const PlayerTheme *swatch;
        if (idx >= SKIN_COUNT) {
            break;
        }
        swatch = skin_get(idx);
        if (idx == cursor) {
            ui_gfx_round_fill(12, y - 2, 212, 18, 4, th->card);
            ui_gfx_fill(12, y - 2, 3, 18, th->accent);
        }
        ui_gfx_circle_fill(28, y + 6, 5, swatch->accent);
        snprintf(line, sizeof(line), "%s%s", (idx == skin_get_id()) ? "" : "", skin_name(idx));
        ui_font_text_clip(42, y + 1, 160, col, line, UI_FONT_SM);
        if (idx == skin_get_id()) {
            ui_gfx_circle_fill(210, y + 6, 3, th->accent);
        }
    }

    ui_font_text(248, 52, th->muted, "PREVIEW", UI_FONT_SM);
    ui_draw_np_preview_box(240, 68, 228, 110, cursor);
    ui_font_text_clip(240, 188, 228, th->text, skin_name(cursor), UI_FONT_MD);
    ui_font_text(240, 210, th->muted, "accent palette", UI_FONT_SM);
}

void ui_draw_message(const char *title, const char *line1, const char *line2, int playing) {
    const PlayerTheme *th = theme_active();
    ui_clear(th->bg);
    if (!skin_is_neon(th)) {
        ui_gfx_grad_v(0, 0, UI_SCREEN_W, UI_SCREEN_H, th->header, th->bg);
    }
    ui_draw_header(title, playing);
    if (skin_is_neon(th)) {
        if (line1) {
            ui_font_text_clip(24, 100, UI_SCREEN_W - 48, th->text, line1, UI_FONT_MD);
        }
        if (line2) {
            ui_font_text_clip(24, 132, UI_SCREEN_W - 48, th->muted, line2, UI_FONT_SM);
        }
        ui_draw_footer_hints("Back", "OK");
        return;
    }
    ui_gfx_glass(24, 72, UI_SCREEN_W - 48, 128, 10, th->panel, th->chrome_hi, 235);
    ui_gfx_fill(24, 80, 4, 112, th->accent);
    if (line1) {
        ui_font_text_clip(44, 100, UI_SCREEN_W - 90, th->text, line1, UI_FONT_MD);
    }
    if (line2) {
        ui_font_text_clip(44, 132, UI_SCREEN_W - 90, th->muted, line2, UI_FONT_SM);
    }
}

void ui_draw_setup(
    const char *title,
    const int octets[4],
    int selected_octet,
    int port,
    int focus_port,
    int playing
) {
    const PlayerTheme *th = theme_active();
    char part[16];
    int i;
    int x;
    int w;

    ui_clear(th->bg);
    ui_draw_header(title ? title : "Setup", playing);

    ui_font_text(UI_SCREEN_W / 2 - 40, 70, th->text, "IP Address", UI_FONT_SM);

    x = 40;
    for (i = 0; i < 4; i++) {
        if (!focus_port && i == selected_octet) {
            snprintf(part, sizeof(part), "[%d]", octets[i] & 255);
            w = ui_font_text_w(part, UI_FONT_LG);
            ui_font_text(x, 100, th->accent, part, UI_FONT_LG);
        } else {
            snprintf(part, sizeof(part), "%d", octets[i] & 255);
            w = ui_font_text_w(part, UI_FONT_LG);
            ui_font_text(x, 100, th->text, part, UI_FONT_LG);
        }
        x += w + 4;
        if (i < 3) {
            ui_font_text(x, 100, th->muted, ".", UI_FONT_LG);
            x += ui_font_text_w(".", UI_FONT_LG) + 4;
        }
    }

    ui_font_text(UI_SCREEN_W / 2 - 16, 150, th->text, "Port", UI_FONT_SM);
    if (focus_port) {
        snprintf(part, sizeof(part), "[%d]", port);
        ui_font_text(UI_SCREEN_W / 2 - ui_font_text_w(part, UI_FONT_LG) / 2, 172, th->accent, part, UI_FONT_LG);
    } else {
        snprintf(part, sizeof(part), "%d", port);
        ui_font_text(UI_SCREEN_W / 2 - ui_font_text_w(part, UI_FONT_LG) / 2, 172, th->text, part, UI_FONT_LG);
    }

    ui_font_text(UI_SCREEN_W / 2 - 70, 210, th->muted, "L/R move  U/D +-1", UI_FONT_SM);
    ui_draw_footer_hints("Back", "Save");
}

void ui_draw_info(
    const char *title,
    const char *line1,
    const char *line2,
    const char *line3,
    const char *footer,
    int playing
) {
    const PlayerTheme *th = theme_active();
    int y = 92;
    ui_clear(th->bg);
    ui_gfx_grad_v(0, 0, UI_SCREEN_W, UI_SCREEN_H, th->header, th->bg);
    ui_draw_header(title, playing);
    ui_gfx_glass(18, 56, UI_SCREEN_W - 36, 176, 10, th->panel, th->chrome_hi, 235);
    ui_gfx_fill(18, 64, 4, 160, th->accent);
    if (line1 && line1[0]) {
        ui_font_text_clip(34, y, UI_SCREEN_W - 60, th->text, line1, UI_FONT_MD);
        y += 28;
    }
    if (line2 && line2[0]) {
        ui_font_text_clip(34, y, UI_SCREEN_W - 60, th->muted, line2, UI_FONT_SM);
        y += 22;
    }
    if (line3 && line3[0]) {
        ui_font_text_clip(34, y, UI_SCREEN_W - 60, th->muted, line3, UI_FONT_SM);
        y += 22;
    }
    if (footer && footer[0]) {
        ui_font_text_clip(34, 210, UI_SCREEN_W - 60, th->accent, footer, UI_FONT_SM);
    }
}

void ui_set_debug(const char *line1, const char *line2) {
#ifdef DEBUG_HUD
    if (line1) {
        strncpy(g_dbg1, line1, sizeof(g_dbg1) - 1);
        g_dbg1[sizeof(g_dbg1) - 1] = '\0';
    } else {
        g_dbg1[0] = '\0';
    }
    if (line2) {
        strncpy(g_dbg2, line2, sizeof(g_dbg2) - 1);
        g_dbg2[sizeof(g_dbg2) - 1] = '\0';
    } else {
        g_dbg2[0] = '\0';
    }
#else
    (void)line1;
    (void)line2;
#endif
}

void ui_draw_debug_overlay(void) {
#ifdef DEBUG_HUD
    ui_gfx_fill(0, 0, UI_SCREEN_W, 2, UI_COL_ORANGE);
    ui_font_text(18, 2, UI_COL_ORANGE, "DBG", UI_FONT_SM);
    ui_gfx_fill(0, UI_SCREEN_H - 14, UI_SCREEN_W, 14, UI_RGB(40, 30, 0));
    if (g_dbg1[0]) {
        ui_font_text_clip(4, UI_SCREEN_H - 12, UI_SCREEN_W - 8, UI_COL_ORANGE, g_dbg1, UI_FONT_SM);
    }
    if (g_dbg2[0]) {
        ui_font_text_clip(240, UI_SCREEN_H - 12, 236, UI_COL_ORANGE, g_dbg2, UI_FONT_SM);
    }
#endif
}

const u32 *ui_backbuffer(void) {
    return g_draw;
}

int ui_save_screenshot(const char *path) {
    (void)path;
    return -1; /* screenshots removed in release */
}
