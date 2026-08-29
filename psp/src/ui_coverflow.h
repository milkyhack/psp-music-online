#ifndef UI_COVERFLOW_H
#define UI_COVERFLOW_H

#include "theme.h"
#include "ui_image.h"

#include <psptypes.h>

/* Sony PSP Player Coverflow v2.5 — 3D carousel (perspective + Y-rotate + reflection). */
#define UI_COVERFLOW_DISTANCE 4.0f
#define UI_COVERFLOW_ZAXIS (-3.3f)
/* Original homebrew uses 128×128 cover textures in VRAM. */
#define UI_COVERFLOW_TEX 128
#define UI_COVERFLOW_TEX_CACHE 8

void ui_coverflow_reset(void);
void ui_coverflow_prepare(int cursor, int count, const int *track_ids);
void ui_coverflow_prepare_hero(const UiCover *hero_cover);
void ui_coverflow_set_center(const UiCover *hero_cover);
int ui_coverflow_is_active(void);
void ui_coverflow_animate(void);

void ui_coverflow_draw_stage(const PlayerTheme *th, const char *title, int playing, int footer_h);
void ui_coverflow_draw_label(const PlayerTheme *th, const char *name, int footer_h);

int ui_coverflow_wants_3d(void);
void ui_coverflow_draw_covers_cpu(void);
void ui_coverflow_on_present_done(void);

/* Draw 3D carousel into the display buffer (offset-based GU — never raw 0x40... ptrs). */
void ui_coverflow_render_gu(int vram_page, int buf_stride);

#endif /* UI_COVERFLOW_H */
