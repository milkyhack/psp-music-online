#include "paths.h"

#include <pspiofilemgr.h>
#include <stdio.h>
#include <string.h>

/*
 * Absolute base dir of the running EBOOT (no trailing slash), e.g.
 * ms0:/PSP/GAME/music
 *
 * Relative sceIoOpen fails with 0x8002032C (NOCWD) from worker threads —
 * they have no current working directory. Always join against this base.
 */
static char g_base[256] = "ms0:/PSP/GAME/music";

void paths_init(void) {
    /* Fallback base; prefer paths_init_argv when available. */
}

void paths_init_argv(const char *argv0) {
    char *slash;
    if (!argv0 || !argv0[0]) {
        return;
    }
    strncpy(g_base, argv0, sizeof(g_base) - 1);
    g_base[sizeof(g_base) - 1] = '\0';
    slash = strrchr(g_base, '/');
    if (!slash) {
        slash = strrchr(g_base, '\\');
    }
    if (slash) {
        *slash = '\0';
    } else {
        /* argv without path — keep default ms0:/PSP/GAME/music */
        strncpy(g_base, "ms0:/PSP/GAME/music", sizeof(g_base) - 1);
    }
}

const char *paths_base(void) {
    return g_base;
}

char *paths_join(char *out, int out_sz, const char *rel) {
    if (!rel) {
        rel = "";
    }
    if (rel[0] == '/' || (rel[0] && strchr(rel, ':'))) {
        /* Already absolute / device path */
        snprintf(out, out_sz, "%s", rel);
    } else {
        snprintf(out, out_sz, "%s/%s", g_base, rel);
    }
    return out;
}

void paths_ensure_data(void) {
    char p[280];
    paths_join(p, sizeof(p), "data");
    sceIoMkdir(p, 0777);
    paths_join(p, sizeof(p), "data/offline");
    sceIoMkdir(p, 0777);
}

int paths_is_update_companion(void) {
    const char *base = paths_base();
    const char *slash;
    const char *name;
    char fold[64];
    size_t i;
    size_t n;

    if (!base || !base[0]) {
        return 0;
    }
    slash = strrchr(base, '/');
    name = slash ? (slash + 1) : base;
    n = strlen(name);
    if (n >= sizeof(fold)) {
        n = sizeof(fold) - 1;
    }
    for (i = 0; i < n; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 'a' + 'A');
        }
        fold[i] = c;
    }
    fold[n] = '\0';
    if (strcmp(fold, "PSPMUSICUPD") == 0 || strcmp(fold, "PSPMUSIC_UPD") == 0) {
        return 1;
    }
    /* Loose fallback: folder name contains MUSICUPD */
    return strstr(fold, "MUSICUPD") != NULL ? 1 : 0;
}

char *paths_music_join(char *out, int out_sz, const char *rel) {
    char parent[256];
    char *slash;
    const char *base = paths_base();

    if (!rel) {
        rel = "";
    }
    if (!base || !base[0]) {
        snprintf(out, out_sz, "ms0:/PSP/GAME/PSPMUSIC/%s", rel);
        return out;
    }
    strncpy(parent, base, sizeof(parent) - 1);
    parent[sizeof(parent) - 1] = '\0';
    slash = strrchr(parent, '/');
    if (slash) {
        *slash = '\0';
    }
    if (rel[0] == '\0') {
        snprintf(out, out_sz, "%s/PSPMUSIC", parent);
    } else {
        snprintf(out, out_sz, "%s/PSPMUSIC/%s", parent, rel);
    }
    return out;
}

void paths_request_purge_update_companion(void) {
    /* Never delete companion folders from code — ARK/MS hung on tree delete. */
}

void paths_purge_update_companion(void) {
    /* Permanent XMB icon: Game → PSP Music Update. */
}
