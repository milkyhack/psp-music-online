#ifndef THEME_H
#define THEME_H

#include <psptypes.h>

#define SKIN_COUNT 15
#define THEME_COUNT SKIN_COUNT

/* Visualizer modes — smooth reinterpretations of each theme language */
enum {
    VIZ_WINAMP_SPEC = 0,
    VIZ_SCOPE,
    VIZ_LED_BAR,
    VIZ_ANALOG_VU,
    VIZ_CASSETTE_METERS,
    VIZ_CRT_SCAN,
    VIZ_SOFT_SPEC,   /* was PIXEL_SPEC — smooth blocks */
    VIZ_MD_WAVE,
    VIZ_CD_RING,
    VIZ_MATRIX_RAIN,
    VIZ_CYBER_GRID,
    VIZ_XMB_WAVE,
    VIZ_DC_ORANGE,
    VIZ_ARCADE_NEON,
    VIZ_DOS_BARS     /* was DOS_ASCII — soft phosphor bars */
};

#define VIZ_PIXEL_SPEC VIZ_SOFT_SPEC
#define VIZ_DOS_ASCII VIZ_DOS_BARS

/* Now-playing composition / hardware shell */
enum {
    COMP_WALKMAN = 0,
    COMP_WINAMP,
    COMP_CASSETTE,
    COMP_MINIDISC,
    COMP_CD,
    COMP_GAMEBOY,
    COMP_GBC,
    COMP_DOS,
    COMP_MATRIX,
    COMP_CYBER,
    COMP_CRT,
    COMP_PS2,
    COMP_XMB,
    COMP_DREAMCAST,
    COMP_ARCADE
};

enum {
    CHROME_METAL = 0,
    CHROME_PLASTIC,
    CHROME_FLAT,
    CHROME_NEON,
    CHROME_WOOD
};

enum {
    PROG_BAR = 0,
    PROG_LED,
    PROG_NEEDLE,
    PROG_BLOCKS,
    PROG_RING
};

enum {
    CURSOR_BAR = 0,
    CURSOR_BLOCK,
    CURSOR_UNDER,
    CURSOR_GLOW
};

enum {
    TYPE_NORMAL = 0,
    TYPE_LCD_BIG,
    TYPE_MONO,
    TYPE_TINY
};

typedef struct {
    const char *name;
    u32 bg;
    u32 chrome;
    u32 chrome_hi;
    u32 chrome_lo;
    u32 lcd;
    u32 lcd_fg;
    u32 lcd_dim;
    u32 accent;
    u32 seek;
    u32 text;
    u32 muted;
    u32 bar_bg;
    u32 motif;
    /* Extended surface tokens for shared screens */
    u32 panel;
    u32 card;
    u32 header;
    u32 success;
    u32 danger;
    u8 viz;
    u8 composition;
    u8 chrome_style;
    u8 progress_style;
    u8 cursor_style;
    u8 type_mode;
    u8 radius;       /* default corner radius 2..8 */
    u8 density;      /* 0 compact, 1 normal, 2 spacious */
    /* Full design system §38 (derived at runtime if 0) */
    u32 background;
    u32 surface;
    u32 surface_elevated;
    u32 primary;
    u32 secondary;
    u32 text_secondary;
    u32 border;
    u32 active;
    u32 hover;
    u32 pressed;
    u32 disabled;
    u32 error;
    u32 warning;
} PlayerSkin;

typedef PlayerSkin PlayerTheme;

void theme_init(void);
int theme_load(void);
int theme_save(void);

int skin_get_id(void);
void skin_set_id(int id);
int skin_preview_id(void);
void skin_set_preview(int id);
void skin_apply_preview(void);

const PlayerSkin *skin_get(int id);
const PlayerSkin *skin_active(void);
const char *skin_name(int id);

/* Compat shims (skin == former theme/layout id). */
int theme_get_id(void);
int layout_get_id(void);
void theme_set_id(int id);
void layout_set_id(int id);
int theme_preview_id(void);
int layout_preview_id(void);
void theme_set_preview(int theme_id, int layout_id);
void theme_apply_preview(void);
const PlayerTheme *theme_get(int id);
const PlayerTheme *theme_active(void);
const char *theme_name(int id);
const char *layout_name(int id);

#endif
