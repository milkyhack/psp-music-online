#include "ui_gfx.h"

#include <string.h>

static u32 *g_buf = NULL;

void ui_gfx_bind(u32 *backbuffer) {
    g_buf = backbuffer;
}

u32 *ui_gfx_buffer(void) {
    return g_buf;
}

u32 ui_gfx_rgb(int r, int g, int b) {
    if (r < 0) {
        r = 0;
    }
    if (r > 255) {
        r = 255;
    }
    if (g < 0) {
        g = 0;
    }
    if (g > 255) {
        g = 255;
    }
    if (b < 0) {
        b = 0;
    }
    if (b > 255) {
        b = 255;
    }
    return 0xFF000000u | ((u32)b << 16) | ((u32)g << 8) | (u32)r;
}

void ui_gfx_unpack(u32 c, int *r, int *g, int *b) {
    if (r) {
        *r = (int)(c & 0xFF);
    }
    if (g) {
        *g = (int)((c >> 8) & 0xFF);
    }
    if (b) {
        *b = (int)((c >> 16) & 0xFF);
    }
}

u32 ui_gfx_lerp(u32 a, u32 b, int t) {
    int ar, ag, ab, br, bg, bb;
    if (t <= 0) {
        return a;
    }
    if (t >= 256) {
        return b;
    }
    ui_gfx_unpack(a, &ar, &ag, &ab);
    ui_gfx_unpack(b, &br, &bg, &bb);
    return ui_gfx_rgb(
        ar + ((br - ar) * t) / 256,
        ag + ((bg - ag) * t) / 256,
        ab + ((bb - ab) * t) / 256
    );
}

static void put(int x, int y, u32 color) {
    if (!g_buf) {
        return;
    }
    if (x < 0 || y < 0 || x >= UI_GFX_W || y >= UI_GFX_H) {
        return;
    }
    g_buf[y * UI_GFX_STRIDE + x] = color;
}

static void blend(int x, int y, u32 color, int alpha) {
    int sr, sg, sb, dr, dg, db;
    u32 dst;
    if (!g_buf || alpha <= 0) {
        return;
    }
    if (x < 0 || y < 0 || x >= UI_GFX_W || y >= UI_GFX_H) {
        return;
    }
    if (alpha >= 255) {
        g_buf[y * UI_GFX_STRIDE + x] = color;
        return;
    }
    dst = g_buf[y * UI_GFX_STRIDE + x];
    ui_gfx_unpack(color, &sr, &sg, &sb);
    ui_gfx_unpack(dst, &dr, &dg, &db);
    g_buf[y * UI_GFX_STRIDE + x] = ui_gfx_rgb(
        dr + ((sr - dr) * alpha) / 255,
        dg + ((sg - dg) * alpha) / 255,
        db + ((sb - db) * alpha) / 255
    );
}

void ui_gfx_clear(u32 color) {
    int i;
    if (!g_buf) {
        return;
    }
    for (i = 0; i < UI_GFX_STRIDE * UI_GFX_H; i++) {
        g_buf[i] = color;
    }
}

void ui_gfx_fill(int x, int y, int w, int h, u32 color) {
    int yy;
    if (!g_buf) {
        return;
    }
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > UI_GFX_W) {
        w = UI_GFX_W - x;
    }
    if (y + h > UI_GFX_H) {
        h = UI_GFX_H - y;
    }
    if (w <= 0 || h <= 0) {
        return;
    }
    for (yy = y; yy < y + h; yy++) {
        int xx;
        u32 *row = g_buf + yy * UI_GFX_STRIDE;
        for (xx = x; xx < x + w; xx++) {
            row[xx] = color;
        }
    }
}

void ui_gfx_fill_alpha(int x, int y, int w, int h, u32 color, int alpha) {
    int yy, xx;
    if (alpha >= 255) {
        ui_gfx_fill(x, y, w, h, color);
        return;
    }
    if (alpha <= 0 || !g_buf) {
        return;
    }
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > UI_GFX_W) {
        w = UI_GFX_W - x;
    }
    if (y + h > UI_GFX_H) {
        h = UI_GFX_H - y;
    }
    if (w <= 0 || h <= 0) {
        return;
    }
    for (yy = y; yy < y + h; yy++) {
        for (xx = x; xx < x + w; xx++) {
            blend(xx, yy, color, alpha);
        }
    }
}

void ui_gfx_hline(int x, int y, int w, u32 color) {
    ui_gfx_fill(x, y, w, 1, color);
}

void ui_gfx_vline(int x, int y, int h, u32 color) {
    ui_gfx_fill(x, y, 1, h, color);
}

void ui_gfx_rect(int x, int y, int w, int h, u32 color) {
    ui_gfx_hline(x, y, w, color);
    ui_gfx_hline(x, y + h - 1, w, color);
    ui_gfx_vline(x, y, h, color);
    ui_gfx_vline(x + w - 1, y, h, color);
}

void ui_gfx_round_fill(int x, int y, int w, int h, int r, u32 color) {
    int i;
    if (r < 1) {
        ui_gfx_fill(x, y, w, h, color);
        return;
    }
    if (r > 10) {
        r = 10;
    }
    if (r * 2 > w) {
        r = w / 2;
    }
    if (r * 2 > h) {
        r = h / 2;
    }
    ui_gfx_fill(x + r, y, w - 2 * r, h, color);
    ui_gfx_fill(x, y + r, w, h - 2 * r, color);
    for (i = 0; i < r; i++) {
        /* Quarter-circle inset approximation with soft edge */
        int inset = r - 1 - i;
        int soft = (i == 0) ? 180 : 255;
        if (soft >= 255) {
            ui_gfx_fill(x + inset, y + i, w - 2 * inset, 1, color);
            ui_gfx_fill(x + inset, y + h - 1 - i, w - 2 * inset, 1, color);
        } else {
            ui_gfx_fill_alpha(x + inset, y + i, w - 2 * inset, 1, color, soft);
            ui_gfx_fill_alpha(x + inset, y + h - 1 - i, w - 2 * inset, 1, color, soft);
        }
    }
}

void ui_gfx_round_fill_alpha(int x, int y, int w, int h, int r, u32 color, int alpha) {
    int i;
    if (alpha >= 255) {
        ui_gfx_round_fill(x, y, w, h, r, color);
        return;
    }
    if (r < 1) {
        ui_gfx_fill_alpha(x, y, w, h, color, alpha);
        return;
    }
    if (r > 10) {
        r = 10;
    }
    ui_gfx_fill_alpha(x + r, y, w - 2 * r, h, color, alpha);
    ui_gfx_fill_alpha(x, y + r, w, h - 2 * r, color, alpha);
    for (i = 0; i < r; i++) {
        int inset = r - 1 - i;
        ui_gfx_fill_alpha(x + inset, y + i, w - 2 * inset, 1, color, alpha);
        ui_gfx_fill_alpha(x + inset, y + h - 1 - i, w - 2 * inset, 1, color, alpha);
    }
}

void ui_gfx_grad_v(int x, int y, int w, int h, u32 top, u32 bot) {
    int i;
    if (h <= 0 || w <= 0) {
        return;
    }
    for (i = 0; i < h; i++) {
        int t = (i * 256) / (h > 1 ? h - 1 : 1);
        ui_gfx_fill(x, y + i, w, 1, ui_gfx_lerp(top, bot, t));
    }
}

void ui_gfx_grad_h(int x, int y, int w, int h, u32 left, u32 right) {
    int i;
    if (h <= 0 || w <= 0) {
        return;
    }
    for (i = 0; i < w; i++) {
        int t = (i * 256) / (w > 1 ? w - 1 : 1);
        ui_gfx_fill(x + i, y, 1, h, ui_gfx_lerp(left, right, t));
    }
}

void ui_gfx_circle_fill(int cx, int cy, int r, u32 color) {
    int y;
    int outer;
    int inner2;
    if (r < 1) {
        put(cx, cy, color);
        return;
    }
    /* Coverage AA: solid core + soft 1px rim (fixed-point distance). */
    outer = (r + 1) * (r + 1);
    inner2 = (r > 1) ? (r - 1) * (r - 1) : 0;
    for (y = -r - 1; y <= r + 1; y++) {
        int x;
        int yy = y * y;
        for (x = -r - 1; x <= r + 1; x++) {
            int d2 = x * x + yy;
            int a;
            if (d2 > outer) {
                continue;
            }
            if (d2 <= inner2) {
                put(cx + x, cy + y, color);
                continue;
            }
            /* Approximate rim coverage without sqrt. */
            {
                int rr = r * r;
                int gap = d2 - rr;
                if (gap <= 0) {
                    a = 220;
                } else {
                    a = 180 - gap * 40 / (2 * r + 1);
                    if (a < 0) a = 0;
                }
            }
            if (a > 0) {
                blend(cx + x, cy + y, color, a);
            }
        }
    }
}

void ui_gfx_circle_fill_alpha(int cx, int cy, int r, u32 color, int alpha) {
    int y;
    int outer;
    if (r < 1) {
        blend(cx, cy, color, alpha);
        return;
    }
    outer = (r + 1) * (r + 1);
    for (y = -r - 1; y <= r + 1; y++) {
        int x;
        int yy = y * y;
        for (x = -r - 1; x <= r + 1; x++) {
            int d2 = x * x + yy;
            int a = alpha;
            if (d2 > outer) {
                continue;
            }
            if (d2 > r * r) {
                a = alpha / 2;
            }
            blend(cx + x, cy + y, color, a);
        }
    }
}

void ui_gfx_ring(int cx, int cy, int r, int thick, u32 color) {
    int y;
    int r2 = r * r;
    int ri = r - thick;
    int ri2 = ri > 0 ? ri * ri : 0;
    for (y = -r; y <= r; y++) {
        int x;
        int yy = y * y;
        for (x = -r; x <= r; x++) {
            int d = x * x + yy;
            if (d <= r2 && d >= ri2) {
                put(cx + x, cy + y, color);
            }
        }
    }
}

void ui_gfx_bevel(int x, int y, int w, int h, int raised, u32 hi, u32 lo) {
    u32 hicol = raised ? hi : lo;
    u32 locol = raised ? lo : hi;
    ui_gfx_hline(x, y, w, hicol);
    ui_gfx_vline(x, y, h, hicol);
    ui_gfx_hline(x, y + h - 1, w, locol);
    ui_gfx_vline(x + w - 1, y, h, locol);
}

void ui_gfx_panel(int x, int y, int w, int h, u32 face, u32 hi, u32 lo, int r) {
    ui_gfx_round_fill(x, y, w, h, r, face);
    ui_gfx_bevel(x, y, w, h, 1, hi, lo);
    ui_gfx_hline(x + 1, y + 1, w - 2, hi);
}

void ui_gfx_inset(int x, int y, int w, int h, u32 face, u32 hi, u32 lo, int r) {
    ui_gfx_round_fill(x, y, w, h, r, face);
    ui_gfx_bevel(x, y, w, h, 0, hi, lo);
}

void ui_gfx_glass(int x, int y, int w, int h, int r, u32 face, u32 hi, int alpha) {
    ui_gfx_round_fill_alpha(x, y, w, h, r, face, alpha);
    /* Top specular strip */
    ui_gfx_fill_alpha(x + r, y + 1, w - 2 * r, 1, hi, 70);
    ui_gfx_hairline_rect(x, y, w, h, hi, 55);
}

void ui_gfx_hairline_rect(int x, int y, int w, int h, u32 color, int alpha) {
    if (w < 2 || h < 2) {
        return;
    }
    ui_gfx_fill_alpha(x, y, w, 1, color, alpha);
    ui_gfx_fill_alpha(x, y + h - 1, w, 1, color, alpha);
    ui_gfx_fill_alpha(x, y, 1, h, color, alpha);
    ui_gfx_fill_alpha(x + w - 1, y, 1, h, color, alpha);
}

void ui_gfx_bloom(int cx, int cy, int r, u32 color, int alpha) {
    int ring;
    if (r < 4) {
        ui_gfx_circle_fill_alpha(cx, cy, r, color, alpha);
        return;
    }
    for (ring = 0; ring < 4; ring++) {
        int rr = r - ring * (r / 5);
        int aa = alpha - ring * (alpha / 5);
        if (rr < 2 || aa < 8) {
            break;
        }
        ui_gfx_circle_fill_alpha(cx, cy, rr, color, aa);
    }
}

void ui_gfx_brush(int x, int y, int w, int h, u32 mid, u32 hi, u32 lo) {
    int row;
    ui_gfx_grad_v(x, y, w, h, hi, lo);
    /* Soft highlight — not hard pixel stripes */
    for (row = 0; row < h; row += 6) {
        ui_gfx_fill_alpha(x, y + row, w, 1, hi, 40);
        if (row + 3 < h) {
            ui_gfx_fill_alpha(x, y + row + 3, w, 1, lo, 30);
        }
    }
    (void)mid;
}

void ui_gfx_line(int x0, int y0, int x1, int y1, u32 color) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int adx = dx < 0 ? -dx : dx;
    int ady = dy < 0 ? -dy : dy;
    int steps = adx > ady ? adx : ady;
    int i;
    if (steps < 1) {
        put(x0, y0, color);
        return;
    }
    for (i = 0; i <= steps; i++) {
        put(x0 + (dx * i) / steps, y0 + (dy * i) / steps, color);
    }
}
