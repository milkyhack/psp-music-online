#include "offline.h"
#include "paths.h"

#include <pspiofilemgr.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void offline_ensure_dirs(void) {
    paths_ensure_data();
    sceIoMkdir("ms0:/MUSIC", 0777);
}

static int copy_file(const char *src, const char *dst) {
    SceUID in = sceIoOpen(src, PSP_O_RDONLY, 0777);
    if (in < 0) {
        return -1;
    }
    SceUID out = sceIoOpen(dst, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (out < 0) {
        sceIoClose(in);
        return -2;
    }
    char buf[4096];
    for (;;) {
        int n = sceIoRead(in, buf, sizeof(buf));
        if (n <= 0) {
            break;
        }
        sceIoWrite(out, buf, n);
    }
    sceIoClose(in);
    sceIoClose(out);
    return 0;
}

int offline_path_for(int track_id, char *out, int out_sz) {
    char rel[64];
    snprintf(rel, sizeof(rel), "data/offline/%d.mp3", track_id);
    paths_join(out, out_sz, rel);
    return 0;
}

static void offline_idx_path(char *out, int out_sz) {
    paths_join(out, out_sz, "data/offline/index.txt");
}

static void music_idx_path(char *out, int out_sz) {
    paths_join(out, out_sz, "data/offline/music_index.txt");
}

/* Prefer legacy MP3, then MUSIC index FLAC path for this track id. */
int offline_locate_path(int track_id, char *out, int out_sz) {
    char path[280];
    char idx[280];
    char buf[8192];
    SceUID fd;
    int n;
    char *line;

    if (!out || out_sz <= 0) {
        return -1;
    }
    offline_path_for(track_id, path, sizeof(path));
    fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
    if (fd >= 0) {
        sceIoClose(fd);
        strncpy(out, path, out_sz - 1);
        out[out_sz - 1] = '\0';
        return 0;
    }

    music_idx_path(idx, sizeof(idx));
    fd = sceIoOpen(idx, PSP_O_RDONLY, 0777);
    if (fd < 0) {
        return -1;
    }
    n = sceIoRead(fd, buf, sizeof(buf) - 1);
    sceIoClose(fd);
    if (n <= 0) {
        return -1;
    }
    buf[n] = '\0';
    line = buf;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        int id = 0;
        char fpath[280];
        if (nl) {
            *nl = '\0';
        }
        fpath[0] = '\0';
        if (sscanf(line, "%d|%*[^|]|%*[^|]|%*[^|]|%*d|%279[^\n]", &id, fpath) >= 1 &&
            id == track_id && fpath[0]) {
            SceUID f2 = sceIoOpen(fpath, PSP_O_RDONLY, 0777);
            if (f2 >= 0) {
                sceIoClose(f2);
                strncpy(out, fpath, out_sz - 1);
                out[out_sz - 1] = '\0';
                return 0;
            }
        }
        if (!nl) {
            break;
        }
        line = nl + 1;
    }
    return -1;
}

int offline_resolve_path(const OfflineTrack *t, char *out, int out_sz) {
    if (!t || !out || out_sz <= 0) {
        return -1;
    }
    if (t->path[0]) {
        strncpy(out, t->path, out_sz - 1);
        out[out_sz - 1] = '\0';
        return 0;
    }
    return offline_path_for(t->id, out, out_sz);
}

int offline_has(int track_id) {
    char path[280];
    char idx[280];
    char buf[8192];
    SceUID fd;
    int n;
    char *line;

    offline_path_for(track_id, path, sizeof(path));
    fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
    if (fd >= 0) {
        sceIoClose(fd);
        return 1;
    }

    music_idx_path(idx, sizeof(idx));
    fd = sceIoOpen(idx, PSP_O_RDONLY, 0777);
    if (fd < 0) {
        return 0;
    }
    n = sceIoRead(fd, buf, sizeof(buf) - 1);
    sceIoClose(fd);
    if (n <= 0) {
        return 0;
    }
    buf[n] = '\0';
    line = buf;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        int id = 0;
        char fpath[280];
        if (nl) {
            *nl = '\0';
        }
        fpath[0] = '\0';
        if (sscanf(line, "%d|%*[^|]|%*[^|]|%*[^|]|%*d|%279[^\n]", &id, fpath) >= 1 &&
            id == track_id && fpath[0]) {
            SceUID f2 = sceIoOpen(fpath, PSP_O_RDONLY, 0777);
            if (f2 >= 0) {
                sceIoClose(f2);
                return 1;
            }
        }
        if (!nl) {
            break;
        }
        line = nl + 1;
    }
    return 0;
}

static int ends_with_ci(const char *name, const char *ext) {
    int n;
    int e;
    int i;
    if (!name || !ext) {
        return 0;
    }
    n = (int)strlen(name);
    e = (int)strlen(ext);
    if (n < e) {
        return 0;
    }
    for (i = 0; i < e; i++) {
        char a = name[n - e + i];
        char b = ext[i];
        if (a >= 'A' && a <= 'Z') {
            a = (char)(a - 'A' + 'a');
        }
        if (a != b) {
            return 0;
        }
    }
    return 1;
}

static void basename_no_ext(const char *path, char *out, int out_sz) {
    const char *slash = strrchr(path, '/');
    const char *base = slash ? slash + 1 : path;
    const char *dot;
    int i = 0;
    while (base[i] && i < out_sz - 1) {
        out[i] = base[i];
        i++;
    }
    out[i] = '\0';
    dot = strrchr(out, '.');
    if (dot) {
        *(char *)dot = '\0';
    }
}

static void parse_music_path(
    const char *full,
    char *artist,
    int artist_sz,
    char *album,
    int album_sz,
    char *title,
    int title_sz
) {
    /* ms0:/MUSIC/Artist/Album/NN - Title.flac */
    char work[320];
    char *p;
    char *parts[6];
    int n = 0;
    strncpy(work, full, sizeof(work) - 1);
    work[sizeof(work) - 1] = '\0';
    artist[0] = album[0] = title[0] = '\0';
    p = work;
    while (*p && n < 6) {
        while (*p == '/') {
            p++;
        }
        if (!*p) {
            break;
        }
        parts[n++] = p;
        while (*p && *p != '/') {
            p++;
        }
        if (*p) {
            *p++ = '\0';
        }
    }
    /* expect MUSIC, Artist, Album, file */
    if (n >= 4 && strcmp(parts[1], "MUSIC") == 0) {
        strncpy(artist, parts[2], artist_sz - 1);
        artist[artist_sz - 1] = '\0';
        if (n >= 5) {
            strncpy(album, parts[3], album_sz - 1);
            album[album_sz - 1] = '\0';
            basename_no_ext(parts[4], title, title_sz);
        } else {
            basename_no_ext(parts[3], title, title_sz);
        }
    } else {
        basename_no_ext(full, title, title_sz);
    }
    /* Strip leading "NN - " */
    if (title[0] >= '0' && title[0] <= '9') {
        char *dash = strstr(title, " - ");
        if (dash) {
            memmove(title, dash + 3, strlen(dash + 3) + 1);
        }
    }
}

static int already_has_path(const OfflineTrack *items, int n, const char *path) {
    int i;
    for (i = 0; i < n; i++) {
        if (items[i].path[0] && strcmp(items[i].path, path) == 0) {
            return 1;
        }
    }
    return 0;
}

static int scan_music_dir(const char *dir, OfflineTrack *out, int max_items, int *count) {
    SceUID dfd;
    SceIoDirent de;
    if (*count >= max_items) {
        return 0;
    }
    dfd = sceIoDopen(dir);
    if (dfd < 0) {
        return -1;
    }
    memset(&de, 0, sizeof(de));
    while (*count < max_items && sceIoDread(dfd, &de) > 0) {
        char child[320];
        if (de.d_name[0] == '.') {
            memset(&de, 0, sizeof(de));
            continue;
        }
        snprintf(child, sizeof(child), "%s/%s", dir, de.d_name);
        if (FIO_S_ISDIR(de.d_stat.st_mode)) {
            scan_music_dir(child, out, max_items, count);
        } else if (ends_with_ci(de.d_name, ".flac")) {
            OfflineTrack *t;
            if (already_has_path(out, *count, child)) {
                memset(&de, 0, sizeof(de));
                continue;
            }
            t = &out[*count];
            memset(t, 0, sizeof(*t));
            t->id = 100000 + *count; /* synthetic local id */
            t->is_flac = 1;
            strncpy(t->path, child, sizeof(t->path) - 1);
            parse_music_path(
                child,
                t->artist,
                sizeof(t->artist),
                t->album,
                sizeof(t->album),
                t->title,
                sizeof(t->title)
            );
            if (!t->title[0]) {
                strncpy(t->title, de.d_name, sizeof(t->title) - 1);
            }
            (*count)++;
        }
        memset(&de, 0, sizeof(de));
    }
    sceIoDclose(dfd);
    return 0;
}

static int load_music_index(OfflineTrack *out, int max_items, int start) {
    char idx[280];
    char buf[8192];
    int n;
    int count = start;
    SceUID fd;
    char *line;

    music_idx_path(idx, sizeof(idx));
    fd = sceIoOpen(idx, PSP_O_RDONLY, 0777);
    if (fd < 0) {
        return start;
    }
    n = sceIoRead(fd, buf, sizeof(buf) - 1);
    sceIoClose(fd);
    if (n <= 0) {
        return start;
    }
    buf[n] = '\0';
    line = buf;
    while (count < max_items && line && *line) {
        char *nl = strchr(line, '\n');
        OfflineTrack *t;
        if (nl) {
            *nl = '\0';
        }
        t = &out[count];
        memset(t, 0, sizeof(*t));
        if (sscanf(
                line,
                "%d|%95[^|]|%95[^|]|%95[^|]|%d|%279[^\n]",
                &t->id,
                t->artist,
                t->album,
                t->title,
                &t->rating,
                t->path
            ) >= 6) {
            SceUID f2 = sceIoOpen(t->path, PSP_O_RDONLY, 0777);
            if (f2 >= 0) {
                sceIoClose(f2);
                t->is_flac = 1;
                count++;
            }
        }
        if (!nl) {
            break;
        }
        line = nl + 1;
    }
    return count;
}

static int write_music_index(const OfflineTrack *items, int n) {
    char idx[280];
    char line[400];
    int i;
    SceUID fd;
    music_idx_path(idx, sizeof(idx));
    paths_ensure_data();
    fd = sceIoOpen(idx, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        int len;
        if (!items[i].is_flac || !items[i].path[0]) {
            continue;
        }
        len = snprintf(
            line,
            sizeof(line),
            "%d|%s|%s|%s|%d|%s\n",
            items[i].id,
            items[i].artist,
            items[i].album,
            items[i].title,
            items[i].rating,
            items[i].path
        );
        sceIoWrite(fd, line, len);
    }
    sceIoClose(fd);
    return 0;
}

int offline_load(OfflineTrack *out, int max_items) {
    char idx[280];
    SceUID fd;
    char buf[8192];
    int n;
    int count = 0;
    char *line;

    if (!out || max_items <= 0) {
        return 0;
    }

    offline_idx_path(idx, sizeof(idx));
    fd = sceIoOpen(idx, PSP_O_RDONLY, 0777);
    if (fd >= 0) {
        n = sceIoRead(fd, buf, sizeof(buf) - 1);
        sceIoClose(fd);
        if (n > 0) {
            buf[n] = '\0';
            line = buf;
            while (count < max_items && line && *line) {
                char *nl = strchr(line, '\n');
                OfflineTrack *t;
                if (nl) {
                    *nl = '\0';
                }
                t = &out[count];
                memset(t, 0, sizeof(*t));
                if (sscanf(
                        line,
                        "%d|%95[^|]|%95[^|]|%95[^|]|%d|%63s",
                        &t->id,
                        t->artist,
                        t->album,
                        t->title,
                        &t->rating,
                        t->file
                    ) >= 5) {
                    char path[280];
                    offline_path_for(t->id, path, sizeof(path));
                    {
                        SceUID f2 = sceIoOpen(path, PSP_O_RDONLY, 0777);
                        if (f2 >= 0) {
                            sceIoClose(f2);
                            count++;
                        }
                    }
                }
                if (!nl) {
                    break;
                }
                line = nl + 1;
            }
        }
    }

    count = load_music_index(out, max_items, count);
    scan_music_dir("ms0:/MUSIC", out, max_items, &count);
    return count;
}

int offline_count(void) {
    OfflineTrack tmp[OFFLINE_MAX];
    return offline_load(tmp, OFFLINE_MAX);
}

int offline_save(
    int track_id,
    const char *artist,
    const char *album,
    const char *title,
    int rating,
    const char *src_mp3_path
) {
    char dest[280];
    char idx[280];
    char line[320];
    offline_ensure_dirs();
    offline_path_for(track_id, dest, sizeof(dest));

    if (copy_file(src_mp3_path, dest) < 0) {
        return -1;
    }

    OfflineTrack items[OFFLINE_MAX];
    int n = offline_load(items, OFFLINE_MAX);
    int i;
    int found = 0;
    for (i = 0; i < n; i++) {
        if (items[i].id == track_id && !items[i].is_flac) {
            strncpy(items[i].artist, artist ? artist : "", MAX_NAME - 1);
            strncpy(items[i].album, album ? album : "", MAX_NAME - 1);
            strncpy(items[i].title, title ? title : "", MAX_NAME - 1);
            items[i].rating = rating;
            snprintf(items[i].file, sizeof(items[i].file), "%d.mp3", track_id);
            found = 1;
            break;
        }
    }
    if (!found && n < OFFLINE_MAX) {
        items[n].id = track_id;
        strncpy(items[n].artist, artist ? artist : "", MAX_NAME - 1);
        strncpy(items[n].album, album ? album : "", MAX_NAME - 1);
        strncpy(items[n].title, title ? title : "", MAX_NAME - 1);
        items[n].rating = rating;
        snprintf(items[n].file, sizeof(items[n].file), "%d.mp3", track_id);
        n++;
    }

    offline_idx_path(idx, sizeof(idx));
    {
        SceUID wfd = sceIoOpen(idx, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
        if (wfd < 0) {
            return -2;
        }
        for (i = 0; i < n; i++) {
            int len;
            if (items[i].is_flac) {
                continue;
            }
            len = snprintf(
                line,
                sizeof(line),
                "%d|%s|%s|%s|%d|%s\n",
                items[i].id,
                items[i].artist,
                items[i].album,
                items[i].title,
                items[i].rating,
                items[i].file
            );
            sceIoWrite(wfd, line, len);
        }
        sceIoClose(wfd);
    }
    return 0;
}

int offline_register_flac(
    int track_id,
    const char *artist,
    const char *album,
    const char *title,
    int rating,
    const char *flac_path
) {
    OfflineTrack items[OFFLINE_MAX];
    int n;
    int i;
    int found = 0;
    if (!flac_path || !flac_path[0]) {
        return -1;
    }
    offline_ensure_dirs();
    n = load_music_index(items, OFFLINE_MAX, 0);
    for (i = 0; i < n; i++) {
        if (items[i].id == track_id ||
            (items[i].path[0] && strcmp(items[i].path, flac_path) == 0)) {
            items[i].id = track_id;
            items[i].is_flac = 1;
            strncpy(items[i].artist, artist ? artist : "", MAX_NAME - 1);
            strncpy(items[i].album, album ? album : "", MAX_NAME - 1);
            strncpy(items[i].title, title ? title : "", MAX_NAME - 1);
            items[i].rating = rating;
            strncpy(items[i].path, flac_path, sizeof(items[i].path) - 1);
            found = 1;
            break;
        }
    }
    if (!found && n < OFFLINE_MAX) {
        memset(&items[n], 0, sizeof(items[n]));
        items[n].id = track_id;
        items[n].is_flac = 1;
        strncpy(items[n].artist, artist ? artist : "", MAX_NAME - 1);
        strncpy(items[n].album, album ? album : "", MAX_NAME - 1);
        strncpy(items[n].title, title ? title : "", MAX_NAME - 1);
        items[n].rating = rating;
        strncpy(items[n].path, flac_path, sizeof(items[n].path) - 1);
        n++;
    }
    return write_music_index(items, n);
}

int offline_delete(int track_id) {
    char path[280];
    OfflineTrack items[OFFLINE_MAX];
    char idx[280];
    char line[320];
    int n;
    int i;

    offline_path_for(track_id, path, sizeof(path));
    sceIoRemove(path);

    n = offline_load(items, OFFLINE_MAX);
    for (i = 0; i < n; i++) {
        if (items[i].id == track_id && items[i].path[0]) {
            sceIoRemove(items[i].path);
        }
    }

    offline_idx_path(idx, sizeof(idx));
    {
        SceUID fd = sceIoOpen(idx, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
        if (fd < 0) {
            return -1;
        }
        for (i = 0; i < n; i++) {
            int len;
            if (items[i].id == track_id || items[i].is_flac) {
                continue;
            }
            len = snprintf(
                line,
                sizeof(line),
                "%d|%s|%s|%s|%d|%s\n",
                items[i].id,
                items[i].artist,
                items[i].album,
                items[i].title,
                items[i].rating,
                items[i].file
            );
            sceIoWrite(fd, line, len);
        }
        sceIoClose(fd);
    }
    {
        OfflineTrack keep[OFFLINE_MAX];
        int k = 0;
        for (i = 0; i < n; i++) {
            if (items[i].is_flac && items[i].id != track_id) {
                keep[k++] = items[i];
            }
        }
        write_music_index(keep, k);
    }
    return 0;
}

int offline_total_bytes(void) {
    OfflineTrack items[OFFLINE_MAX];
    int n = offline_load(items, OFFLINE_MAX);
    int i;
    int total = 0;
    for (i = 0; i < n; i++) {
        char path[280];
        SceIoStat st;
        offline_resolve_path(&items[i], path, sizeof(path));
        memset(&st, 0, sizeof(st));
        if (sceIoGetstat(path, &st) >= 0) {
            total += (int)st.st_size;
        }
    }
    return total;
}

int offline_to_list_items(ListItem *items, int max_items) {
    OfflineTrack tracks[OFFLINE_MAX];
    int n = offline_load(tracks, OFFLINE_MAX);
    int i;
    if (n > max_items) {
        n = max_items;
    }
    for (i = 0; i < n; i++) {
        items[i].id = tracks[i].id;
        strncpy(items[i].name, tracks[i].title, MAX_NAME - 1);
        items[i].name[MAX_NAME - 1] = '\0';
        items[i].extra = tracks[i].rating;
    }
    return n;
}
