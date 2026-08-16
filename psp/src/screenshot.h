#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <psptypes.h>

/* Save 480x272 8888 framebuffer. Returns 0 on success. */
int screenshot_save_bmp(const char *path, const u32 *fb, int buf_width, int width, int height);
int screenshot_save_png(const char *path, const u32 *fb, int buf_width, int width, int height);

#endif
