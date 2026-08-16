#ifndef UI_GRAPHICS_H
#define UI_GRAPHICS_H

#include <psptypes.h>
#include "ui_image.h"

#define UI_SCREEN_W 480
#define UI_SCREEN_H 272

/* PSP 8888 = 0xAABBGGRR */
#define UI_RGB(r, g, b) (0xFF000000u | ((u32)(b) << 16) | ((u32)(g) << 8) | (u32)(r))

/* Fallback / legacy aliases — prefer skin_active() tokens */
#define UI_COL_BG        UI_RGB(18, 18, 18)
#define UI_COL_HEADER    UI_RGB(24, 24, 24)
#define UI_COL_PANEL     UI_RGB(32, 32, 32)
#define UI_COL_CARD      UI_RGB(48, 48, 48)
#define UI_COL_SELECT    UI_RGB(55, 55, 55)
#define UI_COL_ACCENT    UI_RGB(29, 185, 84)
#define UI_COL_ORANGE    UI_RGB(255, 165, 0)
#define UI_COL_TEXT      UI_RGB(255, 255, 255)
#define UI_COL_TEXT_INV  UI_RGB(255, 255, 255)
#define UI_COL_MUTED     UI_RGB(179, 179, 179)
#define UI_COL_LINE      UI_RGB(50, 50, 50)
#define UI_COL_BAR_BG    UI_RGB(64, 64, 64)
#define UI_COL_BAR_FG    UI_RGB(29, 185, 84)
#define UI_COL_SILVER    UI_RGB(48, 48, 48)
#define UI_COL_BLACK     UI_RGB(0, 0, 0)
#define UI_COL_CHROME    UI_RGB(48, 48, 52)
#define UI_COL_CHROME_HI UI_RGB(90, 90, 98)
#define UI_COL_CHROME_LO UI_RGB(28, 28, 32)
#define UI_COL_LCD       UI_RGB(0, 0, 0)
#define UI_COL_LCD_GREEN UI_RGB(40, 220, 80)
#define UI_COL_LCD_DIM   UI_RGB(0, 100, 40)
#define UI_COL_SEEK      UI_RGB(255, 180, 0)
#define UI_COL_HERO_TOP  UI_RGB(29, 185, 84)

#define UI_QUEUE_VISIBLE 8
#define UI_EQ_COUNT 5

typedef struct {
    const char *title;
    const char *artist;
    const char *album;
    int rating;
    int offline_saved;
    const char *status;
    int playing;
    int paused;
    int elapsed_ms;
    int duration_ms;
    int volume; /* 0..100 */
    int bitrate_kbps;
    int sample_khz;
    int stereo;
    int shuffle;
    int repeat;
    int show_eq;
    int eq_preset; /* applied */
    int eq_cursor; /* highlighted in EQ panel */
    int theme_id;
    int layout_id;
    /* Real queue */
    const char **queue_titles;
    const char **queue_artists;
    int queue_count;
    int queue_index;  /* currently playing index, -1 if unknown */
    int queue_cursor; /* selection when queue_focus */
    int queue_focus;  /* 1 = queue overlay / TOC focused */
    /* Download / buffer / format */
    int buffering;
    int dl_bytes;
    int dl_total;
    int save_pending;
    int lossless;
    int bit_depth;
    int online;
    float buffered_sec;
    int buffer_state;
    const char *format_name;
    const UiCover *cover;
    /* Explicit download UI (§48) — distinct from stream buffer */
    int dl_state; /* DownloadState */
    int dl_percent;
    float dl_speed_bps;
    const char *dl_name;
} UiNowPlaying;

typedef struct {
    const char *now_title;
    const char *now_artist;
    int elapsed_ms;
    int duration_ms;
    int paused;
    int track_id; /* for mini cover */
} UiMiniPlayer;

void ui_init(void);
void ui_begin(void);
void ui_end(void);

void ui_clear(u32 color);
void ui_fill(int x, int y, int w, int h, u32 color);
void ui_text(int x, int y, u32 color, const char *s);
void ui_text_clip(int x, int y, int max_w, u32 color, const char *s);

void ui_draw_header(const char *title, int playing);
void ui_set_loading(int on);
void ui_draw_list(
    const char *title,
    const char **labels,
    const char **rights,
    int count,
    int cursor,
    int playing
);
void ui_draw_library(
    const char *title,
    const char **labels,
    const char **rights,
    const int *track_ids,
    int count,
    int cursor,
    int playing,
    const UiMiniPlayer *mini
);
/* Same as ui_draw_library, plus optional per-row icons (UI_ICON_* or UI_ICON_NONE). */
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
);
void ui_draw_now_playing(const UiNowPlaying *np);
void ui_draw_message(const char *title, const char *line1, const char *line2, int playing);
/* Neon Terminal / Setup: coloured octets + footer hints. */
void ui_draw_setup(
    const char *title,
    const int octets[4],
    int selected_octet,
    int port,
    int focus_port,
    int playing
);
void ui_draw_footer_hints(const char *x_label, const char *o_label);

/* Up to 4 body lines + footer hint (info screens). */
void ui_draw_info(
    const char *title,
    const char *line1,
    const char *line2,
    const char *line3,
    const char *footer,
    int playing
);

void ui_draw_appearance(
    int focus_themes,
    int theme_cursor,
    int layout_cursor,
    const char *title_hint,
    const char *artist_hint,
    int playing
);

void ui_set_debug(const char *line1, const char *line2);
void ui_draw_debug_overlay(void);
const u32 *ui_backbuffer(void);
int ui_save_screenshot(const char *path);

#endif
