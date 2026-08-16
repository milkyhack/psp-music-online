#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "ui_image.h"

#include "http.h"
#include "paths.h"
#include "ui_gfx.h"

#include <jpeglib.h>
#include <png.h>
#include <pspiofilemgr.h>

static UiCover g_cache[UI_COVER_CACHE];
static int g_active = -1;
static unsigned g_tick;
static int g_present_x = 0;
static int g_present_y = 0;
static int g_present_w = 0;
static int g_present_h = 0;

/* Brief negative cache so failed covers don't hammer HTTP every frame. */
#define UI_COVER_FAIL_MAX 32
#define UI_COVER_FAIL_COOLDOWN 45 /* ~0.75s — don't lock out covers for long */
static struct {
    int track_id;
    unsigned marked_at;
} g_fail[UI_COVER_FAIL_MAX];
static unsigned g_fail_clock;

static int cover_fail_blocked(int track_id) {
    int i;
    g_fail_clock++;
    for (i = 0; i < UI_COVER_FAIL_MAX; i++) {
        if (g_fail[i].track_id == track_id) {
            if ((g_fail_clock - g_fail[i].marked_at) < UI_COVER_FAIL_COOLDOWN) {
                return 1;
            }
            g_fail[i].track_id = 0;
            return 0;
        }
    }
    return 0;
}

static void cover_fail_mark(int track_id) {
    int i;
    int slot = 0;
    unsigned oldest = g_fail[0].marked_at;
    for (i = 0; i < UI_COVER_FAIL_MAX; i++) {
        if (g_fail[i].track_id == 0 || g_fail[i].track_id == track_id) {
            slot = i;
            break;
        }
        if (g_fail[i].marked_at < oldest) {
            oldest = g_fail[i].marked_at;
            slot = i;
        }
    }
    g_fail[slot].track_id = track_id;
    g_fail[slot].marked_at = g_fail_clock;
}

void ui_image_init(void) {
    int i;
    memset(g_cache, 0, sizeof(g_cache));
    memset(g_fail, 0, sizeof(g_fail));
    g_active = -1;
    g_tick = 1;
    g_fail_clock = 1;
    g_present_x = 0;
    g_present_y = 0;
    g_present_w = 0;
    g_present_h = 0;
    for (i = 0; i < UI_COVER_CACHE; i++) {
        memset(&g_cache[i].gpu, 0, sizeof(g_cache[i].gpu));
    }
}

void ui_image_clear_cover(void) {
    g_active = -1;
    g_present_w = 0;
    g_present_h = 0;
}

void ui_image_clear_present(void) {
    g_present_w = 0;
    g_present_h = 0;
}

const UiCover *ui_image_cover(void) {
    if (g_active < 0 || g_active >= UI_COVER_CACHE) {
        return NULL;
    }
    if (!g_cache[g_active].ready) {
        return NULL;
    }
    return &g_cache[g_active];
}

const UiCover *ui_image_cover_for(int track_id) {
    int i;
    if (track_id == 0) {
        return NULL;
    }
    for (i = 0; i < UI_COVER_CACHE; i++) {
        if (g_cache[i].ready && g_cache[i].track_id == track_id) {
            g_cache[i].tick = ++g_tick;
            return &g_cache[i];
        }
    }
    return NULL;
}

const UiGpuTex *ui_image_cover_gpu(void) {
    const UiCover *c = ui_image_cover();
    if (!c || !c->gpu.ready) {
        return NULL;
    }
    return &c->gpu;
}

void ui_image_set_present_rect(int x, int y, int w, int h) {
    g_present_x = x;
    g_present_y = y;
    g_present_w = w;
    g_present_h = h;
}

void ui_image_get_present_rect(int *x, int *y, int *w, int *h) {
    if (x) {
        *x = g_present_x;
    }
    if (y) {
        *y = g_present_y;
    }
    if (w) {
        *w = g_present_w;
    }
    if (h) {
        *h = g_present_h;
    }
}

static int lru_slot(void) {
    int i;
    int best = 0;
    unsigned oldest = g_cache[0].tick;
    for (i = 0; i < UI_COVER_CACHE; i++) {
        if (!g_cache[i].ready) {
            return i;
        }
        if (g_cache[i].tick < oldest) {
            oldest = g_cache[i].tick;
            best = i;
        }
    }
    return best;
}

static void png_read_sce(png_structp png_ptr, png_bytep data, png_size_t length) {
    SceUID fd = (SceUID)(int)png_get_io_ptr(png_ptr);
    int n = sceIoRead(fd, data, (int)length);
    if (n != (int)length) {
        png_error(png_ptr, "read");
    }
}

static int decode_png_file(const char *path, UiCover *out, int track_id) {
    SceUID fd;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep *rows = NULL;
    png_byte header[8];
    int width;
    int height;
    int y;
    int ok = -1;

    fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
    if (fd < 0) {
        return -1;
    }
    if (sceIoRead(fd, header, 8) != 8 || png_sig_cmp(header, 0, 8) != 0) {
        sceIoClose(fd);
        return -1;
    }

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        sceIoClose(fd);
        return -1;
    }
    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        sceIoClose(fd);
        return -1;
    }
    if (setjmp(png_jmpbuf(png_ptr))) {
        if (rows) {
            free(rows);
        }
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        sceIoClose(fd);
        return -1;
    }

    png_set_read_fn(png_ptr, (png_voidp)(int)fd, png_read_sce);
    png_set_sig_bytes(png_ptr, 8);
    png_read_info(png_ptr, info_ptr);
    width = (int)png_get_image_width(png_ptr, info_ptr);
    height = (int)png_get_image_height(png_ptr, info_ptr);
    if (width <= 0 || height <= 0 || width > 256 || height > 256) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        sceIoClose(fd);
        return -1;
    }

    {
        int color_type = png_get_color_type(png_ptr, info_ptr);
        int bit_depth = png_get_bit_depth(png_ptr, info_ptr);
        if (bit_depth == 16) {
            png_set_strip_16(png_ptr);
        }
        if (color_type == PNG_COLOR_TYPE_PALETTE) {
            png_set_palette_to_rgb(png_ptr);
        }
        if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
            png_set_expand_gray_1_2_4_to_8(png_ptr);
        }
        if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
            png_set_tRNS_to_alpha(png_ptr);
        }
        if (color_type == PNG_COLOR_TYPE_RGB ||
            color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_PALETTE) {
            png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
        }
        if (color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
            png_set_gray_to_rgb(png_ptr);
        }
        png_read_update_info(png_ptr, info_ptr);
    }

    rows = (png_bytep *)malloc(sizeof(png_bytep) * (size_t)height);
    if (!rows) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        sceIoClose(fd);
        return -1;
    }
    for (y = 0; y < height; y++) {
        rows[y] = (png_bytep)malloc(png_get_rowbytes(png_ptr, info_ptr));
        if (!rows[y]) {
            int k;
            for (k = 0; k < y; k++) {
                free(rows[k]);
            }
            free(rows);
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
            sceIoClose(fd);
            return -1;
        }
    }
    png_read_image(png_ptr, rows);

    memset(out->pixels, 0, sizeof(out->pixels));
    for (y = 0; y < UI_COVER_SIZE; y++) {
        int sy = (y * height) / UI_COVER_SIZE;
        int x;
        png_bytep row = rows[sy];
        for (x = 0; x < UI_COVER_SIZE; x++) {
            int sx = (x * width) / UI_COVER_SIZE;
            png_bytep p = row + sx * 4;
            out->pixels[y * UI_COVER_SIZE + x] =
                0xFF000000u | ((u32)p[2] << 16) | ((u32)p[1] << 8) | (u32)p[0];
        }
    }

    for (y = 0; y < height; y++) {
        free(rows[y]);
    }
    free(rows);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    sceIoClose(fd);

    out->width = UI_COVER_SIZE;
    out->height = UI_COVER_SIZE;
    out->track_id = track_id;
    out->ready = 1;
    out->tick = ++g_tick;
    ui_gpu_upload_cover(&out->gpu, out->pixels, track_id);
    ok = 0;
    return ok;
}

/* Keep BMP reader as fallback for older caches. */
typedef struct {
    unsigned short type;
    unsigned int size;
    unsigned short reserved1;
    unsigned short reserved2;
    unsigned int off_bits;
} __attribute__((packed)) BmpFileHeader;

typedef struct {
    unsigned int size;
    int width;
    int height;
    unsigned short planes;
    unsigned short bit_count;
    unsigned int compression;
    unsigned int size_image;
    int x_pels;
    int y_pels;
    unsigned int clr_used;
    unsigned int clr_important;
} __attribute__((packed)) BmpInfoHeader;

static int decode_bmp_file(const char *path, UiCover *out, int track_id) {
    BmpFileHeader fh;
    BmpInfoHeader ih;
    SceUID fd;
    int y;
    unsigned char row[UI_COVER_SIZE * 3];

    fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
    if (fd < 0) {
        return -1;
    }
    if (sceIoRead(fd, &fh, sizeof(fh)) != sizeof(fh) ||
        sceIoRead(fd, &ih, sizeof(ih)) != sizeof(ih)) {
        sceIoClose(fd);
        return -1;
    }
    if (fh.type != 0x4D42 || ih.bit_count != 24 || ih.compression != 0 ||
        ih.width != UI_COVER_SIZE || ih.height != UI_COVER_SIZE) {
        sceIoClose(fd);
        return -1;
    }
    if (sceIoLseek(fd, (int)fh.off_bits, PSP_SEEK_SET) < 0) {
        sceIoClose(fd);
        return -1;
    }
    for (y = UI_COVER_SIZE - 1; y >= 0; y--) {
        int x;
        if (sceIoRead(fd, row, UI_COVER_SIZE * 3) != UI_COVER_SIZE * 3) {
            sceIoClose(fd);
            return -1;
        }
        for (x = 0; x < UI_COVER_SIZE; x++) {
            unsigned char b = row[x * 3 + 0];
            unsigned char g = row[x * 3 + 1];
            unsigned char r = row[x * 3 + 2];
            out->pixels[y * UI_COVER_SIZE + x] =
                0xFF000000u | ((u32)b << 16) | ((u32)g << 8) | (u32)r;
        }
    }
    sceIoClose(fd);
    out->width = UI_COVER_SIZE;
    out->height = UI_COVER_SIZE;
    out->track_id = track_id;
    out->ready = 1;
    out->tick = ++g_tick;
    ui_gpu_upload_cover(&out->gpu, out->pixels, track_id);
    return 0;
}

static int decode_png_mem(const unsigned char *data, int len, UiCover *out, int track_id);
static int decode_jpeg_mem(const unsigned char *data, int len, UiCover *out, int track_id);

int ui_image_load_cover(const char *host, int port, int track_id) {
    return ui_image_load_cover_ex(host, port, track_id, 1, 0);
}

/* persist_ms ignored: covers never written to MS (offline music only). */
int ui_image_load_cover_ex(
    const char *host,
    int port,
    int track_id,
    int make_active,
    int persist_ms
) {
    char path_png[280];
    char api[112];
    int slot;
    int i;
    int is_album = 0;
    int abs_id = track_id;
    char *body = NULL;
    int body_len = 0;

    if (track_id < 0) {
        is_album = 1;
        abs_id = -track_id;
    }
    if (abs_id <= 0) {
        if (make_active) {
            g_active = -1;
            g_present_w = 0;
            g_present_h = 0;
        }
        return -1;
    }
    for (i = 0; i < UI_COVER_CACHE; i++) {
        if (g_cache[i].ready && g_cache[i].track_id == track_id) {
            g_cache[i].tick = ++g_tick;
            if (make_active) {
                g_active = i;
            }
            return 0;
        }
    }
    if (cover_fail_blocked(track_id)) {
        return -1;
    }

    /* Decode into a temp slot first — never evict cache on failed HTTP. */
    {
        UiCover tmp;
        int ok = 0;
        memset(&tmp, 0, sizeof(tmp));

        if (is_album) {
            snprintf(path_png, sizeof(path_png), "%s/data/covers/a%d.png", paths_base(), abs_id);
        } else {
            snprintf(path_png, sizeof(path_png), "%s/data/covers/%d.png", paths_base(), abs_id);
        }
        {
            SceUID fd = sceIoOpen(path_png, PSP_O_RDONLY, 0777);
            if (fd >= 0) {
                sceIoClose(fd);
                if (decode_png_file(path_png, &tmp, track_id) == 0) {
                    ok = 1;
                }
            }
        }

        if (!ok) {
            if (is_album) {
                snprintf(api, sizeof(api), "/api/covers/album/%d/thumbnail?size=%d&format=png", abs_id, UI_COVER_SIZE);
            } else {
                snprintf(api, sizeof(api), "/api/covers/%d/thumbnail?size=%d&format=png", abs_id, UI_COVER_SIZE);
            }
            /* Online: RAM only — never write covers to Memory Stick. */
            if (http_get(host, port, api, &body, &body_len) == HTTP_OK && body && body_len > 8) {
                const unsigned char *raw = (const unsigned char *)body;
                if (raw[0] == 0x89 && decode_png_mem(raw, body_len, &tmp, track_id) == 0) {
                    ok = 1;
                } else if (raw[0] == 0xFF && decode_jpeg_mem(raw, body_len, &tmp, track_id) == 0) {
                    ok = 1;
                }
                (void)persist_ms;
                free(body);
                body = NULL;
            } else {
                free(body);
                body = NULL;
            }
        }
        /* Thumbnail failed — try full JPEG cover (same routes web admin uses). */
        if (!ok) {
            if (is_album) {
                snprintf(api, sizeof(api), "/api/covers/album/%d", abs_id);
            } else {
                snprintf(api, sizeof(api), "/api/covers/%d", abs_id);
            }
            if (http_get(host, port, api, &body, &body_len) == HTTP_OK && body && body_len > 8) {
                const unsigned char *raw = (const unsigned char *)body;
                if (raw[0] == 0xFF && decode_jpeg_mem(raw, body_len, &tmp, track_id) == 0) {
                    ok = 1;
                } else if (raw[0] == 0x89 && decode_png_mem(raw, body_len, &tmp, track_id) == 0) {
                    ok = 1;
                }
                free(body);
                body = NULL;
            } else {
                free(body);
                body = NULL;
            }
        }

        if (!ok) {
            ui_gpu_free_tex(&tmp.gpu);
            cover_fail_mark(track_id);
            if (make_active) {
                g_active = -1;
                g_present_w = 0;
                g_present_h = 0;
            }
            return -1;
        }

        slot = lru_slot();
        if (!make_active && g_active == slot && g_cache[slot].ready) {
            int alt = (slot + 1) % UI_COVER_CACHE;
            int best = alt;
            unsigned oldest = g_cache[alt].tick;
            for (i = 0; i < UI_COVER_CACHE; i++) {
                if (i == g_active) {
                    continue;
                }
                if (!g_cache[i].ready) {
                    best = i;
                    break;
                }
                if (g_cache[i].tick < oldest) {
                    oldest = g_cache[i].tick;
                    best = i;
                }
            }
            slot = best;
        }
        if (g_cache[slot].ready) {
            ui_gpu_free_tex(&g_cache[slot].gpu);
        }
        g_cache[slot] = tmp;
        if (make_active) {
            g_active = slot;
        }
        return 0;
    }
}

typedef struct {
    const unsigned char *data;
    int len;
    int pos;
} PngMemReader;

static void png_mem_read(png_structp png_ptr, png_bytep out, png_size_t need) {
    PngMemReader *r = (PngMemReader *)png_get_io_ptr(png_ptr);
    int left;
    if (!r) {
        png_error(png_ptr, "io");
        return;
    }
    left = r->len - r->pos;
    if ((int)need > left) {
        png_error(png_ptr, "eof");
        return;
    }
    memcpy(out, r->data + r->pos, need);
    r->pos += (int)need;
}

static int decode_png_mem(const unsigned char *data, int len, UiCover *out, int track_id) {
    png_structp png_ptr;
    png_infop info_ptr;
    PngMemReader reader;
    png_uint_32 width;
    png_uint_32 height;
    int bit_depth;
    int color_type;
    png_bytep *rows = NULL;
    png_uint_32 y;

    if (!data || len < 8 || !out) {
        return -1;
    }
    if (png_sig_cmp((png_bytep)data, 0, 8) != 0) {
        return -1;
    }

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        return -1;
    }
    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return -1;
    }
    if (setjmp(png_jmpbuf(png_ptr))) {
        if (rows) {
            for (y = 0; y < height; y++) {
                free(rows[y]);
            }
            free(rows);
        }
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return -1;
    }
    reader.data = data;
    reader.len = len;
    reader.pos = 0;
    png_set_read_fn(png_ptr, &reader, png_mem_read);
    png_read_info(png_ptr, info_ptr);
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    }
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png_ptr);
    }
    if (bit_depth == 16) {
        png_set_strip_16(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png_ptr);
    }
    png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
    png_read_update_info(png_ptr, info_ptr);
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

    rows = (png_bytep *)malloc(sizeof(png_bytep) * height);
    if (!rows) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return -1;
    }
    for (y = 0; y < height; y++) {
        rows[y] = (png_bytep)malloc(png_get_rowbytes(png_ptr, info_ptr));
        if (!rows[y]) {
            png_uint_32 k;
            for (k = 0; k < y; k++) {
                free(rows[k]);
            }
            free(rows);
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
            return -1;
        }
    }
    png_read_image(png_ptr, rows);
    memset(out->pixels, 0, sizeof(out->pixels));
    for (y = 0; y < UI_COVER_SIZE; y++) {
        int sy = (int)((y * height) / UI_COVER_SIZE);
        int x;
        png_bytep row = rows[sy];
        for (x = 0; x < UI_COVER_SIZE; x++) {
            int sx = (int)((x * width) / UI_COVER_SIZE);
            png_bytep p = row + sx * 4;
            out->pixels[y * UI_COVER_SIZE + x] =
                0xFF000000u | ((u32)p[2] << 16) | ((u32)p[1] << 8) | (u32)p[0];
        }
    }
    for (y = 0; y < height; y++) {
        free(rows[y]);
    }
    free(rows);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    out->width = UI_COVER_SIZE;
    out->height = UI_COVER_SIZE;
    out->track_id = track_id;
    out->ready = 1;
    out->tick = ++g_tick;
    ui_gpu_upload_cover(&out->gpu, out->pixels, track_id);
    return 0;
}

struct JpegErr {
    struct jpeg_error_mgr pub;
    jmp_buf jb;
};

static void jpeg_err_exit(j_common_ptr cinfo) {
    struct JpegErr *err = (struct JpegErr *)cinfo->err;
    longjmp(err->jb, 1);
}

static int decode_jpeg_mem(const unsigned char *data, int len, UiCover *out, int track_id) {
    struct jpeg_decompress_struct cinfo;
    struct JpegErr jerr;
    JSAMPARRAY buffer;
    int row_stride;
    int y;
    unsigned char *rgb = NULL;
    int width;
    int height;

    if (!data || len < 4 || !out) {
        return -1;
    }
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpeg_err_exit;
    if (setjmp(jerr.jb)) {
        jpeg_destroy_decompress(&cinfo);
        free(rgb);
        return -1;
    }
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, (unsigned char *)data, (unsigned long)len);
    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&cinfo);
        return -1;
    }
    /* Downscale large embedded/full covers so PSP RAM stays safe. */
    cinfo.scale_num = 1;
    cinfo.scale_denom = 1;
    while ((cinfo.image_width / cinfo.scale_denom > 512 ||
            cinfo.image_height / cinfo.scale_denom > 512) &&
           cinfo.scale_denom < 8) {
        cinfo.scale_denom <<= 1;
    }
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);
    width = (int)cinfo.output_width;
    height = (int)cinfo.output_height;
    row_stride = width * (int)cinfo.output_components;
    rgb = (unsigned char *)malloc((size_t)row_stride * (size_t)height);
    if (!rgb) {
        jpeg_destroy_decompress(&cinfo);
        return -1;
    }
    buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, (JDIMENSION)row_stride, 1);
    y = 0;
    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        memcpy(rgb + y * row_stride, buffer[0], (size_t)row_stride);
        y++;
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    memset(out->pixels, 0, sizeof(out->pixels));
    for (y = 0; y < UI_COVER_SIZE; y++) {
        int sy = (y * height) / UI_COVER_SIZE;
        int x;
        if (sy >= height) {
            sy = height - 1;
        }
        for (x = 0; x < UI_COVER_SIZE; x++) {
            int sx = (x * width) / UI_COVER_SIZE;
            unsigned char *p;
            if (sx >= width) {
                sx = width - 1;
            }
            p = rgb + sy * row_stride + sx * 3;
            out->pixels[y * UI_COVER_SIZE + x] =
                0xFF000000u | ((u32)p[2] << 16) | ((u32)p[1] << 8) | (u32)p[0];
        }
    }
    free(rgb);
    out->width = UI_COVER_SIZE;
    out->height = UI_COVER_SIZE;
    out->track_id = track_id;
    out->ready = 1;
    out->tick = ++g_tick;
    ui_gpu_upload_cover(&out->gpu, out->pixels, track_id);
    return 0;
}

int ui_image_load_cover_mem(
    int track_id,
    const unsigned char *data,
    int len,
    int is_jpeg,
    int make_active,
    int write_once_cache
) {
    int slot;
    int i;

    if (!data || len <= 0 || track_id == 0) {
        return -1;
    }
    for (i = 0; i < UI_COVER_CACHE; i++) {
        if (g_cache[i].ready && g_cache[i].track_id == track_id) {
            g_cache[i].tick = ++g_tick;
            if (make_active) {
                g_active = i;
            }
            return 0;
        }
    }
    slot = lru_slot();
    if (g_cache[slot].ready) {
        ui_gpu_free_tex(&g_cache[slot].gpu);
        memset(&g_cache[slot], 0, sizeof(g_cache[slot]));
    }
    if (is_jpeg) {
        if (decode_jpeg_mem(data, len, &g_cache[slot], track_id) != 0) {
            return -1;
        }
    } else {
        if (decode_png_mem(data, len, &g_cache[slot], track_id) != 0) {
            return -1;
        }
    }
    (void)write_once_cache;
    if (make_active) {
        g_active = slot;
    }
    return 0;
}

void ui_image_draw_cover(int x, int y, int w, int h, const UiCover *cover) {
    ui_image_draw_cover_ex(x, y, w, h, cover, 0);
}

void ui_image_draw_cover_ex(int x, int y, int w, int h, const UiCover *cover, int gpu_overlay) {
    int dy;
    u32 *buf = ui_gfx_buffer();
    if (!cover || !cover->ready || w <= 0 || h <= 0 || !buf) {
        return;
    }
    /* Only Now Playing (large) covers drive the GPU overlay — list/mini stay CPU-only. */
    if (gpu_overlay) {
        int i;
        for (i = 0; i < UI_COVER_CACHE; i++) {
            if (g_cache[i].ready && g_cache[i].track_id == cover->track_id) {
                g_active = i;
                break;
            }
        }
        ui_image_set_present_rect(x, y, w, h);
        /* Skip soft blit when GPU will draw the same rect — avoids double-filter mush. */
        return;
    }
    /* Sharp nearest-neighbor for list/mini thumbs. */
    for (dy = 0; dy < h; dy++) {
        int dx;
        int sy = (dy * cover->height) / h;
        if (sy >= cover->height) {
            sy = cover->height - 1;
        }
        for (dx = 0; dx < w; dx++) {
            int sx = (dx * cover->width) / w;
            int px = x + dx;
            int py = y + dy;
            if (sx >= cover->width) {
                sx = cover->width - 1;
            }
            if (px < 0 || py < 0 || px >= UI_GFX_W || py >= UI_GFX_H) {
                continue;
            }
            buf[py * UI_GFX_STRIDE + px] = cover->pixels[sy * cover->width + sx];
        }
    }
}
