#include "config.h"
#include "paths.h"
#include "debug_log.h"

#include <pspiofilemgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void config_path(char *out, int out_sz) {
    paths_join(out, out_sz, "data/server.cfg");
}

void config_path_alt(char *out, int out_sz) {
    /* Fallback next to EBOOT if data/ write fails on some MS layouts. */
    paths_join(out, out_sz, "server.cfg");
}

void cache_mp3_path(char *out, int out_sz) {
    paths_join(out, out_sz, "data/cache.mp3");
}

static void strip_cfg_line(char *s) {
    char *p;
    /* UTF-8 BOM */
    if ((unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) {
        memmove(s, s + 3, strlen(s + 3) + 1);
    }
    for (p = s; *p; p++) {
        if (*p == '\r' || *p == '\n') {
            *p = '\0';
            break;
        }
    }
}

static int parse_cfg_buf(const char *buf, ServerConfig *cfg) {
    char host[MAX_HOST];
    int port = DEFAULT_PORT;
    char key[MAX_API_KEY];
    char tmp[192];
    int n;

    strncpy(tmp, buf, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    strip_cfg_line(tmp);

    host[0] = '\0';
    key[0] = '\0';
    n = sscanf(tmp, "%63s %d %63s", host, &port, key);
    if (n < 1 || host[0] == '\0') {
        return 0;
    }
    /* Accept "host" or "host:port" glued */
    {
        char *colon = strchr(host, ':');
        if (colon) {
            *colon = '\0';
            if (colon[1]) {
                port = atoi(colon + 1);
            }
        }
    }
    strncpy(cfg->host, host, MAX_HOST - 1);
    cfg->host[MAX_HOST - 1] = '\0';
    if (port > 0 && port < 65536) {
        cfg->port = port;
    }
    if (n >= 3 && key[0] && strcmp(key, "-") != 0) {
        strncpy(cfg->api_key, key, MAX_API_KEY - 1);
        cfg->api_key[MAX_API_KEY - 1] = '\0';
    }
    return 1;
}

static int load_from_path(const char *path, ServerConfig *cfg) {
    SceUID fd;
    char buf[192];
    int n;

    fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
    if (fd < 0) {
        return 0;
    }
    n = sceIoRead(fd, buf, sizeof(buf) - 1);
    sceIoClose(fd);
    if (n <= 0) {
        return 0;
    }
    buf[n] = '\0';
    return parse_cfg_buf(buf, cfg);
}

int config_load(ServerConfig *cfg) {
    char path[280];
    char alt[280];

    memset(cfg, 0, sizeof(*cfg));
    strncpy(cfg->host, "192.168.0.2", MAX_HOST - 1);
    cfg->port = DEFAULT_PORT;
    cfg->api_key[0] = '\0';

    config_path(path, sizeof(path));
    if (load_from_path(path, cfg)) {
        char d[160];
        snprintf(d, sizeof(d), "{\"path\":\"%.80s\",\"host\":\"%.40s\",\"port\":%d}", path, cfg->host, cfg->port);
        dbg_log("E", "config.c:load", "loaded", d);
        return 1;
    }

    config_path_alt(alt, sizeof(alt));
    if (load_from_path(alt, cfg)) {
        char d[160];
        snprintf(d, sizeof(d), "{\"path\":\"%.80s\",\"host\":\"%.40s\",\"port\":%d}", alt, cfg->host, cfg->port);
        dbg_log("E", "config.c:load", "loaded_alt", d);
        /* Migrate into data/ for next save */
        config_save(cfg);
        return 1;
    }

    /* Companion Update icon: reuse main app's server.cfg from PSPMUSIC. */
    if (paths_is_update_companion()) {
        char music_cfg[280];
        paths_music_join(music_cfg, sizeof(music_cfg), "data/server.cfg");
        if (load_from_path(music_cfg, cfg)) {
            char d[160];
            snprintf(d, sizeof(d), "{\"path\":\"%.80s\",\"host\":\"%.40s\",\"port\":%d}",
                     music_cfg, cfg->host, cfg->port);
            dbg_log("E", "config.c:load", "loaded_music", d);
            return 1;
        }
        paths_music_join(music_cfg, sizeof(music_cfg), "server.cfg");
        if (load_from_path(music_cfg, cfg)) {
            dbg_log("E", "config.c:load", "loaded_music_alt", "{}");
            return 1;
        }
    }

    dbg_log("E", "config.c:load", "default", "{\"host\":\"192.168.0.2\"}");
    return 0;
}

static int write_cfg_file(const char *path, const ServerConfig *cfg) {
    SceUID fd;
    char buf[192];
    int n;
    int w;

    sceIoRemove(path);
    fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0) {
        /* Some CFW prefer RDWR for create */
        fd = sceIoOpen(path, PSP_O_RDWR | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    }
    if (fd < 0) {
        return -1;
    }
    n = snprintf(
        buf,
        sizeof(buf),
        "%s %d %s\n",
        cfg->host,
        cfg->port,
        cfg->api_key[0] ? cfg->api_key : "-"
    );
    if (n < 0) {
        n = (int)strlen(buf);
    }
    w = sceIoWrite(fd, buf, (unsigned)n);
    sceIoClose(fd);
    if (w != n) {
        return -2;
    }
    return 0;
}

static void ms_sync(void) {
    /* Best-effort flush so the next boot sees the new IP. */
    sceIoSync("ms0:", 0);
}

int config_save(const ServerConfig *cfg) {
    char path[280];
    char alt[280];
    ServerConfig verify;
    int rc;
    char d[192];

    if (!cfg || !cfg->host[0]) {
        return -1;
    }

    paths_ensure_data();
    config_path(path, sizeof(path));
    config_path_alt(alt, sizeof(alt));

    rc = write_cfg_file(path, cfg);
    snprintf(
        d,
        sizeof(d),
        "{\"path\":\"%.80s\",\"host\":\"%.40s\",\"port\":%d,\"rc\":%d}",
        path,
        cfg->host,
        cfg->port,
        rc
    );
    dbg_log("E", "config.c:save", "write_data", d);

    /* Always also write root server.cfg as backup */
    write_cfg_file(alt, cfg);

    if (rc != 0) {
        /* data/ failed — try root as primary */
        rc = write_cfg_file(alt, cfg);
        snprintf(
            d,
            sizeof(d),
            "{\"path\":\"%.80s\",\"host\":\"%.40s\",\"rc\":%d}",
            alt,
            cfg->host,
            rc
        );
        dbg_log("E", "config.c:save", "write_alt", d);
        if (rc != 0) {
            return -1;
        }
    }

    ms_sync();

    /* Round-trip verify — catch silent MS write failures */
    memset(&verify, 0, sizeof(verify));
    strncpy(verify.host, "bad", MAX_HOST - 1);
    verify.port = -1;
    if (!load_from_path(path, &verify) && !load_from_path(alt, &verify)) {
        dbg_log("E", "config.c:save", "verify_fail_open", "{}");
        return -1;
    }
    if (strcmp(verify.host, cfg->host) != 0 || verify.port != cfg->port) {
        snprintf(
            d,
            sizeof(d),
            "{\"want\":\"%.40s %d\",\"got\":\"%.40s %d\"}",
            cfg->host,
            cfg->port,
            verify.host,
            verify.port
        );
        dbg_log("E", "config.c:save", "verify_mismatch", d);
        return -1;
    }

    dbg_log("E", "config.c:save", "ok", "{}");
    return 0;
}
