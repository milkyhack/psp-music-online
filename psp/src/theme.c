#include "theme.h"
#include "paths.h"
#include "player.h"

#include <pspiofilemgr.h>
#include <stdio.h>
#include <string.h>

#define UI_RGB(r, g, b) (0xFF000000u | ((u32)(b) << 16) | ((u32)(g) << 8) | (u32)(r))

static int g_skin_id = 0;
static int g_prev_skin = 0;

/*
 * 15 modern dark colour presets — one shared AAA layout.
 * Names are English. Palettes stay dark with a single accent.
 */
static const PlayerSkin SKINS[SKIN_COUNT] = {
    /* 0 Neon Terminal — 1:1 with README docs/assets mockups (#1ABC3B on pure black) */
    {"Neon Terminal",
     UI_RGB(0, 0, 0), UI_RGB(18, 22, 18), UI_RGB(48, 64, 48), UI_RGB(8, 10, 8),
     UI_RGB(0, 0, 0), UI_RGB(32, 200, 68), UI_RGB(18, 90, 40), UI_RGB(32, 200, 68),
     UI_RGB(32, 200, 68), UI_RGB(255, 255, 255), UI_RGB(136, 136, 136), UI_RGB(24, 24, 24),
     UI_RGB(32, 200, 68),
     UI_RGB(0, 0, 0), UI_RGB(39, 39, 39), UI_RGB(0, 0, 0),
     UI_RGB(32, 200, 68), UI_RGB(233, 70, 70),
     VIZ_MATRIX_RAIN, COMP_MATRIX, CHROME_NEON, PROG_BAR, CURSOR_BAR, TYPE_MONO, 4, 1},

    /* 1 Ocean */
    {"Ocean",
     UI_RGB(6, 10, 18), UI_RGB(24, 36, 52), UI_RGB(40, 60, 84), UI_RGB(12, 18, 28),
     UI_RGB(8, 12, 20), UI_RGB(240, 248, 255), UI_RGB(90, 120, 150), UI_RGB(64, 180, 255),
     UI_RGB(64, 180, 255), UI_RGB(240, 248, 255), UI_RGB(140, 170, 200), UI_RGB(28, 40, 56),
     UI_RGB(64, 180, 255),
     UI_RGB(14, 22, 34), UI_RGB(28, 42, 62), UI_RGB(8, 12, 20),
     UI_RGB(64, 200, 180), UI_RGB(233, 70, 70),
     VIZ_SCOPE, COMP_CD, CHROME_FLAT, PROG_BAR, CURSOR_GLOW, TYPE_NORMAL, 10, 2},

    /* 2 Violet */
    {"Violet",
     UI_RGB(10, 8, 16), UI_RGB(34, 28, 48), UI_RGB(56, 46, 78), UI_RGB(18, 14, 26),
     UI_RGB(12, 10, 18), UI_RGB(250, 245, 255), UI_RGB(130, 110, 160), UI_RGB(180, 110, 255),
     UI_RGB(180, 110, 255), UI_RGB(250, 245, 255), UI_RGB(170, 150, 200), UI_RGB(36, 30, 52),
     UI_RGB(180, 110, 255),
     UI_RGB(20, 16, 30), UI_RGB(38, 32, 56), UI_RGB(12, 10, 18),
     UI_RGB(150, 120, 255), UI_RGB(233, 70, 70),
     VIZ_SOFT_SPEC, COMP_CYBER, CHROME_FLAT, PROG_BAR, CURSOR_GLOW, TYPE_NORMAL, 10, 2},

    /* 3 Ember */
    {"Ember",
     UI_RGB(12, 8, 8), UI_RGB(40, 26, 22), UI_RGB(64, 40, 32), UI_RGB(20, 12, 10),
     UI_RGB(14, 10, 8), UI_RGB(255, 248, 240), UI_RGB(160, 110, 90), UI_RGB(255, 120, 64),
     UI_RGB(255, 140, 70), UI_RGB(255, 248, 240), UI_RGB(190, 150, 130), UI_RGB(42, 28, 24),
     UI_RGB(255, 120, 64),
     UI_RGB(22, 14, 12), UI_RGB(40, 26, 22), UI_RGB(12, 8, 8),
     UI_RGB(255, 150, 80), UI_RGB(233, 70, 70),
     VIZ_LED_BAR, COMP_WALKMAN, CHROME_FLAT, PROG_BAR, CURSOR_GLOW, TYPE_NORMAL, 10, 2},

    /* 4 Mint */
    {"Mint",
     UI_RGB(6, 14, 12), UI_RGB(22, 40, 36), UI_RGB(36, 64, 56), UI_RGB(10, 20, 18),
     UI_RGB(8, 16, 14), UI_RGB(240, 255, 250), UI_RGB(100, 150, 135), UI_RGB(60, 220, 170),
     UI_RGB(60, 220, 170), UI_RGB(240, 255, 250), UI_RGB(150, 190, 175), UI_RGB(24, 42, 38),
     UI_RGB(60, 220, 170),
     UI_RGB(12, 24, 20), UI_RGB(26, 44, 38), UI_RGB(6, 14, 12),
     UI_RGB(60, 220, 170), UI_RGB(233, 70, 70),
     VIZ_MD_WAVE, COMP_MINIDISC, CHROME_FLAT, PROG_BAR, CURSOR_GLOW, TYPE_NORMAL, 10, 2},

    /* 5 Rose */
    {"Rose",
     UI_RGB(14, 8, 12), UI_RGB(42, 26, 36), UI_RGB(68, 40, 56), UI_RGB(22, 12, 18),
     UI_RGB(16, 10, 14), UI_RGB(255, 245, 250), UI_RGB(170, 120, 145), UI_RGB(255, 90, 140),
     UI_RGB(255, 90, 140), UI_RGB(255, 245, 250), UI_RGB(200, 150, 170), UI_RGB(44, 28, 38),
     UI_RGB(255, 90, 140),
     UI_RGB(24, 14, 20), UI_RGB(44, 28, 38), UI_RGB(14, 8, 12),
     UI_RGB(255, 110, 150), UI_RGB(233, 70, 70),
     VIZ_SOFT_SPEC, COMP_ARCADE, CHROME_FLAT, PROG_BAR, CURSOR_GLOW, TYPE_NORMAL, 10, 2},

    /* 6 Amber */
    {"Amber",
     UI_RGB(12, 10, 6), UI_RGB(40, 34, 18), UI_RGB(64, 54, 28), UI_RGB(20, 16, 8),
     UI_RGB(14, 12, 8), UI_RGB(255, 250, 235), UI_RGB(170, 150, 100), UI_RGB(255, 190, 50),
     UI_RGB(255, 190, 50), UI_RGB(255, 250, 235), UI_RGB(200, 180, 130), UI_RGB(42, 36, 20),
     UI_RGB(255, 190, 50),
     UI_RGB(22, 18, 10), UI_RGB(40, 34, 18), UI_RGB(12, 10, 6),
     UI_RGB(255, 200, 70), UI_RGB(233, 70, 70),
     VIZ_ANALOG_VU, COMP_CASSETTE, CHROME_FLAT, PROG_BAR, CURSOR_GLOW, TYPE_NORMAL, 10, 2},

    /* 7 Ice */
    {"Ice",
     UI_RGB(8, 10, 14), UI_RGB(28, 34, 42), UI_RGB(48, 58, 70), UI_RGB(14, 18, 24),
     UI_RGB(10, 12, 16), UI_RGB(245, 250, 255), UI_RGB(130, 150, 170), UI_RGB(140, 210, 255),
     UI_RGB(140, 210, 255), UI_RGB(245, 250, 255), UI_RGB(170, 190, 210), UI_RGB(30, 36, 46),
     UI_RGB(140, 210, 255),
     UI_RGB(16, 20, 28), UI_RGB(32, 40, 52), UI_RGB(8, 10, 14),
     UI_RGB(140, 210, 255), UI_RGB(233, 70, 70),
     VIZ_CRT_SCAN, COMP_CRT, CHROME_FLAT, PROG_BAR, CURSOR_GLOW, TYPE_NORMAL, 10, 2},

    /* 8 Midnight — former default (Spotify-green premium) */
    {"Midnight",
     UI_RGB(7, 7, 10), UI_RGB(28, 28, 36), UI_RGB(58, 58, 72), UI_RGB(14, 14, 18),
     UI_RGB(10, 10, 14), UI_RGB(244, 244, 248), UI_RGB(138, 138, 150), UI_RGB(30, 215, 96),
     UI_RGB(30, 215, 96), UI_RGB(244, 244, 248), UI_RGB(138, 138, 150), UI_RGB(36, 36, 48),
     UI_RGB(30, 215, 96),
     UI_RGB(16, 16, 24), UI_RGB(28, 28, 38), UI_RGB(12, 12, 18),
     UI_RGB(30, 215, 96), UI_RGB(233, 70, 70),
     VIZ_SOFT_SPEC, COMP_WINAMP, CHROME_FLAT, PROG_BAR, CURSOR_GLOW, TYPE_NORMAL, 12, 2},

    /* 9 Graphite */
    {"Graphite",
     UI_RGB(10, 10, 12), UI_RGB(32, 32, 36), UI_RGB(52, 52, 58), UI_RGB(18, 18, 20),
     UI_RGB(12, 12, 14), UI_RGB(250, 250, 252), UI_RGB(130, 130, 138), UI_RGB(220, 220, 230),
     UI_RGB(220, 220, 230), UI_RGB(250, 250, 252), UI_RGB(170, 170, 178), UI_RGB(36, 36, 40),
     UI_RGB(220, 220, 230),
     UI_RGB(20, 20, 24), UI_RGB(36, 36, 42), UI_RGB(10, 10, 12),
     UI_RGB(200, 200, 210), UI_RGB(233, 70, 70),
     VIZ_SOFT_SPEC, COMP_PS2, CHROME_FLAT, PROG_BAR, CURSOR_GLOW, TYPE_NORMAL, 10, 2},

    /* 10 Crimson */
    {"Crimson",
     UI_RGB(12, 6, 8), UI_RGB(40, 18, 24), UI_RGB(64, 28, 36), UI_RGB(20, 10, 12),
     UI_RGB(14, 8, 10), UI_RGB(255, 245, 248), UI_RGB(170, 100, 120), UI_RGB(255, 60, 90),
     UI_RGB(255, 60, 90), UI_RGB(255, 245, 248), UI_RGB(200, 140, 155), UI_RGB(42, 20, 26),
     UI_RGB(255, 60, 90),
     UI_RGB(22, 12, 14), UI_RGB(42, 20, 26), UI_RGB(12, 6, 8),
     UI_RGB(255, 80, 100), UI_RGB(255, 60, 90),
     VIZ_ARCADE_NEON, COMP_ARCADE, CHROME_FLAT, PROG_BAR, CURSOR_GLOW, TYPE_NORMAL, 10, 2},

    /* 11 Indigo */
    {"Indigo",
     UI_RGB(8, 8, 18), UI_RGB(28, 28, 52), UI_RGB(46, 46, 84), UI_RGB(14, 14, 28),
     UI_RGB(10, 10, 20), UI_RGB(240, 242, 255), UI_RGB(120, 120, 170), UI_RGB(110, 120, 255),
     UI_RGB(110, 120, 255), UI_RGB(240, 242, 255), UI_RGB(160, 165, 210), UI_RGB(30, 30, 56),
     UI_RGB(110, 120, 255),
     UI_RGB(16, 16, 32), UI_RGB(32, 32, 60), UI_RGB(8, 8, 18),
     UI_RGB(110, 120, 255), UI_RGB(233, 70, 70),
     VIZ_XMB_WAVE, COMP_XMB, CHROME_FLAT, PROG_BAR, CURSOR_GLOW, TYPE_NORMAL, 10, 2},

    /* 12 Sand */
    {"Sand",
     UI_RGB(16, 14, 12), UI_RGB(42, 38, 32), UI_RGB(68, 60, 50), UI_RGB(24, 20, 16),
     UI_RGB(18, 16, 14), UI_RGB(255, 250, 240), UI_RGB(160, 145, 120), UI_RGB(230, 190, 120),
     UI_RGB(230, 190, 120), UI_RGB(255, 250, 240), UI_RGB(190, 175, 150), UI_RGB(44, 40, 34),
     UI_RGB(230, 190, 120),
     UI_RGB(26, 22, 18), UI_RGB(46, 40, 34), UI_RGB(16, 14, 12),
     UI_RGB(230, 190, 120), UI_RGB(233, 70, 70),
     VIZ_SOFT_SPEC, COMP_DREAMCAST, CHROME_FLAT, PROG_BAR, CURSOR_GLOW, TYPE_NORMAL, 10, 2},

    /* 13 Forest */
    {"Forest",
     UI_RGB(6, 12, 8), UI_RGB(20, 36, 24), UI_RGB(34, 58, 40), UI_RGB(10, 18, 12),
     UI_RGB(8, 14, 10), UI_RGB(240, 255, 245), UI_RGB(100, 140, 110), UI_RGB(80, 200, 110),
     UI_RGB(80, 200, 110), UI_RGB(240, 255, 245), UI_RGB(140, 180, 150), UI_RGB(22, 38, 26),
     UI_RGB(80, 200, 110),
     UI_RGB(12, 22, 14), UI_RGB(26, 42, 30), UI_RGB(6, 12, 8),
     UI_RGB(80, 200, 110), UI_RGB(233, 70, 70),
     VIZ_SOFT_SPEC, COMP_GAMEBOY, CHROME_FLAT, PROG_BAR, CURSOR_GLOW, TYPE_NORMAL, 10, 2},

    /* 14 Aurora */
    {"Aurora",
     UI_RGB(6, 8, 14), UI_RGB(22, 30, 48), UI_RGB(36, 50, 78), UI_RGB(10, 14, 24),
     UI_RGB(8, 10, 16), UI_RGB(245, 250, 255), UI_RGB(110, 140, 180), UI_RGB(90, 220, 200),
     UI_RGB(90, 220, 200), UI_RGB(245, 250, 255), UI_RGB(150, 180, 210), UI_RGB(24, 34, 52),
     UI_RGB(90, 220, 200),
     UI_RGB(12, 18, 30), UI_RGB(28, 40, 62), UI_RGB(6, 8, 14),
     UI_RGB(90, 220, 200), UI_RGB(233, 70, 70),
     VIZ_DC_ORANGE, COMP_DREAMCAST, CHROME_FLAT, PROG_BAR, CURSOR_GLOW, TYPE_NORMAL, 10, 2},
};

void theme_init(void) {
    g_skin_id = 0;
    g_prev_skin = 0;
}

static void theme_path(char *out, int out_sz) {
    paths_join(out, out_sz, "data/ui.cfg");
}

int theme_load(void) {
    char path[280];
    char line[96];
    SceUID fd;
    int skin = 0;
    int eq = 0;
    int n;

    theme_path(path, sizeof(path));
    fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
    if (fd < 0) {
        return -1;
    }
    n = sceIoRead(fd, line, sizeof(line) - 1);
    sceIoClose(fd);
    if (n <= 0) {
        return -1;
    }
    line[n] = '\0';
    if (sscanf(line, "skin %d eq %d", &skin, &eq) >= 1) {
        if (skin < 0) {
            skin = 0;
        }
        if (skin >= SKIN_COUNT) {
            skin = SKIN_COUNT - 1;
        }
        g_skin_id = skin;
        g_prev_skin = skin;
        if (eq >= 0 && eq < PLAYER_EQ_COUNT) {
            player_set_eq_preset(eq);
        }
        return 0;
    }
    return -1;
}

int theme_save(void) {
    char path[280];
    char line[64];
    SceUID fd;
    int n;

    paths_ensure_data();
    theme_path(path, sizeof(path));
    fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0) {
        return -1;
    }
    n = snprintf(line, sizeof(line), "skin %d eq %d\n", g_skin_id, player_get_eq_preset());
    sceIoWrite(fd, line, n);
    sceIoClose(fd);
    return 0;
}

int skin_get_id(void) {
    return g_skin_id;
}

void skin_set_id(int id) {
    if (id < 0) {
        id = 0;
    }
    if (id >= SKIN_COUNT) {
        id = SKIN_COUNT - 1;
    }
    g_skin_id = id;
    g_prev_skin = id;
}

int skin_preview_id(void) {
    return g_prev_skin;
}

void skin_set_preview(int id) {
    if (id < 0) {
        id = 0;
    }
    if (id >= SKIN_COUNT) {
        id = SKIN_COUNT - 1;
    }
    g_prev_skin = id;
}

void skin_apply_preview(void) {
    g_skin_id = g_prev_skin;
    theme_save();
}

const PlayerSkin *skin_get(int id) {
    static PlayerSkin resolved;
    const PlayerSkin *src;
    if (id < 0 || id >= SKIN_COUNT) {
        id = 0;
    }
    src = &SKINS[id];
    resolved = *src;
    /* Derive §38 tokens from existing palette when unset (0). */
    if (!resolved.background) resolved.background = resolved.bg;
    if (!resolved.surface) resolved.surface = resolved.panel;
    if (!resolved.surface_elevated) resolved.surface_elevated = resolved.card;
    if (!resolved.primary) resolved.primary = resolved.accent;
    if (!resolved.secondary) resolved.secondary = resolved.motif;
    if (!resolved.text_secondary) resolved.text_secondary = resolved.muted;
    if (!resolved.border) resolved.border = resolved.chrome_lo;
    if (!resolved.active) resolved.active = resolved.accent;
    if (!resolved.hover) resolved.hover = resolved.chrome_hi;
    if (!resolved.pressed) resolved.pressed = resolved.chrome;
    if (!resolved.disabled) resolved.disabled = resolved.bar_bg;
    if (!resolved.error) resolved.error = resolved.danger;
    if (!resolved.warning) resolved.warning = resolved.seek;
    return &resolved;
}

const PlayerSkin *skin_active(void) {
    return skin_get(g_skin_id);
}

const char *skin_name(int id) {
    return skin_get(id)->name;
}

int theme_get_id(void) {
    return skin_get_id();
}

int layout_get_id(void) {
    return skin_get_id();
}

void theme_set_id(int id) {
    skin_set_id(id);
}

void layout_set_id(int id) {
    skin_set_id(id);
}

int theme_preview_id(void) {
    return skin_preview_id();
}

int layout_preview_id(void) {
    return skin_preview_id();
}

void theme_set_preview(int theme_id, int layout_id) {
    (void)layout_id;
    skin_set_preview(theme_id);
}

void theme_apply_preview(void) {
    skin_apply_preview();
}

const PlayerTheme *theme_get(int id) {
    return skin_get(id);
}

const PlayerTheme *theme_active(void) {
    return skin_active();
}

const char *theme_name(int id) {
    return skin_name(id);
}

const char *layout_name(int id) {
    return skin_name(id);
}
