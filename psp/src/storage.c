#include "storage.h"

#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <stdio.h>
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
    return sceIoSync("ms0:", 0);
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
        storage_sync();
        return 0;
    }
    /* Fallback: copy then delete tmp. */
    {
        int in_fd = storage_open_read(tmp_path);
        int out_fd;
        char buf[4096];
        int n;
        if (in_fd < 0) {
            return -1;
        }
        out_fd = storage_open_write(final_path, 1);
        if (out_fd < 0) {
            storage_close(in_fd);
            return -1;
        }
        for (;;) {
            n = storage_read(in_fd, buf, sizeof(buf));
            if (n < 0) {
                storage_close(in_fd);
                storage_close(out_fd);
                return -1;
            }
            if (n == 0) {
                break;
            }
            if (storage_write(out_fd, buf, n) != n) {
                storage_close(in_fd);
                storage_close(out_fd);
                return -1;
            }
        }
        storage_close(in_fd);
        storage_close(out_fd);
    }
    (void)storage_remove(tmp_path);
    g_st.temporaryFilesDeleted++;
    storage_sync();
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
