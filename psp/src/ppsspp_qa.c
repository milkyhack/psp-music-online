#include "ppsspp_qa.h"

#include "paths.h"
#include "ui.h"

#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QA_MAX_STEPS 48
#define QA_NAME_MAX 32
#define QA_LINE_MAX 96

typedef enum {
    QA_STEP_WAIT = 0,
    QA_STEP_CAPTURE,
    QA_STEP_CONNECT,
    QA_STEP_SCREEN,
    QA_STEP_PICK_ALBUM,
    QA_STEP_PICK_TRACK,
    QA_STEP_PLAY,
    QA_STEP_SKIN,
    QA_STEP_REPEAT,
    QA_STEP_SHUFFLE,
    QA_STEP_EQ_NEXT,
    QA_STEP_DONE
} QaStepKind;

typedef struct {
    QaStepKind kind;
    int arg;
    char name[QA_NAME_MAX];
    char screen[QA_NAME_MAX];
} QaStep;

static QaStep g_steps[QA_MAX_STEPS];
static int g_step_n = 0;
static int g_step_i = 0;
static int g_wait_left = 0;
static int g_active = 0;
static int g_exit = 0;
static int g_cmd_pending = 0;
static int g_post_nav_wait = 0;
static PpssppQaCmd g_cmd;

static void qa_log(const char *msg) {
    char path[280];
    SceUID fd;

    if (!msg) {
        return;
    }
    paths_join(path, sizeof(path), "data/qa");
    sceIoMkdir(path, 0777);
    paths_join(path, sizeof(path), "data/qa/status.log");
    fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0666);
    if (fd >= 0) {
        sceIoWrite(fd, msg, (SceSize)strlen(msg));
        sceIoWrite(fd, "\n", 1);
        sceIoClose(fd);
    }
}

static void qa_trim(char *s) {
    char *start = s;
    char *p;

    while (*start == ' ' || *start == '\t') {
        start++;
    }
    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }
    p = s + strlen(s);
    while (p > s && (p[-1] == ' ' || p[-1] == '\t' || p[-1] == '\r' || p[-1] == '\n')) {
        p--;
    }
    *p = '\0';
}

static int qa_push(QaStepKind kind, int arg, const char *a, const char *b) {
    QaStep *st;
    if (g_step_n >= QA_MAX_STEPS) {
        return -1;
    }
    st = &g_steps[g_step_n++];
    memset(st, 0, sizeof(*st));
    st->kind = kind;
    st->arg = arg;
    if (a) {
        strncpy(st->name, a, QA_NAME_MAX - 1);
    }
    if (b) {
        strncpy(st->screen, b, QA_NAME_MAX - 1);
    }
    return 0;
}

static int qa_parse_line(char *line) {
    char *cmd;
    char *arg1 = NULL;
    char *arg2 = NULL;

    if (!line || !line[0] || line[0] == '#') {
        return 0;
    }
    qa_trim(line);
    if (!line[0] || line[0] == '#') {
        return 0;
    }
    cmd = line;
    arg1 = strchr(line, ' ');
    if (arg1) {
        *arg1++ = '\0';
        qa_trim(arg1);
        arg2 = strchr(arg1, ' ');
        if (arg2) {
            *arg2++ = '\0';
            qa_trim(arg2);
        }
    }
    if (strcmp(cmd, "wait") == 0 && arg1) {
        return qa_push(QA_STEP_WAIT, atoi(arg1), NULL, NULL);
    }
    if (strcmp(cmd, "capture") == 0 && arg1) {
        return qa_push(QA_STEP_CAPTURE, 0, arg1, NULL);
    }
    if (strcmp(cmd, "connect") == 0) {
        return qa_push(QA_STEP_CONNECT, 0, NULL, NULL);
    }
    if (strcmp(cmd, "screen") == 0 && arg1) {
        return qa_push(QA_STEP_SCREEN, 0, NULL, arg1);
    }
    if (strcmp(cmd, "pick_album") == 0 && arg1) {
        return qa_push(QA_STEP_PICK_ALBUM, atoi(arg1), NULL, NULL);
    }
    if (strcmp(cmd, "pick_track") == 0 && arg1) {
        return qa_push(QA_STEP_PICK_TRACK, atoi(arg1), NULL, NULL);
    }
    if (strcmp(cmd, "play") == 0) {
        return qa_push(QA_STEP_PLAY, 0, NULL, NULL);
    }
    if (strcmp(cmd, "skin") == 0 && arg1) {
        return qa_push(QA_STEP_SKIN, atoi(arg1), NULL, NULL);
    }
    if (strcmp(cmd, "repeat") == 0) {
        return qa_push(QA_STEP_REPEAT, 0, NULL, NULL);
    }
    if (strcmp(cmd, "shuffle") == 0) {
        return qa_push(QA_STEP_SHUFFLE, 0, NULL, NULL);
    }
    if (strcmp(cmd, "eq_next") == 0) {
        return qa_push(QA_STEP_EQ_NEXT, 0, NULL, NULL);
    }
    if (strcmp(cmd, "done") == 0) {
        return qa_push(QA_STEP_DONE, 0, NULL, NULL);
    }
    return 0;
}

static int qa_load_default(void) {
    qa_push(QA_STEP_WAIT, 45, NULL, NULL);
    qa_push(QA_STEP_CAPTURE, 0, "home", NULL);
    qa_push(QA_STEP_CONNECT, 0, NULL, NULL);
    qa_push(QA_STEP_WAIT, 90, NULL, NULL);
    qa_push(QA_STEP_SCREEN, 0, NULL, "library");
    qa_push(QA_STEP_CAPTURE, 0, "library", NULL);
    qa_push(QA_STEP_SCREEN, 0, NULL, "albums");
    qa_push(QA_STEP_PICK_ALBUM, 0, NULL, NULL);
    qa_push(QA_STEP_PICK_TRACK, 0, NULL, NULL);
    qa_push(QA_STEP_PLAY, 0, NULL, NULL);
    qa_push(QA_STEP_WAIT, 180, NULL, NULL);
    qa_push(QA_STEP_CAPTURE, 0, "now-playing", NULL);
    qa_push(QA_STEP_SCREEN, 0, NULL, "setup");
    qa_push(QA_STEP_CAPTURE, 0, "setup", NULL);
    qa_push(QA_STEP_SKIN, 15, NULL, NULL);
    qa_push(QA_STEP_SCREEN, 0, NULL, "albums");
    qa_push(QA_STEP_WAIT, 120, NULL, NULL);
    qa_push(QA_STEP_CAPTURE, 0, "coverflow", NULL);
    qa_push(QA_STEP_DONE, 0, NULL, NULL);
    return 0;
}

static int qa_load_script_file(void) {
    char path[280];
    char buf[4096];
    SceUID fd;
    int n;
    char *p;
    char line[QA_LINE_MAX];

    paths_join(path, sizeof(path), "data/ppsspp_qa.txt");
    fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0) {
        return -1;
    }
    n = sceIoRead(fd, buf, sizeof(buf) - 1);
    sceIoClose(fd);
    if (n <= 0) {
        return -1;
    }
    buf[n] = '\0';
    g_step_n = 0;
    p = buf;
    while (p && *p) {
        char *nl = strchr(p, '\n');
        int len;
        if (nl) {
            len = (int)(nl - p);
        } else {
            len = (int)strlen(p);
        }
        if (len >= QA_LINE_MAX) {
            len = QA_LINE_MAX - 1;
        }
        memcpy(line, p, (size_t)len);
        line[len] = '\0';
        qa_parse_line(line);
        if (!nl) {
            break;
        }
        p = nl + 1;
    }
    if (g_step_n == 0) {
        return -1;
    }
    return 0;
}

int net_want_ppsspp_qa(void) {
    char path[280];
    SceUID fd;

    paths_join(path, sizeof(path), "data/ppsspp_qa.txt");
    fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd >= 0) {
        sceIoClose(fd);
        return 1;
    }
    return 0;
}

void ppsspp_qa_init(void) {
    char msg[64];

    g_step_i = 0;
    g_wait_left = 0;
    g_active = 0;
    g_exit = 0;
    g_cmd_pending = 0;
    memset(&g_cmd, 0, sizeof(g_cmd));

    if (!net_want_ppsspp_qa()) {
        return;
    }
    g_step_n = 0;
    if (qa_load_script_file() != 0) {
        qa_load_default();
    }
    g_active = 1;
    paths_join(msg, sizeof(msg), "data/qa");
    sceIoMkdir(msg, 0777);
    qa_log("qa_start");
}

int ppsspp_qa_active(void) {
    return g_active && !g_exit;
}

static int qa_capture_bmp(const char *name) {
    char path[280];
    char msg[96];

    if (!name || !name[0]) {
        return -1;
    }
    paths_join(path, sizeof(path), "data/qa");
    sceIoMkdir(path, 0777);
    {
        char rel[128];
        snprintf(rel, sizeof(rel), "data/qa/%s.bmp", name);
        paths_join(path, sizeof(path), rel);
    }
    if (ui_save_screenshot(path) != 0) {
        snprintf(msg, sizeof(msg), "capture_fail %s", name);
        qa_log(msg);
        return -1;
    }
    snprintf(msg, sizeof(msg), "capture_ok %s", name);
    qa_log(msg);
    return 0;
}

static void qa_issue_cmd(PpssppQaAction act, int index, int skin) {
    g_cmd.action = act;
    g_cmd.index = index;
    g_cmd.skin_id = skin;
    g_cmd_pending = 1;
}

static void qa_run_step(void) {
    QaStep *st;
    char msg[64];

    if (g_cmd_pending || g_wait_left > 0) {
        return;
    }
    if (g_step_i >= g_step_n) {
        g_exit = 1;
        qa_log("done");
        return;
    }
    st = &g_steps[g_step_i++];
    switch (st->kind) {
        case QA_STEP_WAIT:
            g_wait_left = st->arg > 0 ? st->arg : 1;
            snprintf(msg, sizeof(msg), "wait %d", st->arg);
            qa_log(msg);
            break;
        case QA_STEP_CAPTURE:
            qa_capture_bmp(st->name);
            break;
        case QA_STEP_CONNECT:
            qa_log("connect");
            qa_issue_cmd(PPSSPP_QA_ACT_CONNECT, 0, 0);
            break;
        case QA_STEP_SCREEN:
            qa_log(st->screen);
            if (strcmp(st->screen, "home") == 0) {
                qa_issue_cmd(PPSSPP_QA_ACT_HOME, 0, 0);
            } else if (strcmp(st->screen, "library") == 0) {
                qa_issue_cmd(PPSSPP_QA_ACT_LIBRARY, 0, 0);
            } else if (strcmp(st->screen, "albums") == 0) {
                qa_issue_cmd(PPSSPP_QA_ACT_ALBUMS, 0, 0);
            } else if (strcmp(st->screen, "setup") == 0) {
                qa_issue_cmd(PPSSPP_QA_ACT_SETUP, 0, 0);
            } else if (strcmp(st->screen, "now-playing") == 0) {
                qa_issue_cmd(PPSSPP_QA_ACT_NOW_PLAYING, 0, 0);
            }
            break;
        case QA_STEP_PICK_ALBUM:
            qa_issue_cmd(PPSSPP_QA_ACT_PICK_ALBUM, st->arg, 0);
            break;
        case QA_STEP_PICK_TRACK:
            qa_issue_cmd(PPSSPP_QA_ACT_PICK_TRACK, st->arg, 0);
            break;
        case QA_STEP_PLAY:
            qa_log("play");
            qa_issue_cmd(PPSSPP_QA_ACT_PLAY, 0, 0);
            break;
        case QA_STEP_SKIN:
            snprintf(msg, sizeof(msg), "skin %d", st->arg);
            qa_log(msg);
            qa_issue_cmd(PPSSPP_QA_ACT_SKIN, 0, st->arg);
            break;
        case QA_STEP_REPEAT:
            qa_log("repeat");
            qa_issue_cmd(PPSSPP_QA_ACT_TOGGLE_REPEAT, 0, 0);
            break;
        case QA_STEP_SHUFFLE:
            qa_log("shuffle");
            qa_issue_cmd(PPSSPP_QA_ACT_TOGGLE_SHUFFLE, 0, 0);
            break;
        case QA_STEP_EQ_NEXT:
            qa_log("eq_next");
            qa_issue_cmd(PPSSPP_QA_ACT_EQ_NEXT, 0, 0);
            break;
        case QA_STEP_DONE:
            g_exit = 1;
            qa_log("done");
            break;
        default:
            break;
    }
}

int ppsspp_qa_poll_cmd(PpssppQaCmd *out) {
    if (!g_cmd_pending || !out) {
        return 0;
    }
    *out = g_cmd;
    return 1;
}

void ppsspp_qa_cmd_done(void) {
    if (g_cmd.action != PPSSPP_QA_ACT_NONE && g_cmd.action != PPSSPP_QA_ACT_PLAY) {
        g_post_nav_wait = 45;
    }
    if (g_cmd.action == PPSSPP_QA_ACT_PLAY) {
        g_post_nav_wait = 90;
    }
    g_cmd_pending = 0;
    memset(&g_cmd, 0, sizeof(g_cmd));
    if (g_post_nav_wait > 0) {
        g_wait_left = g_post_nav_wait;
        g_post_nav_wait = 0;
    }
}

int ppsspp_qa_tick(void) {
    if (!g_active || g_exit) {
        return g_exit;
    }
    if (g_wait_left > 0) {
        g_wait_left--;
        return 0;
    }
    if (!g_cmd_pending) {
        qa_run_step();
    }
    return g_exit;
}
