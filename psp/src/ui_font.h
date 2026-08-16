#ifndef UI_FONT_H
#define UI_FONT_H

#include <psptypes.h>

enum {
    UI_FONT_SM = 0, /* 8px bitmap — HUD / metadata */
    UI_FONT_MD = 1, /* 16px bitmap — lists / body */
    UI_FONT_LG = 2  /* 16px bitmap — titles */
};

void ui_font_init(void);

int ui_font_char_w(int size);
int ui_font_char_h(int size);
int ui_font_text_w(const char *s, int size);

void ui_font_glyph(int x, int y, u32 color, char ch, int size);
void ui_font_text(int x, int y, u32 color, const char *s, int size);
void ui_font_text_clip(int x, int y, int max_w, u32 color, const char *s, int size);
void ui_font_text_shadow(int x, int y, u32 color, u32 shadow, const char *s, int size);

/* Soft vector-like transport icons (not 1-bit sprites). */
enum {
    UI_ICON_PREV = 0,
    UI_ICON_PLAY,
    UI_ICON_PAUSE,
    UI_ICON_STOP,
    UI_ICON_NEXT,
    UI_ICON_EQ,
    UI_ICON_SHUF,
    UI_ICON_RPT,
    UI_ICON_DL,
    UI_ICON_COUNT
};

void ui_font_icon(int x, int y, int size, int icon, u32 color);

#endif
