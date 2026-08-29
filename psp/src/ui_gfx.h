#ifndef UI_GFX_API_H
#define UI_GFX_API_H

#include <psptypes.h>

#define UI_GFX_W 480
#define UI_GFX_H 272
#define UI_GFX_STRIDE 512

void ui_gfx_bind(u32 *backbuffer);

void ui_gfx_clear(u32 color);
void ui_gfx_fill(int x, int y, int w, int h, u32 color);
void ui_gfx_fill_alpha(int x, int y, int w, int h, u32 color, int alpha /*0..255*/);
void ui_gfx_hline(int x, int y, int w, u32 color);
void ui_gfx_vline(int x, int y, int h, u32 color);
void ui_gfx_rect(int x, int y, int w, int h, u32 color);

/* Soft rounded fill (r capped for PSP perf). */
void ui_gfx_round_fill(int x, int y, int w, int h, int r, u32 color);
void ui_gfx_round_fill_alpha(int x, int y, int w, int h, int r, u32 color, int alpha);

/* Vertical / horizontal gradient. */
void ui_gfx_grad_v(int x, int y, int w, int h, u32 top, u32 bot);
void ui_gfx_grad_h(int x, int y, int w, int h, u32 left, u32 right);

/* Soft circle / ring (cheap coverage). */
void ui_gfx_circle_fill(int cx, int cy, int r, u32 color);
void ui_gfx_circle_fill_alpha(int cx, int cy, int r, u32 color, int alpha);
void ui_gfx_ring(int cx, int cy, int r, int thick, u32 color);

/* Soft bevel / panel helpers. */
void ui_gfx_bevel(int x, int y, int w, int h, int raised, u32 hi, u32 lo);
void ui_gfx_panel(int x, int y, int w, int h, u32 face, u32 hi, u32 lo, int r);
void ui_gfx_inset(int x, int y, int w, int h, u32 face, u32 hi, u32 lo, int r);

/* Premium PSP chrome: glass face, hairline frame, soft bloom. */
void ui_gfx_glass(int x, int y, int w, int h, int r, u32 face, u32 hi, int alpha);
void ui_gfx_hairline_rect(int x, int y, int w, int h, u32 color, int alpha);
void ui_gfx_bloom(int cx, int cy, int r, u32 color, int alpha);

/* Soft brushed strip (subtle, not pixel stripes). */
void ui_gfx_brush(int x, int y, int w, int h, u32 mid, u32 hi, u32 lo);

/* Line (Bresenham). */
void ui_gfx_line(int x0, int y0, int x1, int y1, u32 color);

/* Mix two ABGR8888 colors by t (0..256). */
u32 ui_gfx_lerp(u32 a, u32 b, int t);

/* Extract / pack. */
u32 ui_gfx_rgb(int r, int g, int b);
void ui_gfx_unpack(u32 c, int *r, int *g, int *b);

u32 *ui_gfx_buffer(void);

#endif /* UI_GFX_API_H */
