#ifndef CF_TRACE_H
#define CF_TRACE_H

#include "theme.h"

/* Cover Flow crash/debug trail — always on, batched MS + optional server upload. */
void cf_trace_init(void);
void cf_trace_step(const char *msg);
void cf_trace_val(const char *msg, int val);
void cf_trace_draw(const PlayerTheme *th);
void cf_trace_flush_file(void);
int cf_trace_flush_http(const char *host, int port);
void cf_trace_mode_3d(int on);
const char *cf_trace_last(void);

#endif
