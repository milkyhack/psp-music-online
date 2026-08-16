#ifndef OFFLINE_H
#define OFFLINE_H

#include "jutil.h"

#define OFFLINE_MAX 128

typedef struct {
    int id;
    char artist[MAX_NAME];
    char album[MAX_NAME];
    char title[MAX_NAME];
    int rating;
    char file[64];
    /* Full path for FLAC under ms0:/MUSIC/... (empty = legacy data/offline/{id}.mp3). */
    char path[280];
    int is_flac;
} OfflineTrack;

void offline_ensure_dirs(void);
int offline_has(int track_id);
int offline_count(void);
int offline_load(OfflineTrack *out, int max_items);
int offline_save(
    int track_id,
    const char *artist,
    const char *album,
    const char *title,
    int rating,
    const char *src_mp3_path
);
/* Register a verified FLAC under ms0:/MUSIC (no second copy). */
int offline_register_flac(
    int track_id,
    const char *artist,
    const char *album,
    const char *title,
    int rating,
    const char *flac_path
);
int offline_path_for(int track_id, char *out, int out_sz);
/* Locate existing offline file (legacy MP3 or MUSIC FLAC). */
int offline_locate_path(int track_id, char *out, int out_sz);
/* Resolve playback path for a loaded OfflineTrack entry. */
int offline_resolve_path(const OfflineTrack *t, char *out, int out_sz);
int offline_to_list_items(ListItem *items, int max_items);
int offline_delete(int track_id);
/* Total size of saved offline audio in bytes. */
int offline_total_bytes(void);

#endif
