#ifndef DOWNLOAD_H
#define DOWNLOAD_H

typedef enum {
    DL_IDLE = 0,
    DL_RUNNING,
    DL_PAUSED,
    DL_VERIFYING,
    DL_COMPLETE,
    DL_INCOMPLETE,
    DL_ERROR
} DownloadState;

typedef struct {
    DownloadState state;
    int track_id;
    char title[96];
    char artist[96];
    char album[96];
    char tmp_path[320];
    char final_path[320];
    int bytes;
    int total;
    float speed_bps;
    char error[64];
    int percent;
} DownloadStatus;

void download_init(void);
const DownloadStatus *download_status(void);

/* Start FLAC download to ms0:/MUSIC/... . Returns 0 on started. */
int download_start(
    const char *host,
    int port,
    int track_id,
    const char *title,
    const char *artist,
    const char *album,
    int track_num
);

int download_resume(const char *host, int port);
void download_pause(void);
void download_cancel(void);
int download_delete_incomplete(void);

/* Build FAT-safe path under ms0:/MUSIC/ */
void download_build_paths(
    char *tmp_out,
    int tmp_sz,
    char *final_out,
    int final_sz,
    const char *artist,
    const char *album,
    const char *title,
    int track_num
);

#endif
