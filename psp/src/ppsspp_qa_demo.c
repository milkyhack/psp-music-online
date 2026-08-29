#include "ppsspp_qa_demo.h"
#include "ppsspp_qa.h"
#include "paths.h"

#include <pspiofilemgr.h>
#include <string.h>

#define QA_LIB_N 5

static char g_lib_title[QA_LIB_N][80];
static char g_lib_artist[QA_LIB_N][48];
static char g_lib_dur[QA_LIB_N][12];
static const char *g_lib_labels[QA_LIB_N];
static const char *g_lib_rights[QA_LIB_N];
static const char *g_lib_subs[QA_LIB_N];

static int ppsspp_qa_demo_file_on(void) {
    char path[280];
    SceUID fd;

    paths_join(path, sizeof(path), "data/ppsspp_qa_demo.txt");
    fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0) {
        return 0;
    }
    sceIoClose(fd);
    return 1;
}

int ppsspp_qa_demo_active(void) {
    return net_want_ppsspp_qa() && ppsspp_qa_demo_file_on();
}

void ppsspp_qa_demo_patch_home(void) {
    /* README home.png — fixed marketing copy for visual parity. */
    /* Patched from main.c set_home_menu via item name lookup. */
    (void)0;
}

int ppsspp_qa_demo_library(
    const char ***labels,
    const char ***rights,
    const char ***sublines,
    int *count,
    int *cursor,
    UiMiniPlayer *mini
) {
    static const char *titles[QA_LIB_N] = {
        "Strobe",
        "Get Lucky (feat. Pharrell Williams)",
        "Everything In Its Right Place",
        "The Less I Know The Better",
        "Die For You",
    };
    static const char *artists[QA_LIB_N] = {
        "deadmau5",
        "Daft Punk",
        "Radiohead",
        "Tame Impala",
        "The Weeknd",
    };
    static const char *durs[QA_LIB_N] = {
        "10:37", "6:09", "4:11", "3:36", "4:20",
    };
    int i;

    if (!ppsspp_qa_demo_active()) {
        return 0;
    }

    for (i = 0; i < QA_LIB_N; i++) {
        strncpy(g_lib_title[i], titles[i], sizeof(g_lib_title[i]) - 1);
        strncpy(g_lib_artist[i], artists[i], sizeof(g_lib_artist[i]) - 1);
        strncpy(g_lib_dur[i], durs[i], sizeof(g_lib_dur[i]) - 1);
        g_lib_labels[i] = g_lib_title[i];
        g_lib_rights[i] = g_lib_dur[i];
        g_lib_subs[i] = g_lib_artist[i];
    }

    if (labels) {
        *labels = g_lib_labels;
    }
    if (rights) {
        *rights = g_lib_rights;
    }
    if (sublines) {
        *sublines = g_lib_subs;
    }
    if (count) {
        *count = QA_LIB_N;
    }
    if (cursor) {
        *cursor = 2;
    }
    if (mini) {
        memset(mini, 0, sizeof(*mini));
        mini->now_title = g_lib_title[2];
        mini->now_artist = g_lib_artist[2];
        mini->elapsed_ms = 83000;
        mini->duration_ms = 251000;
        mini->paused = 0;
        mini->track_id = 1;
    }
    return 1;
}

void ppsspp_qa_demo_np(UiNowPlaying *np) {
    if (!ppsspp_qa_demo_active() || !np) {
        return;
    }
    np->title = "A Walk";
    np->artist = "Tycho";
    np->album = "Awake";
    np->elapsed_ms = 151000;
    np->duration_ms = 317000;
    np->playing = 1;
    np->paused = 0;
    np->shuffle = 0;
    np->repeat = 0;
    np->status = NULL;
    np->cover = NULL;
}

void ppsspp_qa_demo_setup(int octets[4], int *selected, int *port, int *focus_port) {
    if (!octets) {
        return;
    }
    octets[0] = 192;
    octets[1] = 168;
    octets[2] = 31;
    octets[3] = 95;
    if (selected) {
        *selected = 3;
    }
    if (port) {
        *port = 8084;
    }
    if (focus_port) {
        *focus_port = 0;
    }
}

const char *ppsspp_qa_demo_cf_screen_title(void) {
    return "Album Theater";
}

const char *ppsspp_qa_demo_cf_track(void) {
    return "A Walk";
}

const char *ppsspp_qa_demo_cf_artist(void) {
    return "Tycho";
}

const char *ppsspp_qa_demo_cf_album(void) {
    return "Awake";
}
