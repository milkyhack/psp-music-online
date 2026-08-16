#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

/*
 * Release (default): all debug logging compiles out — zero MS I/O / Wi‑Fi spam.
 * QA: build with `make DEBUG_HUD=1` for on-device debug.log + HUD.
 */
#ifdef DEBUG_HUD

void dbg_log(const char *hypothesisId, const char *location, const char *message, const char *data_json);
void dbg_step(const char *step);
void dbg_flush(void);
const char *dbg_last_step(void);
int dbg_remote_flush(const char *host, int port);
void dbg_remote_tick(const char *host, int port, int online);
int dbg_pending_count(void);

#else

#define dbg_log(...) ((void)0)
#define dbg_step(...) ((void)0)
#define dbg_flush() ((void)0)
#define dbg_last_step() "-"
#define dbg_remote_flush(host, port) (0)
#define dbg_remote_tick(host, port, online) ((void)0)
#define dbg_pending_count() (0)

#endif

#endif
