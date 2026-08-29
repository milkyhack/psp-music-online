#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

#define HTTP_OK 0
#define HTTP_ERR -1
#define HTTP_ABORTED -2

/* Optional X-Api-Key sent on every request when non-empty. */
void http_set_api_key(const char *key);

/* Set from exit/HOME / Stop / Skip so TCP and recv loops abort quickly. */
void http_set_abort(int aborting);
int http_should_abort(void);

/* Short reason from last http_get_file_ex failure (e.g. "tcp", "hdr", "trunc"). */
const char *http_last_fail_reason(void);

/* GET url into malloc'd buffer (nul-terminated). Caller frees with free(). */
int http_get(const char *host, int port, const char *path, char **out_body, int *out_len);

/*
 * Progress callback during body download.
 * bytes = bytes written so far, total = Content-Length or -1 if unknown.
 */
typedef void (*HttpProgressFn)(int bytes, int total, void *userdata);

/* GET binary url into file path (blocking, abortable). */
int http_get_file(const char *host, int port, const char *path, const char *filepath);

/*
 * Like http_get_file with progress + optional Content-Length out.
 * Returns HTTP_OK, HTTP_ERR, or HTTP_ABORTED.
 */
int http_get_file_ex(
    const char *host,
    int port,
    const char *path,
    const char *filepath,
    HttpProgressFn progress,
    void *userdata,
    int *out_content_length
);

/*
 * GET into file with optional Range resume (range_start >= 0).
 * range_start < 0 means full GET. Appends when range_start > 0.
 */
int http_get_file_range(
    const char *host,
    int port,
    const char *path,
    const char *filepath,
    int range_start,
    HttpProgressFn progress,
    void *userdata,
    int *out_content_length,
    int *out_total_size
);

/* Stream body callback — NO Memory Stick write. Returns 0 to continue, <0 to abort. */
typedef int (*HttpStreamFn)(const void *data, int len, void *userdata);

/*
 * GET body into RAM via callback. Supports Range when range_start >= 0.
 * Accepts 200 and 206. out_total_size is full resource size when known.
 */
int http_get_stream(
    const char *host,
    int port,
    const char *path,
    int range_start,
    HttpStreamFn on_data,
    void *userdata,
    HttpProgressFn progress,
    void *progress_ud,
    int *out_content_length,
    int *out_total_size
);

/* PUT JSON body, optional response body (may be NULL). */
int http_put_json(const char *host, int port, const char *path, const char *json,
                  char **out_body, int *out_len);

/* POST JSON body (e.g. /api/plays). */
int http_post_json(const char *host, int port, const char *path, const char *json,
                   char **out_body, int *out_len);

/* POST with empty body (e.g. /api/scan). */
int http_post(const char *host, int port, const char *path, char **out_body, int *out_len);

#endif
