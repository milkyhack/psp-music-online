#include "http.h"
#include "debug_log.h"

#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspsdk.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* From Wipeout / PSP-FTPD: SO_NONBLOCK for sceNetInetSetsockopt */
#ifndef SO_NONBLOCK
#define SO_NONBLOCK 0x1009
#endif
#define PSP_EINPROGRESS 0x77
#define PSP_EALREADY 0x78
#define PSP_EISCONN 0x7F

static volatile int g_http_abort = 0;
static volatile int g_http_sock = -1;
static char g_http_fail[24] = "";
static char g_api_key[64] = "";

void http_set_api_key(const char *key) {
    if (!key) {
        g_api_key[0] = '\0';
        return;
    }
    strncpy(g_api_key, key, sizeof(g_api_key) - 1);
    g_api_key[sizeof(g_api_key) - 1] = '\0';
}

static void http_auth_line(char *out, int out_sz) {
    if (!out || out_sz <= 0) {
        return;
    }
    if (!g_api_key[0]) {
        out[0] = '\0';
        return;
    }
    snprintf(out, out_sz, "X-Api-Key: %s\r\n", g_api_key);
}

void http_set_abort(int aborting) {
    g_http_abort = aborting ? 1 : 0;
    if (aborting && g_http_sock >= 0) {
        /* Unblock a stuck recv in the download thread. */
        sceNetInetClose(g_http_sock);
        g_http_sock = -1;
    }
}

int http_should_abort(void) {
    return g_http_abort;
}

const char *http_last_fail_reason(void) {
    return g_http_fail[0] ? g_http_fail : "err";
}

static void http_set_fail(const char *reason) {
    if (!reason) {
        reason = "err";
    }
    strncpy(g_http_fail, reason, sizeof(g_http_fail) - 1);
    g_http_fail[sizeof(g_http_fail) - 1] = '\0';
}

static int inet_in_progress(int en) {
    return en == PSP_EINPROGRESS || en == PSP_EALREADY;
}

static int parse_ipv4(const char *host, unsigned char out[4]) {
    unsigned a, b, c, d;
    if (sscanf(host, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
        return -1;
    }
    if (a > 255 || b > 255 || c > 255 || d > 255) {
        return -1;
    }
    out[0] = (unsigned char)a;
    out[1] = (unsigned char)b;
    out[2] = (unsigned char)c;
    out[3] = (unsigned char)d;
    return 0;
}

/*
 * Native sceNetInet* connect with timeout (PSP-FTPD pattern).
 * Retries with a fresh socket after EALREADY (0x78) — common after Skip kills a DL thread.
 */
static int open_tcp_once(const char *host, int port, unsigned char ip[4]) {
    (void)host;
    struct sockaddr_in sin;
    int sock;
    int err;
    int noblock;
    int i;
    int last_errno = 0;

    sock = sceNetInetSocket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        char d[48];
        snprintf(d, sizeof(d), "{\"sock\":%d,\"errno\":%d}", sock, sceNetInetGetErrno());
        dbg_log("B", "http.c:open_tcp", "socket_fail", d);
        return -1;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons((unsigned short)port);
    memcpy(&sin.sin_addr.s_addr, ip, 4);

    noblock = 1;
    sceNetInetSetsockopt(sock, SOL_SOCKET, SO_NONBLOCK, &noblock, sizeof(noblock));

    err = sceNetInetConnect(sock, (struct sockaddr *)&sin, sizeof(sin));
    if (err == 0) {
        noblock = 0;
        sceNetInetSetsockopt(sock, SOL_SOCKET, SO_NONBLOCK, &noblock, sizeof(noblock));
        dbg_log("B", "http.c:open_tcp", "connected_immediate", "{}");
        return sock;
    }

    last_errno = sceNetInetGetErrno();
    if (err == -1 && inet_in_progress(last_errno)) {
        for (i = 0; i < 50; i++) {
            if (g_http_abort) {
                break;
            }
            sceKernelDelayThread(40000);
            err = sceNetInetConnect(sock, (struct sockaddr *)&sin, sizeof(sin));
            last_errno = sceNetInetGetErrno();
            if (err == 0 || (err == -1 && last_errno == PSP_EISCONN)) {
                noblock = 0;
                sceNetInetSetsockopt(sock, SOL_SOCKET, SO_NONBLOCK, &noblock, sizeof(noblock));
                {
                    char d[64];
                    snprintf(d, sizeof(d), "{\"ok\":1,\"poll\":%d}", i);
                    dbg_log("B", "http.c:open_tcp", "connected", d);
                }
                return sock;
            }
            if (err == -1 && !inet_in_progress(last_errno) && last_errno != PSP_EISCONN) {
                break;
            }
        }
    }

    {
        char d[96];
        snprintf(
            d,
            sizeof(d),
            "{\"err\":%d,\"errno\":%d,\"ip\":\"%u.%u.%u.%u\",\"port\":%d}",
            err,
            last_errno,
            ip[0],
            ip[1],
            ip[2],
            ip[3],
            port
        );
        dbg_log("B", "http.c:open_tcp", "connect_fail", d);
    }
    sceNetInetClose(sock);
    /* Encode last errno in negative range for retry decision (keep sock invalid). */
    if (last_errno == PSP_EALREADY) {
        return -120;
    }
    return -1;
}

static int open_tcp(const char *host, int port) {
    unsigned char ip[4];
    int attempt;
    int sock;

    if (parse_ipv4(host, ip) < 0) {
        dbg_log("B", "http.c:open_tcp", "bad_ip", "{}");
        return -1;
    }

    for (attempt = 0; attempt < 3; attempt++) {
        if (g_http_abort) {
            return -1;
        }
        sock = open_tcp_once(host, port, ip);
        if (sock >= 0) {
            return sock;
        }
        /* After Skip/kill the stack often returns EALREADY — wait and retry. */
        if (sock == -120 || attempt < 2) {
            sceKernelDelayThread(150000);
            continue;
        }
        break;
    }
    return -1;
}

static void sock_close(int sock) {
    if (sock >= 0) {
        if (g_http_sock == sock) {
            g_http_sock = -1;
        }
        sceNetInetClose(sock);
    }
}

static int sock_send_all(int sock, const char *buf, int len) {
    int sent = 0;
    while (sent < len) {
        int n = sceNetInetSend(sock, buf + sent, (size_t)(len - sent), 0);
        if (n <= 0) {
            return -1;
        }
        sent += n;
    }
    return 0;
}

static int sock_recv(int sock, void *buf, int len) {
    if (g_http_abort) {
        return -1;
    }
    return (int)sceNetInetRecv(sock, buf, (size_t)len, 0);
}

/* Polling recv so Stop can abort during long server waits (ffmpeg).
 * idle_timeout_us: 0 = wait forever (abort still works); else fail after idle. */
static int sock_recv_abortable_ex(int sock, void *buf, int len, unsigned idle_timeout_us) {
    int noblock = 1;
    unsigned idle_us = 0;

    if (g_http_abort) {
        return -1;
    }

    sceNetInetSetsockopt(sock, SOL_SOCKET, SO_NONBLOCK, &noblock, sizeof(noblock));

    for (;;) {
        int r;
        if (g_http_abort) {
            noblock = 0;
            sceNetInetSetsockopt(sock, SOL_SOCKET, SO_NONBLOCK, &noblock, sizeof(noblock));
            return -1;
        }
        r = (int)sceNetInetRecv(sock, buf, (size_t)len, 0);
        if (r > 0) {
            noblock = 0;
            sceNetInetSetsockopt(sock, SOL_SOCKET, SO_NONBLOCK, &noblock, sizeof(noblock));
            return r;
        }
        if (r == 0) {
            noblock = 0;
            sceNetInetSetsockopt(sock, SOL_SOCKET, SO_NONBLOCK, &noblock, sizeof(noblock));
            return 0;
        }
        /* Any would-block / transient error: keep waiting (PSP errno varies). */
        sceKernelDelayThread(20000);
        if (idle_timeout_us > 0) {
            idle_us += 20000;
            if (idle_us >= idle_timeout_us) {
                noblock = 0;
                sceNetInetSetsockopt(sock, SOL_SOCKET, SO_NONBLOCK, &noblock, sizeof(noblock));
                return -1;
            }
        }
    }
}

static int sock_recv_abortable(int sock, void *buf, int len) {
    return sock_recv_abortable_ex(sock, buf, len, 0);
}

static int parse_content_length(const char *hdr) {
    const char *p = strstr(hdr, "Content-Length:");
    if (!p) {
        p = strstr(hdr, "content-length:");
    }
    if (!p) {
        p = strstr(hdr, "Content-length:");
    }
    if (!p) {
        return -1;
    }
    return atoi(p + 15);
}

static int read_response(int sock, char **out_body, int *out_len) {
    char *buf = NULL;
    int cap = 0;
    int len = 0;
    char chunk[2048];

    for (;;) {
        int n = sock_recv(sock, chunk, sizeof(chunk));
        if (n <= 0) {
            break;
        }
        if (len + n + 1 > cap) {
            int ncap = cap ? cap * 2 : 8192;
            while (ncap < len + n + 1) {
                ncap *= 2;
            }
            char *nb = (char *)realloc(buf, ncap);
            if (!nb) {
                free(buf);
                return HTTP_ERR;
            }
            buf = nb;
            cap = ncap;
        }
        memcpy(buf + len, chunk, n);
        len += n;
        buf[len] = '\0';
    }

    if (!buf || len <= 0) {
        free(buf);
        return HTTP_ERR;
    }

    char *body = strstr(buf, "\r\n\r\n");
    if (!body) {
        free(buf);
        return HTTP_ERR;
    }
    body += 4;
    int body_len = len - (int)(body - buf);

    if (strncmp(buf, "HTTP/1.", 7) != 0) {
        free(buf);
        return HTTP_ERR;
    }
    int status = atoi(buf + 9);
    if (status < 200 || status >= 300) {
        free(buf);
        return HTTP_ERR;
    }

    char *out = (char *)malloc(body_len + 1);
    if (!out) {
        free(buf);
        return HTTP_ERR;
    }
    memcpy(out, body, body_len);
    out[body_len] = '\0';
    free(buf);

    if (out_body) {
        *out_body = out;
    } else {
        free(out);
    }
    if (out_len) {
        *out_len = body_len;
    }
    return HTTP_OK;
}

int http_get(const char *host, int port, const char *path, char **out_body, int *out_len) {
    int sock = open_tcp(host, port);
    if (sock < 0) {
        return HTTP_ERR;
    }

    char req[576];
    char auth[96];
    http_auth_line(auth, sizeof(auth));
    int n = snprintf(
        req,
        sizeof(req),
        "GET %s HTTP/1.0\r\nHost: %s:%d\r\nConnection: close\r\nUser-Agent: PSPMusic/0.1\r\n%s\r\n",
        path,
        host,
        port,
        auth
    );
    if (sock_send_all(sock, req, n) < 0) {
        sock_close(sock);
        return HTTP_ERR;
    }

    int rc = read_response(sock, out_body, out_len);
    sock_close(sock);
    return rc;
}

int http_get_file_ex(
    const char *host,
    int port,
    const char *path,
    const char *filepath,
    HttpProgressFn progress,
    void *userdata,
    int *out_content_length
) {
    int sock;
    int content_length = -1;
    int written = 0;
    int complete;
    SceUID fd = -1;

    g_http_fail[0] = '\0';
    if (out_content_length) {
        *out_content_length = -1;
    }

    dbg_step("dl_enter");

    if (g_http_abort) {
        http_set_fail("abort");
        dbg_step("err_abort");
        return HTTP_ABORTED;
    }

    sock = open_tcp(host, port);
    if (sock < 0) {
        if (g_http_abort) {
            http_set_fail("abort");
            dbg_step("err_abort");
            return HTTP_ABORTED;
        }
        http_set_fail("tcp");
        dbg_step("err_tcp");
        return HTTP_ERR;
    }
    g_http_sock = sock;
    dbg_step("tcp_ok");

    {
        char req[576];
        char auth[96];
        int n;
        http_auth_line(auth, sizeof(auth));
        n = snprintf(
            req,
            sizeof(req),
            "GET %s HTTP/1.0\r\nHost: %s:%d\r\nConnection: close\r\nUser-Agent: PSPMusic/0.1\r\n%s\r\n",
            path,
            host,
            port,
            auth
        );
        if (sock_send_all(sock, req, n) < 0) {
            sock_close(sock);
            if (g_http_abort) {
                http_set_fail("abort");
                dbg_step("err_abort");
                return HTTP_ABORTED;
            }
            http_set_fail("send");
            dbg_step("err_send");
            return HTTP_ERR;
        }
    }
    dbg_step("req_sent");

    {
        char hdr[2048];
        int hlen = 0;
        char *sep;
        int header_bytes;
        int leftover;

        while (hlen < (int)sizeof(hdr) - 1) {
            int r;
            if (g_http_abort) {
                sock_close(sock);
                http_set_fail("abort");
                dbg_step("err_abort");
                return HTTP_ABORTED;
            }
            /* Header: poll+abort; transient errno retries inside. */
            r = sock_recv_abortable(sock, hdr + hlen, 1);
            if (r < 0) {
                sock_close(sock);
                if (g_http_abort) {
                    http_set_fail("abort");
                    dbg_step("err_abort");
                    return HTTP_ABORTED;
                }
                http_set_fail("hdr");
                dbg_step("err_hdr");
                return HTTP_ERR;
            }
            if (r == 0) {
                sock_close(sock);
                http_set_fail("hdr");
                dbg_step("err_hdr_eof");
                return HTTP_ERR;
            }
            hlen += r;
            hdr[hlen] = '\0';
            if (hlen >= 4 && strstr(hdr, "\r\n\r\n")) {
                break;
            }
        }

        if (strncmp(hdr, "HTTP/1.", 7) != 0) {
            sock_close(sock);
            http_set_fail("hdr");
            dbg_step("err_hdr_proto");
            return HTTP_ERR;
        }
        {
            int status = atoi(hdr + 9);
            if (status < 200 || status >= 300) {
                sock_close(sock);
                http_set_fail("status");
                dbg_step("err_status");
                return HTTP_ERR;
            }
        }

        content_length = parse_content_length(hdr);
        if (out_content_length) {
            *out_content_length = content_length;
        }

        sep = strstr(hdr, "\r\n\r\n");
        if (!sep) {
            sock_close(sock);
            http_set_fail("hdr");
            dbg_step("err_hdr_sep");
            return HTTP_ERR;
        }
        header_bytes = (int)(sep + 4 - hdr);
        leftover = hlen - header_bytes;
        dbg_step("hdr_ok");

        fd = sceIoOpen(filepath, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
        if (fd < 0) {
            {
                char d[160];
                char step[48];
                snprintf(
                    d,
                    sizeof(d),
                    "{\"fd\":%d,\"path\":\"%.96s\"}",
                    fd,
                    filepath ? filepath : "?"
                );
                dbg_log("B", "http.c:get_file", "open_fail", d);
                snprintf(step, sizeof(step), "err_file_%d", fd);
                dbg_step(step);
            }
            sock_close(sock);
            http_set_fail("file");
            return HTTP_ERR;
        }
        if (leftover > 0) {
            sceIoWrite(fd, sep + 4, leftover);
            written += leftover;
            if (progress) {
                progress(written, content_length, userdata);
            }
        }

        dbg_step("body");
        {
            char chunk[4096];
            for (;;) {
                int r;
                if (g_http_abort) {
                    /* Finish only if we already have the full file. */
                    break;
                }
                /* Body: blocking recv; abort checked between chunks. */
                r = sock_recv(sock, chunk, sizeof(chunk));
                if (r < 0) {
                    if (g_http_abort) {
                        break;
                    }
                    sceIoClose(fd);
                    sock_close(sock);
                    http_set_fail("recv");
                    dbg_step("err_recv");
                    return HTTP_ERR;
                }
                if (r == 0) {
                    break; /* EOF */
                }
                sceIoWrite(fd, chunk, r);
                written += r;
                if (progress) {
                    progress(written, content_length, userdata);
                }
                if (content_length > 0 && written >= content_length) {
                    break;
                }
            }
        }

        sceIoClose(fd);
        fd = -1;
    }

    sock_close(sock);

    if (content_length > 0) {
        complete = (written == content_length);
    } else {
        complete = (written > 0);
    }

    /* Complete file wins over a late abort (Stop after last byte). */
    if (complete) {
        dbg_step("done");
        return HTTP_OK;
    }
    if (g_http_abort) {
        http_set_fail("abort");
        dbg_step("err_abort");
        dbg_log("B", "http.c:get_file", "http_abort", "{}");
        return HTTP_ABORTED;
    }
    if (written <= 0) {
        http_set_fail("empty");
        dbg_step("err_empty");
        return HTTP_ERR;
    }
    if (content_length > 0 && written != content_length) {
        http_set_fail("trunc");
        dbg_step("err_trunc");
        return HTTP_ERR;
    }
    dbg_step("done");
    return HTTP_OK;
}

int http_get_file(const char *host, int port, const char *path, const char *filepath) {
    return http_get_file_ex(host, port, path, filepath, NULL, NULL, NULL);
}

static int http_json_method(
    const char *method,
    const char *host,
    int port,
    const char *path,
    const char *json,
    char **out_body,
    int *out_len
) {
    int sock = open_tcp(host, port);
    if (sock < 0) {
        return HTTP_ERR;
    }

    int jlen = (int)strlen(json);
    char req[832];
    char auth[96];
    int n;
    http_auth_line(auth, sizeof(auth));
    n = snprintf(
        req,
        sizeof(req),
        "%s %s HTTP/1.0\r\nHost: %s:%d\r\nConnection: close\r\n"
        "Content-Type: application/json\r\nContent-Length: %d\r\n"
        "User-Agent: PSPMusic/0.1\r\n%s\r\n%s",
        method,
        path,
        host,
        port,
        jlen,
        auth,
        json
    );
    if (sock_send_all(sock, req, n) < 0) {
        sock_close(sock);
        return HTTP_ERR;
    }

    int rc = read_response(sock, out_body, out_len);
    sock_close(sock);
    return rc;
}

int http_put_json(const char *host, int port, const char *path, const char *json,
                  char **out_body, int *out_len) {
    return http_json_method("PUT", host, port, path, json, out_body, out_len);
}

int http_post_json(const char *host, int port, const char *path, const char *json,
                   char **out_body, int *out_len) {
    return http_json_method("POST", host, port, path, json, out_body, out_len);
}

int http_post(const char *host, int port, const char *path, char **out_body, int *out_len) {
    int sock = open_tcp(host, port);
    if (sock < 0) {
        return HTTP_ERR;
    }

    char req[576];
    char auth[96];
    int n;
    http_auth_line(auth, sizeof(auth));
    n = snprintf(
        req,
        sizeof(req),
        "POST %s HTTP/1.0\r\nHost: %s:%d\r\nConnection: close\r\n"
        "Content-Length: 0\r\nUser-Agent: PSPMusic/0.1\r\n%s\r\n",
        path,
        host,
        port,
        auth
    );
    if (sock_send_all(sock, req, n) < 0) {
        sock_close(sock);
        return HTTP_ERR;
    }

    int rc = read_response(sock, out_body, out_len);
    sock_close(sock);
    return rc;
}

static int parse_content_range_total(const char *hdr) {
    /* Content-Range: bytes 0-99/1234 */
    const char *p = strstr(hdr, "Content-Range:");
    const char *slash;
    if (!p) {
        p = strstr(hdr, "content-range:");
    }
    if (!p) {
        return -1;
    }
    slash = strchr(p, '/');
    if (!slash) {
        return -1;
    }
    return atoi(slash + 1);
}

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
) {
    int sock;
    int content_length = -1;
    int total_size = -1;
    int written = 0;
    int status;

    g_http_fail[0] = '\0';
    if (out_content_length) {
        *out_content_length = -1;
    }
    if (out_total_size) {
        *out_total_size = -1;
    }
    if (!on_data) {
        http_set_fail("cb");
        return HTTP_ERR;
    }
    if (g_http_abort) {
        http_set_fail("abort");
        return HTTP_ABORTED;
    }

    sock = open_tcp(host, port);
    if (sock < 0) {
        if (g_http_abort) {
            http_set_fail("abort");
            return HTTP_ABORTED;
        }
        http_set_fail("tcp");
        return HTTP_ERR;
    }
    g_http_sock = sock;

    {
        char req[640];
        char auth[96];
        char range_line[48];
        int n;
        http_auth_line(auth, sizeof(auth));
        if (range_start >= 0) {
            snprintf(range_line, sizeof(range_line), "Range: bytes=%d-\r\n", range_start);
        } else {
            range_line[0] = '\0';
        }
        n = snprintf(
            req,
            sizeof(req),
            "GET %s HTTP/1.0\r\nHost: %s:%d\r\nConnection: close\r\n"
            "User-Agent: PSPMusic/1.0\r\n%s%s\r\n",
            path,
            host,
            port,
            range_line,
            auth
        );
        if (sock_send_all(sock, req, n) < 0) {
            sock_close(sock);
            http_set_fail(g_http_abort ? "abort" : "send");
            return g_http_abort ? HTTP_ABORTED : HTTP_ERR;
        }
    }

    {
        char hdr[2048];
        int hlen = 0;
        char *sep;
        int leftover;

        while (hlen < (int)sizeof(hdr) - 1) {
            int r;
            if (g_http_abort) {
                sock_close(sock);
                http_set_fail("abort");
                return HTTP_ABORTED;
            }
            r = sock_recv_abortable(sock, hdr + hlen, 1);
            if (r <= 0) {
                sock_close(sock);
                http_set_fail(g_http_abort ? "abort" : "hdr");
                return g_http_abort ? HTTP_ABORTED : HTTP_ERR;
            }
            hlen += r;
            hdr[hlen] = '\0';
            if (hlen >= 4 && strstr(hdr, "\r\n\r\n")) {
                break;
            }
        }

        if (strncmp(hdr, "HTTP/1.", 7) != 0) {
            sock_close(sock);
            http_set_fail("hdr");
            return HTTP_ERR;
        }
        status = atoi(hdr + 9);
        if (status != 200 && status != 206) {
            sock_close(sock);
            http_set_fail("status");
            return HTTP_ERR;
        }

        content_length = parse_content_length(hdr);
        total_size = parse_content_range_total(hdr);
        if (total_size < 0 && content_length > 0 && range_start < 0) {
            total_size = content_length;
        }
        if (total_size < 0 && content_length > 0 && range_start >= 0) {
            total_size = range_start + content_length;
        }
        if (out_content_length) {
            *out_content_length = content_length;
        }
        if (out_total_size) {
            *out_total_size = total_size;
        }

        sep = strstr(hdr, "\r\n\r\n");
        if (!sep) {
            sock_close(sock);
            http_set_fail("hdr");
            return HTTP_ERR;
        }
        leftover = hlen - (int)(sep + 4 - hdr);
        if (leftover > 0) {
            if (on_data(sep + 4, leftover, userdata) < 0) {
                sock_close(sock);
                http_set_fail("abort");
                return HTTP_ABORTED;
            }
            written += leftover;
            if (progress) {
                progress(written, content_length > 0 ? content_length : total_size, progress_ud);
            }
        }

        {
            char chunk[4096];
            for (;;) {
                int r;
                if (g_http_abort) {
                    break;
                }
                /* Idle timeout: stalled Wi‑Fi used to freeze download forever. */
                r = sock_recv_abortable_ex(sock, chunk, sizeof(chunk), 15000000u);
                if (r < 0) {
                    if (g_http_abort) {
                        break;
                    }
                    sock_close(sock);
                    http_set_fail("recv_timeout");
                    return HTTP_ERR;
                }
                if (r == 0) {
                    break;
                }
                if (on_data(chunk, r, userdata) < 0) {
                    sock_close(sock);
                    http_set_fail("abort");
                    return HTTP_ABORTED;
                }
                written += r;
                if (progress) {
                    progress(written, content_length > 0 ? content_length : total_size, progress_ud);
                }
                if (content_length > 0 && written >= content_length) {
                    break;
                }
            }
        }
    }

    sock_close(sock);

    if (content_length > 0 && written == content_length) {
        return HTTP_OK;
    }
    if (content_length < 0 && written > 0 && !g_http_abort) {
        return HTTP_OK;
    }
    if (g_http_abort) {
        http_set_fail("abort");
        return HTTP_ABORTED;
    }
    if (written <= 0) {
        http_set_fail("empty");
        return HTTP_ERR;
    }
    if (content_length > 0 && written != content_length) {
        http_set_fail("trunc");
        return HTTP_ERR;
    }
    return HTTP_OK;
}

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
) {
    int sock;
    int content_length = -1;
    int total_size = -1;
    int written = 0;
    int status;
    SceUID fd = -1;
    int append = (range_start > 0);

    g_http_fail[0] = '\0';
    if (out_content_length) {
        *out_content_length = -1;
    }
    if (out_total_size) {
        *out_total_size = -1;
    }
    if (!filepath) {
        http_set_fail("file");
        return HTTP_ERR;
    }
    if (g_http_abort) {
        http_set_fail("abort");
        return HTTP_ABORTED;
    }

    sock = open_tcp(host, port);
    if (sock < 0) {
        http_set_fail(g_http_abort ? "abort" : "tcp");
        return g_http_abort ? HTTP_ABORTED : HTTP_ERR;
    }
    g_http_sock = sock;

    {
        char req[640];
        char auth[96];
        char range_line[48];
        int n;
        http_auth_line(auth, sizeof(auth));
        if (range_start >= 0) {
            snprintf(range_line, sizeof(range_line), "Range: bytes=%d-\r\n", range_start);
        } else {
            range_line[0] = '\0';
        }
        n = snprintf(
            req,
            sizeof(req),
            "GET %s HTTP/1.0\r\nHost: %s:%d\r\nConnection: close\r\n"
            "User-Agent: PSPMusic/1.0\r\n%s%s\r\n",
            path,
            host,
            port,
            range_line,
            auth
        );
        if (sock_send_all(sock, req, n) < 0) {
            sock_close(sock);
            http_set_fail(g_http_abort ? "abort" : "send");
            return g_http_abort ? HTTP_ABORTED : HTTP_ERR;
        }
    }

    {
        char hdr[2048];
        int hlen = 0;
        char *sep;
        int leftover;

        while (hlen < (int)sizeof(hdr) - 1) {
            int r;
            if (g_http_abort) {
                sock_close(sock);
                http_set_fail("abort");
                return HTTP_ABORTED;
            }
            r = sock_recv_abortable(sock, hdr + hlen, 1);
            if (r <= 0) {
                sock_close(sock);
                http_set_fail(g_http_abort ? "abort" : "hdr");
                return g_http_abort ? HTTP_ABORTED : HTTP_ERR;
            }
            hlen += r;
            hdr[hlen] = '\0';
            if (hlen >= 4 && strstr(hdr, "\r\n\r\n")) {
                break;
            }
        }

        status = atoi(hdr + 9);
        if (status != 200 && status != 206) {
            sock_close(sock);
            http_set_fail("status");
            return HTTP_ERR;
        }

        content_length = parse_content_length(hdr);
        total_size = parse_content_range_total(hdr);
        if (total_size < 0 && content_length > 0 && range_start < 0) {
            total_size = content_length;
        }
        if (total_size < 0 && content_length > 0 && range_start >= 0) {
            total_size = range_start + content_length;
        }
        if (out_content_length) {
            *out_content_length = content_length;
        }
        if (out_total_size) {
            *out_total_size = total_size;
        }

        sep = strstr(hdr, "\r\n\r\n");
        if (!sep) {
            sock_close(sock);
            http_set_fail("hdr");
            return HTTP_ERR;
        }

        if (append) {
            fd = sceIoOpen(filepath, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
        } else {
            fd = sceIoOpen(filepath, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
        }
        if (fd < 0) {
            sock_close(sock);
            http_set_fail("file");
            return HTTP_ERR;
        }

        leftover = hlen - (int)(sep + 4 - hdr);
        if (leftover > 0) {
            sceIoWrite(fd, sep + 4, leftover);
            written += leftover;
            if (progress) {
                progress(
                    (range_start > 0 ? range_start : 0) + written,
                    total_size > 0 ? total_size : content_length,
                    userdata
                );
            }
        }

        {
            char chunk[4096];
            for (;;) {
                int r;
                if (g_http_abort) {
                    break;
                }
                /* Idle timeout — OTA used to freeze forever at ~50% on WiFi stalls. */
                r = sock_recv_abortable_ex(sock, chunk, sizeof(chunk), 20000000u);
                if (r < 0) {
                    if (g_http_abort) {
                        break;
                    }
                    sceIoClose(fd);
                    sock_close(sock);
                    http_set_fail("recv_timeout");
                    return HTTP_ERR;
                }
                if (r == 0) {
                    break;
                }
                if (sceIoWrite(fd, chunk, r) != r) {
                    sceIoClose(fd);
                    sock_close(sock);
                    http_set_fail("file");
                    return HTTP_ERR;
                }
                written += r;
                if (progress) {
                    progress(
                        (range_start > 0 ? range_start : 0) + written,
                        total_size > 0 ? total_size : content_length,
                        userdata
                    );
                }
                if (content_length > 0 && written >= content_length) {
                    break;
                }
            }
        }
        sceIoClose(fd);
    }

    sock_close(sock);
    if (content_length > 0 && written == content_length) {
        return HTTP_OK;
    }
    if (content_length < 0 && written > 0 && !g_http_abort) {
        return HTTP_OK;
    }
    if (g_http_abort) {
        http_set_fail("abort");
        return HTTP_ABORTED;
    }
    if (written <= 0) {
        http_set_fail("empty");
        return HTTP_ERR;
    }
    if (content_length > 0 && written != content_length) {
        http_set_fail("trunc");
        return HTTP_ERR;
    }
    return HTTP_OK;
}
