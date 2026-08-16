#ifndef UI_GPU_H
#define UI_GPU_H

#include <psptypes.h>

#define UI_GPU_ATLAS_W 256
#define UI_GPU_ATLAS_H 256
#define UI_GPU_FONT_W 512
#define UI_GPU_FONT_H 256
#define UI_GPU_FONT_CELL 32
#define UI_GPU_FONT_COLS 16
#define UI_GPU_LABELS_W 512
#define UI_GPU_LABELS_H 128

/* Atlas UV regions (pixels). */
enum {
    UI_ATLAS_FALLBACK = 0, /* 0,0 96x96 */
    UI_ATLAS_CARD,         /* 96,0 128x64 */
    UI_ATLAS_ICON_PREV,    /* 0,96 32x32 */
    UI_ATLAS_ICON_PLAY,
    UI_ATLAS_ICON_PAUSE,
    UI_ATLAS_ICON_STOP,
    UI_ATLAS_ICON_NEXT,
    UI_ATLAS_MINI_BAR,     /* 0,160 256x32 */
    UI_ATLAS_PILL,         /* 0,192 64x24 */
    UI_ATLAS_PLAY_BIG,     /* 160,192 64x64 green play */
    UI_ATLAS_COUNT
};

/* Pre-baked AA chrome labels (labels.rgba). */
enum {
    UI_LABEL_NOW_PLAYING = 0,
    UI_LABEL_PAUSED,
    UI_LABEL_SHUF,
    UI_LABEL_RPT,
    UI_LABEL_MUSIC,
    UI_LABEL_UP_NEXT,
    UI_LABEL_LOADING,
    UI_LABEL_NOW_PLAYING_GREEN,
    UI_LABEL_SAVED,
    UI_LABEL_COUNT
};

typedef struct {
    void *data; /* 16-byte aligned ABGR8888, power-of-two dims */
    int width;
    int height;
    int stride; /* pixels per row (tbw) */
    int ready;
} UiGpuTex;

void ui_gpu_init(void);
void ui_gpu_shutdown(void);

/* Re-init after utility/net dialogs steal GU state. */
void ui_gpu_reset(void);

UiGpuTex *ui_gpu_atlas(void);
UiGpuTex *ui_gpu_font(void);
UiGpuTex *ui_gpu_labels(void);

/* Upload / replace a cover-sized texture (96x96 stored in 128x128). */
int ui_gpu_upload_cover(UiGpuTex *tex, const u32 *pixels_96, int track_id);
void ui_gpu_free_tex(UiGpuTex *tex);

/*
 * Compose one frame:
 *  1) DMA-copy the software backbuffer into VRAM (sceGuCopyImage)
 *  2) Draw optional cover / atlas overlays with linear-filtered textures
 */
void ui_gpu_present(
    u32 *soft_buf,
    int buf_stride,
    u32 *vram_dst,
    const UiGpuTex *cover,
    int cover_x,
    int cover_y,
    int cover_w,
    int cover_h
);

/* CPU-side atlas blit into soft buffer (when GU overlays are not used). */
void ui_gpu_blit_atlas_cpu(u32 *dst, int dst_stride, int region, int x, int y, int w, int h, u32 tint);
void ui_gpu_blit_font_cpu(u32 *dst, int dst_stride, int x, int y, u32 color, const char *s, int size);
void ui_gpu_blit_label_cpu(u32 *dst, int dst_stride, int label, int x, int y);

#endif
