#include "storage.h"

#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static StorageStats g_st;

void storage_init(void) {
    memset(&g_st, 0, sizeof(g_st));
}

const StorageStats *storage_stats(void) {
    return &g_st;
}

void storage_reset_stats(void) {
    memset(&g_st, 0, sizeof(g_st));
}

int storage_open_write(const char *path, int trunc) {
    int flags = PSP_O_WRONLY | PSP_O_CREAT;
    if (trunc) {
        flags |= PSP_O_TRUNC;
    }
    return sceIoOpen(path, flags, 0777);
}

int storage_open_append(const char *path) {
    int fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    return fd;
}

int storage_open_read(const char *path) {
    return sceIoOpen(path, PSP_O_RDONLY, 0777);
}

int storage_write(int fd, const void *data, int len) {
    int n;
    if (fd < 0 || !data || len <= 0) {
        return -1;
    }
    n = sceIoWrite(fd, data, (unsigned)len);
    if (n > 0) {
        g_st.memoryStickWrites++;
        g_st.memoryStickBytesWritten += (unsigned long long)n;
    }
    return n;
}

int storage_read(int fd, void *data, int len) {
    if (fd < 0 || !data || len <= 0) {
        return -1;
    }
    return sceIoRead(fd, data, (unsigned)len);
}

int storage_close(int fd) {
    if (fd < 0) {
        return -1;
    }
    return sceIoClose(fd);
}

int storage_remove(const char *path) {
    int sz;
    if (!path || !path[0]) {
        return -1;
    }
    sz = storage_size(path);
    if (sceIoRemove(path) < 0) {
        return -1;
    }
    g_st.memoryStickDeletes++;
    if (sz > 0) {
        g_st.memoryStickBytesDeleted += (unsigned long long)sz;
    }
    return 0;
}

int storage_rename(const char *oldpath, const char *newpath) {
    if (!oldpath || !newpath) {
        return -1;
    }
    /* PSP: remove destination first if present. */
    sceIoRemove(newpath);
    if (sceIoRename(oldpath, newpath) < 0) {
        return -1;
    }
    g_st.memoryStickWrites++;
    return 0;
}

int storage_mkdir(const char *path) {
    if (!path) {
        return -1;
    }
    return sceIoMkdir(path, 0777);
}

int storage_exists(const char *path) {
    SceIoStat st;
    if (!path) {
        return 0;
    }
    return sceIoGetstat(path, &st) >= 0;
}

int storage_size(const char *path) {
    SceIoStat st;
    if (!path || sceIoGetstat(path, &st) < 0) {
        return -1;
    }
    return (int)st.st_size;
}

int storage_sync(void) {
    /* Avoid calling this on the playback path. Full-volume sync rewrites FAT. */
    return sceIoSync("ms0:", 0);
}

int storage_write_file(const char *path, const void *data, int len) {
    int fd;
    int n;
    if (!path || !data || len < 0) {
        return -1;
    }
    fd = storage_open_write(path, 1);
    if (fd < 0) {
        return -1;
    }
    n = storage_write(fd, data, len);
    storage_close(fd);
    return n == len ? 0 : -1;
}

int storage_write_file_if_changed(const char *path, const void *data, int len) {
    int fd;
    int n;
    char old[256];
    if (!path || !data || len < 0) {
        return -1;
    }
    if (len < (int)sizeof(old)) {
        fd = storage_open_read(path);
        if (fd >= 0) {
            n = storage_read(fd, old, (int)sizeof(old));
            storage_close(fd);
            if (n == len && memcmp(old, data, (size_t)len) == 0) {
                return 0;
            }
        }
    }
    return storage_write_file(path, data, len);
}

int storage_writer_open(StorageWriter *w, const char *path, int append) {
    if (!w || !path) {
        return -1;
    }
    memset(w, 0, sizeof(*w));
    w->fd = append ? storage_open_append(path) : storage_open_write(path, 1);
    if (w->fd < 0) {
        w->fd = -1;
        return -1;
    }
    w->buf = (unsigned char *)malloc(STORAGE_WBUF);
    w->cap = w->buf ? STORAGE_WBUF : 0;
    w->used = 0;
    return 0;
}

int storage_writer_write(StorageWriter *w, const void *data, int len) {
    const unsigned char *src = (const unsigned char *)data;
    if (!w || w->fd < 0 || !data || len < 0) {
        return -1;
    }
    if (len == 0) {
        return 0;
    }
    if (w->cap <= 0) {
        return storage_write(w->fd, data, len) == len ? 0 : -1;
    }
    while (len > 0) {
        int space = w->cap - w->used;
        int n = len < space ? len : space;
        memcpy(w->buf + w->used, src, (size_t)n);
        w->used += n;
        src += n;
        len -= n;
        if (w->used >= w->cap) {
            if (storage_write(w->fd, w->buf, w->used) != w->used) {
                return -1;
            }
            w->used = 0;
        }
    }
    return 0;
}

int storage_writer_close(StorageWriter *w) {
    int rc = 0;
    if (!w) {
        return -1;
    }
    if (w->fd >= 0 && w->buf && w->used > 0) {
        if (storage_write(w->fd, w->buf, w->used) != w->used) {
            rc = -1;
        }
        w->used = 0;
    }
    if (w->fd >= 0) {
        if (storage_close(w->fd) < 0) {
            rc = -1;
        }
        w->fd = -1;
    }
    free(w->buf);
    w->buf = NULL;
    w->cap = 0;
    return rc;
}

int storage_temp_create(const char *path) {
    int fd = storage_open_write(path, 1);
    if (fd >= 0) {
        g_st.temporaryFilesCreated++;
    }
    return fd;
}

int storage_temp_finalize(const char *tmp_path, const char *final_path) {
    /* PSP rename-over-existing often fails; remove target first. */
    if (storage_exists(final_path)) {
        (void)storage_remove(final_path);
    }
    if (storage_rename(tmp_path, final_path) == 0) {
        g_st.temporaryFilesDeleted++;
        /* No sceIoSync("ms0:") — full-volume flush wears the FAT. */
        return 0;
    }
    /* Fallback: copy then delete tmp. Prefer rename; this path doubles wear. */
    {
        int in_fd = storage_open_read(tmp_path);
        StorageWriter wr;
        unsigned char *buf;
        int n;
        int ok = 1;
        if (in_fd < 0) {
            return -1;
        }
        buf = (unsigned char *)malloc(STORAGE_WBUF);
        if (!buf) {
            storage_close(in_fd);
            return -1;
        }
        if (storage_writer_open(&wr, final_path, 0) < 0) {
            free(buf);
            storage_close(in_fd);
            return -1;
        }
        for (;;) {
            n = storage_read(in_fd, buf, STORAGE_WBUF);
            if (n < 0) {
                ok = 0;
                break;
            }
            if (n == 0) {
                break;
            }
            if (storage_writer_write(&wr, buf, n) < 0) {
                ok = 0;
                break;
            }
        }
        free(buf);
        storage_close(in_fd);
        if (storage_writer_close(&wr) < 0) {
            ok = 0;
        }
        if (!ok) {
            return -1;
        }
    }
    (void)storage_remove(tmp_path);
    g_st.temporaryFilesDeleted++;
    return 0;
}

int storage_temp_discard(const char *tmp_path) {
    if (storage_remove(tmp_path) == 0) {
        g_st.temporaryFilesDeleted++;
        return 0;
    }
    return -1;
}

long long storage_ms_free_bytes(void) {
    /* Best-effort: not all firmwares expose free space cleanly. */
    return -1;
}
