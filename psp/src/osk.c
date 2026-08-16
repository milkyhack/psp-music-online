#include "osk.h"

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <psputility.h>
#include <psputility_osk.h>
#include <psputility_sysparam.h>
#include <stdio.h>
#include <string.h>

#define OSK_BUF_WIDTH 512
#define OSK_SCR_W 480
#define OSK_SCR_H 272
#define OSK_FRAME_SIZE (OSK_BUF_WIDTH * OSK_SCR_H * 4)

static unsigned int __attribute__((aligned(16))) g_osk_gu_list[262144];
static int g_gu_ready = 0;
static int g_gu_was_used = 0;

int osk_gu_was_used(void) {
    return g_gu_was_used;
}

static void configure_dialog(pspUtilityDialogCommon *dialog, size_t dialog_size) {
    memset(dialog, 0, sizeof(*dialog));
    dialog->size = (unsigned int)dialog_size;
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &dialog->language);
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_UNKNOWN, &dialog->buttonSwap);
    dialog->graphicsThread = 17;
    dialog->accessThread = 19;
    dialog->fontThread = 18;
    dialog->soundThread = 16;
}

static void gu_init_for_dialog(void) {
    sceGuInit();
    sceGuStart(GU_DIRECT, g_osk_gu_list);
    sceGuDrawBuffer(GU_PSM_8888, (void *)0, OSK_BUF_WIDTH);
    sceGuDispBuffer(OSK_SCR_W, OSK_SCR_H, (void *)OSK_FRAME_SIZE, OSK_BUF_WIDTH);
    sceGuDepthBuffer((void *)(OSK_FRAME_SIZE * 2), OSK_BUF_WIDTH);
    sceGuOffset(2048 - (OSK_SCR_W / 2), 2048 - (OSK_SCR_H / 2));
    sceGuViewport(2048, 2048, OSK_SCR_W, OSK_SCR_H);
    sceGuDepthRange(0xc350, 0x2710);
    sceGuScissor(0, 0, OSK_SCR_W, OSK_SCR_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuFrontFace(GU_CW);
    sceGuShadeModel(GU_SMOOTH);
    sceGuEnable(GU_CULL_FACE);
    sceGuEnable(GU_CLIP_PLANES);
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
    g_gu_ready = 1;
}

static void gu_term_dialog(void) {
    if (!g_gu_ready) {
        return;
    }
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
    g_gu_ready = 0;
}

static void draw_dialog_bg(void) {
    sceGuStart(GU_DIRECT, g_osk_gu_list);
    sceGuClearColor(0xff554433);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
}

static void ascii_to_utf16(const char *src, unsigned short *dst, int max_w) {
    int i = 0;
    if (!dst || max_w <= 0) {
        return;
    }
    if (!src) {
        dst[0] = 0;
        return;
    }
    while (src[i] && i < max_w - 1) {
        dst[i] = (unsigned short)(unsigned char)src[i];
        i++;
    }
    dst[i] = 0;
}

static void utf16_to_latin1(const unsigned short *src, char *dst, int dst_sz) {
    int i = 0;
    if (!dst || dst_sz <= 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    while (src[i] && i < dst_sz - 1) {
        unsigned short ch = src[i];
        if (ch < 128) {
            dst[i] = (char)ch;
        } else {
            dst[i] = '?';
        }
        i++;
    }
    dst[i] = '\0';
}

int osk_prompt(const char *title, const char *desc, const char *initial, char *out, int out_sz) {
    SceUtilityOskParams params;
    SceUtilityOskData osk;
    unsigned short result[128];
    unsigned short initial_w[128];
    unsigned short desc_w[128];
    int status;
    int running = 1;
    int confirmed = 0;

    if (!out || out_sz <= 0) {
        return 0;
    }
    out[0] = '\0';

    memset(&params, 0, sizeof(params));
    memset(&osk, 0, sizeof(osk));
    memset(result, 0, sizeof(result));

    {
        char prompt[96];
        prompt[0] = '\0';
        if (title && desc) {
            strncpy(prompt, title, sizeof(prompt) - 1);
            strncat(prompt, " — ", sizeof(prompt) - strlen(prompt) - 1);
            strncat(prompt, desc, sizeof(prompt) - strlen(prompt) - 1);
        } else if (title) {
            strncpy(prompt, title, sizeof(prompt) - 1);
        } else if (desc) {
            strncpy(prompt, desc, sizeof(prompt) - 1);
        } else {
            strncpy(prompt, "Search", sizeof(prompt) - 1);
        }
        prompt[sizeof(prompt) - 1] = '\0';
        ascii_to_utf16(prompt, desc_w, 128);
    }
    ascii_to_utf16(initial ? initial : "", initial_w, 128);

    g_gu_was_used = 0;
    gu_init_for_dialog();
    g_gu_was_used = 1;

    configure_dialog(&params.base, sizeof(params));
    osk.language = PSP_UTILITY_OSK_LANGUAGE_DEFAULT;
    osk.inputtype = PSP_UTILITY_OSK_INPUTTYPE_LATIN_LOWERCASE |
                    PSP_UTILITY_OSK_INPUTTYPE_LATIN_UPPERCASE |
                    PSP_UTILITY_OSK_INPUTTYPE_LATIN_DIGIT;
    osk.lines = 1;
    osk.desc = desc_w;
    osk.intext = initial_w;
    osk.outtextlength = (int)(sizeof(result) / sizeof(result[0]));
    osk.outtextlimit = 32;
    osk.outtext = result;

    params.datacount = 1;
    params.data = &osk;

    if (sceUtilityOskInitStart(&params) < 0) {
        gu_term_dialog();
        return 0;
    }

    while (running) {
        status = sceUtilityOskGetStatus();
        switch (status) {
            case PSP_UTILITY_DIALOG_NONE:
                running = 0;
                break;
            case PSP_UTILITY_DIALOG_INIT:
            case PSP_UTILITY_DIALOG_VISIBLE:
                draw_dialog_bg();
                sceUtilityOskUpdate(1);
                sceDisplayWaitVblankStart();
                break;
            case PSP_UTILITY_DIALOG_QUIT:
                confirmed = (osk.result == PSP_UTILITY_OSK_RESULT_CHANGED);
                sceUtilityOskShutdownStart();
                break;
            case PSP_UTILITY_DIALOG_FINISHED:
                running = 0;
                break;
            default:
                running = 0;
                break;
        }
    }

    gu_term_dialog();

    if (confirmed) {
        utf16_to_latin1(result, out, out_sz);
        return 1;
    }
    return 0;
}
