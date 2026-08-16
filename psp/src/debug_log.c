#ifdef DEBUG_HUD

#include "debug_log.h"
#include "paths.h"
#include "updater.h"

#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define DBG_RING 48
#define DBG_STEP_LEN 80
#define DBG_HYP_LEN 8
#define DBG_LOC_LEN 48
#define DBG_MSG_LEN 40
#define DBG_DATA_LEN 120

typedef struct {
    char hyp[DBG_HYP_LEN];
    char loc[DBG_LOC_LEN];
    char msg[DBG_MSG_LEN];
    char data[DBG_DATA_LEN];
    unsigned ts;
} DbgEvent;

static SceUID g_dbg_mtx = -1;
static DbgEvent g_ev[DBG_RING];
static volatile int g_ev_w = 0;
static volatile int g_ev_r = 0;
static char g_ring_step[DBG_RING][DBG_STEP_LEN];
static volatile int g_ring_w = 0;
static volatile int g_ring_r = 0;
static char g_last_step[DBG_STEP_LEN] = "-";
static char g_session[24];

static void dbg_lock_init(void) {
    if (g_dbg_mtx < 0) {
        g_dbg_mtx = sceKernelCreateSema("dbglog", 0, 1, 1, NULL);
    }
}

static void dbg_lock(void) {
    dbg_lock_init();
    if (g_dbg_mtx >= 0) {
        sceKernelWaitSema(g_dbg_mtx, 1, NULL);
    }
}

static void dbg_unlock(void) {
    if (g_dbg_mtx >= 0) {
        sceKernelSignalSema(g_dbg_mtx, 1);
    }
}

static void ensure_session(void) {
    if (g_session[0]) {
        return;
    }
    snprintf(g_session, sizeof(g_session), "psp-%u", (unsigned)sceKernelGetSystemTimeLow());
}

static void copy_trunc(char *dst, int dst_sz, const char *src) {
    int i = 0;
    if (!src) {
        src = "";
    }
    while (i < dst_sz - 1 && src[i]) {
        char c = src[i];
        if (c == '"' || c == '\\' || (unsigned char)c < 32) {
            c = ' ';
        }
        dst[i] = c;
        i++;
    }
    dst[i] = '\0';
}

void dbg_log(const char *hypothesisId, const char *location, const char *message, const char *data_json) {
    DbgEvent *e;
    int slot;
    int pending;

    dbg_lock();
    ensure_session();
    pending = g_ev_w - g_ev_r;
    if (pending >= DBG_RING) {
        g_ev_r = g_ev_w - DBG_RING + 1;
    }
    slot = g_ev_w % DBG_RING;
    e = &g_ev[slot];
    copy_trunc(e->hyp, sizeof(e->hyp), hypothesisId);
    copy_trunc(e->loc, sizeof(e->loc), location);
    copy_trunc(e->msg, sizeof(e->msg), message);
    copy_trunc(e->data, sizeof(e->data), data_json && data_json[0] ? data_json : "{}");
    e->ts = (unsigned)sceKernelGetSystemTimeLow();
    g_ev_w++;
    dbg_unlock();

    {
        char line[512];
        char path[300];
        int n;
        SceUID fd;
        n = snprintf(
            line,
            sizeof(line),
            "{\"sessionId\":\"%s\",\"hypothesisId\":\"%s\",\"location\":\"%s\",\"message\":\"%s\",\"data\":%s,\"timestamp\":%u}\n",
            g_session,
            e->hyp,
            e->loc,
            e->msg,
            e->data[0] ? e->data : "{}",
            e->ts
        );
        if (n > 0) {
            paths_join(path, sizeof(path), "debug.log");
            fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
            if (fd >= 0) {
                sceIoWrite(fd, line, (unsigned)strlen(line));
                sceIoClose(fd);
            }
        }
    }
}

void dbg_step(const char *step) {
    int slot;
    if (!step) {
        step = "?";
    }
    strncpy(g_last_step, step, DBG_STEP_LEN - 1);
    g_last_step[DBG_STEP_LEN - 1] = '\0';

    slot = g_ring_w % DBG_RING;
    strncpy(g_ring_step[slot], step, DBG_STEP_LEN - 1);
    g_ring_step[slot][DBG_STEP_LEN - 1] = '\0';
    g_ring_w++;
}

void dbg_flush(void) {
    while (g_ring_r != g_ring_w) {
        int slot = g_ring_r % DBG_RING;
        char d[96];
        snprintf(d, sizeof(d), "{\"step\":\"%.60s\"}", g_ring_step[slot]);
        dbg_log("B", "dbg_step", "step", d);
        g_ring_r++;
    }
}

const char *dbg_last_step(void) {
    return g_last_step[0] ? g_last_step : "-";
}

int dbg_pending_count(void) {
    int n = g_ev_w - g_ev_r;
    return n > 0 ? n : 0;
}

int dbg_remote_flush(const char *host, int port) {
    (void)host;
    (void)port;
    dbg_lock();
    g_ev_r = g_ev_w;
    dbg_unlock();
    return 0;
}

void dbg_remote_tick(const char *host, int port, int online) {
    (void)host;
    (void)port;
    (void)online;
}

#endif /* DEBUG_HUD */
