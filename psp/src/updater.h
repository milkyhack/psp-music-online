#ifndef UPDATER_H
#define UPDATER_H

typedef enum {
    UPD_IDLE = 0,
    UPD_CHECKING,
    UPD_AVAILABLE,
    UPD_UP_TO_DATE,
    UPD_DOWNLOADING,
    UPD_VERIFYING,
    UPD_COMPLETE,
    UPD_INCOMPLETE,
    UPD_ERROR
} UpdateState;

typedef struct {
    UpdateState state;
    char current_version[24];
    char remote_version[24];
    int remote_code;
    int local_code;   /* installed PSPMUSIC DISC code; 0 if missing */
    int app_missing;  /* 1 = no PSPMUSIC/EBOOT.PBP yet */
    int bytes;
    int total;
    float speed_bps;
    int percent;
    char notes[96];
    char error[64];
} UpdateStatus;

#define APP_VERSION "1.3.8"
#define APP_VERSION_CODE 158

void updater_init(void);
const UpdateStatus *updater_status(void);

int updater_check(const char *host, int port);
int updater_start_download(const char *host, int port);
void updater_cancel(void);
int updater_resume(const char *host, int port);
int updater_delete_incomplete(void);

#endif
