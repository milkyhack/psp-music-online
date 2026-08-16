#include "download.h"
#include "http.h"
#include "storage.h"
#include "metrics.h"

#include <pspkernel.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static DownloadStatus g_dl;
static SceUID g_thid = -1;
static volatile int g_pause = 0;
static volatile int g_cancel = 0;
static char g_host[64];
static int g_port;
static char g_api[96];
static unsigned g_last_t;
static int g_last_bytes;

static void sanitize(char *dst, int dst_sz, const char *src) {
    int i = 0, j = 0;
    if (!dst || dst_sz <= 1) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    while (src[i] && j < dst_sz - 1) {
        unsigned char c = (unsigned char)src[i++];
        if (c < 32 || c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            continue;
        }
        dst[j++] = (char)c;
    }
    while (j > 0 && dst[j - 1] == ' ') {
        j--;
    }
    dst[j] = '\0';
    if (j == 0) {
        strncpy(dst, "Unknown", dst_sz - 1);
    }
}

void download_build_paths(
    char *tmp_out,
    int tmp_sz,
    char *final_out,
    int final_sz,
    const char *artist,
    const char *album,
    const char *title,
    int track_num
) {
    char a[64], al[64], t[64];
    char dir1[160], dir2[220];
    sanitize(a, sizeof(a), artist && artist[0] ? artist : "Unknown Artist");
    sanitize(al, sizeof(al), album && album[0] ? album : "Unknown Album");
    sanitize(t, sizeof(t), title && title[0] ? title : "Track");

    snprintf(dir1, sizeof(dir1), "ms0:/MUSIC");
    storage_mkdir(dir1);
    snprintf(dir1, sizeof(dir1), "ms0:/MUSIC/%s", a);
    storage_mkdir(dir1);
    snprintf(dir2, sizeof(dir2), "ms0:/MUSIC/%s/%s", a, al);
    storage_mkdir(dir2);

    if (track_num > 0) {
        snprintf(final_out, final_sz, "%s/%02d - %s.flac", dir2, track_num, t);
        snprintf(tmp_out, tmp_sz, "%s/%02d - %s.tmp", dir2, track_num, t);
    } else {
        snprintf(final_out, final_sz, "%s/%s.flac", dir2, t);
        snprintf(tmp_out, tmp_sz, "%s/%s.tmp", dir2, t);
    }
}

void download_init(void) {
    memset(&g_dl, 0, sizeof(g_dl));
    g_dl.state = DL_IDLE;
}

const DownloadStatus *download_status(void) {
    return &g_dl;
}

static void progress_cb(int bytes, int total, void *userdata) {
    unsigned now;
    (void)userdata;
    g_dl.bytes = bytes;
    g_dl.total = total;
    if (total > 0) {
        g_dl.percent = (bytes * 100) / total;
        if (g_dl.percent > 100) {
            g_dl.percent = 100;
        }
    }
    now = sceKernelGetSystemTimeLow();
    if (g_last_t != 0 && now != g_last_t) {
        float dt = (float)(now - g_last_t) / 1000000.0f;
        if (dt > 0.05f) {
            g_dl.speed_bps = (float)(bytes - g_last_bytes) / dt;
            g_last_bytes = bytes;
            g_last_t = now;
            metrics_add_network_bytes(
                0,
                (g_dl.speed_bps * 8.0f) / 1000000.0f
            );
        }
    } else {
        g_last_t = now;
        g_last_bytes = bytes;
    }
    while (g_pause && !g_cancel) {
        sceKernelDelayThread(50000);
    }
}

static int download_thread(SceSize args, void *argp) {
    int range = -1;
    int clen = -1;
    int total = -1;
    int rc;
    int existing;
    (void)args;
    (void)argp;

    g_dl.state = DL_RUNNING;
    g_cancel = 0;
    g_pause = 0;
    http_set_abort(0);

    existing = storage_size(g_dl.tmp_path);
    if (existing > 1024) {
        range = existing;
        g_dl.bytes = existing;
    } else if (existing >= 0 && existing <= 1024) {
        storage_temp_discard(g_dl.tmp_path);
        range = -1;
    }

    {
        int fd;
        if (range > 0) {
            /* append via range helper */
        } else {
            fd = storage_temp_create(g_dl.tmp_path);
            if (fd < 0) {
                strncpy(g_dl.error, "Not enough storage", sizeof(g_dl.error) - 1);
                g_dl.state = DL_ERROR;
                g_thid = -1;
                return 0;
            }
            storage_close(fd);
        }
    }

    /* Always request 320k MP3 — PSP sceMp3 is reliable; soft-FLAC is not. */
    snprintf(g_api, sizeof(g_api), "/api/stream/%d?format=mp3", g_dl.track_id);
    rc = http_get_file_range(
        g_host,
        g_port,
        g_api,
        g_dl.tmp_path,
        range,
        progress_cb,
        NULL,
        &clen,
        &total
    );

    if (g_cancel || rc == HTTP_ABORTED) {
        g_dl.state = DL_INCOMPLETE;
        g_thid = -1;
        return 0;
    }
    if (rc != HTTP_OK) {
        strncpy(g_dl.error, http_last_fail_reason(), sizeof(g_dl.error) - 1);
        if (strcmp(g_dl.error, "file") == 0) {
            strncpy(g_dl.error, "Not enough storage", sizeof(g_dl.error) - 1);
            storage_temp_discard(g_dl.tmp_path);
        }
        g_dl.state = DL_ERROR;
        g_thid = -1;
        return 0;
    }

    if (total > 0) {
        g_dl.total = total;
    }
    g_dl.bytes = storage_size(g_dl.tmp_path);
    g_dl.state = DL_VERIFYING;
    strncpy(g_dl.error, "", sizeof(g_dl.error));

    /* Prefer FLAC; accept MP3 if server transcoded (m4a/etc). Adjust final ext. */
    {
        int fd = storage_open_read(g_dl.tmp_path);
        char mag[4];
        int n = 0;
        int sz = storage_size(g_dl.tmp_path);
        int is_flac = 0;
        int is_mp3 = 0;
        if (fd >= 0) {
            n = storage_read(fd, mag, 4);
            storage_close(fd);
        }
        if (sz >= 2048 && n >= 4 && memcmp(mag, "fLaC", 4) == 0) {
            is_flac = 1;
        } else if (sz >= 2048 && n >= 3 &&
                   (memcmp(mag, "ID3", 3) == 0 || (unsigned char)mag[0] == 0xFF)) {
            is_mp3 = 1;
        }
        if (!is_flac && !is_mp3) {
            strncpy(g_dl.error, "Bad audio", sizeof(g_dl.error) - 1);
            g_dl.state = DL_ERROR;
            g_thid = -1;
            return 0;
        }
        if (is_mp3) {
            /* Paths were built as .flac — rewrite final to .mp3 for honesty. */
            char *dot = strrchr(g_dl.final_path, '.');
            if (dot && strcmp(dot, ".flac") == 0) {
                strcpy(dot, ".mp3");
            }
        }
    }

    if (storage_temp_finalize(g_dl.tmp_path, g_dl.final_path) < 0) {
        strncpy(g_dl.error, "Rename failed", sizeof(g_dl.error) - 1);
        g_dl.state = DL_ERROR;
        g_thid = -1;
        return 0;
    }

    g_dl.percent = 100;
    g_dl.state = DL_COMPLETE;
    g_thid = -1;
    return 0;
}

int download_start(
    const char *host,
    int port,
    int track_id,
    const char *title,
    const char *artist,
    const char *album,
    int track_num
) {
    if (g_thid >= 0) {
        return -1;
    }
    memset(&g_dl, 0, sizeof(g_dl));
    g_dl.track_id = track_id;
    strncpy(g_dl.title, title ? title : "Track", sizeof(g_dl.title) - 1);
    strncpy(g_dl.artist, artist ? artist : "", sizeof(g_dl.artist) - 1);
    strncpy(g_dl.album, album ? album : "", sizeof(g_dl.album) - 1);
    strncpy(g_host, host ? host : "", sizeof(g_host) - 1);
    g_port = port;
    download_build_paths(
        g_dl.tmp_path,
        sizeof(g_dl.tmp_path),
        g_dl.final_path,
        sizeof(g_dl.final_path),
        artist,
        album,
        title,
        track_num
    );
    if (storage_exists(g_dl.final_path)) {
        g_dl.state = DL_COMPLETE;
        g_dl.percent = 100;
        return 0;
    }
    g_thid = sceKernelCreateThread("fldl", download_thread, 0x18, 0x10000, 0, NULL);
    if (g_thid < 0) {
        g_dl.state = DL_ERROR;
        strncpy(g_dl.error, "thread", sizeof(g_dl.error) - 1);
        return -1;
    }
    sceKernelStartThread(g_thid, 0, NULL);
    return 0;
}

int download_resume(const char *host, int port) {
    if (g_dl.state != DL_INCOMPLETE && g_dl.state != DL_ERROR && g_dl.state != DL_PAUSED) {
        return -1;
    }
    if (g_dl.state == DL_PAUSED && g_thid >= 0) {
        g_pause = 0;
        g_dl.state = DL_RUNNING;
        return 0;
    }
    if (!storage_exists(g_dl.tmp_path) || g_dl.track_id <= 0) {
        return -1;
    }
    strncpy(g_host, host ? host : g_host, sizeof(g_host) - 1);
    if (port > 0) {
        g_port = port;
    }
    g_thid = sceKernelCreateThread("fldl", download_thread, 0x18, 0x10000, 0, NULL);
    if (g_thid < 0) {
        return -1;
    }
    sceKernelStartThread(g_thid, 0, NULL);
    return 0;
}

void download_pause(void) {
    g_pause = 1;
    g_dl.state = DL_PAUSED;
}

void download_cancel(void) {
    g_cancel = 1;
    g_pause = 0;
    http_set_abort(1);
}

int download_delete_incomplete(void) {
    download_cancel();
    if (g_thid >= 0) {
        SceUInt to = 2000000;
        sceKernelWaitThreadEnd(g_thid, &to);
        g_thid = -1;
    }
    storage_temp_discard(g_dl.tmp_path);
    /* Never leave a partial .flac */
    if (storage_exists(g_dl.final_path) && storage_size(g_dl.final_path) < 1024) {
        storage_remove(g_dl.final_path);
    }
    g_dl.state = DL_IDLE;
    return 0;
}
