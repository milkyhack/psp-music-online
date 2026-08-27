#ifndef UI_IMAGE_H
#define UI_IMAGE_H

#include <psptypes.h>
#include "ui_gpu.h"

/* 160px soft buffer; GPU pads into 256² POT. Was 128→144 upscale mush. */
#define UI_COVER_SIZE 160
#define UI_COVER_CACHE 8
#define UI_COVER_GPU_POT 256

typedef struct {
    u32 pixels[UI_COVER_SIZE * UI_COVER_SIZE];
    UiGpuTex gpu;
    int width;
    int height;
    int track_id;
    int ready;
    unsigned tick;
} UiCover;

void ui_image_init(void);
void ui_image_clear_cover(void);
void ui_image_clear_present(void);
const UiCover *ui_image_cover(void);
const UiCover *ui_image_cover_for(int track_id);

/*
 * Loads a UI_COVER_SIZE cover. Prefers disk cache under data/covers/, then
 * downloads PNG thumbnail from the server. LRU keeps several covers in RAM.
 * make_active=1 sets this cover as the Now Playing GPU overlay source.
 */
int ui_image_load_cover(const char *host, int port, int track_id);
/* persist_ms ignored — covers stay in RAM only (MS wear). */
int ui_image_load_cover_ex(
    const char *host,
    int port,
    int track_id,
    int make_active,
    int persist_ms
);
/* Decode JPEG/PNG cover bytes in RAM (FLAC PICTURE). Write-once cache optional. */
int ui_image_load_cover_mem(
    int track_id,
    const unsigned char *data,
    int len,
    int is_jpeg,
    int make_active,
    int write_once_cache
);

/* Soft/CPU blit. gpu_overlay=1 also registers the GU linear overlay (NP only). */
void ui_image_draw_cover(int x, int y, int w, int h, const UiCover *cover);
void ui_image_draw_cover_ex(int x, int y, int w, int h, const UiCover *cover, int gpu_overlay);

/* GPU texture for the active NP cover (may be NULL). */
const UiGpuTex *ui_image_cover_gpu(void);
void ui_image_set_present_rect(int x, int y, int w, int h);
void ui_image_get_present_rect(int *x, int *y, int *w, int *h);

#endif
