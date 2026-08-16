#include "screenshot.h"

#include <png.h>
#include <pspiofilemgr.h>
#include <stdlib.h>
#include <string.h>

#pragma pack(push, 1)
typedef struct {
    unsigned short type;
    unsigned int size;
    unsigned short reserved1;
    unsigned short reserved2;
    unsigned int off_bits;
} BmpFileHeader;

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
} BmpInfoHeader;
#pragma pack(pop)

int screenshot_save_bmp(const char *path, const u32 *fb, int buf_width, int width, int height) {
    BmpFileHeader fh;
    BmpInfoHeader ih;
    SceUID fd;
    int y;
    int row_bytes;
    int pad;
    unsigned char padbytes[4] = {0, 0, 0, 0};
    unsigned char row[480 * 3];

    if (!path || !fb || width <= 0 || height <= 0 || width > 480) {
        return -1;
    }

    row_bytes = width * 3;
    pad = (4 - (row_bytes % 4)) % 4;

    memset(&fh, 0, sizeof(fh));
    memset(&ih, 0, sizeof(ih));
    fh.type = 0x4D42; /* 'BM' */
    fh.off_bits = sizeof(fh) + sizeof(ih);
    fh.size = fh.off_bits + (unsigned)((row_bytes + pad) * height);

    ih.size = sizeof(ih);
    ih.width = width;
    ih.height = height;
    ih.planes = 1;
    ih.bit_count = 24;
    ih.compression = 0;
    ih.size_image = (unsigned)((row_bytes + pad) * height);

    fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0) {
        return -2;
    }
    sceIoWrite(fd, &fh, sizeof(fh));
    sceIoWrite(fd, &ih, sizeof(ih));

    /* BMP is bottom-up */
    for (y = height - 1; y >= 0; y--) {
        int x;
        const u32 *src = fb + y * buf_width;
        for (x = 0; x < width; x++) {
            u32 p = src[x];
            /* PSP 8888 AABBGGRR -> BMP B,G,R */
            row[x * 3 + 0] = (unsigned char)((p >> 16) & 0xFF);
            row[x * 3 + 1] = (unsigned char)((p >> 8) & 0xFF);
            row[x * 3 + 2] = (unsigned char)(p & 0xFF);
        }
        sceIoWrite(fd, row, (unsigned)row_bytes);
        if (pad) {
            sceIoWrite(fd, padbytes, (unsigned)pad);
        }
    }

    sceIoClose(fd);
    return 0;
}

static void png_write_sce(png_structp png_ptr, png_bytep data, png_size_t length) {
    SceUID fd = (SceUID)(int)png_get_io_ptr(png_ptr);
    if (sceIoWrite(fd, data, (int)length) != (int)length) {
        png_error(png_ptr, "write");
    }
}

static void png_flush_sce(png_structp png_ptr) {
    (void)png_ptr;
}

int screenshot_save_png(const char *path, const u32 *fb, int buf_width, int width, int height) {
    SceUID fd;
    png_structp png_ptr;
    png_infop info_ptr;
    png_bytep row = NULL;
    int y;

    if (!path || !fb || width <= 0 || height <= 0 || width > 480) {
        return -1;
    }

    fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0) {
        return -2;
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        sceIoClose(fd);
        return -3;
    }
    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, NULL);
        sceIoClose(fd);
        return -3;
    }
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        if (row) {
            free(row);
        }
        sceIoClose(fd);
        return -4;
    }

    png_set_write_fn(png_ptr, (png_voidp)(int)fd, png_write_sce, png_flush_sce);
    png_set_IHDR(
        png_ptr,
        info_ptr,
        (png_uint_32)width,
        (png_uint_32)height,
        8,
        PNG_COLOR_TYPE_RGB,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
    );
    png_write_info(png_ptr, info_ptr);

    row = (png_bytep)malloc((size_t)width * 3u);
    if (!row) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        sceIoClose(fd);
        return -5;
    }
    for (y = 0; y < height; y++) {
        int x;
        const u32 *src = fb + y * buf_width;
        for (x = 0; x < width; x++) {
            u32 p = src[x];
            row[x * 3 + 0] = (png_byte)(p & 0xFF);
            row[x * 3 + 1] = (png_byte)((p >> 8) & 0xFF);
            row[x * 3 + 2] = (png_byte)((p >> 16) & 0xFF);
        }
        png_write_row(png_ptr, row);
    }
    png_write_end(png_ptr, NULL);
    free(row);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    sceIoClose(fd);
    return 0;
}
