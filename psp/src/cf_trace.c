#include "cf_trace.h"

#include "http.h"
#include "paths.h"
#include "ui.h"
#include "ui_font.h"
#include "ui_gfx.h"
#include "updater.h"

#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CF_TRACE_RING 40
#define CF_TRACE_MSG 56
#define CF_TRACE_FILE_MAX (64 * 1024)

typedef struct {
    unsigned ts;
    char msg[CF_TRACE_MSG];
    int val;
    int has_val;
} CfTraceEv;

static CfTraceEv g_ev[CF_TRACE_RING];
static int g_ev_w = 0;
static int g_http_flushed = 0;
static char g_last[CF_TRACE_MSG] = "cf_init";
static int g_last_val = 0;
static int g_last_has_val = 0;
static char g_mode_line[64] = "mode=?";
static char g_log_path[280];
static int g_log_ready = 0;
static int g_log_fail = 0;
static int g_enabled = 0;

static int cf_trace_enabled_file(void) {
    char path[280];
    SceUID fd;
    paths_join(path, sizeof(path), "data/cf_trace_on.txt");
    fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0) {
        return 0;
    }
    sceIoClose(fd);
    return 1;
}

static void copy_msg(char *dst, int dst_sz, const char *src) {
    int i = 0;
    if (!src) {
        src = "?";
    }
    while (i < dst_sz - 1 && src[i]) {
        char c = src[i];
        if ((unsigned char)c < 32 || c == '"') {
            c = ' ';
        }
        dst[i] = c;
        i++;
    }
    dst[i] = '\0';
}

static void cf_trace_ensure_log(void) {
    SceIoStat st;

    if (g_log_ready) {
        return;
    }
    paths_ensure_data();
    paths_join(g_log_path, sizeof(g_log_path), "data/cf_trace.log");
    if (sceIoGetstat(g_log_path, &st) >= 0 && st.st_size > CF_TRACE_FILE_MAX) {
        sceIoRemove(g_log_path);
    }
    g_log_ready = 1;
}

static void cf_trace_disk_write(unsigned ts, const char *msg, int val, int has_val) {
    char line[128];
    SceUID fd;
    int n;

    cf_trace_ensure_log();
    if (has_val) {
        n = snprintf(line, sizeof(line), "%u %s %d\n", ts, msg, val);
    } else {
        n = snprintf(line, sizeof(line), "%u %s\n", ts, msg);
    }
    if (n <= 0) {
        return;
    }

    fd = sceIoOpen(g_log_path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd < 0) {
        g_log_fail++;
        return;
    }
    sceIoWrite(fd, line, (SceSize)n);
    sceIoClose(fd);
}

static void cf_trace_push(unsigned ts, const char *msg, int val, int has_val) {
    CfTraceEv *e;
    int slot;

    slot = g_ev_w % CF_TRACE_RING;
    e = &g_ev[slot];
    e->ts = ts;
    copy_msg(e->msg, sizeof(e->msg), msg);
    e->val = val;
    e->has_val = has_val;
    g_ev_w++;

    /* Write immediately — crash during 3D may never reach ui_end(). */
    cf_trace_disk_write(ts, msg, val, has_val);
}

void cf_trace_init(void) {
    char boot[160];

    memset(g_ev, 0, sizeof(g_ev));
    g_ev_w = 0;
    g_http_flushed = 0;
    g_log_ready = 0;
    g_log_fail = 0;
    g_enabled = cf_trace_enabled_file();
    strncpy(g_last, "cf_init", sizeof(g_last) - 1);
    if (!g_enabled) {
        snprintf(g_mode_line, sizeof(g_mode_line), "render=?");
        return;
    }

    cf_trace_ensure_log();
    snprintf(
        boot,
        sizeof(boot),
        "=== boot %s code=%d base=%s ===\n",
        APP_VERSION,
        APP_VERSION_CODE,
        paths_base()
    );
    {
        SceUID fd = sceIoOpen(g_log_path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
        if (fd >= 0) {
            sceIoWrite(fd, boot, (SceSize)strlen(boot));
            sceIoClose(fd);
        } else {
            g_log_fail = 1;
        }
    }
    cf_trace_step("cf_trace_ready");
}

void cf_trace_val(const char *msg, int val) {
    if (!g_enabled) {
        return;
    }
    copy_msg(g_last, sizeof(g_last), msg);
    g_last_val = val;
    g_last_has_val = (msg && msg[0]) ? 1 : 0;
    cf_trace_push((unsigned)sceKernelGetSystemTimeLow(), g_last, val, 1);
}

void cf_trace_step(const char *msg) {
    if (!g_enabled) {
        return;
    }
    copy_msg(g_last, sizeof(g_last), msg);
    g_last_has_val = 0;
    cf_trace_push((unsigned)sceKernelGetSystemTimeLow(), g_last, 0, 0);
}

void cf_trace_mode_3d(int on) {
    if (!g_enabled) {
        return;
    }
    snprintf(g_mode_line, sizeof(g_mode_line), "render=%s", on ? "3D" : "CPU");
    cf_trace_val(on ? "mode_3d" : "mode_cpu", on);
}

const char *cf_trace_last(void) {
    return g_last;
}

void cf_trace_draw(const PlayerTheme *th) {
    if (!g_enabled) {
        return;
    }
    char line[96];
    u32 col = th ? th->seek : UI_COL_ORANGE;

    if (g_last_has_val) {
        snprintf(line, sizeof(line), "CF:%s=%d", g_last, g_last_val);
    } else {
        snprintf(line, sizeof(line), "CF:%s", g_last);
    }
    ui_gfx_fill_alpha(260, 0, 220, 42, UI_RGB(0, 0, 0), 180);
    ui_font_text_clip(264, 2, 212, col, line, UI_FONT_SM);
    if (g_log_fail > 0) {
        ui_font_text(264, 14, UI_RGB(255, 80, 80), "LOG ERR", UI_FONT_SM);
    } else {
        ui_font_text_clip(264, 14, 212, th ? th->muted : UI_COL_MUTED, g_mode_line, UI_FONT_SM);
    }
}

void cf_trace_flush_file(void) {
    if (!g_enabled) {
        return;
    }
    /* Disk writes are immediate; record a heartbeat for crash post-mortem. */
    cf_trace_val("heartbeat", g_log_fail);
}

int cf_trace_flush_http(const char *host, int port) {
    if (!g_enabled) {
        return 0;
    }
    char body[4096];
    char *resp = NULL;
    int resp_len = 0;
    int pending;
    int i;
    int start;
    int pos;
    int rc;

    if (!host || !host[0] || port <= 0) {
        return -1;
    }
    pending = g_ev_w - g_http_flushed;
    if (pending <= 0) {
        return 0;
    }
    if (pending > 12) {
        start = g_ev_w - 12;
    } else {
        start = g_ev_w - pending;
    }

    pos = snprintf(
        body,
        sizeof(body),
        "{\"device\":\"psp\",\"app_version\":\"%s\",\"app_code\":%d,"
        "\"session\":\"coverflow\",\"events\":[",
        APP_VERSION,
        APP_VERSION_CODE
    );
    for (i = start; i < g_ev_w && pos < (int)sizeof(body) - 96; i++) {
        CfTraceEv *e = &g_ev[i % CF_TRACE_RING];
        if (i > start) {
            body[pos++] = ',';
        }
        if (e->has_val) {
            pos += snprintf(
                body + pos,
                sizeof(body) - (size_t)pos,
                "{\"t\":%u,\"m\":\"%.48s\",\"v\":%d}",
                e->ts,
                e->msg,
                e->val
            );
        } else {
            pos += snprintf(
                body + pos,
                sizeof(body) - (size_t)pos,
                "{\"t\":%u,\"m\":\"%.48s\"}",
                e->ts,
                e->msg
            );
        }
    }
    if (pos >= (int)sizeof(body) - 4) {
        return -1;
    }
    snprintf(body + pos, sizeof(body) - (size_t)pos, "]}");

    rc = http_post_json(host, port, "/api/client/logs", body, &resp, &resp_len);
    if (resp) {
        free(resp);
    }
    if (rc == 200 || rc == 201) {
        g_http_flushed = g_ev_w;
        return 0;
    }
    return -1;
}
