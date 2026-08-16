#ifndef STORAGE_H
#define STORAGE_H

#include <stddef.h>

typedef struct {
    unsigned long long memoryStickWrites;
    unsigned long long memoryStickDeletes;
    unsigned long long memoryStickBytesWritten;
    unsigned long long memoryStickBytesDeleted;
    unsigned long long temporaryFilesCreated;
    unsigned long long temporaryFilesDeleted;
} StorageStats;

void storage_init(void);
const StorageStats *storage_stats(void);
void storage_reset_stats(void);

/* Persistent MS I/O — all permanent writes go through here. */
int storage_open_write(const char *path, int trunc);
int storage_open_append(const char *path);
int storage_open_read(const char *path);
int storage_write(int fd, const void *data, int len);
int storage_read(int fd, void *data, int len);
int storage_close(int fd);
int storage_remove(const char *path);
int storage_rename(const char *oldpath, const char *newpath);
int storage_mkdir(const char *path);
int storage_exists(const char *path);
int storage_size(const char *path);
int storage_sync(void);

/* Temp file lifecycle (still on MS, but tracked separately). */
int storage_temp_create(const char *path);
int storage_temp_finalize(const char *tmp_path, const char *final_path);
int storage_temp_discard(const char *tmp_path);

/* Free space on ms0: in bytes; -1 if unknown. */
long long storage_ms_free_bytes(void);

#endif
