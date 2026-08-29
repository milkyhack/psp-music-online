#ifndef PATHS_H
#define PATHS_H

/* Call once at startup. Resolves folder that contains EBOOT.PBP. */
void paths_init(void);

/* Preferred: strip EBOOT filename from argv[0] → absolute base dir. */
void paths_init_argv(const char *argv0);

/* Directory of the running app, e.g. "ms0:/PSP/GAME/music" (no trailing slash). */
const char *paths_base(void);

/* Ensure base/data and base/data/offline exist. */
void paths_ensure_data(void);

/* Fill out with full path under app dir. Returns out. */
char *paths_join(char *out, int out_sz, const char *rel);

/* 1 if running from Game/PSPMUSICUPD (XMB "PSP Music Update" icon). */
int paths_is_update_companion(void);

/*
 * Join under sibling Game/PSPMUSIC (main player). Used by companion
 * to read server.cfg / install EBOOT. Returns out.
 */
char *paths_music_join(char *out, int out_sz, const char *rel);

/*
 * Intentionally no-op: deleting PSPMUSICUPD at runtime hung ARK/MS.
 * Companion icon stays permanently in Game.
 */
void paths_purge_update_companion(void);
void paths_request_purge_update_companion(void);

#endif
