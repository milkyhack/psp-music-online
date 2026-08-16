#include "updater.h"
#include "http.h"
#include "storage.h"
#include "paths.h"
#include "jutil.h"
#include "debug_log.h"

#include <pspkernel.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    unsigned state[8];
    unsigned long long bitlen;
    unsigned char data[64];
    unsigned datalen;
} SHA256_CTX;

static const unsigned K256[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};

#define RR(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define CH(x,y,z) (((x)&(y))^(~(x)&(z)))
#define MAJ(x,y,z) (((x)&(y))^((x)&(z))^((y)&(z)))
#define EP0(x) (RR(x,2)^RR(x,13)^RR(x,22))
#define EP1(x) (RR(x,6)^RR(x,11)^RR(x,25))
#define SIG0(x) (RR(x,7)^RR(x,18)^((x)>>3))
#define SIG1(x) (RR(x,17)^RR(x,19)^((x)>>10))

static void sha256_transform(SHA256_CTX *ctx, const unsigned char data[]) {
    unsigned a,b,c,d,e,f,g,h,i,j,t1,t2,m[64];
    for (i = 0, j = 0; i < 16; ++i, j += 4)
        m[i] = ((unsigned)data[j]<<24)|((unsigned)data[j+1]<<16)|((unsigned)data[j+2]<<8)|(unsigned)data[j+3];
    for (; i < 64; ++i)
        m[i] = SIG1(m[i-2])+m[i-7]+SIG0(m[i-15])+m[i-16];
    a=ctx->state[0];b=ctx->state[1];c=ctx->state[2];d=ctx->state[3];
    e=ctx->state[4];f=ctx->state[5];g=ctx->state[6];h=ctx->state[7];
    for (i = 0; i < 64; ++i) {
        t1 = h+EP1(e)+CH(e,f,g)+K256[i]+m[i];
        t2 = EP0(a)+MAJ(a,b,c);
        h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    ctx->state[0]+=a;ctx->state[1]+=b;ctx->state[2]+=c;ctx->state[3]+=d;
    ctx->state[4]+=e;ctx->state[5]+=f;ctx->state[6]+=g;ctx->state[7]+=h;
}

static void sha256_init(SHA256_CTX *ctx) {
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0]=0x6a09e667;ctx->state[1]=0xbb67ae85;ctx->state[2]=0x3c6ef372;ctx->state[3]=0xa54ff53a;
    ctx->state[4]=0x510e527f;ctx->state[5]=0x9b05688c;ctx->state[6]=0x1f83d9ab;ctx->state[7]=0x5be0cd19;
}

static void sha256_update(SHA256_CTX *ctx, const unsigned char *data, size_t len) {
    size_t i;
    for (i = 0; i < len; ++i) {
        ctx->data[ctx->datalen] = data[i];
        ctx->datalen++;
        if (ctx->datalen == 64) {
            sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

static void sha256_final(SHA256_CTX *ctx, unsigned char hash[32]) {
    unsigned i = ctx->datalen;
    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;
        while (i < 56) ctx->data[i++] = 0x00;
    } else {
        ctx->data[i++] = 0x80;
        while (i < 64) ctx->data[i++] = 0x00;
        sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }
    ctx->bitlen += ctx->datalen * 8ULL;
    ctx->data[63]=(unsigned char)ctx->bitlen;
    ctx->data[62]=(unsigned char)(ctx->bitlen>>8);
    ctx->data[61]=(unsigned char)(ctx->bitlen>>16);
    ctx->data[60]=(unsigned char)(ctx->bitlen>>24);
    ctx->data[59]=(unsigned char)(ctx->bitlen>>32);
    ctx->data[58]=(unsigned char)(ctx->bitlen>>40);
    ctx->data[57]=(unsigned char)(ctx->bitlen>>48);
    ctx->data[56]=(unsigned char)(ctx->bitlen>>56);
    sha256_transform(ctx, ctx->data);
    for (i = 0; i < 4; ++i) {
        hash[i]   =(unsigned char)((ctx->state[0]>>(24-i*8))&0xff);
        hash[i+4] =(unsigned char)((ctx->state[1]>>(24-i*8))&0xff);
        hash[i+8] =(unsigned char)((ctx->state[2]>>(24-i*8))&0xff);
        hash[i+12]=(unsigned char)((ctx->state[3]>>(24-i*8))&0xff);
        hash[i+16]=(unsigned char)((ctx->state[4]>>(24-i*8))&0xff);
        hash[i+20]=(unsigned char)((ctx->state[5]>>(24-i*8))&0xff);
        hash[i+24]=(unsigned char)((ctx->state[6]>>(24-i*8))&0xff);
        hash[i+28]=(unsigned char)((ctx->state[7]>>(24-i*8))&0xff);
    }
}

static UpdateStatus g_up;
static SceUID g_thid = -1;
static volatile int g_cancel = 0;
static char g_sha_hex[72];
static char g_tmp[320];
static char g_final[320];
static char g_bak[320];
static int g_expected_size;
static char g_host[64];
static int g_port;
static int g_companion;
static int g_installed_code; /* APP_VERSION_CODE of PSPMUSIC EBOOT when companion */

/*
 * Server sends semver "1.2.19" + version_code 139.
 * XMB PARAM.SFO stores Sony DISC_VERSION "1.39" (= code/100 . code%100).
 * Never parse "1.39" with the semver scheme or 1.39 → 13900 > 1.2.19 → 10219
 * and the updater falsely reports "Already latest".
 */
static int semver_to_code(const char *ver) {
    int a = 0, b = 0, c = 0;
    if (!ver || !ver[0]) {
        return 0;
    }
    if (sscanf(ver, "%d.%d.%d", &a, &b, &c) >= 3) {
        if (a < 0) a = 0;
        if (b < 0) b = 0;
        if (c < 0) c = 0;
        return a * 10000 + b * 100 + c;
    }
    return 0;
}

/* Sony DISC_VERSION "1.39" → APP_VERSION_CODE 139 */
static int disc_version_to_code(const char *ver) {
    int a = 0, b = 0, c = 0;
    int n;
    if (!ver || !ver[0]) {
        return 0;
    }
    n = sscanf(ver, "%d.%d.%d", &a, &b, &c);
    if (n >= 3) {
        /* Legacy mistaken embeds of semver into DISC_VERSION */
        return a * 10000 + b * 100 + c;
    }
    if (n >= 2) {
        if (a < 0) a = 0;
        if (b < 0) b = 0;
        if (b > 99) b = 99;
        return a * 100 + b;
    }
    return 0;
}

static int version_to_code(const char *ver) {
    /* Kept for non-companion paths; prefer semver when three parts exist. */
    int s = semver_to_code(ver);
    return s > 0 ? s : disc_version_to_code(ver);
}

/* Read TITLE and/or DISC_VERSION from EBOOT.PBP PARAM.SFO. */
static int read_eboot_sfo_versions(
    const char *eboot_path,
    char *disc_out,
    int disc_sz,
    char *title_out,
    int title_sz
) {
    int fd;
    unsigned char hdr[40];
    unsigned sfo_off, next_off;
    unsigned char *sfo = NULL;
    int sfo_sz;
    unsigned key_table, data_table, nent;
    unsigned i;
    int got_disc = 0;
    int got_title = 0;

    if (disc_out && disc_sz > 0) {
        disc_out[0] = '\0';
    }
    if (title_out && title_sz > 0) {
        title_out[0] = '\0';
    }
    fd = storage_open_read(eboot_path);
    if (fd < 0) {
        return 0;
    }
    if (storage_read(fd, hdr, 40) != 40 || hdr[1] != 'P' || hdr[2] != 'B' || hdr[3] != 'P') {
        storage_close(fd);
        return 0;
    }
    sfo_off = (unsigned)hdr[8] | ((unsigned)hdr[9] << 8) | ((unsigned)hdr[10] << 16) | ((unsigned)hdr[11] << 24);
    next_off = (unsigned)hdr[12] | ((unsigned)hdr[13] << 8) | ((unsigned)hdr[14] << 16) | ((unsigned)hdr[15] << 24);
    if (next_off <= sfo_off || next_off - sfo_off > 65536) {
        storage_close(fd);
        return 0;
    }
    sfo_sz = (int)(next_off - sfo_off);
    sfo = (unsigned char *)malloc((size_t)sfo_sz);
    if (!sfo) {
        storage_close(fd);
        return 0;
    }
    storage_close(fd);
    fd = storage_open_read(eboot_path);
    if (fd < 0) {
        free(sfo);
        return 0;
    }
    {
        unsigned skip = sfo_off;
        unsigned char dump[512];
        while (skip > 0) {
            int n = (int)(skip > sizeof(dump) ? sizeof(dump) : skip);
            int r = storage_read(fd, dump, n);
            if (r <= 0) {
                storage_close(fd);
                free(sfo);
                return 0;
            }
            skip -= (unsigned)r;
        }
    }
    if (storage_read(fd, sfo, sfo_sz) != sfo_sz) {
        storage_close(fd);
        free(sfo);
        return 0;
    }
    storage_close(fd);

    if (sfo_sz < 20 || sfo[0] != 0x00 || sfo[1] != 'P' || sfo[2] != 'S' || sfo[3] != 'F') {
        free(sfo);
        return 0;
    }
    key_table = (unsigned)sfo[8] | ((unsigned)sfo[9] << 8) | ((unsigned)sfo[10] << 16) | ((unsigned)sfo[11] << 24);
    data_table = (unsigned)sfo[12] | ((unsigned)sfo[13] << 8) | ((unsigned)sfo[14] << 16) | ((unsigned)sfo[15] << 24);
    nent = (unsigned)sfo[16] | ((unsigned)sfo[17] << 8) | ((unsigned)sfo[18] << 16) | ((unsigned)sfo[19] << 24);
    for (i = 0; i < nent; i++) {
        unsigned off = 20 + i * 16;
        unsigned short key_off, data_fmt;
        unsigned data_len, data_off;
        const char *key;
        char buf[64];
        unsigned copy;
        if (off + 16 > (unsigned)sfo_sz) {
            break;
        }
        key_off = (unsigned short)(sfo[off] | (sfo[off + 1] << 8));
        data_fmt = (unsigned short)(sfo[off + 2] | (sfo[off + 3] << 8));
        data_len = (unsigned)sfo[off + 4] | ((unsigned)sfo[off + 5] << 8) |
                   ((unsigned)sfo[off + 6] << 16) | ((unsigned)sfo[off + 7] << 24);
        data_off = (unsigned)sfo[off + 12] | ((unsigned)sfo[off + 13] << 8) |
                   ((unsigned)sfo[off + 14] << 16) | ((unsigned)sfo[off + 15] << 24);
        if (key_table + key_off >= (unsigned)sfo_sz) {
            continue;
        }
        if (!(data_fmt == 0x0204 || data_fmt == 0x0004)) {
            continue;
        }
        if (data_table + data_off >= (unsigned)sfo_sz) {
            continue;
        }
        key = (const char *)(sfo + key_table + key_off);
        copy = data_len;
        if (copy >= sizeof(buf)) {
            copy = sizeof(buf) - 1;
        }
        memcpy(buf, sfo + data_table + data_off, copy);
        buf[copy] = '\0';
        if (strcmp(key, "DISC_VERSION") == 0 && disc_out && disc_sz > 0) {
            strncpy(disc_out, buf, (size_t)disc_sz - 1);
            disc_out[disc_sz - 1] = '\0';
            got_disc = disc_out[0] ? 1 : 0;
        } else if (strcmp(key, "TITLE") == 0 && title_out && title_sz > 0) {
            strncpy(title_out, buf, (size_t)title_sz - 1);
            title_out[title_sz - 1] = '\0';
            got_title = title_out[0] ? 1 : 0;
        }
    }
    free(sfo);
    return got_disc || got_title;
}

/* Prefer "PSP Music 1.2.17" from TITLE; fall back to DISC_VERSION. */
static void set_installed_display_version(const char *title, const char *disc) {
    const char *p;
    int a = 0, b = 0, c = 0;
    if (title && title[0]) {
        p = strstr(title, "PSP Music ");
        if (p) {
            p += 10;
            if (sscanf(p, "%d.%d.%d", &a, &b, &c) >= 3) {
                snprintf(g_up.current_version, sizeof(g_up.current_version), "%d.%d.%d", a, b, c);
                return;
            }
        }
        /* Updater TITLE is "Music Updater x.y.z" — ignore for player version. */
    }
    if (disc && disc[0]) {
        strncpy(g_up.current_version, disc, sizeof(g_up.current_version) - 1);
        g_up.current_version[sizeof(g_up.current_version) - 1] = '\0';
        return;
    }
    strncpy(g_up.current_version, "none", sizeof(g_up.current_version) - 1);
}

static int read_eboot_disc_version(const char *eboot_path, char *ver_out, int ver_sz) {
    return read_eboot_sfo_versions(eboot_path, ver_out, ver_sz, NULL, 0);
}

void updater_init(void) {
    memset(&g_up, 0, sizeof(g_up));
    g_up.state = UPD_IDLE;
    g_installed_code = 0;
    g_up.local_code = 0;
    g_up.app_missing = 0;
    strncpy(g_up.current_version, APP_VERSION, sizeof(g_up.current_version) - 1);
    paths_join(g_final, sizeof(g_final), "EBOOT.PBP");
    paths_join(g_tmp, sizeof(g_tmp), "EBOOT.PBP.tmp");
    paths_join(g_bak, sizeof(g_bak), "EBOOT.PBP.bak");

    /* Companion "PSPMUSICUPD" installs into sibling PSPMUSIC folder. */
    g_companion = paths_is_update_companion();
    if (g_companion) {
        char music_dir[280];
        char disc[24];
        char title[48];
        paths_music_join(music_dir, sizeof(music_dir), "");
        storage_mkdir(music_dir);
        paths_music_join(g_final, sizeof(g_final), "EBOOT.PBP");
        paths_music_join(g_tmp, sizeof(g_tmp), "EBOOT.PBP.tmp");
        paths_music_join(g_bak, sizeof(g_bak), "EBOOT.PBP.bak");
        disc[0] = title[0] = '\0';
        if (storage_exists(g_final) &&
            read_eboot_sfo_versions(g_final, disc, sizeof(disc), title, sizeof(title))) {
            set_installed_display_version(title, disc);
            g_installed_code = disc_version_to_code(disc);
            if (g_installed_code <= 0) {
                g_installed_code = semver_to_code(g_up.current_version);
            }
            g_up.local_code = g_installed_code;
            g_up.app_missing = 0;
        } else {
            strncpy(g_up.current_version, "none", sizeof(g_up.current_version) - 1);
            g_installed_code = 0;
            g_up.local_code = 0;
            g_up.app_missing = 1;
        }
    }
}

const UpdateStatus *updater_status(void) {
    return &g_up;
}

static int verify_eboot(const char *path);

int updater_check(const char *host, int port) {
    char *body = NULL;
    int len = 0;
    int code = 0;
    char ver[24];
    char sha[72];
    char notes[96];
    int size = 0;

    g_up.state = UPD_CHECKING;
    g_up.error[0] = '\0';
    if (http_get(host, port, "/api/client/update", &body, &len) != HTTP_OK || !body) {
        const char *why = http_last_fail_reason();
        if (why && why[0]) {
            snprintf(g_up.error, sizeof(g_up.error), "check: %s", why);
        } else {
            strncpy(g_up.error, "check failed", sizeof(g_up.error) - 1);
        }
        g_up.state = UPD_ERROR;
        free(body);
        return -1;
    }
    ver[0] = sha[0] = notes[0] = '\0';
    jutil_extract_string(body, "version", ver, sizeof(ver));
    jutil_extract_string(body, "sha256", sha, sizeof(sha));
    jutil_extract_string(body, "notes", notes, sizeof(notes));
    jutil_extract_int(body, "version_code", &code);
    jutil_extract_int(body, "size", &size);
    free(body);

    strncpy(g_up.remote_version, ver, sizeof(g_up.remote_version) - 1);
    strncpy(g_up.notes, notes, sizeof(g_up.notes) - 1);
    strncpy(g_sha_hex, sha, sizeof(g_sha_hex) - 1);
    g_sha_hex[sizeof(g_sha_hex) - 1] = '\0';
    g_up.remote_code = code;
    g_up.total = size;
    g_expected_size = size;

    {
        char d[120];
        snprintf(d, sizeof(d), "{\"remote\":%d,\"installed\":%d,\"comp\":%d}",
                 code, g_companion ? g_installed_code : APP_VERSION_CODE, g_companion);
        dbg_log("U", "updater.c:check", "manifest", d);
    }

    /*
     * Companion: compare server version_code to Sony DISC_VERSION in PSPMUSIC/EBOOT.
     * Never auto-download — only set AVAILABLE / UP_TO_DATE.
     */
    if (g_companion) {
        int remote_code = code; /* prefer JSON version_code (139) */
        int local_code = g_installed_code;
        char disc[24];
        char title[48];

        if (remote_code <= 0) {
            remote_code = semver_to_code(ver);
        }
        if (remote_code <= 0) {
            remote_code = disc_version_to_code(ver);
        }

        disc[0] = title[0] = '\0';
        g_up.app_missing = !storage_exists(g_final);
        if (!g_up.app_missing &&
            read_eboot_sfo_versions(g_final, disc, sizeof(disc), title, sizeof(title))) {
            set_installed_display_version(title, disc);
            local_code = disc_version_to_code(disc);
            if (local_code <= 0) {
                local_code = semver_to_code(g_up.current_version);
            }
            g_installed_code = local_code;
        } else if (g_up.app_missing) {
            strncpy(g_up.current_version, "none", sizeof(g_up.current_version) - 1);
            local_code = 0;
            g_installed_code = 0;
        }
        g_up.local_code = local_code;

        {
            char d[160];
            snprintf(
                d,
                sizeof(d),
                "{\"remote\":%d,\"local\":%d,\"miss\":%d,\"disc\":\"%s\",\"cur\":\"%s\"}",
                remote_code,
                local_code,
                g_up.app_missing,
                disc,
                g_up.current_version
            );
            dbg_log("U", "updater.c:check", "companion_compare", d);
        }

        if (remote_code > local_code) {
            g_up.state = UPD_AVAILABLE;
            return 1;
        }
        storage_temp_discard(g_tmp);
        g_up.app_missing = 0;
        g_up.state = UPD_UP_TO_DATE;
        return 0;
    }

    if (code > APP_VERSION_CODE) {
        g_up.state = UPD_AVAILABLE;
        return 1;
    }
    /* Already on this or newer build — clear leftover tmp that caused re-download loops. */
    storage_temp_discard(g_tmp);
    g_up.state = UPD_UP_TO_DATE;
    return 0;
}

static void progress_cb(int bytes, int total, void *ud) {
    (void)ud;
    g_up.bytes = bytes;
    if (total > 0) {
        g_up.total = total;
        g_up.percent = (bytes * 100) / total;
    }
}

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int verify_eboot(const char *path) {
    int fd;
    unsigned char buf[4096];
    unsigned char hash[32];
    SHA256_CTX ctx;
    int n;
    int i;
    char magic[4];
    int sz;

    sz = storage_size(path);
    if (sz < 64 * 1024) {
        return -5;
    }
    if (g_expected_size > 0 && sz != g_expected_size) {
        return -4;
    }

    fd = storage_open_read(path);
    if (fd < 0) {
        return -1;
    }
    n = storage_read(fd, magic, 4);
    if (n != 4 || magic[0] != 0x00 || magic[1] != 'P' || magic[2] != 'B' || magic[3] != 'P') {
        storage_close(fd);
        return -2;
    }
    /* Continue hashing from offset 0 — re-read from start. */
    storage_close(fd);

    fd = storage_open_read(path);
    if (fd < 0) {
        return -1;
    }
    sha256_init(&ctx);
    for (;;) {
        n = storage_read(fd, buf, sizeof(buf));
        if (n < 0) {
            storage_close(fd);
            return -1;
        }
        if (n == 0) {
            break;
        }
        sha256_update(&ctx, buf, (size_t)n);
    }
    storage_close(fd);
    sha256_final(&ctx, hash);

    if ((int)strlen(g_sha_hex) < 64) {
        return -3;
    }
    for (i = 0; i < 32; i++) {
        int hi = hex_nibble(g_sha_hex[i * 2]);
        int lo = hex_nibble(g_sha_hex[i * 2 + 1]);
        if (hi < 0 || lo < 0 || ((hi << 4) | lo) != hash[i]) {
            return -3;
        }
    }
    return 0;
}

static int update_thread(SceSize args, void *argp) {
    int range = -1;
    int clen = -1, total = -1;
    int rc;
    int vrc;
    int attempt;
    (void)args;
    (void)argp;

    g_up.state = UPD_DOWNLOADING;
    g_cancel = 0;
    http_set_abort(0);

    /*
     * Always full re-download. WiFi stalls mid-file used to hang forever in
     * blocking recv; http_get_file_range now idle-times out — retry a few times.
     */
    for (attempt = 0; attempt < 3; attempt++) {
        storage_temp_discard(g_tmp);
        {
            int fd = storage_temp_create(g_tmp);
            if (fd < 0) {
                strncpy(g_up.error, "Not enough storage", sizeof(g_up.error) - 1);
                g_up.state = UPD_ERROR;
                g_thid = -1;
                return 0;
            }
            storage_close(fd);
        }
        range = -1;
        g_up.bytes = 0;
        g_up.percent = 0;
        g_up.state = UPD_DOWNLOADING;
        if (attempt > 0) {
            sceKernelDelayThread(400000);
        }

        rc = http_get_file_range(
            g_host, g_port, "/api/client/EBOOT.PBP", g_tmp, range,
            progress_cb, NULL, &clen, &total
        );
        if (g_cancel || rc == HTTP_ABORTED) {
            g_up.state = UPD_INCOMPLETE;
            g_thid = -1;
            return 0;
        }
        if (rc != HTTP_OK) {
            const char *why = http_last_fail_reason();
            strncpy(g_up.error, why && why[0] ? why : "download failed", sizeof(g_up.error) - 1);
            if (strcmp(g_up.error, "file") == 0) {
                strncpy(g_up.error, "Not enough storage", sizeof(g_up.error) - 1);
                storage_temp_discard(g_tmp);
                g_up.state = UPD_ERROR;
                g_thid = -1;
                return 0;
            }
            if (strcmp(g_up.error, "recv_timeout") == 0 ||
                strcmp(g_up.error, "trunc") == 0 ||
                strcmp(g_up.error, "tcp") == 0 ||
                strcmp(g_up.error, "recv") == 0) {
                storage_temp_discard(g_tmp);
                if (attempt < 2) {
                    continue; /* retry transient WiFi / stall */
                }
                if (strcmp(g_up.error, "recv_timeout") == 0) {
                    strncpy(g_up.error, "WiFi stall — retry", sizeof(g_up.error) - 1);
                } else if (strcmp(g_up.error, "trunc") == 0) {
                    strncpy(g_up.error, "Download cut off — retry", sizeof(g_up.error) - 1);
                }
            }
            g_up.state = UPD_ERROR;
            g_thid = -1;
            return 0;
        }

        g_up.state = UPD_VERIFYING;
        vrc = verify_eboot(g_tmp);
        if (vrc == 0) {
            break;
        }
        {
            char d[64];
            snprintf(d, sizeof(d), "{\"vrc\":%d,\"sz\":%d,\"try\":%d}", vrc, storage_size(g_tmp), attempt);
            dbg_log("U", "updater.c:verify", "fail", d);
        }
        storage_temp_discard(g_tmp);
        if (attempt < 2) {
            continue;
        }
        if (vrc == -3) {
            snprintf(g_up.error, sizeof(g_up.error), "SHA mismatch — retry");
        } else if (vrc == -4) {
            snprintf(g_up.error, sizeof(g_up.error), "Size mismatch — retry");
        } else {
            snprintf(g_up.error, sizeof(g_up.error), "Verify failed (%d)", vrc);
        }
        g_up.state = UPD_ERROR;
        g_thid = -1;
        return 0;
    }

    if (storage_exists(g_final)) {
        storage_remove(g_bak);
        storage_rename(g_final, g_bak);
    }
    if (storage_temp_finalize(g_tmp, g_final) < 0) {
        if (storage_exists(g_bak)) {
            storage_rename(g_bak, g_final);
        }
        strncpy(g_up.error, "Install failed", sizeof(g_up.error) - 1);
        g_up.state = UPD_ERROR;
        g_thid = -1;
        return 0;
    }

    storage_remove(g_bak);
    storage_temp_discard(g_tmp);
    paths_request_purge_update_companion();

    g_up.percent = 100;
    g_up.state = UPD_COMPLETE;
    g_up.app_missing = 0;
    g_up.local_code = g_up.remote_code;
    g_installed_code = g_up.remote_code;
    strncpy(g_up.current_version, g_up.remote_version, sizeof(g_up.current_version) - 1);
    dbg_log("U", "updater.c:done", "complete", "{}");
    g_thid = -1;
    return 0;
}

int updater_start_download(const char *host, int port) {
    if (g_thid >= 0) {
        SceKernelThreadInfo info;
        memset(&info, 0, sizeof(info));
        info.size = sizeof(info);
        /* After a hung/killed download, thid can be stale and block all retries. */
        if (sceKernelReferThreadStatus(g_thid, &info) < 0) {
            g_thid = -1;
        } else {
            return -1;
        }
    }
    /* Always refresh manifest so sha256/size match the file we will fetch. */
    if (updater_check(host, port) <= 0) {
        return -1;
    }
    strncpy(g_host, host ? host : "", sizeof(g_host) - 1);
    g_port = port;
    g_cancel = 0;
    http_set_abort(0);
    g_thid = sceKernelCreateThread("upd", update_thread, 0x18, 0x12000, 0, NULL);
    if (g_thid < 0) {
        return -1;
    }
    sceKernelStartThread(g_thid, 0, NULL);
    return 0;
}

void updater_cancel(void) {
    g_cancel = 1;
    http_set_abort(1);
}

int updater_resume(const char *host, int port) {
    return updater_start_download(host, port);
}

int updater_delete_incomplete(void) {
    updater_cancel();
    if (g_thid >= 0) {
        SceUInt to = 2000000;
        sceKernelWaitThreadEnd(g_thid, &to);
        g_thid = -1;
    }
    storage_temp_discard(g_tmp);
    g_up.state = UPD_IDLE;
    return 0;
}
