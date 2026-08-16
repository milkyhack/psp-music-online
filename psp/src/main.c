#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "config.h"
#include "paths.h"
#include "net.h"
#include "http.h"
#include "jutil.h"
#include "player.h"
#include "ui.h"
#include "ui_image.h"
#include "ui_gpu.h"
#include "ui_gfx.h"
#include "ui_font.h"
#include "offline.h"
#include "debug_log.h"
#include "theme.h"
#include "osk.h"
#include "ringbuf.h"
#include "storage.h"
#include "metrics.h"
#include "download.h"
#include "updater.h"
#include "flac_dec.h"

#include <pspnet_apctl.h>

PSP_MODULE_INFO("PSP Music", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
/* Negative = leave KB free for net PRX (SDK netsample pattern). Fixed 4–12MB heap broke sceNetInit. */
PSP_HEAP_SIZE_KB(-2048);
PSP_HEAP_THRESHOLD_SIZE_KB(1024);

enum {
    SCREEN_HOME = 0,
    SCREEN_MUSIC,
    SCREEN_ARTISTS,
    SCREEN_ALBUMS,
    SCREEN_TRACKS,
    SCREEN_TOP,
    SCREEN_SEARCH,
    SCREEN_GENRES,
    SCREEN_OFFLINE,
    SCREEN_PLAYING,
    SCREEN_SETUP,
    SCREEN_APPEARANCE,
    SCREEN_SETTINGS,
    SCREEN_INFO,
    SCREEN_DIAG,
    SCREEN_UPDATE
};

enum {
    BROWSE_NONE = 0,
    BROWSE_ARTIST,
    BROWSE_ALL,
    BROWSE_GENRE,
    BROWSE_RATED
};

enum {
    LIST_NONE = 0,
    LIST_ARTISTS,
    LIST_ALL_ALBUMS,
    LIST_ARTIST_ALBUMS,
    LIST_GENRE_ALBUMS,
    LIST_RATED_ALBUMS,
    LIST_GENRES,
    LIST_TRACKS,
    LIST_ALL_SONGS,
    LIST_TOP,
    LIST_SEARCH
};

static ServerConfig g_cfg;
static int g_screen = SCREEN_HOME;
static int g_net_ok = 0;
static int g_online_mode = 0;

static ListItem g_items[MAX_LIST_ITEMS];
static char g_rights[MAX_LIST_ITEMS][MAX_HOST];
static const char *g_labels_ptr[MAX_LIST_ITEMS];
static const char *g_rights_ptr[MAX_LIST_ITEMS];
static int g_count = 0;
static int g_cursor = 0;
static int g_offset = 0;

static int g_artist_id = 0;
static int g_album_id = 0;
static int g_track_id = 0;
static char g_artist_name[MAX_NAME];
static char g_album_name[MAX_NAME];
static char g_genre_name[MAX_NAME];
static int g_album_browse = BROWSE_NONE;
static int g_list_kind = LIST_NONE;
static int g_info_return_screen = SCREEN_HOME;
static char g_info_title[48];
static char g_info_line1[96];
static char g_info_line2[MAX_INFO];
static char g_info_line3[MAX_INFO];
static char g_now_title[MAX_NAME];
static char g_now_artist[MAX_NAME];
static char g_now_album[MAX_NAME];
static int g_now_rating = 0;
static int g_now_duration_ms = 0;
static int g_now_offline = 0;
static int g_from_offline = 0;
static int g_show_eq = 0;
static int g_eq_preset = 0;
static int g_eq_cursor = 0;
static int g_appear_skin = 0;
static int g_setup_octet = 0; /* 0..3 which IPv4 octet is selected */
static int g_setup_focus = 0; /* 0=host octets, 1=port */
static int g_shuffle = 0;
static int g_repeat = 0;
static int g_shuffle_order[MAX_LIST_ITEMS];
static int g_shuffle_pos = 0;
static int g_play_index = -1;
static int g_queue_focus = 0;
static int g_queue_cursor = 0;
static int g_return_screen = SCREEN_TRACKS;
static int g_home_np = -1;
static int g_home_offline = 0;
static int g_home_online = 1;
static int g_home_top = -1;
static int g_home_wifi = -1;
static int g_home_setup = -1;
static int g_home_appear = -1;
static int g_home_more = -1;
static int g_home_settings = 2; /* legacy alias → more/settings row */
static int g_icons[MAX_LIST_ITEMS];
static int g_icons_n = 0;
static int g_appear_return = SCREEN_SETTINGS;
static int g_setup_return = SCREEN_SETTINGS;
#ifdef DEBUG_HUD
static int g_need_wifi_shot = 0;
#endif
static const char *g_queue_titles[MAX_LIST_ITEMS];
static const char *g_queue_artists[MAX_LIST_ITEMS];

static char g_status[128];
static char g_search_query[MAX_NAME];
static char g_title_buf[96];
static int g_offline_delete_confirm = 0;
static OfflineTrack g_offline_meta[OFFLINE_MAX];

/* Progressive stream into RAM ring (NO Memory Stick writes). */
static volatile int g_dl_bytes = 0;
static volatile int g_dl_total = -1;
static volatile int g_dl_done = 0;
static volatile int g_dl_err = 0;
static volatile int g_dl_aborted = 0;
static SceUID g_dl_thid = -1;
static int g_play_started = 0;
static int g_buffering = 0;
static int g_save_pending = 0;
static char g_dl_api[64];
static char g_dl_fail_reason[32];
static int g_tcp_auto_retry = 0;
static int g_decoder_auto_retry = 0; /* one free restart after sceMp3Init ate the header */
static int g_cover_after_dl = 0;     /* fetch cover only when stream download finished */
static int g_show_track_info = 0;    /* overlay on Now Playing — no screen jump */
static int g_seek_hold_ms = 0;       /* analog scrub delta from base */
static int g_seek_preview_ms = -1;   /* live preview while stick held; -1 = off */
static int g_seek_base_ms = 0;
static int g_seek_dragging = 0;
static int g_seek_busy = 0;          /* 1 while stream restart after seek */
static int g_play_reported = 0;      /* 1 after /api/plays for current track */
static int g_seek_resume_ms = 0;     /* elapsed to show/apply after start_ms seek */
static int g_pending_start_ms = 0;   /* start playback from this ms (server resume) */
static int g_boot_update_mode = 0;   /* launched from PSPMUSICUPD companion */
static RingBuf *g_stream_ring = NULL;
static PlayerCodec g_stream_codec = PLAYER_CODEC_MP3;
static int g_stream_lossless = 0;
static int g_now_sample_rate = 0;
static int g_now_bit_depth = 0;
static int g_now_track_num = 0;
static char g_now_format[8];
static unsigned g_stream_received = 0; /* for Range reconnect */

/* Set from exit callback — abort blocking net/HTTP loops */
static volatile int g_app_exiting = 0;

/* Persist pause/stop position on the PC server (not Memory Stick). */
static void position_save_if_needed(int force) {
    char json[128];
    char *body = NULL;
    int ms;
    int paused;
    if (g_track_id <= 0 || !g_online_mode || !net_is_connected()) {
        return;
    }
    if (g_dl_thid >= 0 && !g_dl_done && !force) {
        return;
    }
    ms = player_elapsed_ms();
    if (g_seek_dragging && g_seek_preview_ms >= 0) {
        ms = g_seek_preview_ms;
    }
    if (ms < 0) {
        ms = 0;
    }
    paused = (!player_is_active() || player_is_paused() || force) ? 1 : 0;
    snprintf(
        json,
        sizeof(json),
        "{\"track_id\":%d,\"position_ms\":%d,\"paused\":%s}",
        g_track_id,
        ms,
        paused ? "true" : "false"
    );
    if (http_put_json(g_cfg.host, g_cfg.port, "/api/playback/state", json, &body, NULL) == HTTP_OK) {
        free(body);
    }
}

static int exit_callback(int arg1, int arg2, void *common) {
    (void)arg1;
    (void)arg2;
    (void)common;
    /*
     * Keep this minimal. Calling net_shutdown()/player_stop()/file I/O here
     * deadlocks the PSP when HOME is pressed during Wi-Fi dialog or TCP poll.
     */
    g_app_exiting = 1;
    http_set_abort(1);
    sceKernelExitGame();
    return 0;
}

static int callback_thread(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static int setup_callbacks(void) {
    int thid = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0) {
        sceKernelStartThread(thid, 0, 0);
    }
    return thid;
}

static void set_status(const char *msg) {
    strncpy(g_status, msg, sizeof(g_status) - 1);
    g_status[sizeof(g_status) - 1] = '\0';
}

static void bind_list_ptrs(void) {
    int i;
    for (i = 0; i < g_count; i++) {
        g_labels_ptr[i] = g_items[i].name;
        g_rights_ptr[i] = g_rights[i];
    }
    g_icons_n = 0;
}

static void load_artists(void);
static void load_genres(void);
static void load_genre_albums(void);
static void load_rated_albums(void);
static void load_albums(void);
static void load_all_albums(void);
static void load_tracks(void);
static void load_all_songs(void);
static void load_search(void);
static void load_top(void);

static void clip_copy(char *dst, int dst_sz, const char *src) {
    int i;

    if (!dst || dst_sz <= 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    for (i = 0; i < dst_sz - 1 && src[i]; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

static void format_host_port(char *out, int out_sz) {
    if (!out || out_sz <= 0) {
        return;
    }
    snprintf(out, out_sz, "%s:%d", g_cfg.host, g_cfg.port);
}

static void set_home_menu(void) {
    g_count = 0;
    g_cursor = 0;
    g_home_np = g_home_top = g_home_wifi = g_home_setup = g_home_appear = g_home_more = -1;
    g_icons_n = 0;
#define ADD(label, right, icon) \
    do { \
        clip_copy(g_items[g_count].name, MAX_NAME, label); \
        g_items[g_count].id = g_count; \
        g_items[g_count].extra = 0; \
        clip_copy(g_rights[g_count], (int)sizeof(g_rights[0]), right); \
        g_icons[g_count] = (icon); \
        g_count++; \
        g_icons_n = g_count; \
    } while (0)

    /* Jump back to the live player without re-selecting a track. */
    if (g_track_id > 0 || player_is_active() || g_buffering || g_play_started) {
        char right[40];
        const char *t = g_now_title[0] ? g_now_title : "Playing";
        snprintf(right, sizeof(right), "%.18s", t);
        g_home_np = g_count;
        ADD("Now Playing", right, UI_ICON_NOTE);
    }

    {
        char r[32];
        int n = offline_count();
        int mb = (offline_total_bytes() + 512 * 1024) / (1024 * 1024);
        if (mb > 0) {
            snprintf(r, sizeof(r), "%d · %dMB", n, mb);
        } else {
            snprintf(r, sizeof(r), "%d songs", n);
        }
        g_home_offline = g_count;
        ADD("Offline Music", r, UI_ICON_NOTE);
    }
    g_home_online = g_count;
    ADD("Online Library", g_online_mode ? "ready" : (g_net_ok ? "no server" : "need WiFi"), UI_ICON_GLOBE);

    g_home_settings = g_count;
    ADD("Settings", "IP · theme · WiFi", UI_ICON_GEAR);
#undef ADD
    bind_list_ptrs();
    g_icons_n = g_count;
}

static void set_settings_menu(void) {
    char hp[72];
    g_count = 0;
    g_cursor = 0;
    g_icons_n = 0;
#define ADD(label, right) \
    do { \
        clip_copy(g_items[g_count].name, MAX_NAME, label); \
        g_items[g_count].id = g_count; \
        g_items[g_count].extra = 0; \
        clip_copy(g_rights[g_count], (int)sizeof(g_rights[0]), right); \
        g_count++; \
    } while (0)

    format_host_port(hp, sizeof(hp));
    ADD("Server IP/Port", hp);
    ADD("Theme", "skins");
    ADD("Connect Wi-Fi", g_net_ok ? "OK" : "Press X");
    ADD("Controls help", "buttons");
    ADD("API Key", g_cfg.api_key[0] ? "set" : "off");
    ADD("Diagnostics", "audio/net/ms");
    ADD("Version", APP_VERSION);
#undef ADD
    bind_list_ptrs();
}

static void set_music_menu(void) {
    static const char *labels[] = {
        "Search",
        "Songs",
        "Albums",
        "Genres",
        "Top Tracks",
        "Popular Albums"
    };
    int i;
    g_count = 0;
    g_cursor = 0;
    for (i = 0; i < 6; i++) {
        clip_copy(g_items[i].name, MAX_NAME, labels[i]);
        g_items[i].id = i;
        g_rights[i][0] = '\0';
    }
    g_count = 6;
    g_list_kind = LIST_NONE;
    g_album_browse = BROWSE_NONE;
    bind_list_ptrs();
}

static void url_encode(const char *in, char *out, int out_sz) {
    static const char *hex = "0123456789ABCDEF";
    int j = 0;
    unsigned char c;

    if (!in || !out || out_sz < 2) {
        return;
    }
    for (; *in && j < out_sz - 4; in++) {
        c = (unsigned char)*in;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out[j++] = (char)c;
        } else if (c == ' ') {
            out[j++] = '%';
            out[j++] = '2';
            out[j++] = '0';
        } else {
            out[j++] = '%';
            out[j++] = hex[c >> 4];
            out[j++] = hex[c & 0x0F];
        }
    }
    out[j] = '\0';
}

static void format_album_rights(int i) {
    ListItem *it = &g_items[i];
    if (it->play_count > 0) {
        snprintf(g_rights[i], sizeof(g_rights[i]), "%dx", it->play_count);
    } else if (it->user_rating > 0) {
        snprintf(g_rights[i], sizeof(g_rights[i]), "%d*", it->user_rating);
    } else if (it->year > 0) {
        snprintf(g_rights[i], sizeof(g_rights[i]), "%d", it->year);
    } else if (it->genre[0]) {
        snprintf(g_rights[i], sizeof(g_rights[i]), "%.10s", it->genre);
    } else if (it->net_score > 0) {
        snprintf(g_rights[i], sizeof(g_rights[i]), "n%d", it->net_score);
    } else {
        g_rights[i][0] = '>';
        g_rights[i][1] = '\0';
    }
}

static void format_artist_rights(int i) {
    ListItem *it = &g_items[i];
    if (it->genre[0]) {
        snprintf(g_rights[i], sizeof(g_rights[i]), "%.10s", it->genre);
    } else {
        snprintf(g_rights[i], sizeof(g_rights[i]), "%d", it->extra);
    }
}

static int api_get(const char *path, char **body) {
    int len = 0;
    int rc;
    set_status("Loading...");
    ui_set_loading(1);
    /* #region agent log */
    {
        char d[128];
        snprintf(d, sizeof(d), "{\"host\":\"%s\",\"port\":%d,\"path\":\"%s\"}", g_cfg.host, g_cfg.port, path);
        dbg_log("B", "main.c:api_get", "http_request", d);
    }
    /* #endregion */
    rc = http_get(g_cfg.host, g_cfg.port, path, body, &len);
    ui_set_loading(0);
    /* #region agent log */
    {
        char d[64];
        snprintf(d, sizeof(d), "{\"rc\":%d,\"len\":%d}", rc, len);
        dbg_log("B", "main.c:api_get", "http_result", d);
    }
    /* #endregion */
    if (rc != HTTP_OK) {
        set_status("HTTP error");
        return -1;
    }
    set_status("OK");
    return 0;
}

static void format_track_rights(int i) {
    char dur[12];
    dur[0] = '\0';
    if (g_items[i].duration > 0) {
        snprintf(dur, sizeof(dur), "%d:%02d", g_items[i].duration / 60, g_items[i].duration % 60);
    }
    if (offline_has(g_items[i].id)) {
        if (dur[0]) {
            snprintf(g_rights[i], sizeof(g_rights[i]), "*%s", dur);
        } else {
            snprintf(g_rights[i], sizeof(g_rights[i]), "*%d", g_items[i].extra);
        }
    } else if (g_items[i].play_count > 0 && g_items[i].extra > 0) {
        snprintf(g_rights[i], sizeof(g_rights[i]), "%dx %d*", g_items[i].play_count, g_items[i].extra);
    } else if (g_items[i].play_count > 0 && dur[0]) {
        snprintf(g_rights[i], sizeof(g_rights[i]), "%dx %s", g_items[i].play_count, dur);
    } else if (g_items[i].play_count > 0) {
        snprintf(g_rights[i], sizeof(g_rights[i]), "%dx", g_items[i].play_count);
    } else if (g_items[i].extra > 0 && dur[0]) {
        snprintf(g_rights[i], sizeof(g_rights[i]), "%d* %s", g_items[i].extra, dur);
    } else if (g_items[i].extra > 0) {
        snprintf(g_rights[i], sizeof(g_rights[i]), "%d*", g_items[i].extra);
    } else if (dur[0]) {
        clip_copy(g_rights[i], (int)sizeof(g_rights[i]), dur);
    } else if (g_items[i].genre[0]) {
        snprintf(g_rights[i], sizeof(g_rights[i]), "%.8s", g_items[i].genre);
    } else if (g_items[i].artist[0]) {
        snprintf(g_rights[i], sizeof(g_rights[i]), "%.10s", g_items[i].artist);
    } else {
        g_rights[i][0] = '\0';
    }
}

static void shuffle_rebuild(int anchor_index) {
    int i;
    int j;
    unsigned seed = (unsigned)sceKernelGetSystemTimeLow() ^ (unsigned)g_count;

    for (i = 0; i < g_count; i++) {
        g_shuffle_order[i] = i;
    }
    for (i = g_count - 1; i > 0; i--) {
        seed = seed * 1103515245u + 12345u;
        j = (int)((seed >> 16) % (unsigned)(i + 1));
        {
            int tmp = g_shuffle_order[i];
            g_shuffle_order[i] = g_shuffle_order[j];
            g_shuffle_order[j] = tmp;
        }
    }
    g_shuffle_pos = 0;
    if (anchor_index >= 0 && anchor_index < g_count) {
        for (i = 0; i < g_count; i++) {
            if (g_shuffle_order[i] == anchor_index) {
                g_shuffle_pos = i;
                break;
            }
        }
    }
}

static void shuffle_reset(void) {
    g_shuffle_pos = 0;
    memset(g_shuffle_order, 0, sizeof(g_shuffle_order));
}

static int list_supports_pagination(void) {
    switch (g_screen) {
        case SCREEN_ARTISTS:
        case SCREEN_ALBUMS:
        case SCREEN_GENRES:
        case SCREEN_TRACKS:
        case SCREEN_TOP:
        case SCREEN_SEARCH:
            return 1;
        default:
            return 0;
    }
}

static void reload_current_list(void) {
    switch (g_list_kind) {
        case LIST_ARTISTS:
            load_artists();
            break;
        case LIST_ALL_ALBUMS:
            load_all_albums();
            break;
        case LIST_ARTIST_ALBUMS:
            load_albums();
            break;
        case LIST_GENRE_ALBUMS:
            load_genre_albums();
            break;
        case LIST_RATED_ALBUMS:
            load_rated_albums();
            break;
        case LIST_GENRES:
            load_genres();
            break;
        case LIST_TRACKS:
            load_tracks();
            break;
        case LIST_ALL_SONGS:
            load_all_songs();
            break;
        case LIST_TOP:
            load_top();
            break;
        case LIST_SEARCH:
            load_search();
            break;
        default:
            break;
    }
}

static void list_page_prev(void) {
    if (g_offset >= MAX_LIST_ITEMS) {
        g_offset -= MAX_LIST_ITEMS;
        g_cursor = 0;
        reload_current_list();
    }
}

static void list_page_next(void) {
    if (g_count >= MAX_LIST_ITEMS) {
        g_offset += MAX_LIST_ITEMS;
        g_cursor = 0;
        reload_current_list();
    }
}

static void load_artists(void) {
    char path[96];
    char *body = NULL;
    int i;
    snprintf(path, sizeof(path), "/api/artists?offset=%d&limit=%d", g_offset, MAX_LIST_ITEMS);
    g_count = 0;
    g_cursor = 0;
    if (api_get(path, &body) == 0) {
        g_count = jutil_parse_named_list(body, g_items, MAX_LIST_ITEMS);
        free(body);
        for (i = 0; i < g_count; i++) {
            format_artist_rights(i);
        }
    }
    g_list_kind = LIST_ARTISTS;
    bind_list_ptrs();
}

static void load_genres(void) {
    char path[96];
    char *body = NULL;
    int i;
    snprintf(path, sizeof(path), "/api/genres?offset=%d&limit=%d", g_offset, MAX_LIST_ITEMS);
    g_count = 0;
    g_cursor = 0;
    set_status("Loading genres...");
    if (api_get(path, &body) == 0) {
        g_count = jutil_parse_genre_list(body, g_items, MAX_LIST_ITEMS);
        free(body);
        for (i = 0; i < g_count; i++) {
            g_items[i].id = g_offset + i;
            if (g_items[i].extra > 0) {
                snprintf(g_rights[i], sizeof(g_rights[i]), "%d", g_items[i].extra);
            } else {
                g_rights[i][0] = '\0';
            }
        }
        if (g_count > 0) {
            snprintf(g_status, sizeof(g_status), "%d genres", g_count);
        } else {
            set_status("No genres found");
        }
    } else {
        set_status("Genres load failed");
    }
    g_list_kind = LIST_GENRES;
    g_album_browse = BROWSE_NONE;
    bind_list_ptrs();
}

static void load_genre_albums(void) {
    char path[384];
    char *body = NULL;
    char enc[MAX_NAME * 3];
    int i;
    url_encode(g_genre_name, enc, sizeof(enc));
    snprintf(
        path,
        sizeof(path),
        "/api/albums?genre=%s&offset=%d&limit=%d&sort=year",
        enc,
        g_offset,
        MAX_LIST_ITEMS
    );
    g_count = 0;
    g_cursor = 0;
    g_album_browse = BROWSE_GENRE;
    g_artist_id = 0;
    if (api_get(path, &body) == 0) {
        g_count = jutil_parse_named_list(body, g_items, MAX_LIST_ITEMS);
        free(body);
        body = NULL;
        for (i = 0; i < g_count; i++) {
            format_album_rights(i);
        }
    }
    /* Genre folders are track-based; if albums are empty, show tracks. */
    if (g_count <= 0) {
        snprintf(
            path,
            sizeof(path),
            "/api/tracks?genre=%s&offset=%d&limit=%d&sort=title",
            enc,
            g_offset,
            MAX_LIST_ITEMS
        );
        if (api_get(path, &body) == 0) {
            g_count = jutil_parse_tracks(body, g_items, MAX_LIST_ITEMS);
            free(body);
            g_list_kind = LIST_TRACKS;
            g_screen = SCREEN_TRACKS;
            bind_list_ptrs();
            if (g_count <= 0) {
                set_status("No music in genre");
            }
            return;
        }
    }
    g_list_kind = LIST_GENRE_ALBUMS;
    bind_list_ptrs();
}

static void load_rated_albums(void) {
    char path[128];
    char *body = NULL;
    int i;
    snprintf(
        path,
        sizeof(path),
        "/api/albums?min_play_count=1&sort=plays&offset=%d&limit=%d",
        g_offset,
        MAX_LIST_ITEMS
    );
    g_count = 0;
    g_cursor = 0;
    g_album_browse = BROWSE_RATED;
    g_artist_id = 0;
    if (api_get(path, &body) == 0) {
        g_count = jutil_parse_named_list(body, g_items, MAX_LIST_ITEMS);
        free(body);
        for (i = 0; i < g_count; i++) {
            format_album_rights(i);
        }
    }
    g_list_kind = LIST_RATED_ALBUMS;
    bind_list_ptrs();
}

static void try_connect(void);
static int try_server(void);
static void report_current_play(int completed);
static void try_resume_from_server(void);
static void reap_download_thread(void);

static void copy_now_playing_meta(const char *title, const char *artist, const char *album) {
    clip_copy(g_now_title, sizeof(g_now_title), title ? title : "");
    clip_copy(g_now_artist, sizeof(g_now_artist), artist ? artist : "");
    clip_copy(g_now_album, sizeof(g_now_album), album ? album : "");
}

static int ensure_online(void) {
    /* Fully online with live AP — stay. */
    if (g_online_mode && net_is_connected()) {
        return 1;
    }
    /* No Wi-Fi yet → official Network Settings dialog (WLAN list). */
    if (!net_is_connected()) {
        try_connect();
    }
    if (!net_is_connected()) {
        g_net_ok = 0;
        g_online_mode = 0;
        return 0;
    }
    g_net_ok = 1;
    return try_server();
}

static int parse_float_rating_from_json(const char *json, const char *key, int *stars) {
    char pattern[40];
    const char *p;
    float f;
    if (!json || !key || !stars) {
        return -1;
    }
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(json, pattern);
    if (!p) {
        return -1;
    }
    p = strchr(p, ':');
    if (!p) {
        return -1;
    }
    f = (float)atof(p + 1);
    if (f <= 0.0f) {
        *stars = 0;
    } else if (f > 5.0f) {
        *stars = 5;
    } else {
        *stars = (int)(f + 0.5f);
    }
    return 0;
}

static void show_album_info(int album_id) {
    char path[64];
    char *body = NULL;
    char genre[48];
    char artist[MAX_NAME];
    char summary[MAX_INFO];
    int year = 0;
    int rating = 0;
    int net = 0;

    snprintf(path, sizeof(path), "/api/albums/%d", album_id);
    if (api_get(path, &body) != 0) {
        set_status("Info failed");
        return;
    }
    jutil_extract_string(body, "name", g_info_title, sizeof(g_info_title));
    jutil_extract_string(body, "genre", genre, sizeof(genre));
    jutil_extract_string(body, "artist", artist, sizeof(artist));
    jutil_extract_string(body, "summary", summary, sizeof(summary));
    jutil_extract_int(body, "year", &year);
    jutil_extract_int(body, "external_score", &net);
    parse_float_rating_from_json(body, "user_rating", &rating);
    free(body);

    snprintf(g_info_line1, sizeof(g_info_line1), "%s", artist[0] ? artist : "Unknown");
    g_info_line2[0] = '\0';
    if (genre[0]) {
        strncat(g_info_line2, genre, sizeof(g_info_line2) - 1);
    }
    if (year > 0) {
        char part[24];
        snprintf(part, sizeof(part), "%s%d", g_info_line2[0] ? " · " : "", year);
        strncat(g_info_line2, part, sizeof(g_info_line2) - strlen(g_info_line2) - 1);
    }
    if (rating > 0) {
        char part[24];
        snprintf(part, sizeof(part), " · %d/5", rating);
        strncat(g_info_line2, part, sizeof(g_info_line2) - strlen(g_info_line2) - 1);
    }
    if (net > 0) {
        char part[24];
        snprintf(part, sizeof(part), " · net %d", net);
        strncat(g_info_line2, part, sizeof(g_info_line2) - strlen(g_info_line2) - 1);
    }
    clip_copy(g_info_line3, sizeof(g_info_line3), summary[0] ? summary : "No description yet.");
    g_info_return_screen = g_screen;
    g_screen = SCREEN_INFO;
}

static void show_artist_info(int artist_id) {
    char path[64];
    char *body = NULL;
    char genre[48];
    char country[48];
    char bio[MAX_INFO];
    int net = 0;

    snprintf(path, sizeof(path), "/api/artists/%d", artist_id);
    if (api_get(path, &body) != 0) {
        set_status("Info failed");
        return;
    }
    jutil_extract_string(body, "name", g_info_title, sizeof(g_info_title));
    jutil_extract_string(body, "genre", genre, sizeof(genre));
    jutil_extract_string(body, "country", country, sizeof(country));
    jutil_extract_string(body, "bio", bio, sizeof(bio));
    jutil_extract_int(body, "external_score", &net);
    free(body);

    g_info_line1[0] = '\0';
    if (country[0]) {
        clip_copy(g_info_line1, sizeof(g_info_line1), country);
    }
    if (genre[0]) {
        if (g_info_line1[0]) {
            strncat(g_info_line1, " · ", sizeof(g_info_line1) - strlen(g_info_line1) - 1);
        }
        strncat(g_info_line1, genre, sizeof(g_info_line1) - strlen(g_info_line1) - 1);
    }
    if (!g_info_line1[0]) {
        clip_copy(g_info_line1, sizeof(g_info_line1), "Artist");
    }
    if (net > 0) {
        snprintf(g_info_line2, sizeof(g_info_line2), "Network score %d", net);
    } else {
        g_info_line2[0] = '\0';
    }
    clip_copy(g_info_line3, sizeof(g_info_line3), bio[0] ? bio : "No bio yet — run Info on server.");
    g_info_return_screen = g_screen;
    g_screen = SCREEN_INFO;
}

static void load_albums(void) {
    char path[96];
    char *body = NULL;
    int i;
    snprintf(
        path,
        sizeof(path),
        "/api/albums?artist_id=%d&offset=%d&limit=%d",
        g_artist_id,
        g_offset,
        MAX_LIST_ITEMS
    );
    g_count = 0;
    g_cursor = 0;
    g_album_browse = BROWSE_ARTIST;
    if (api_get(path, &body) == 0) {
        g_count = jutil_parse_named_list(body, g_items, MAX_LIST_ITEMS);
        free(body);
        for (i = 0; i < g_count; i++) {
            format_album_rights(i);
        }
    }
    g_list_kind = LIST_ARTIST_ALBUMS;
    bind_list_ptrs();
}

static void load_all_albums(void) {
    char path[96];
    char *body = NULL;
    int i;
    snprintf(path, sizeof(path), "/api/albums?offset=%d&limit=%d&sort=year", g_offset, MAX_LIST_ITEMS);
    g_count = 0;
    g_cursor = 0;
    g_album_browse = BROWSE_ALL;
    if (api_get(path, &body) == 0) {
        g_count = jutil_parse_named_list(body, g_items, MAX_LIST_ITEMS);
        free(body);
        for (i = 0; i < g_count; i++) {
            format_album_rights(i);
        }
    }
    g_list_kind = LIST_ALL_ALBUMS;
    bind_list_ptrs();
}

static void load_tracks(void) {
    char path[96];
    char *body = NULL;
    int i;
    snprintf(
        path,
        sizeof(path),
        "/api/tracks?album_id=%d&offset=%d&limit=%d",
        g_album_id,
        g_offset,
        MAX_LIST_ITEMS
    );
    g_count = 0;
    g_cursor = 0;
    if (api_get(path, &body) == 0) {
        g_count = jutil_parse_tracks(body, g_items, MAX_LIST_ITEMS);
        free(body);
        for (i = 0; i < g_count; i++) {
            format_track_rights(i);
        }
    }
    g_list_kind = LIST_TRACKS;
    bind_list_ptrs();
}

static void load_all_songs(void) {
    char path[128];
    char *body = NULL;
    int i;
    snprintf(
        path,
        sizeof(path),
        "/api/tracks?offset=%d&limit=%d&sort=title",
        g_offset,
        MAX_LIST_ITEMS
    );
    g_count = 0;
    g_cursor = 0;
    g_album_browse = BROWSE_NONE;
    set_status("Loading songs...");
    if (api_get(path, &body) == 0) {
        g_count = jutil_parse_tracks(body, g_items, MAX_LIST_ITEMS);
        free(body);
        for (i = 0; i < g_count; i++) {
            format_track_rights(i);
        }
        snprintf(g_status, sizeof(g_status), "%d songs", g_count);
    } else {
        set_status("Songs load failed");
    }
    g_list_kind = LIST_ALL_SONGS;
    bind_list_ptrs();
}

static void load_search(void) {
    char path[384];
    char *body = NULL;
    char enc[MAX_NAME * 3];
    int i;
    int api_ok;

    if (!g_search_query[0]) {
        g_count = 0;
        g_list_kind = LIST_SEARCH;
        bind_list_ptrs();
        set_status("Enter a search");
        return;
    }
    url_encode(g_search_query, enc, sizeof(enc));
    snprintf(
        path,
        sizeof(path),
        "/api/tracks?q=%s&offset=%d&limit=%d&sort=title",
        enc,
        g_offset,
        MAX_LIST_ITEMS
    );
    g_count = 0;
    api_ok = (api_get(path, &body) == 0);
    if (api_ok) {
        g_count = jutil_parse_tracks(body, g_items, MAX_LIST_ITEMS);
        free(body);
        for (i = 0; i < g_count; i++) {
            format_track_rights(i);
        }
    }
    g_list_kind = LIST_SEARCH;
    bind_list_ptrs();
    if (!api_ok) {
        set_status("Search failed");
    } else if (g_count == 0) {
        set_status("No results");
    }
}

static void load_top(void) {
    char path[128];
    char *body = NULL;
    int i;
    snprintf(
        path,
        sizeof(path),
        "/api/tracks?sort=plays&offset=%d&limit=%d",
        g_offset,
        MAX_LIST_ITEMS
    );
    g_count = 0;
    if (api_get(path, &body) == 0) {
        g_count = jutil_parse_tracks(body, g_items, MAX_LIST_ITEMS);
        free(body);
        for (i = 0; i < g_count; i++) {
            format_track_rights(i);
        }
    }
    g_list_kind = LIST_TOP;
    bind_list_ptrs();
}

static void load_offline_list(void) {
    int i;
    int n = offline_load(g_offline_meta, OFFLINE_MAX);
    g_count = 0;
    g_cursor = 0;
    if (n > MAX_LIST_ITEMS) {
        n = MAX_LIST_ITEMS;
    }
    for (i = 0; i < n; i++) {
        g_items[i].id = g_offline_meta[i].id;
        strncpy(g_items[i].name, g_offline_meta[i].title, MAX_NAME - 1);
        g_items[i].name[MAX_NAME - 1] = '\0';
        strncpy(g_items[i].artist, g_offline_meta[i].artist, MAX_NAME - 1);
        g_items[i].artist[MAX_NAME - 1] = '\0';
        strncpy(g_items[i].album, g_offline_meta[i].album, MAX_NAME - 1);
        g_items[i].album[MAX_NAME - 1] = '\0';
        g_items[i].duration = 0;
        g_items[i].extra = g_offline_meta[i].rating;
        strncpy(g_rights[i], g_offline_meta[i].artist, sizeof(g_rights[i]) - 1);
    }
    g_count = n;
    bind_list_ptrs();
}

static void bind_queue_ptrs(void) {
    int i;
    for (i = 0; i < g_count && i < MAX_LIST_ITEMS; i++) {
        g_queue_titles[i] = g_items[i].name;
        g_queue_artists[i] = g_items[i].artist[0] ? g_items[i].artist : g_rights[i];
    }
}

static int list_shows_track_covers(void) {
    return g_screen == SCREEN_TRACKS || g_screen == SCREEN_TOP ||
           g_screen == SCREEN_SEARCH || g_screen == SCREEN_OFFLINE ||
           g_screen == SCREEN_ALBUMS;
}

static const char *screen_title(void) {
    switch (g_screen) {
        case SCREEN_HOME: return "Music";
        case SCREEN_MUSIC: return "Library";
        case SCREEN_ARTISTS: return "Artists";
        case SCREEN_TRACKS: return "Songs";
        case SCREEN_TOP: return "Most Played";
        case SCREEN_SEARCH: return "Search";
        case SCREEN_GENRES: return "Genres";
        case SCREEN_OFFLINE: return "Offline";
        case SCREEN_SETUP: return "Setup";
        case SCREEN_APPEARANCE: return "Appearance";
        case SCREEN_SETTINGS: return "Settings";
        case SCREEN_INFO: return "Info";
        case SCREEN_DIAG: return "Diagnostics";
        case SCREEN_UPDATE: return "Update";
        case SCREEN_ALBUMS:
            if (g_album_browse == BROWSE_GENRE && g_genre_name[0]) {
                return g_genre_name;
            }
            if (g_album_browse == BROWSE_RATED) {
                return "Popular Albums";
            }
            if (g_artist_id && g_artist_name[0]) {
                return g_artist_name;
            }
            return "Albums";
        default: return "Music";
    }
}

static const char *screen_title_with_page(void) {
    const char *base = screen_title();

    if (g_screen == SCREEN_SEARCH && g_search_query[0]) {
        snprintf(g_title_buf, sizeof(g_title_buf), "%s: %.16s", base, g_search_query);
        base = g_title_buf;
    }
    if (!list_supports_pagination()) {
        return base;
    }
    if (g_offset > 0 || g_count >= MAX_LIST_ITEMS) {
        int from = g_offset + 1;
        int to = g_offset + g_count;
        /* Reuse g_title_buf; if base already points here, format into a temp first. */
        if (base == g_title_buf) {
            char tmp[96];
            clip_copy(tmp, (int)sizeof(tmp), g_title_buf);
            snprintf(g_title_buf, sizeof(g_title_buf), "%.48s (%d-%d)", tmp, from, to);
        } else {
            snprintf(g_title_buf, sizeof(g_title_buf), "%.48s (%d-%d)", base, from, to);
        }
        return g_title_buf;
    }
    return base;
}

static void fill_mini_player(UiMiniPlayer *mini) {
    memset(mini, 0, sizeof(*mini));
    if (!g_play_started && !player_is_active()) {
        return;
    }
    mini->now_title = g_now_title;
    mini->now_artist = g_now_artist;
    mini->elapsed_ms = player_elapsed_ms();
    {
        int dur = player_duration_ms();
        if (dur <= 0) {
            dur = g_now_duration_ms;
        }
        mini->duration_ms = dur;
    }
    mini->paused = player_is_paused();
    mini->track_id = g_track_id;
}

static void draw_current_library(void) {
    UiMiniPlayer mini;
    int ids[MAX_LIST_ITEMS];
    int i;
    const int *id_ptr = NULL;
    static int s_warm_rr = 0;
    static int s_warm_screen = -1;

    fill_mini_player(&mini);
    if (list_shows_track_covers()) {
        int row_h = 32;
        int top = 40;
        int footer_h = (mini.now_title && mini.now_title[0]) ? 48 : 0;
        int visible = (272 - top - footer_h) / row_h;
        int start = 0;
        int tried = 0;

        for (i = 0; i < g_count && i < MAX_LIST_ITEMS; i++) {
            ids[i] = (g_screen == SCREEN_ALBUMS) ? -g_items[i].id : g_items[i].id;
        }
        id_ptr = ids;

        if (visible < 1) {
            visible = 1;
        }
        if (g_cursor >= start + visible) {
            start = g_cursor - visible + 1;
        }
        if (g_cursor < start) {
            start = g_cursor;
        }
        if (start < 0) {
            start = 0;
        }
        if (g_screen != s_warm_screen) {
            s_warm_rr = 0;
            s_warm_screen = g_screen;
        }

        /*
         * Warm visible covers like 1.2.15: one fetch per frame when the stream
         * socket is free. Prefer the cursor row first.
         */
        reap_download_thread();
        if (g_online_mode && g_count > 0 && !g_buffering && g_dl_thid < 0) {
            int prefer = g_cursor;
            for (tried = 0; tried < visible + 1; tried++) {
                int idx;
                int cid;
                if (tried == 0 && prefer >= start && prefer < start + visible) {
                    idx = prefer;
                } else {
                    idx = start + ((s_warm_rr + (tried > 0 ? tried - 1 : 0)) % visible);
                }
                if (idx < 0 || idx >= g_count) {
                    continue;
                }
                cid = (g_screen == SCREEN_ALBUMS) ? -g_items[idx].id : g_items[idx].id;
                if (cid == 0 || ui_image_cover_for(cid)) {
                    continue;
                }
                ui_image_load_cover_ex(g_cfg.host, g_cfg.port, cid, 0, 0);
                if (tried > 0) {
                    s_warm_rr = (s_warm_rr + tried) % (visible > 0 ? visible : 1);
                }
                break; /* one network fetch per frame */
            }
        }
    }
    ui_draw_library_ex(
        screen_title_with_page(),
        g_labels_ptr,
        g_rights_ptr,
        (g_screen == SCREEN_HOME && g_icons_n > 0) ? g_icons : NULL,
        id_ptr,
        g_count,
        g_cursor,
        player_is_active(),
        (mini.now_title && mini.now_title[0]) ? &mini : NULL,
        g_screen == SCREEN_HOME
    );
}

static void play_local_file(
    int id,
    const char *path,
    const char *title,
    const char *artist,
    const char *album,
    int rating,
    int from_offline
) {
    set_status("Playing...");
    int rc = player_play_file(path);
    if (rc < 0) {
        snprintf(g_status, sizeof(g_status), "Play error %d", rc);
        return;
    }
    g_track_id = id;
    g_now_rating = rating;
    g_from_offline = from_offline;
    g_now_offline = offline_has(id);
    copy_now_playing_meta(title, artist, album);
    ui_image_clear_cover();
    if (from_offline) {
        unsigned char *pic = NULL;
        int plen = 0;
        int is_jpeg = 0;
        if (flac_extract_cover(path, &pic, &plen, &is_jpeg) == 0 && pic) {
            ui_image_load_cover_mem(id, pic, plen, is_jpeg, 1, 0);
            free(pic);
        } else if (g_online_mode) {
            ui_image_load_cover_ex(g_cfg.host, g_cfg.port, id, 1, 0);
        }
    } else if (g_online_mode) {
        ui_image_load_cover_ex(g_cfg.host, g_cfg.port, id, 1, 0);
    }
    g_show_eq = 0;
    g_queue_focus = 0;
    g_play_started = 1;
    g_buffering = 0;
    player_set_known_duration_ms(g_now_duration_ms);
    g_screen = SCREEN_PLAYING;
}

#ifdef DEBUG_HUD
static volatile int g_dl_log_bucket = -1;
#endif

static void dl_progress_cb(int bytes, int total, void *userdata) {
    (void)userdata;
    g_dl_bytes = bytes;
    if (total > 0) {
        g_dl_total = total;
    }
    player_set_bytes_received((unsigned)((g_stream_received > 0 ? g_stream_received : 0) + bytes));
#ifdef DEBUG_HUD
    {
        int bucket = bytes / (64 * 1024);
        if (bucket != g_dl_log_bucket) {
            char d[72];
            g_dl_log_bucket = bucket;
            snprintf(d, sizeof(d), "{\"bytes\":%d,\"total\":%d}", bytes, g_dl_total);
            dbg_log("B", "main.c:dl", "progress", d);
        }
    }
#endif
}

static void set_dl_fail(const char *reason) {
    if (!reason) {
        reason = "err";
    }
    strncpy(g_dl_fail_reason, reason, sizeof(g_dl_fail_reason) - 1);
    g_dl_fail_reason[sizeof(g_dl_fail_reason) - 1] = '\0';
}

static void status_fail(const char *reason) {
    const char *human = "Playback failed";
    set_dl_fail(reason);
    if (reason) {
        if (strcmp(reason, "tcp") == 0 || strcmp(reason, "err_tcp") == 0) {
            human = "Can't reach server";
        } else if (strcmp(reason, "hdr") == 0) {
            human = "Bad server response";
        } else if (strcmp(reason, "trunc") == 0) {
            human = "Download cut off";
        } else if (strcmp(reason, "empty") == 0) {
            human = "Empty file";
        } else if (strcmp(reason, "abort") == 0) {
            human = "Cancelled";
        } else if (strcmp(reason, "thread") == 0 || strcmp(reason, "start") == 0) {
            human = "Player busy";
        } else if (strcmp(reason, "unsupported") == 0) {
            human = "Unsupported format";
        } else if (strcmp(reason, "decoder") == 0) {
            human = "Decoder error";
        } else if (strcmp(reason, "status") == 0) {
            human = "Track not found";
        } else if (strcmp(reason, "recv_timeout") == 0 || strcmp(reason, "recv") == 0) {
            human = "Network stall";
        } else if (strcmp(reason, "oom") == 0) {
            human = "Out of memory";
        }
    }
    snprintf(g_status, sizeof(g_status), "%s", human);
}

static int stream_on_data(const void *data, int len, void *userdata) {
    size_t w;
    int spins = 0;
    (void)userdata;
    if (!g_stream_ring || len <= 0) {
        return -1;
    }
    w = ringbuf_write_wait(g_stream_ring, data, (size_t)len, 200000);
    if ((int)w < len) {
        /* backpressure — keep trying briefly; never spin forever */
        while ((int)w < len && !http_should_abort()) {
            size_t n = ringbuf_write_wait(
                g_stream_ring,
                (const char *)data + w,
                (size_t)(len - (int)w),
                200000
            );
            if (n == 0) {
                if (++spins > 300) { /* ~3s */
                    return -1;
                }
                sceKernelDelayThread(10000);
                continue;
            }
            spins = 0;
            w += n;
        }
    }
    metrics_add_network_bytes((unsigned)len, 0);
    return http_should_abort() ? -1 : 0;
}

static int download_thread(SceSize args, void *argp) {
    int clen = -1;
    int total = -1;
    int rc;
    int range = -1;
    (void)args;
    (void)argp;
    dbg_step("dl_thr");

    if (g_stream_received > 0) {
        range = (int)g_stream_received;
    }

    rc = http_get_stream(
        g_cfg.host,
        g_cfg.port,
        g_dl_api,
        range,
        stream_on_data,
        NULL,
        dl_progress_cb,
        NULL,
        &clen,
        &total
    );
    if (total > 0) {
        g_dl_total = total;
    } else if (clen > 0 && range < 0) {
        g_dl_total = clen;
    }
    if (rc == HTTP_ABORTED) {
        g_dl_aborted = 1;
        set_dl_fail("abort");
        dbg_log("B", "main.c:dl", "http_abort", "{}");
    } else if (rc != HTTP_OK) {
        g_dl_err = 1;
        set_dl_fail(http_last_fail_reason());
        dbg_log("B", "main.c:dl", "http_err", "{}");
    } else {
        g_stream_received += (unsigned)g_dl_bytes;
        if (g_stream_ring) {
            ringbuf_set_eof(g_stream_ring, 1);
        }
        player_set_stream_eof();
        dbg_log("B", "main.c:dl", "http_done", "{}");
    }
    g_dl_done = 1;
    return 0;
}

static void join_download(void) {
    if (g_dl_thid < 0) {
        return;
    }
    http_set_abort(1);
    if (g_stream_ring) {
        ringbuf_set_abort(g_stream_ring, 1);
    }
    {
        /* Keep seek/skip snappy — don't stall the UI for seconds. */
        SceUInt timeout = 450000;
        int wr = sceKernelWaitThreadEnd(g_dl_thid, &timeout);
        if (wr < 0) {
            if (!g_dl_done) {
                g_dl_err = 1;
                set_dl_fail("killed");
                g_dl_done = 1;
                dbg_step("killed");
            }
            sceKernelTerminateDeleteThread(g_dl_thid);
            sceKernelDelayThread(30000);
        } else {
            sceKernelDeleteThread(g_dl_thid);
        }
    }
    g_dl_thid = -1;
    http_set_abort(0);
}

/* Free finished HTTP thread so list covers can use the socket again. */
static void reap_download_thread(void) {
    SceUInt timeout = 0;
    if (g_dl_thid < 0 || !g_dl_done) {
        return;
    }
    if (sceKernelWaitThreadEnd(g_dl_thid, &timeout) >= 0) {
        sceKernelDeleteThread(g_dl_thid);
        g_dl_thid = -1;
    }
}

static void clear_stream_ring(void) {
    if (g_stream_ring) {
        ringbuf_destroy(g_stream_ring);
        g_stream_ring = NULL;
    }
    g_dl_bytes = 0;
    g_dl_total = -1;
    g_dl_done = 0;
    g_dl_err = 0;
    g_dl_aborted = 0;
    g_dl_fail_reason[0] = '\0';
    g_save_pending = 0;
    g_stream_received = 0;
}

/* clear_temp: 1 = discard stream (Skip / new track); 0 = keep for Stop replay */
static void stop_playback_all(int clear_temp) {
    if (g_play_started || player_is_active() || g_buffering) {
        report_current_play(0);
    }
    http_set_abort(1);
    join_download();
    player_stop();
    g_play_started = 0;
    g_buffering = 0;
    g_cover_after_dl = 0;
    g_seek_hold_ms = 0;
    if (clear_temp) {
        clear_stream_ring();
        g_decoder_auto_retry = 0;
    } else {
        g_save_pending = 0;
        g_dl_done = 1;
    }
    http_set_abort(0);
    dbg_log("P", "main.c:stop", clear_temp ? "stop_clear" : "stop_keep", "{}");
}

static int save_offline_from_cache(void) {
    /* Explicit download mode — never use stream cache. */
    if (g_track_id <= 0) {
        return -1;
    }
    if (offline_has(g_track_id) || g_from_offline) {
        g_now_offline = 1;
        set_status("Already offline");
        return 0;
    }
    if (download_start(
            g_cfg.host,
            g_cfg.port,
            g_track_id,
            g_now_title,
            g_now_artist,
            g_now_album,
            g_now_track_num
        ) == 0) {
        set_status("Downloading...");
        return 0;
    }
    set_status("Download failed");
    return -1;
}

static void seek_stream_to_ms(int target_ms) {
    int dur = player_duration_ms();
    char d[80];

    /* Ignore seek while already restarting the stream. */
    if (g_buffering || g_seek_busy) {
        return;
    }
    if (dur <= 0) {
        dur = g_now_duration_ms;
    }
    if (dur < 2000 || g_track_id <= 0) {
        set_status("Seek N/A");
        return;
    }
    /* Offline local files: restart online stream seek not available — N/A for now. */
    if (g_from_offline) {
        set_status("Seek online only");
        return;
    }
    if (target_ms < 0) {
        target_ms = 0;
    }
    if (target_ms > dur - 1000) {
        target_ms = dur - 1000;
    }
    snprintf(d, sizeof(d), "{\"ms\":%d}", target_ms);
    dbg_log("S", "main.c:seek", "start_ms_seek", d);

    g_seek_busy = 1;
    join_download();
    player_stop();
    clear_stream_ring();

    {
        size_t ring_sz = ringbuf_recommend_size(320, 8);
        g_stream_ring = ringbuf_create(ring_sz);
        if (!g_stream_ring) {
            status_fail("oom");
            g_seek_busy = 0;
            return;
        }
        metrics_reset_session();
        /* Server aligns to MP3 frame; do not use raw Range. */
        g_stream_received = 0;
        g_seek_resume_ms = target_ms;
        g_stream_codec = PLAYER_CODEC_MP3;
        g_stream_lossless = 0;
        snprintf(g_dl_api, sizeof(g_dl_api), "/api/stream/%d?start_ms=%d", g_track_id, target_ms);
        g_dl_bytes = 0;
        g_dl_total = -1;
        g_dl_done = 0;
        g_dl_err = 0;
        g_dl_aborted = 0;
        g_play_started = 0;
        g_buffering = 1;
        /* Keep LRU cover if present; else retry after this seek stream ends. */
        g_cover_after_dl = ui_image_cover_for(g_track_id) ? 0 : 1;
        g_now_duration_ms = dur;
        player_set_known_duration_ms(dur);
        snprintf(g_status, sizeof(g_status), "Seek %d:%02d",
                 (target_ms / 1000) / 60, (target_ms / 1000) % 60);
        http_set_abort(0);
        g_dl_thid = sceKernelCreateThread(
            "httpdl",
            download_thread,
            0x18,
            0x10000,
            PSP_THREAD_ATTR_USER,
            NULL
        );
        if (g_dl_thid >= 0) {
            sceKernelStartThread(g_dl_thid, 0, NULL);
        } else {
            status_fail("thread");
            g_buffering = 0;
        }
    }
    g_seek_busy = 0;
}

static void play_track_online(int id, const char *title, int rating);

/* After stream reaps: fetch cover only if pre-stream thumbnail missed. */
static void maybe_fetch_cover_after_stream(void) {
    if (!g_cover_after_dl) {
        return;
    }
    if (!g_play_started || !g_dl_done || g_dl_thid >= 0) {
        return;
    }
    g_cover_after_dl = 0;
    if (g_online_mode && g_track_id > 0 && !ui_image_cover_for(g_track_id)) {
        dbg_log("C", "main.c:cover", "cover_after_dl", "{}");
        ui_image_load_cover_ex(g_cfg.host, g_cfg.port, g_track_id, 1, 0);
    }
}

static void try_start_play_from_buffer(void) {
    int rc;
    float need_sec = 3.0f;
    float have;
    size_t used;
    size_t cap;
    int ready;

    if (g_play_started || !g_buffering) {
        return;
    }
    if (g_dl_err) {
        const char *why = g_dl_fail_reason[0] ? g_dl_fail_reason : "err";
        if (g_tcp_auto_retry &&
            (strcmp(why, "tcp") == 0 || strcmp(why, "err_tcp") == 0)) {
            int tid = g_track_id;
            char title[MAX_NAME];
            int rating = g_now_rating;
            g_tcp_auto_retry = 0;
            strncpy(title, g_now_title, MAX_NAME - 1);
            title[MAX_NAME - 1] = '\0';
            set_status("Wi-Fi dropped — retrying");
            g_buffering = 0;
            sceKernelDelayThread(500000);
            play_track_online(tid, title, rating);
            return;
        }
        g_tcp_auto_retry = 0;
        status_fail(why);
        g_buffering = 0;
        return;
    }
    if (g_dl_aborted) {
        set_status("Load cancelled");
        g_buffering = 0;
        return;
    }

    if (!g_stream_ring) {
        return;
    }

    used = ringbuf_used(g_stream_ring);
    cap = ringbuf_capacity(g_stream_ring);

    /* Early sniff — never wait for FLAC-sized prebuffer on MP3. */
    if (used >= 4) {
        unsigned char mag[4];
        size_t n = ringbuf_peek(g_stream_ring, mag, 4);
        if (n >= 4 && memcmp(mag, "fLaC", 4) == 0) {
            g_stream_codec = PLAYER_CODEC_FLAC;
            g_stream_lossless = 1;
            need_sec = 5.0f;
        } else if (n >= 3 && (memcmp(mag, "ID3", 3) == 0 || mag[0] == 0xFF)) {
            g_stream_codec = PLAYER_CODEC_MP3;
            g_stream_lossless = 0;
            need_sec = 2.0f;
        } else {
            /* Unknown magic — assume MP3 (server always sends mp3 for m4a). */
            g_stream_codec = PLAYER_CODEC_MP3;
            g_stream_lossless = 0;
            need_sec = 2.0f;
        }
    }

    have = ringbuf_buffered_seconds(
        g_stream_ring,
        g_stream_lossless ? 900 : 320
    );
    if (g_stream_codec == PLAYER_CODEC_FLAC) {
        need_sec = 5.0f;
    }

    /*
     * CRITICAL: ring write blocks when full. If we require more bytes than
     * capacity (or 10% of a huge file), download stalls forever at ~3-5%.
     * Start when we have enough audio OR the ring is >=75% full OR >=96KB MP3.
     * For FLAC do not start on % full alone — that caused "buf 1s" freezes.
     */
    ready = 0;
    if (have >= need_sec) {
        ready = 1;
    }
    if (g_dl_total > 0 && g_dl_bytes >= g_dl_total) {
        ready = 1;
    }
    if (g_dl_done && used >= 2048) {
        ready = 1;
    }
    if (!g_stream_lossless && cap > 0 && used * 100 >= cap * 75) {
        ready = 1;
    }
    if (g_stream_lossless && cap > 0 && used * 100 >= cap * 85 && have >= 3.0f) {
        ready = 1;
    }
    if (!g_stream_lossless && used >= 96 * 1024) {
        ready = 1;
    }
    if (!g_stream_lossless && g_dl_total > 0 && g_dl_bytes * 20 >= g_dl_total && used >= 48 * 1024) {
        ready = 1;
    }
    if (g_stream_lossless && used >= 256 * 1024 && have >= 3.0f) {
        ready = 1;
    }

    if (!ready) {
        if (g_dl_bytes > 0) {
            int pct = (g_dl_total > 0) ? (g_dl_bytes * 100) / g_dl_total : 0;
            snprintf(
                g_status,
                sizeof(g_status),
                "Loading %d%% (%.0fs)",
                pct,
                have
            );
        } else {
            set_status("Loading...");
        }
        return;
    }

    {
        char d[120];
        snprintf(
            d,
            sizeof(d),
            "{\"used\":%u,\"cap\":%u,\"have\":%.1f,\"bytes\":%d,\"total\":%d,\"codec\":%d}",
            (unsigned)used,
            (unsigned)cap,
            have,
            g_dl_bytes,
            g_dl_total,
            (int)g_stream_codec
        );
        dbg_log("P", "main.c:start", "play_try", d);
    }

    rc = player_play_ring(g_stream_ring, g_stream_codec, g_dl_total);
    if (rc < 0) {
        const char *pe = player_last_error();
        char d[96];
        snprintf(
            d,
            sizeof(d),
            "{\"rc\":%d,\"err\":\"%.40s\",\"retry\":%d}",
            rc,
            pe ? pe : "",
            g_decoder_auto_retry
        );
        dbg_log("P", "main.c:start", "play_fail", d);
        /* Never block UI on log flush here — it competed with FLAC start. */

        /*
         * sceMp3Init consumes ring bytes. Retrying with a broken stream
         * deadlocks at Loading 3%. One clean restart from byte 0, then fail.
         */
        if (!g_decoder_auto_retry && g_seek_resume_ms <= 0) {
            int tid = g_track_id;
            char title[MAX_NAME];
            int rating = g_now_rating;
            g_decoder_auto_retry = 1;
            strncpy(title, g_now_title, MAX_NAME - 1);
            title[MAX_NAME - 1] = '\0';
            set_status("Decoder retry...");
            g_buffering = 0;
            sceKernelDelayThread(200000);
            play_track_online(tid, title, rating);
            return;
        }
        if (pe && pe[0]) {
            snprintf(g_status, sizeof(g_status), "%s", pe);
        } else {
            status_fail("decoder");
        }
        http_set_abort(1);
        g_buffering = 0;
        g_seek_resume_ms = 0;
        return;
    }

    g_play_started = 1;
    g_buffering = 0;
    g_tcp_auto_retry = 0;
    g_decoder_auto_retry = 0;
    player_set_known_duration_ms(g_now_duration_ms);
    if (g_seek_resume_ms > 0) {
        player_set_elapsed_ms(g_seek_resume_ms);
        g_seek_resume_ms = 0;
    }
    set_status("Playing...");
    /* Fallback only if pre-stream cover missed. */
    if (!ui_image_cover_for(g_track_id)) {
        g_cover_after_dl = 1;
    }
    maybe_fetch_cover_after_stream();
}

static void play_track_online(int id, const char *title, int rating) {
    char local[280];
    size_t ring_sz;
    int i;

    /* Stop previous stream BEFORE any new HTTP (cover must not steal the socket). */
    stop_playback_all(1);

    /* Online play is always MP3 320 from server (incl. FLAC sources). */
    g_stream_codec = PLAYER_CODEC_MP3;
    g_stream_lossless = 0;
    g_now_format[0] = '\0';
    g_cover_after_dl = 0;
    g_show_track_info = 0;
    g_seek_hold_ms = 0;

    /* Prefer metadata from the current list row. */
    for (i = 0; i < g_count; i++) {
        if (g_items[i].id == id) {
            if (g_items[i].artist[0]) {
                strncpy(g_artist_name, g_items[i].artist, MAX_NAME - 1);
                g_artist_name[MAX_NAME - 1] = '\0';
            }
            if (g_items[i].album[0] &&
                strcmp(g_items[i].album, "Media.localized") != 0 &&
                strcmp(g_items[i].album, "Unknown Album") != 0) {
                strncpy(g_album_name, g_items[i].album, MAX_NAME - 1);
                g_album_name[MAX_NAME - 1] = '\0';
            } else {
                g_album_name[0] = '\0';
            }
            if ((!title || !title[0]) && g_items[i].name[0]) {
                title = g_items[i].name;
            }
            g_now_sample_rate = g_items[i].sample_rate;
            g_now_bit_depth = g_items[i].bit_depth;
            g_now_track_num = g_items[i].track_num;
            strncpy(g_now_format, g_items[i].format, sizeof(g_now_format) - 1);
            g_now_format[sizeof(g_now_format) - 1] = '\0';
            /* Do not soft-decode FLAC over Wi‑Fi — server sends 320k MP3. */
            g_stream_codec = PLAYER_CODEC_MP3;
            g_stream_lossless = 0;
            break;
        }
    }

    if (offline_has(id)) {
        if (offline_locate_path(id, local, sizeof(local)) == 0) {
            play_local_file(id, local, title, g_artist_name, g_album_name, rating, 1);
            return;
        }
        /* Stale offline index entry without file — drop it. */
        (void)offline_delete(id);
    }

    /*
     * Pre-stream cover (restored 1.2.21 behavior). Defer-until-EOF left NP blank
     * for the whole FLAC. Thumbnail is 128px PNG — short GET, then audio socket.
     * LRU hit activates without HTTP. Miss → fallback after stream reaps.
     */
    ui_image_clear_cover();
    g_cover_after_dl = 0;
    if (g_online_mode && id > 0) {
        if (ui_image_load_cover_ex(g_cfg.host, g_cfg.port, id, 1, 0) != 0 ||
            !ui_image_cover_for(id)) {
            g_cover_after_dl = 1;
        }
    }

    g_track_id = id;
    g_now_rating = rating;
    g_from_offline = 0;
    g_now_offline = 0;
    g_play_reported = 0;
    copy_now_playing_meta(title, g_artist_name, g_album_name);
    g_show_eq = 0;
    g_queue_focus = 0;
    g_screen = SCREEN_PLAYING;

    ring_sz = ringbuf_recommend_size(
        g_stream_lossless ? 900 : 320,
        g_stream_lossless ? 10 : 8
    );
    g_stream_ring = ringbuf_create(ring_sz);
    if (!g_stream_ring) {
        status_fail("oom");
        return;
    }
    metrics_reset_session();
    metrics_set_buffer((unsigned)ring_sz, 0, 0);

    snprintf(g_dl_api, sizeof(g_dl_api), "/api/stream/%d?format=mp3", id);
    g_dl_bytes = 0;
    g_dl_total = -1;
    g_dl_done = 0;
    g_dl_err = 0;
    g_dl_aborted = 0;
#ifdef DEBUG_HUD
    g_dl_log_bucket = -1;
#endif
    g_dl_fail_reason[0] = '\0';
    g_save_pending = 0;
    g_play_started = 0;
    g_buffering = 1;
    g_stream_received = 0;
    g_seek_resume_ms = 0;

    if (g_pending_start_ms > 0) {
        int start_ms = g_pending_start_ms;
        g_pending_start_ms = 0;
        if (start_ms > 0) {
            g_seek_resume_ms = start_ms;
            g_stream_codec = PLAYER_CODEC_MP3;
            g_stream_lossless = 0;
            snprintf(g_dl_api, sizeof(g_dl_api), "/api/stream/%d?start_ms=%d", id, start_ms);
        }
    }

    http_set_abort(0);
    set_status("Buffering...");
    dbg_log("C", "main.c:play", "buffering_start", "{}");

    /* Re-arm EOF fallback if pre-stream cover still missing. */
    if (!ui_image_cover_for(id)) {
        g_cover_after_dl = 1;
    }

    g_dl_thid = sceKernelCreateThread(
        "httpdl",
        download_thread,
        0x18,
        0x10000,
        PSP_THREAD_ATTR_USER,
        NULL
    );
    if (g_dl_thid < 0) {
        status_fail("thread");
        g_buffering = 0;
        return;
    }
    if (sceKernelStartThread(g_dl_thid, 0, NULL) < 0) {
        sceKernelDeleteThread(g_dl_thid);
        g_dl_thid = -1;
        status_fail("start");
        g_buffering = 0;
    }
}

static void play_track_offline_idx(int idx) {
    char path[320];
    OfflineTrack *t = &g_offline_meta[idx];
    SceUID fd;
    stop_playback_all(1);
    offline_resolve_path(t, path, sizeof(path));
    fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
    if (fd < 0) {
        set_status("Offline file missing");
        if (t->id > 0 && t->id < 100000) {
            (void)offline_delete(t->id);
        }
        load_offline_list();
        return;
    }
    sceIoClose(fd);
    play_local_file(t->id, path, t->title, t->artist, t->album, t->rating, 1);
}

static void rate_current(int rating) {
    if (g_track_id <= 0) {
        return;
    }
    if (rating < 0) {
        rating = 0;
    }
    if (rating > 5) {
        rating = 5;
    }
    g_now_rating = rating;
    if (!g_online_mode) {
        set_status("Rated locally");
        return;
    }
    char path[64];
    char json[32];
    char *body = NULL;
    snprintf(path, sizeof(path), "/api/tracks/%d/rating", g_track_id);
    snprintf(json, sizeof(json), "{\"rating\":%d}", rating);
    if (http_put_json(g_cfg.host, g_cfg.port, path, json, &body, NULL) == HTTP_OK) {
        snprintf(g_status, sizeof(g_status), "Rated %d/5", rating);
        free(body);
    } else {
        set_status("Rate failed");
    }
}

/* Report listen time to server for Top / Popular Albums. */
static void report_play(int track_id, int ms_listened, int completed) {
    char json[96];
    char *body = NULL;
    if (track_id <= 0 || !g_online_mode || !net_is_connected()) {
        return;
    }
    if (ms_listened < 0) {
        ms_listened = 0;
    }
    /* Don't contend with the stream socket. */
    if (g_dl_thid >= 0 && !g_dl_done) {
        return;
    }
    snprintf(
        json,
        sizeof(json),
        "{\"track_id\":%d,\"ms_listened\":%d,\"completed\":%s}",
        track_id,
        ms_listened,
        completed ? "true" : "false"
    );
    if (http_post_json(g_cfg.host, g_cfg.port, "/api/plays", json, &body, NULL) == HTTP_OK) {
        free(body);
    }
}

static void report_current_play(int completed) {
    int ms = player_elapsed_ms();
    int tid = g_track_id;
    if (tid <= 0 || g_play_reported) {
        return;
    }
    report_play(tid, ms, completed);
    g_play_reported = 1;
}

static void save_offline_current(void) {
    if (g_track_id <= 0) {
        return;
    }
    if (offline_has(g_track_id) || g_from_offline) {
        set_status("Already offline");
        g_now_offline = 1;
        return;
    }
    g_save_pending = 0;
    save_offline_from_cache();
}

static void poll_download_side_effects(void) {
    const DownloadStatus *ds = download_status();
    if (ds->state == DL_COMPLETE && g_track_id == ds->track_id) {
        g_now_offline = 1;
        if (ds->final_path[0]) {
            const char *dot = strrchr(ds->final_path, '.');
            int as_flac = (dot && (strcmp(dot, ".flac") == 0 || strcmp(dot, ".FLAC") == 0));
            if (as_flac) {
                offline_register_flac(
                    ds->track_id,
                    ds->artist,
                    ds->album,
                    ds->title,
                    g_now_rating,
                    ds->final_path
                );
            } else {
                offline_save(
                    ds->track_id,
                    ds->artist,
                    ds->album,
                    ds->title,
                    g_now_rating,
                    ds->final_path
                );
            }
        }
        /* Cover stays in RAM — no MS write (offline music file only). */
        if (g_online_mode && !ui_image_cover_for(ds->track_id)) {
            ui_image_load_cover_ex(g_cfg.host, g_cfg.port, ds->track_id, 1, 0);
        }
        set_status("Download complete");
    } else if (ds->state == DL_RUNNING || ds->state == DL_VERIFYING) {
        snprintf(
            g_status,
            sizeof(g_status),
            "DL %d%% %.1fMB/s",
            ds->percent,
            ds->speed_bps / (1024.0f * 1024.0f)
        );
    } else if (ds->state == DL_ERROR && ds->error[0]) {
        snprintf(g_status, sizeof(g_status), "%s", ds->error);
    }

    /* Network lost: keep playing from RAM; reconnect with Range. */
    if (g_play_started && g_dl_done && g_dl_err && g_stream_ring &&
        ringbuf_used(g_stream_ring) > 0 && g_dl_thid < 0) {
        metrics_set_state(BUF_NETWORK_LOST);
        set_status("Reconnecting...");
        g_dl_done = 0;
        g_dl_err = 0;
        g_stream_received = player_bytes_received();
        http_set_abort(0);
        if (g_stream_ring) {
            ringbuf_set_abort(g_stream_ring, 0);
            ringbuf_set_eof(g_stream_ring, 0);
        }
        g_dl_thid = sceKernelCreateThread(
            "httpdl",
            download_thread,
            0x18,
            0x10000,
            PSP_THREAD_ATTR_USER,
            NULL
        );
        if (g_dl_thid >= 0) {
            sceKernelStartThread(g_dl_thid, 0, NULL);
        }
    }
}

static void replay_current_track(void) {
    if (g_track_id <= 0) {
        return;
    }
    if (g_from_offline) {
        char path[320];
        int i;
        for (i = 0; i < OFFLINE_MAX; i++) {
            if (g_offline_meta[i].id == g_track_id) {
                offline_resolve_path(&g_offline_meta[i], path, sizeof(path));
                play_local_file(
                    g_track_id,
                    path,
                    g_now_title,
                    g_now_artist,
                    g_now_album,
                    g_now_rating,
                    1
                );
                return;
            }
        }
        offline_path_for(g_track_id, path, sizeof(path));
        if (offline_locate_path(g_track_id, path, sizeof(path)) != 0) {
            offline_path_for(g_track_id, path, sizeof(path));
        }
        play_local_file(
            g_track_id,
            path,
            g_now_title,
            g_now_artist,
            g_now_album,
            g_now_rating,
            1
        );
        return;
    }
    play_track_online(g_track_id, g_now_title, g_now_rating);
}

static const char *screen_title(void);

/*
 * After Wi-Fi dialog: restore UI and mark net state.
 * Do NOT probe the HTTP server here — blocking connect freezes on a black
 * screen (SO_*TIMEO is ignored for connect on PSP).
 */
static void finish_wifi_result(int ok, int restore_ui, int allow_setup_jump) {
    (void)allow_setup_jump;

    if (restore_ui) {
        /* #region agent log */
        dbg_log("E", "main.c:finish_wifi", "ui_init_before", "{}");
        /* #endregion */
        ui_init();
        ui_gpu_reset();
        /* #region agent log */
        dbg_log("E", "main.c:finish_wifi", "ui_init_after", "{}");
        /* #endregion */
    }

    if (!ok) {
        g_net_ok = 0;
        g_online_mode = 0;
        if (net_last_step()[0] && strcmp(net_last_step(), "WLAN_OFF") == 0) {
            set_status("Offline — turn WLAN ON");
        } else if (net_last_error() != 0) {
            snprintf(
                g_status,
                sizeof(g_status),
                "Offline (%s %08X)",
                net_last_step()[0] ? net_last_step() : "Net",
                (unsigned)net_last_error()
            );
        } else {
            set_status("Offline mode");
        }
        /* #region agent log */
        dbg_log("A", "main.c:finish_wifi", "offline_ok", "{}");
        /* #endregion */
        return;
    }

    g_net_ok = 1;
    /* Keep previous online_mode if already talking to server; else probe later */
    if (!g_online_mode) {
        set_status("Wi-Fi OK — open Online Library");
    }
    /* #region agent log */
    dbg_log("B", "main.c:finish_wifi", "wifi_ok_no_http", "{}");
    /* #endregion */
}

/* Probe PC server — only from menu, never mid-dialog teardown */
static int try_server(void) {
    char *body = NULL;
    /* #region agent log */
    {
        union SceNetApctlInfo info;
        char d[96];
        if (sceNetApctlGetInfo(8, &info) == 0) {
            snprintf(d, sizeof(d), "{\"psp_ip\":\"%s\"}", info.ip);
        } else {
            snprintf(d, sizeof(d), "{\"psp_ip\":\"?\"}");
        }
        dbg_log("B", "main.c:try_server", "enter", d);
    }
    /* #endregion */
    if (!g_net_ok && !net_is_connected()) {
        set_status("Connect WiFi first");
        return 0;
    }
    g_net_ok = 1;
    if (api_get("/api/status", &body) == 0) {
        g_online_mode = 1;
        set_status("Online OK");
        free(body);
        /* #region agent log */
        dbg_log("B", "main.c:try_server", "server_ok", "{}");
        /* #endregion */
        try_resume_from_server();
        return 1;
    }
    g_online_mode = 0;
    set_status("No server — check IP/firewall");
    /* #region agent log */
    dbg_log("B", "main.c:try_server", "server_fail", "{}");
    /* #endregion */
    return 0;
}

/* Continue last track/position stored on the PC (not on Memory Stick). */
static void try_resume_from_server(void) {
    char *body = NULL;
    int tid = 0;
    int pos = 0;
    int paused = 1;
    int rating = 0;
    char title[MAX_NAME];
    static int s_did_resume;

    if (s_did_resume) {
        return;
    }
    if (player_is_active() || g_buffering || g_play_started) {
        return;
    }
    if (http_get(g_cfg.host, g_cfg.port, "/api/playback/state", &body, NULL) != HTTP_OK || !body) {
        return;
    }
    jutil_extract_int(body, "track_id", &tid);
    jutil_extract_int(body, "position_ms", &pos);
    jutil_extract_int(body, "rating", &rating);
    {
        const char *p = strstr(body, "\"paused\"");
        if (p) {
            p = strchr(p, ':');
            if (p && strstr(p, "false")) {
                paused = 0;
            }
        }
    }
    title[0] = '\0';
    jutil_extract_string(body, "title", title, sizeof(title));
    {
        char artist[MAX_NAME];
        char album[MAX_NAME];
        artist[0] = album[0] = '\0';
        jutil_extract_string(body, "artist", artist, sizeof(artist));
        jutil_extract_string(body, "album", album, sizeof(album));
        if (artist[0]) {
            clip_copy(g_artist_name, sizeof(g_artist_name), artist);
        }
        if (album[0]) {
            clip_copy(g_album_name, sizeof(g_album_name), album);
        }
    }
    free(body);
    if (tid <= 0 || !title[0]) {
        /* Missing/stale track after rescan — never start a 404 stream. */
        return;
    }
    s_did_resume = 1;
    g_pending_start_ms = (pos > 1500) ? pos : 0;
    g_now_duration_ms = 0;
    {
        float dur = 0.0f;
        /* duration may be float seconds in JSON */
        (void)dur;
    }
    set_status(paused ? "Resume (paused)" : "Resuming...");
    play_track_online(tid, title, rating);
    if (paused) {
        /* Pause after buffer starts — short poll, don't freeze UI for seconds */
        int i;
        for (i = 0; i < 40; i++) {
            if (g_buffering || (g_dl_thid >= 0 && !g_dl_done)) {
                try_start_play_from_buffer();
            }
            if (g_play_started) {
                player_pause();
                set_status("Paused — SELECT play");
                break;
            }
            if (g_dl_err) {
                break;
            }
            sceKernelDelayThread(50000);
        }
    }
}

static void format_updater_screen(char *line1, int l1sz, char *line2, int l2sz) {
    const UpdateStatus *us = updater_status();
    const char *cur = us->current_version[0] ? us->current_version : "?";
    const char *rem = us->remote_version[0] ? us->remote_version : "?";

    if (us->state == UPD_CHECKING) {
        snprintf(line1, l1sz, "PSP Music");
        snprintf(line2, l2sz, "Checking server...");
        return;
    }
    if (us->state == UPD_DOWNLOADING || us->state == UPD_VERIFYING) {
        if (us->app_missing) {
            snprintf(line1, l1sz, "Downloading PSP Music %s", rem);
        } else {
            snprintf(line1, l1sz, "Updating %s -> %s", cur, rem);
        }
        snprintf(
            line2,
            l2sz,
            "%d%%  %d/%d KB",
            us->percent,
            us->bytes / 1024,
            us->total > 0 ? us->total / 1024 : 0
        );
        return;
    }
    if (us->state == UPD_COMPLETE) {
        snprintf(line1, l1sz, "PSP Music %s ready", rem);
        snprintf(line2, l2sz, "Exit, then open PSP Music");
        return;
    }
    if (us->state == UPD_ERROR) {
        snprintf(line1, l1sz, "PSP Music: %s", us->app_missing ? "not installed" : cur);
        snprintf(line2, l2sz, "%s", us->error[0] ? us->error : "Check failed");
        return;
    }
    if (us->state == UPD_INCOMPLETE) {
        snprintf(line1, l1sz, "PSP Music: %s", us->app_missing ? "not installed" : cur);
        snprintf(line2, l2sz, "Incomplete — X resume");
        return;
    }
    if (us->state == UPD_AVAILABLE) {
        if (us->app_missing || us->local_code <= 0) {
            snprintf(line1, l1sz, "PSP Music: not installed");
            snprintf(line2, l2sz, "X = download %s   O = quit", rem);
        } else {
            snprintf(line1, l1sz, "PSP Music: %s (outdated)", cur);
            snprintf(line2, l2sz, "X = update to %s   O = quit", rem);
        }
        return;
    }
    if (us->state == UPD_UP_TO_DATE) {
        snprintf(line1, l1sz, "PSP Music: %s", rem[0] != '?' ? rem : cur);
        snprintf(line2, l2sz, "You already have the latest version");
        return;
    }
    /* IDLE */
    if (us->app_missing) {
        snprintf(line1, l1sz, "PSP Music: not installed");
        snprintf(line2, l2sz, "X = Wi-Fi & check   O = quit");
    } else {
        snprintf(line1, l1sz, "PSP Music: %s", cur);
        snprintf(line2, l2sz, "X = Wi-Fi & check   O = quit");
    }
}

static void try_connect(void) {
    int ok;
    SceCtrlData pad;
    SceCtrlData prev;
    /* #region agent log */
    dbg_log("A", "main.c:try_connect", "try_connect_enter", "{}");
    /* #endregion */
    stop_playback_all(1);

    /*
     * Log showed wlan:0 → instant offline + home (looked like broken Connect).
     * Wait here with a readable message until switch is ON or Circle cancels.
     */
    memset(&prev, 0, sizeof(prev));
    while (!net_wlan_on()) {
        unsigned int pressed;
        if (g_app_exiting) {
            finish_wifi_result(0, 0, 0);
            set_home_menu();
            g_screen = SCREEN_HOME;
            return;
        }
        set_status("Flip WLAN switch UP — O cancel");
        ui_begin();
        ui_clear(UI_COL_BG);
        ui_draw_message(
            "Wi-Fi",
            "Turn the WLAN switch ON",
            "Then the connection list opens. O = cancel",
            0
        );
#ifdef DEBUG_HUD
        ui_draw_debug_overlay();
#endif
        ui_end();

        sceCtrlReadBufferPositive(&pad, 1);
        pressed = pad.Buttons & ~prev.Buttons;
        prev = pad;
        if (pressed & PSP_CTRL_CIRCLE) {
            dbg_log("A", "main.c:try_connect", "wlan_wait_cancel", "{}");
            finish_wifi_result(0, 0, 0);
            set_status("Offline — turn WLAN ON");
            set_home_menu();
            g_screen = SCREEN_HOME;
            return;
        }
        sceKernelDelayThread(50000);
    }

    set_status("Opening Wi-Fi dialog...");
    ok = net_connect_dialog();
    /* #region agent log */
    {
        char d[80];
        snprintf(d, sizeof(d), "{\"ok\":%d,\"last_err\":%d}", ok, net_last_error());
        dbg_log("B", "main.c:try_connect", "net_connect_dialog_returned", d);
    }
    /* #endregion */
    finish_wifi_result(ok, 1, 0);
    if (ok && g_boot_update_mode) {
        g_online_mode = 1;
        g_screen = SCREEN_UPDATE;
        /* Check only — never auto-download. User confirms with X if newer. */
        updater_check(g_cfg.host, g_cfg.port);
        set_status("Checked server");
    } else if (!g_boot_update_mode) {
        set_home_menu();
        g_screen = SCREEN_HOME;
    } else {
        g_screen = SCREEN_UPDATE;
    }
    /* Paint one frame now — main loop is stuck mid-frame during try_connect */
    if (net_gu_was_used()) {
        ui_gpu_reset();
    }
    ui_begin();
    ui_clear(UI_COL_BG);
    if (g_boot_update_mode) {
        char line1[128];
        char line2[128];
        format_updater_screen(line1, sizeof(line1), line2, sizeof(line2));
        ui_draw_message("Music Updater", line1, line2, 0);
    } else {
        draw_current_library();
    }
#ifdef DEBUG_HUD
    ui_draw_debug_overlay();
#endif
    ui_end();
}

static void parse_host_octets(int out[4]) {
    int n1 = 192, n2 = 168, n3 = 0, n4 = 2;
    if (sscanf(g_cfg.host, "%d.%d.%d.%d", &n1, &n2, &n3, &n4) != 4) {
        n1 = 192;
        n2 = 168;
        n3 = 0;
        n4 = 2;
    }
    out[0] = n1 & 255;
    out[1] = n2 & 255;
    out[2] = n3 & 255;
    out[3] = n4 & 255;
}

static void write_host_octets(const int o[4]) {
    snprintf(g_cfg.host, MAX_HOST, "%d.%d.%d.%d", o[0] & 255, o[1] & 255, o[2] & 255, o[3] & 255);
}

static int save_setup_cfg(void);

static void edit_host_octet(int delta) {
    int o[4];
    parse_host_octets(o);
    if (g_setup_octet < 0) {
        g_setup_octet = 0;
    }
    if (g_setup_octet > 3) {
        g_setup_octet = 3;
    }
    o[g_setup_octet] = (o[g_setup_octet] + delta) & 255;
    write_host_octets(o);
    /* Persist immediately so reboot keeps the IP even if user force-quits. */
    save_setup_cfg();
}

static void format_host_with_cursor(char *out, int out_sz) {
    int o[4];
    int i;
    char part[16];
    parse_host_octets(o);
    out[0] = '\0';
    for (i = 0; i < 4; i++) {
        if (i == g_setup_octet && g_setup_focus == 0) {
            snprintf(part, sizeof(part), "[%d]", o[i]);
        } else {
            snprintf(part, sizeof(part), "%d", o[i]);
        }
        if (i > 0) {
            strncat(out, ".", out_sz - (int)strlen(out) - 1);
        }
        strncat(out, part, out_sz - (int)strlen(out) - 1);
    }
}

static int save_setup_cfg(void) {
    char msg[80];
    if (config_save(&g_cfg) == 0) {
        snprintf(msg, sizeof(msg), "Saved %s:%d", g_cfg.host, g_cfg.port);
        set_status(msg);
        return 0;
    }
    snprintf(msg, sizeof(msg), "Save FAIL %s", g_cfg.host);
    set_status(msg);
    return -1;
}

static void begin_play_from_list(void) {
    g_play_index = g_cursor;
    g_return_screen = g_screen;
    if (g_shuffle && g_count > 1) {
        shuffle_rebuild(g_play_index);
    }
}

static void open_now_playing_view(void) {
    /* Resume the Now Playing UI without restarting the current track. */
    if (g_track_id <= 0 && !player_is_active() && !g_buffering && !g_play_started) {
        set_status("Nothing playing");
        return;
    }
    if (g_screen != SCREEN_PLAYING) {
        g_return_screen = g_screen;
    }
    g_show_eq = 0;
    g_queue_focus = 0;
    g_show_track_info = 0;
    g_screen = SCREEN_PLAYING;
    set_status(player_is_paused() ? "Paused" : "Now Playing");
}

static void play_at_index(int idx) {
    if (idx < 0 || idx >= g_count) {
        return;
    }
    g_decoder_auto_retry = 0; /* fresh track — allow one decoder restart */
    g_play_index = idx;
    g_cursor = idx;
    g_now_duration_ms = (g_items[idx].duration > 0) ? g_items[idx].duration * 1000 : 0;
    /* Prefer per-track metadata from list (real queue rows). */
    if (g_items[idx].artist[0]) {
        strncpy(g_artist_name, g_items[idx].artist, MAX_NAME - 1);
        g_artist_name[MAX_NAME - 1] = '\0';
    }
    if (g_items[idx].album[0]) {
        strncpy(g_album_name, g_items[idx].album, MAX_NAME - 1);
        g_album_name[MAX_NAME - 1] = '\0';
    }
    if (g_return_screen == SCREEN_OFFLINE) {
        play_track_offline_idx(idx);
    } else {
        play_track_online(g_items[idx].id, g_items[idx].name, g_items[idx].extra);
    }
}

static void skip_track(int delta) {
    int next;
    if (g_count <= 0) {
        return;
    }
    /* Don't stack skip/seek while HTTP stream is restarting. */
    if (g_seek_busy || (g_buffering && g_dl_thid >= 0 && !g_dl_done && g_dl_bytes < 4096)) {
        set_status("Wait...");
        return;
    }
    report_current_play(0);
    position_save_if_needed(1);
    set_status("Skipping...");
    g_tcp_auto_retry = 1;
    if (g_play_index < 0) {
        g_play_index = g_cursor;
    }
    if (g_shuffle && g_count > 1) {
        if (delta > 0) {
            if (g_shuffle_pos + 1 < g_count) {
                g_shuffle_pos++;
                next = g_shuffle_order[g_shuffle_pos];
            } else if (g_repeat) {
                shuffle_rebuild(g_play_index);
                g_shuffle_pos = (g_shuffle_pos + 1) % g_count;
                next = g_shuffle_order[g_shuffle_pos];
            } else {
                next = g_play_index;
                set_status("Finished");
                return;
            }
        } else {
            if (g_shuffle_pos > 0) {
                g_shuffle_pos--;
                next = g_shuffle_order[g_shuffle_pos];
            } else if (g_repeat) {
                g_shuffle_pos = g_count - 1;
                next = g_shuffle_order[g_shuffle_pos];
            } else {
                next = g_play_index;
                return;
            }
        }
    } else {
        next = g_play_index + delta;
        if (next < 0) {
            next = g_repeat ? (g_count - 1) : 0;
        }
        if (next >= g_count) {
            next = g_repeat ? 0 : (g_count - 1);
        }
    }
    play_at_index(next);
}

/*
 * Playback is independent from the active UI screen.  This keeps an online
 * download progressing and preserves auto-next while the user browses Home,
 * Artists, Albums or Appearance.
 */
static void pump_background_playback(void) {
    int screen_before = g_screen;
    reap_download_thread();
    if (g_buffering || (g_dl_thid >= 0 && !g_dl_done)) {
        try_start_play_from_buffer();
    }
    poll_download_side_effects();

    if (g_play_started && !player_update()) {
        report_current_play(1);
        if (g_repeat && g_play_index >= 0) {
            play_at_index(g_play_index);
        } else if (g_play_index >= 0 && g_play_index < g_count - 1) {
            skip_track(1);
        } else {
            set_status("Finished");
            g_play_started = 0;
            position_save_if_needed(1);
        }
        /*
         * play_at_index opens Now Playing by design.  During background
         * playback, retain the page the listener was browsing instead.
         */
        if (screen_before != SCREEN_PLAYING && g_screen == SCREEN_PLAYING) {
            g_screen = screen_before;
        }
    }
}

int main(int argc, char *argv[]) {
    (void)argc;

    setup_callbacks();
    paths_init();
    if (argv && argv[0]) {
        paths_init_argv(argv[0]);
    }
    storage_init();
    metrics_init();
    download_init();
    ui_init();
    ui_image_init();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    config_load(&g_cfg);
    http_set_api_key(g_cfg.api_key);
    theme_init();
    theme_load();
    g_eq_preset = player_get_eq_preset();
    g_eq_cursor = g_eq_preset;
    g_appear_skin = skin_get_id();
    offline_ensure_dirs();
    set_status("Ready");

    /* XMB companion: Game → "Music Updater" — OTA only lives here. */
    if (paths_is_update_companion()) {
        g_boot_update_mode = 1;
        updater_init();
        g_screen = SCREEN_UPDATE;
        set_status("X = Wi-Fi & update   O = quit");
        dbg_log("U", "main.c:boot", "update_companion", "{}");
    } else {
        set_home_menu();
    }
    /* #region agent log */
    {
        char d[160];
        snprintf(d, sizeof(d), "{\"base\":\"%.120s\",\"comp\":%d}", paths_base(), g_boot_update_mode);
        dbg_log("E", "main.c:main", "app_started", d);
    }
    /* #endregion */

    SceCtrlData pad, old;
    memset(&old, 0, sizeof(old));

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);
        unsigned int pressed = pad.Buttons & ~old.Buttons;
        old = pad;

        ui_begin();
        if (g_screen != SCREEN_PLAYING) {
            pump_background_playback();
        }

        if (g_screen == SCREEN_PLAYING) {
            /* Handle Stop/Pause/Skip/Queue/EQ before UI so we never wait on audio I/O. */
            if (g_show_eq) {
                if (pressed & PSP_CTRL_UP) {
                    g_eq_cursor--;
                    if (g_eq_cursor < 0) {
                        g_eq_cursor = PLAYER_EQ_COUNT - 1;
                    }
                }
                if (pressed & PSP_CTRL_DOWN) {
                    g_eq_cursor++;
                    if (g_eq_cursor >= PLAYER_EQ_COUNT) {
                        g_eq_cursor = 0;
                    }
                }
                if (pressed & PSP_CTRL_LEFT) {
                    g_eq_cursor--;
                    if (g_eq_cursor < 0) {
                        g_eq_cursor = PLAYER_EQ_COUNT - 1;
                    }
                    g_eq_preset = g_eq_cursor;
                    player_set_eq_preset(g_eq_preset);
                    theme_save();
                }
                if (pressed & PSP_CTRL_RIGHT) {
                    g_eq_cursor++;
                    if (g_eq_cursor >= PLAYER_EQ_COUNT) {
                        g_eq_cursor = 0;
                    }
                    g_eq_preset = g_eq_cursor;
                    player_set_eq_preset(g_eq_preset);
                    theme_save();
                }
                if (pressed & PSP_CTRL_CROSS) {
                    g_eq_preset = g_eq_cursor;
                    player_set_eq_preset(g_eq_preset);
                    theme_save();
                    set_status("EQ applied");
                }
                if ((pressed & PSP_CTRL_SELECT) || (pressed & PSP_CTRL_CIRCLE)) {
                    g_show_eq = 0;
                }
            } else if (g_queue_focus) {
                if (pressed & PSP_CTRL_UP && g_queue_cursor > 0) {
                    g_queue_cursor--;
                }
                if (pressed & PSP_CTRL_DOWN && g_queue_cursor < g_count - 1) {
                    g_queue_cursor++;
                }
                if (pressed & PSP_CTRL_CROSS && g_count > 0) {
                    play_at_index(g_queue_cursor);
                    g_queue_focus = 0;
                }
                if ((pressed & PSP_CTRL_CIRCLE) || (pressed & PSP_CTRL_TRIANGLE)) {
                    g_queue_focus = 0;
                }
            } else {
                if (pressed & PSP_CTRL_CROSS) {
                    /* X = Play when stopped, Pause/Resume when active */
                    if (player_is_active()) {
                        player_toggle_pause();
                        if (player_is_paused()) {
                            position_save_if_needed(1);
                        }
                        dbg_log("C", "main.c:playing", "playing_pause", "{}");
                    } else {
                        dbg_log("C", "main.c:playing", "playing_play", "{}");
                        replay_current_track();
                    }
                }
                if (pressed & PSP_CTRL_SQUARE) {
                    const DownloadStatus *ds = download_status();
                    if (g_show_track_info) {
                        int r = g_now_rating + 1;
                        if (r > 5) {
                            r = 0;
                        }
                        rate_current(r);
                    } else if (ds->state == DL_RUNNING || ds->state == DL_PAUSED ||
                        ds->state == DL_INCOMPLETE || ds->state == DL_VERIFYING) {
                        download_delete_incomplete();
                        set_status("Download cancelled");
                    } else {
                        dbg_log("C", "main.c:playing", "playing_stop", "{}");
                        stop_playback_all(0);
                        set_status("Stopped");
                    }
                }
                if (pressed & PSP_CTRL_TRIANGLE) {
                    /* Queue focus — real track list */
                    bind_queue_ptrs();
                    g_queue_focus = 1;
                    g_queue_cursor = (g_play_index >= 0) ? g_play_index : g_cursor;
                    if (g_queue_cursor < 0) {
                        g_queue_cursor = 0;
                    }
                    if (g_queue_cursor >= g_count && g_count > 0) {
                        g_queue_cursor = g_count - 1;
                    }
                }
                if (pressed & PSP_CTRL_UP) {
                    player_set_volume(player_get_volume() + 5);
                    snprintf(g_status, sizeof(g_status), "Volume %d%%", player_get_volume());
                }
                if (pressed & PSP_CTRL_DOWN) {
                    player_set_volume(player_get_volume() - 5);
                    snprintf(g_status, sizeof(g_status), "Volume %d%%", player_get_volume());
                }
                if (pressed & PSP_CTRL_LEFT) {
                    dbg_log("C", "main.c:playing", "playing_skip", "{\"d\":-1}");
                    skip_track(-1);
                }
                if (pressed & PSP_CTRL_RIGHT) {
                    dbg_log("C", "main.c:playing", "playing_skip", "{\"d\":1}");
                    skip_track(1);
                }
                if (pressed & PSP_CTRL_SELECT) {
                    /* SELECT = EQ; L+SELECT = track info (rating with Square). */
                    if (pad.Buttons & PSP_CTRL_LTRIGGER) {
                        g_show_eq = 0;
                        g_show_track_info = !g_show_track_info;
                    } else {
                        g_show_track_info = 0;
                        g_show_eq = 1;
                        g_eq_cursor = g_eq_preset;
                    }
                }
                if ((pressed & PSP_CTRL_LTRIGGER) && !(pad.Buttons & PSP_CTRL_SELECT)) {
                    g_shuffle = !g_shuffle;
                    if (g_shuffle && g_count > 1) {
                        shuffle_rebuild(g_play_index >= 0 ? g_play_index : g_cursor);
                    } else {
                        shuffle_reset();
                    }
                }
                if (pressed & PSP_CTRL_RTRIGGER) {
                    g_repeat = !g_repeat;
                }
                if (pressed & PSP_CTRL_START) {
                    const DownloadStatus *ds = download_status();
                    dbg_log("C", "main.c:playing", "playing_save", "{}");
                    if (ds->state == DL_RUNNING) {
                        download_pause();
                        set_status("Download paused");
                    } else if (ds->state == DL_PAUSED || ds->state == DL_INCOMPLETE) {
                        if (download_resume(g_cfg.host, g_cfg.port) == 0) {
                            set_status("Download resume");
                        }
                    } else {
                        save_offline_current();
                    }
                }
                if (pressed & PSP_CTRL_CIRCLE) {
                    dbg_log("C", "main.c:playing", "playing_back", "{}");
                    /*
                     * O leaves the Now Playing screen only.  The player and
                     * its HTTP download thread stay alive so browsing the
                     * library never interrupts online or offline playback.
                     * Square is the explicit Stop control.
                     */
                    if (g_show_track_info) {
                        g_show_track_info = 0;
                    } else {
                        g_show_eq = 0;
                        g_queue_focus = 0;
                        g_screen = g_return_screen;
                    }
                }
            }

            /* Progressive buffer → start audio; then poll EOF for auto-next. */
            reap_download_thread(); /* free socket so NP cover can load after stream */
            if (g_buffering || (g_dl_thid >= 0 && !g_dl_done)) {
                try_start_play_from_buffer();
            }
            maybe_fetch_cover_after_stream();
            poll_download_side_effects();

            /* Analog stick (circle nub): U/D volume, L/R scrub timeline.
             * Hold left/right to pick a minute; release stick to jump there. */
            if (!g_show_eq && !g_queue_focus && !g_show_track_info) {
                int lx = (int)pad.Lx - 128;
                int ly = (int)pad.Ly - 128;
                int dur = player_duration_ms();
                if (dur <= 0) {
                    dur = g_now_duration_ms;
                }
                if (ly < -55) {
                    player_set_volume(player_get_volume() + 2);
                    snprintf(g_status, sizeof(g_status), "Volume %d%%", player_get_volume());
                } else if (ly > 55) {
                    player_set_volume(player_get_volume() - 2);
                    snprintf(g_status, sizeof(g_status), "Volume %d%%", player_get_volume());
                }
                if (player_is_active() && !g_buffering && dur > 2000 &&
                    (lx > 35 || lx < -35)) {
                    int target;
                    if (!g_seek_dragging) {
                        g_seek_dragging = 1;
                        g_seek_base_ms = player_elapsed_ms();
                        g_seek_hold_ms = 0;
                    }
                    /* Full stick ≈ ±90s per second of wall time (~16ms/frame). */
                    g_seek_hold_ms += (lx * 90) / 8;
                    target = g_seek_base_ms + g_seek_hold_ms;
                    if (target < 0) {
                        target = 0;
                    }
                    if (target > dur - 500) {
                        target = dur - 500;
                    }
                    g_seek_preview_ms = target;
                    snprintf(
                        g_status,
                        sizeof(g_status),
                        "Seek %d:%02d / %d:%02d",
                        (target / 1000) / 60,
                        (target / 1000) % 60,
                        (dur / 1000) / 60,
                        (dur / 1000) % 60
                    );
                } else if (g_seek_dragging) {
                    int go = g_seek_preview_ms;
                    g_seek_dragging = 0;
                    g_seek_hold_ms = 0;
                    g_seek_preview_ms = -1;
                    if (go >= 0) {
                        seek_stream_to_ms(go);
                    }
                }
            }

            if (g_play_started && !player_update()) {
                report_current_play(1);
                if (g_repeat && g_play_index >= 0) {
                    play_at_index(g_play_index);
                } else if (g_play_index >= 0 && g_play_index < g_count - 1) {
                    skip_track(1);
                } else {
                    set_status("Finished");
                    g_play_started = 0;
                }
            }

            {
                UiNowPlaying np;
                memset(&np, 0, sizeof(np));
                bind_queue_ptrs();
                np.title = g_now_title;
                np.artist = g_now_artist;
                np.album = g_now_album;
                np.rating = g_now_rating;
                np.offline_saved = g_now_offline;
                np.status = g_status;
                np.playing = player_is_active() || g_buffering;
                np.paused = player_is_paused();
                np.elapsed_ms = (g_seek_dragging && g_seek_preview_ms >= 0)
                    ? g_seek_preview_ms
                    : player_elapsed_ms();
                {
                    int dur = player_duration_ms();
                    if (dur <= 0) {
                        dur = g_now_duration_ms;
                    }
                    np.duration_ms = dur;
                }
                np.volume = player_get_volume();
                np.bitrate_kbps = player_bitrate_kbps();
                np.sample_khz = player_sample_khz();
                np.stereo = player_is_stereo();
                np.shuffle = g_shuffle;
                np.repeat = g_repeat;
                np.show_eq = g_show_eq;
                np.eq_preset = g_eq_preset;
                np.eq_cursor = g_eq_cursor;
                np.theme_id = theme_get_id();
                np.layout_id = layout_get_id();
                np.queue_titles = g_queue_titles;
                np.queue_artists = g_queue_artists;
                np.queue_count = g_count;
                np.queue_index = g_play_index;
                np.queue_cursor = g_queue_cursor;
                np.queue_focus = g_queue_focus;
                np.buffering = g_buffering;
                np.dl_bytes = g_dl_bytes;
                np.dl_total = g_dl_total;
                np.save_pending = g_save_pending || (download_status()->state == DL_RUNNING);
                {
                    const DownloadStatus *ds = download_status();
                    np.dl_state = (int)ds->state;
                    np.dl_percent = ds->percent;
                    np.dl_speed_bps = ds->speed_bps;
                    np.dl_name = ds->title[0] ? ds->title : NULL;
                    if (ds->state == DL_RUNNING || ds->state == DL_PAUSED ||
                        ds->state == DL_VERIFYING || ds->state == DL_INCOMPLETE) {
                        np.dl_bytes = ds->bytes;
                        np.dl_total = ds->total;
                    }
                }
                np.lossless = player_is_lossless();
                np.bit_depth = player_bit_depth();
                np.online = !g_from_offline;
                np.buffered_sec = player_buffered_seconds();
                np.buffer_state = (int)player_buffer_state();
                np.format_name = player_format_name();
                if ((!np.format_name || !np.format_name[0] || strcmp(np.format_name, "—") == 0) &&
                    np.buffering) {
                    np.format_name = g_stream_lossless ? "FLAC" : "MP3";
                }
                if (player_sample_khz() > 0) {
                    np.sample_khz = player_sample_khz();
                } else if (g_now_sample_rate > 0) {
                    np.sample_khz = g_now_sample_rate / 1000;
                }
                np.cover = ui_image_cover_for(g_track_id);
                if (!np.cover) {
                    np.cover = ui_image_cover();
                }
                ui_draw_now_playing(&np);
                if (g_show_track_info && !g_show_eq) {
                    const PlayerTheme *th = theme_active();
                    char line[96];
                    ui_fill(16, 40, UI_SCREEN_W - 32, 160, th->panel);
                    ui_fill(16, 40, 4, 160, th->accent);
                    ui_text_clip(28, 50, 420, th->text, g_now_title[0] ? g_now_title : "Track");
                    ui_text_clip(28, 70, 420, th->muted, g_now_artist[0] ? g_now_artist : "-");
                    ui_text_clip(28, 88, 420, th->muted, g_now_album[0] ? g_now_album : "-");
                    snprintf(line, sizeof(line), "Rating %d/5  ·  %s",
                             g_now_rating,
                             g_now_format[0] ? g_now_format : (g_stream_lossless ? "flac" : "mp3"));
                    ui_text_clip(28, 110, 420, th->text, line);
                    if (g_now_sample_rate > 0) {
                        snprintf(line, sizeof(line), "%d Hz / %d bit",
                                 g_now_sample_rate, g_now_bit_depth > 0 ? g_now_bit_depth : 16);
                        ui_text_clip(28, 128, 420, th->muted, line);
                    }
                    ui_text_clip(28, 150, 420, th->accent, "Square = rate   O = close info");
                    ui_text_clip(28, 170, 420, th->muted, "SELECT = EQ   Stick L/R = seek");
                }
            }
        } else if (g_screen == SCREEN_APPEARANCE) {
            ui_draw_appearance(
                1,
                g_appear_skin,
                0,
                g_now_title,
                g_now_artist,
                player_is_active()
            );
            if (pressed & PSP_CTRL_LEFT) {
                g_appear_skin--;
                if (g_appear_skin < 0) {
                    g_appear_skin = SKIN_COUNT - 1;
                }
                skin_set_preview(g_appear_skin);
            }
            if (pressed & PSP_CTRL_RIGHT) {
                g_appear_skin++;
                if (g_appear_skin >= SKIN_COUNT) {
                    g_appear_skin = 0;
                }
                skin_set_preview(g_appear_skin);
            }
            if (pressed & PSP_CTRL_CROSS) {
                skin_set_preview(g_appear_skin);
                skin_apply_preview();
                set_status("Theme applied");
            }
            if (pressed & PSP_CTRL_CIRCLE) {
                skin_set_preview(skin_get_id());
                if (g_appear_return == SCREEN_HOME) {
                    set_home_menu();
                    g_screen = SCREEN_HOME;
                } else {
                    set_settings_menu();
                    g_screen = SCREEN_SETTINGS;
                }
            }
        } else if (g_screen == SCREEN_SETUP) {
            int octets[4];
            parse_host_octets(octets);
            ui_draw_setup("Setup", octets, g_setup_octet, g_cfg.port, g_setup_focus, player_is_active());

            /* SELECT toggles IP <-> Port */
            if (pressed & PSP_CTRL_SELECT) {
                g_setup_focus ^= 1;
            }

            if (g_setup_focus == 0) {
                if (pressed & (PSP_CTRL_LEFT | PSP_CTRL_LTRIGGER)) {
                    g_setup_octet--;
                    if (g_setup_octet < 0) {
                        g_setup_octet = 3;
                    }
                }
                if (pressed & (PSP_CTRL_RIGHT | PSP_CTRL_RTRIGGER)) {
                    g_setup_octet++;
                    if (g_setup_octet > 3) {
                        g_setup_octet = 0;
                    }
                }
                if (pressed & PSP_CTRL_UP) {
                    edit_host_octet(1);
                }
                if (pressed & PSP_CTRL_DOWN) {
                    edit_host_octet(-1);
                }
                if (pressed & PSP_CTRL_TRIANGLE) {
                    edit_host_octet(10);
                }
                if (pressed & PSP_CTRL_SQUARE) {
                    edit_host_octet(-10);
                }
            } else {
                if (pressed & PSP_CTRL_UP) {
                    if (g_cfg.port < 65535) {
                        g_cfg.port++;
                        save_setup_cfg();
                    }
                }
                if (pressed & PSP_CTRL_DOWN && g_cfg.port > 1) {
                    g_cfg.port--;
                    save_setup_cfg();
                }
                if (pressed & PSP_CTRL_TRIANGLE) {
                    g_cfg.port += 10;
                    if (g_cfg.port > 65535) {
                        g_cfg.port = 65535;
                    }
                    save_setup_cfg();
                }
                if (pressed & PSP_CTRL_SQUARE) {
                    g_cfg.port -= 10;
                    if (g_cfg.port < 1) {
                        g_cfg.port = 1;
                    }
                    save_setup_cfg();
                }
            }

            if (pressed & (PSP_CTRL_START | PSP_CTRL_CROSS)) {
                save_setup_cfg();
                g_online_mode = 0; /* force re-probe with new IP */
                if (g_setup_return == SCREEN_HOME) {
                    set_home_menu();
                    g_screen = SCREEN_HOME;
                } else {
                    set_settings_menu();
                    g_cursor = 0;
                    g_screen = SCREEN_SETTINGS;
                }
            }
            if (pressed & PSP_CTRL_CIRCLE) {
                /* Auto-save on back — previously only START saved, so IP "didn't stick". */
                save_setup_cfg();
                g_online_mode = 0;
                if (g_setup_return == SCREEN_HOME) {
                    set_home_menu();
                    g_screen = SCREEN_HOME;
                } else {
                    set_settings_menu();
                    g_cursor = 0;
                    g_screen = SCREEN_SETTINGS;
                }
            }
        } else if (g_screen == SCREEN_INFO) {
            ui_draw_info(
                g_info_title,
                g_info_line1,
                g_info_line2,
                g_info_line3,
                "O = back",
                player_is_active()
            );
            if (pressed & PSP_CTRL_CIRCLE) {
                g_screen = g_info_return_screen;
            }
        } else if (g_screen == SCREEN_DIAG) {
            const StorageStats *st = storage_stats();
            const AudioMetrics *m = metrics_get();
            const PlayerTheme *th = theme_active();
            char line[96];
            int y = 56;
            ui_clear(th->bg);
            ui_draw_header("Diagnostics", player_is_active());
            ui_fill(12, 48, UI_SCREEN_W - 24, 200, th->panel);
            ui_fill(12, 56, 3, 184, th->accent);

            snprintf(line, sizeof(line), "AUDIO  %s  %s",
                     m->formatName[0] ? m->formatName : "—",
                     m->formatLossless ? "LOSSLESS" : "LOSSY");
            ui_text_clip(22, y, 440, th->text, line); y += 16;
            snprintf(line, sizeof(line), "  %d Hz / %d bit", m->sampleRate, m->bitDepth);
            ui_text_clip(22, y, 440, th->muted, line); y += 18;

            snprintf(line, sizeof(line), "BUFFER  %.2fMB  used %.0f%%  %.1fs",
                     m->ramBufferSize / (1024.0f * 1024.0f), m->bufferUsedPct, m->bufferedSeconds);
            ui_text_clip(22, y, 440, th->text, line); y += 18;

            snprintf(line, sizeof(line), "NETWORK  %.2f Mbps", m->networkRateMbps);
            ui_text_clip(22, y, 440, th->text, line); y += 18;

            snprintf(line, sizeof(line), "DECODER  %.2f Mbps", m->flacDecodeRateMbps);
            ui_text_clip(22, y, 440, th->text, line); y += 18;

            {
                const char *streason =
                    g_dl_fail_reason[0]
                        ? (const char *)g_dl_fail_reason
                        : (g_buffering ? "buf" : (g_play_started ? "ok" : "-"));
                snprintf(line, sizeof(line), "STREAM  %d/%d  %s", g_dl_bytes, g_dl_total, streason);
            }
            ui_text_clip(22, y, 440, th->muted, line); y += 16;

            snprintf(line, sizeof(line), "STORAGE  writes %llu  MB %.2f",
                     (unsigned long long)st->memoryStickWrites,
                     st->memoryStickBytesWritten / (1024.0f * 1024.0f));
            ui_text_clip(22, y, 440, th->text, line); y += 18;

            snprintf(line, sizeof(line), "REBUFFER  %u  underrun %u",
                     m->rebufferCount, m->bufferUnderruns);
            ui_text_clip(22, y, 440, th->text, line); y += 18;

            {
                char hp[72];
                format_host_port(hp, sizeof(hp));
                snprintf(line, sizeof(line), "SERVER  %s", hp);
                ui_text_clip(22, y, 440, th->muted, line);
            }
            ui_text(22, 236, th->accent, "O = back");

            if (pressed & PSP_CTRL_CIRCLE) {
                set_settings_menu();
                g_screen = SCREEN_SETTINGS;
            }
        } else if (g_screen == SCREEN_UPDATE) {
            /* OTA UI only for Music Updater companion — never from the player. */
            if (!g_boot_update_mode) {
                set_settings_menu();
                g_screen = SCREEN_SETTINGS;
            } else {
            const UpdateStatus *us = updater_status();
            char line1[128];
            char line2[128];
            format_updater_screen(line1, sizeof(line1), line2, sizeof(line2));
            ui_draw_message("Music Updater", line1, line2, 0);
            if (pressed & PSP_CTRL_CROSS) {
                if (!g_net_ok) {
                    try_connect();
                } else if (us->state == UPD_AVAILABLE || us->state == UPD_INCOMPLETE) {
                    updater_start_download(g_cfg.host, g_cfg.port);
                } else if (us->state == UPD_ERROR) {
                    updater_check(g_cfg.host, g_cfg.port);
                } else if (us->state == UPD_IDLE) {
                    updater_check(g_cfg.host, g_cfg.port);
                }
                /* UP_TO_DATE / COMPLETE: X does nothing — already newest */
            }
            if (pressed & PSP_CTRL_CIRCLE) {
                sceKernelExitGame();
            }
            } /* g_boot_update_mode */
        } else if (g_offline_delete_confirm) {
                ui_draw_message(
                    "Delete?",
                    g_items[g_cursor].name,
                    "X = yes   O = no",
                    player_is_active()
                );
                if (pressed & PSP_CTRL_CROSS && g_count > 0) {
                    int tid = g_items[g_cursor].id;
                    if (g_from_offline && g_track_id == tid) {
                        stop_playback_all(1);
                    }
                    if (offline_delete(tid) == 0) {
                        set_status("Deleted");
                        load_offline_list();
                        if (g_cursor >= g_count && g_count > 0) {
                            g_cursor = g_count - 1;
                        }
                    } else {
                        set_status("Delete failed");
                    }
                    g_offline_delete_confirm = 0;
                }
                if (pressed & PSP_CTRL_CIRCLE) {
                    g_offline_delete_confirm = 0;
                }
            } else {
            /* Always show live host:port (Setup may have changed g_cfg). */
            if (g_screen == SCREEN_SETTINGS && g_count > 0) {
                format_host_port(g_rights[0], (int)sizeof(g_rights[0]));
                if (g_count > 2) {
                    clip_copy(
                        g_rights[2],
                        (int)sizeof(g_rights[0]),
                        g_net_ok ? "OK" : "Press X"
                    );
                }
            }
            draw_current_library();

            if (list_supports_pagination()) {
                if (player_is_active() || g_play_started || g_buffering) {
                    /* Music playing: shoulders skip tracks from any menu. */
                    if (pressed & PSP_CTRL_LTRIGGER) {
                        skip_track(-1);
                    }
                    if (pressed & PSP_CTRL_RTRIGGER) {
                        skip_track(1);
                    }
                } else {
                    if (pressed & (PSP_CTRL_LEFT | PSP_CTRL_LTRIGGER)) {
                        list_page_prev();
                    }
                    if (pressed & (PSP_CTRL_RIGHT | PSP_CTRL_RTRIGGER)) {
                        list_page_next();
                    }
                }
            } else if (player_is_active() || g_play_started || g_buffering) {
                if (pressed & PSP_CTRL_LTRIGGER) {
                    skip_track(-1);
                }
                if (pressed & PSP_CTRL_RTRIGGER) {
                    skip_track(1);
                }
            }

            if (pressed & PSP_CTRL_UP && g_cursor > 0) {
                g_cursor--;
            }
            if (pressed & PSP_CTRL_DOWN && g_cursor < g_count - 1) {
                g_cursor++;
            }

            /* Global play/pause + stop while browsing menus. */
            if (pressed & PSP_CTRL_SELECT) {
                if (player_is_active()) {
                    player_toggle_pause();
                    position_save_if_needed(1);
                    if (player_is_paused()) {
                        set_status("Paused");
                    } else {
                        set_status("Playing...");
                    }
                } else if (g_track_id > 0 && !g_buffering) {
                    replay_current_track();
                }
            }
            if (pressed & PSP_CTRL_START &&
                (player_is_active() || g_buffering || g_play_started)) {
                position_save_if_needed(1);
                stop_playback_all(0);
                set_status("Stopped");
            }

            if (pressed & PSP_CTRL_CROSS && g_count > 0) {
                /* #region agent log */
                {
                    char d[96];
                    snprintf(d, sizeof(d), "{\"screen\":%d,\"cursor\":%d,\"count\":%d}", g_screen, g_cursor, g_count);
                    dbg_log("C", "main.c:list_cross", "cross_pressed", d);
                }
                /* #endregion */
                if (g_screen == SCREEN_HOME) {
                    if (g_home_np >= 0 && g_cursor == g_home_np) {
                        open_now_playing_view();
                    } else if (g_cursor == g_home_offline) {
                        /* Offline Music — no WiFi needed */
                        /* #region agent log */
                        dbg_log("C", "main.c:list_cross", "home_offline_before", "{}");
                        /* #endregion */
                        load_offline_list();
                        g_screen = SCREEN_OFFLINE;
                        /* #region agent log */
                        {
                            char d[48];
                            snprintf(d, sizeof(d), "{\"count\":%d}", g_count);
                            dbg_log("C", "main.c:list_cross", "home_offline_after", d);
                        }
                        /* #endregion */
                    } else if (g_cursor == g_home_online) {
                        /* Online Library — Wi-Fi dialog, then server */
                        set_status("Opening Wi-Fi...");
                        if (ensure_online()) {
                            set_music_menu();
                            g_screen = SCREEN_MUSIC;
                        } else {
                            set_home_menu();
                        }
                    } else if (g_cursor == g_home_settings) {
                        set_settings_menu();
                        g_screen = SCREEN_SETTINGS;
                    }
                } else if (g_screen == SCREEN_SETTINGS) {
                    if (g_cursor == 0) {
                        g_setup_octet = 0;
                        g_setup_focus = 0;
                        g_setup_return = SCREEN_SETTINGS;
                        g_screen = SCREEN_SETUP;
                    } else if (g_cursor == 1) {
                        g_appear_skin = skin_get_id();
                        skin_set_preview(g_appear_skin);
                        g_appear_return = SCREEN_SETTINGS;
                        g_screen = SCREEN_APPEARANCE;
                    } else if (g_cursor == 2) {
                        set_status("Opening Wi-Fi...");
                        try_connect();
                        set_settings_menu();
                        g_cursor = 2;
                    } else if (g_cursor == 3) {
                        clip_copy(g_info_title, sizeof(g_info_title), "Controls");
                        clip_copy(g_info_line1, sizeof(g_info_line1), "PSP button map");
                        clip_copy(
                            g_info_line2,
                            sizeof(g_info_line2),
                            "X play/pause  O back  Square stop  "
                            "D-pad L/R skip  U/D volume  "
                            "Stick L/R seek (release=jump)  Stick U/D vol  "
                            "L shuffle  R repeat  SELECT = EQ  "
                            "L+SELECT = track info (Square rates)  "
                            "Triangle = Now Playing (or Home)  START save offline"
                        );
                        clip_copy(
                            g_info_line3,
                            sizeof(g_info_line3),
                            "L+SELECT then Square = rate stars. "
                            "Stick L/R scrub, release to jump. "
                            "Home > Now Playing returns without restarting. "
                            "Updates: Game > Music Updater."
                        );
                        g_info_return_screen = SCREEN_SETTINGS;
                        g_screen = SCREEN_INFO;
                    } else if (g_cursor == 4) {
                        char key[MAX_API_KEY];
                        key[0] = '\0';
                        if (osk_prompt("API Key", "Server X-Api-Key", g_cfg.api_key, key, sizeof(key))) {
                            clip_copy(g_cfg.api_key, sizeof(g_cfg.api_key), key);
                            http_set_api_key(g_cfg.api_key);
                            config_save(&g_cfg);
                            set_status(g_cfg.api_key[0] ? "API key saved" : "API key cleared");
                            set_settings_menu();
                            g_cursor = 4;
                            if (osk_gu_was_used()) {
                                ui_init();
                                ui_gpu_reset();
                            }
                        } else if (osk_gu_was_used()) {
                            ui_init();
                            ui_gpu_reset();
                        }
                    } else if (g_cursor == 5) {
                        g_screen = SCREEN_DIAG;
                    } else if (g_cursor == 6) {
                        char verline[64];
                        snprintf(verline, sizeof(verline), "PSP Music %s  (code %d)", APP_VERSION, APP_VERSION_CODE);
                        clip_copy(g_info_title, sizeof(g_info_title), "Version");
                        clip_copy(g_info_line1, sizeof(g_info_line1), verline);
                        clip_copy(
                            g_info_line2,
                            sizeof(g_info_line2),
                            "XMB Information shows DISC_VERSION as X.YZ (e.g. 1.37)."
                        );
                        clip_copy(
                            g_info_line3,
                            sizeof(g_info_line3),
                            "App updates: open Music Updater from the Game menu."
                        );
                        g_info_return_screen = SCREEN_SETTINGS;
                        g_screen = SCREEN_INFO;
                    }
                } else if (g_screen == SCREEN_MUSIC) {
                    if (g_cursor == 0) {
                        char query[MAX_NAME];
                        query[0] = '\0';
                        if (ensure_online()) {
                            if (osk_prompt("Search", "Track, artist or album", g_search_query, query, sizeof(query))) {
                                /* Restore GU BEFORE network — otherwise UI freezes on PIC1. */
                                if (osk_gu_was_used()) {
                                    ui_init();
                                    ui_gpu_reset();
                                }
                                /* Trim leading spaces; reject empty query. */
                                {
                                    char *p = query;
                                    while (*p == ' ' || *p == '\t') {
                                        p++;
                                    }
                                    if (!*p) {
                                        set_status("Enter a search");
                                        set_music_menu();
                                        g_screen = SCREEN_MUSIC;
                                    } else {
                                        clip_copy(g_search_query, sizeof(g_search_query), p);
                                        g_offset = 0;
                                        set_status("Searching...");
                                        dbg_log("Q", "main.c:search", "query_start", "{}");
                                        load_search();
                                        g_screen = SCREEN_SEARCH;
                                        dbg_log("Q", "main.c:search", "query_done", "{}");
                                    }
                                }
                            } else if (osk_gu_was_used()) {
                                ui_init();
                                ui_gpu_reset();
                            }
                        } else {
                            set_music_menu();
                        }
                    } else if (g_cursor == 1) {
                        g_offset = 0;
                        if (ensure_online()) {
                            load_all_songs();
                            g_screen = SCREEN_TRACKS;
                            g_return_screen = SCREEN_MUSIC;
                        } else {
                            set_music_menu();
                        }
                    } else if (g_cursor == 2) {
                        g_artist_id = 0;
                        g_offset = 0;
                        if (ensure_online()) {
                            load_all_albums();
                            g_screen = SCREEN_ALBUMS;
                            g_return_screen = SCREEN_MUSIC;
                        } else {
                            set_music_menu();
                        }
                    } else if (g_cursor == 3) {
                        g_offset = 0;
                        if (ensure_online()) {
                            load_genres();
                            g_screen = SCREEN_GENRES;
                            g_return_screen = SCREEN_MUSIC;
                        } else {
                            set_music_menu();
                        }
                    } else if (g_cursor == 4) {
                        if (ensure_online()) {
                            g_offset = 0;
                            load_top();
                            g_screen = SCREEN_TOP;
                        } else {
                            set_music_menu();
                        }
                    } else if (g_cursor == 5) {
                        if (ensure_online()) {
                            g_offset = 0;
                            load_rated_albums();
                            g_screen = SCREEN_ALBUMS;
                        } else {
                            set_music_menu();
                        }
                    }
                } else if (g_screen == SCREEN_GENRES) {
                    strncpy(g_genre_name, g_items[g_cursor].name, MAX_NAME - 1);
                    g_genre_name[MAX_NAME - 1] = '\0';
                    g_offset = 0;
                    load_genre_albums();
                    g_screen = SCREEN_ALBUMS;
                } else if (g_screen == SCREEN_ARTISTS) {
                    g_artist_id = g_items[g_cursor].id;
                    strncpy(g_artist_name, g_items[g_cursor].name, MAX_NAME - 1);
                    g_offset = 0;
                    load_albums();
                    g_screen = SCREEN_ALBUMS;
                } else if (g_screen == SCREEN_ALBUMS) {
                    g_album_id = g_items[g_cursor].id;
                    strncpy(g_album_name, g_items[g_cursor].name, MAX_NAME - 1);
                    g_offset = 0;
                    load_tracks();
                    g_screen = SCREEN_TRACKS;
                } else if (g_screen == SCREEN_TRACKS || g_screen == SCREEN_TOP || g_screen == SCREEN_SEARCH) {
                    begin_play_from_list();
                    if (g_items[g_cursor].artist[0]) {
                        strncpy(g_artist_name, g_items[g_cursor].artist, MAX_NAME - 1);
                        g_artist_name[MAX_NAME - 1] = '\0';
                    }
                    if (g_items[g_cursor].album[0]) {
                        strncpy(g_album_name, g_items[g_cursor].album, MAX_NAME - 1);
                        g_album_name[MAX_NAME - 1] = '\0';
                    }
                    g_play_index = g_cursor;
                    g_now_duration_ms = (g_items[g_cursor].duration > 0)
                        ? g_items[g_cursor].duration * 1000 : 0;
                    play_track_online(
                        g_items[g_cursor].id,
                        g_items[g_cursor].name,
                        g_items[g_cursor].extra
                    );
                } else if (g_screen == SCREEN_OFFLINE) {
                    begin_play_from_list();
                    g_play_index = g_cursor;
                    play_track_offline_idx(g_cursor);
                }
            }

            if (pressed & PSP_CTRL_SQUARE && g_count > 0) {
                if (g_screen == SCREEN_OFFLINE) {
                    g_offline_delete_confirm = 1;
                } else if (g_screen == SCREEN_ALBUMS) {
                    show_album_info(g_items[g_cursor].id);
                } else if (g_screen == SCREEN_ARTISTS) {
                    show_artist_info(g_items[g_cursor].id);
                } else if (
                    (g_screen == SCREEN_TRACKS || g_screen == SCREEN_TOP || g_screen == SCREEN_SEARCH) &&
                    g_count > 0
                ) {
                g_track_id = g_items[g_cursor].id;
                int r = g_items[g_cursor].extra + 1;
                if (r > 5) {
                    r = 0;
                }
                rate_current(r);
                g_items[g_cursor].extra = g_now_rating;
                }
            }

            if (pressed & PSP_CTRL_START &&
                !(player_is_active() || g_buffering || g_play_started) &&
                (g_screen == SCREEN_TRACKS || g_screen == SCREEN_TOP || g_screen == SCREEN_SEARCH) &&
                g_count > 0 &&
                g_online_mode) {
                int id = g_items[g_cursor].id;
                const char *artist = g_items[g_cursor].artist[0] ?
                    g_items[g_cursor].artist : g_artist_name;
                const char *album = g_items[g_cursor].album[0] ?
                    g_items[g_cursor].album : g_album_name;
                if (download_start(
                        g_cfg.host,
                        g_cfg.port,
                        id,
                        g_items[g_cursor].name,
                        artist,
                        album,
                        g_items[g_cursor].track_num
                    ) == 0) {
                    set_status("Downloading...");
                } else {
                    set_status("Download failed");
                }
            }

            if (pressed & PSP_CTRL_CIRCLE) {
                if (g_screen == SCREEN_TRACKS) {
                    g_offset = 0;
                    /* Songs (flat library) → Library, not Albums */
                    if (g_list_kind == LIST_ALL_SONGS) {
                        set_music_menu();
                        g_screen = SCREEN_MUSIC;
                    } else if (g_album_browse == BROWSE_GENRE) {
                        /* Genre may open tracks directly (no albums). */
                        load_genres();
                        g_screen = SCREEN_GENRES;
                    } else if (g_album_browse == BROWSE_RATED) {
                        load_rated_albums();
                        g_screen = SCREEN_ALBUMS;
                    } else if (g_artist_id) {
                        load_albums();
                        g_screen = SCREEN_ALBUMS;
                    } else if (g_album_browse == BROWSE_ALL) {
                        load_all_albums();
                        g_screen = SCREEN_ALBUMS;
                    } else {
                        set_music_menu();
                        g_screen = SCREEN_MUSIC;
                    }
                } else if (g_screen == SCREEN_ALBUMS) {
                    if (g_album_browse == BROWSE_GENRE) {
                        g_offset = 0;
                        load_genres();
                        g_screen = SCREEN_GENRES;
                    } else if (g_album_browse == BROWSE_RATED) {
                        set_music_menu();
                        g_screen = SCREEN_MUSIC;
                    } else if (g_artist_id) {
                        g_offset = 0;
                        load_artists();
                        g_screen = SCREEN_ARTISTS;
                    } else {
                        set_music_menu();
                        g_screen = SCREEN_MUSIC;
                    }
                } else if (g_screen == SCREEN_GENRES) {
                    set_music_menu();
                    g_screen = SCREEN_MUSIC;
                } else if (g_screen == SCREEN_ARTISTS || g_screen == SCREEN_MUSIC) {
                    set_home_menu();
                    g_screen = SCREEN_HOME;
                } else if (g_screen == SCREEN_TOP || g_screen == SCREEN_SEARCH) {
                    set_music_menu();
                    g_screen = SCREEN_MUSIC;
                } else if (g_screen == SCREEN_OFFLINE) {
                    set_home_menu();
                    g_screen = SCREEN_HOME;
                } else if (g_screen == SCREEN_SETTINGS) {
                    set_home_menu();
                    g_screen = SCREEN_HOME;
                }
            }

            if (pressed & PSP_CTRL_TRIANGLE) {
                /* Triangle: jump to Now Playing when a session exists; else Home. */
                if (g_track_id > 0 || player_is_active() || g_buffering || g_play_started) {
                    open_now_playing_view();
                } else {
                    set_home_menu();
                    g_screen = SCREEN_HOME;
                }
            }
        } /* screen if/else chain (playing…update…lists) */

#ifdef DEBUG_HUD
        dbg_flush();
        {
            char d1[96];
            snprintf(
                d1,
                sizeof(d1),
                "b%d/%d e%d %s",
                (int)g_dl_bytes,
                (int)g_dl_total,
                (int)g_dl_err,
                dbg_last_step()
            );
            ui_set_debug(d1, g_status[0] ? g_status : NULL);
            ui_draw_debug_overlay();
        }
#endif

        /* Online remote debug disabled (see dbg_remote_* no-ops). */

        ui_end();
    }

    return 0;
}
