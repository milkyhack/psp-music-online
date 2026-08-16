#include "net.h"

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspwlan.h>
#include <pspsdk.h>
#include <psputility.h>
#include <psputility_netconf.h>
#include <psputility_sysparam.h>
#include <string.h>
#include <stdio.h>

#include "debug_log.h"
#include "http.h"

/* Match SDK samples/utility/netdialog exactly — absolute VRAM addrs corrupt the LCD */
#define NET_BUF_WIDTH 512
#define NET_SCR_W 480
#define NET_SCR_H 272
#define NET_PIXEL_SIZE 4
#define NET_FRAME_SIZE (NET_BUF_WIDTH * NET_SCR_H * NET_PIXEL_SIZE)

static int net_modules_loaded = 0;
static int gu_ready = 0;
static int g_net_last_error = 0;
static int g_gu_was_used = 0;
static char g_net_last_step[24];
/* Same size as official netdialog sample */
static unsigned int __attribute__((aligned(16))) gu_list[262144];

int net_last_error(void) {
    return g_net_last_error;
}

const char *net_last_step(void) {
    return g_net_last_step;
}

int net_gu_was_used(void) {
    return g_gu_was_used;
}

static void set_net_fail(const char *step, int err) {
    g_net_last_error = err;
    if (step) {
        strncpy(g_net_last_step, step, sizeof(g_net_last_step) - 1);
        g_net_last_step[sizeof(g_net_last_step) - 1] = '\0';
    } else {
        g_net_last_step[0] = '\0';
    }
}

static int load_net_module(int module) {
    int err = sceUtilityLoadNetModule(module);
    /* Already loaded is OK (partial retry / second call) */
    if (err == (int)0x80110801 || err == (int)0x80110802) {
        return 0;
    }
    return err;
}

int net_init(void) {
    int err;
    int wlan;

    if (net_modules_loaded) {
        g_net_last_error = 0;
        g_net_last_step[0] = '\0';
        return 0;
    }

    g_net_last_error = 0;
    g_net_last_step[0] = '\0';

    /* WLAN slider: 0 = clearly off. Negative/unknown → still try (some CFW). */
    wlan = sceWlanGetSwitchState();
    /* #region agent log */
    {
        char d[48];
        snprintf(d, sizeof(d), "{\"wlan\":%d}", wlan);
        dbg_log("B", "net.c:net_init", "wlan_switch", d);
    }
    /* #endregion */
    if (wlan == 0) {
        set_net_fail("WLAN_OFF", -1);
        /* Caller may wait for switch; do not load modules yet. */
        return -1;
    }

    err = load_net_module(PSP_NET_MODULE_COMMON);
    /* #region agent log */
    {
        char d[64];
        snprintf(d, sizeof(d), "{\"step\":\"COMMON\",\"err\":%d}", err);
        dbg_log("B", "net.c:net_init", "load_module", d);
    }
    /* #endregion */
    if (err < 0) {
        set_net_fail("COMMON", err);
        return err;
    }

    err = load_net_module(PSP_NET_MODULE_INET);
    /* #region agent log */
    {
        char d[64];
        snprintf(d, sizeof(d), "{\"step\":\"INET\",\"err\":%d}", err);
        dbg_log("B", "net.c:net_init", "load_module", d);
    }
    /* #endregion */
    if (err < 0) {
        set_net_fail("INET", err);
        sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
        return err;
    }

    /* Give modules time to link stubs */
    sceKernelDelayThread(200000);

    /* Same path as SDK netsample — pspSdkInetInit does Net/Inet/Resolver/Apctl */
    err = pspSdkInetInit();
    /* #region agent log */
    {
        char d[80];
        snprintf(d, sizeof(d), "{\"step\":\"SdkInetInit\",\"err\":%d}", err);
        dbg_log("B", "net.c:net_init", "stack_init", d);
    }
    /* #endregion */
    if (err < 0) {
        set_net_fail("SdkInet", err);
        pspSdkInetTerm();
        sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
        sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
        return err;
    }

    net_modules_loaded = 1;
    g_net_last_error = 0;
    g_net_last_step[0] = '\0';
    return 0;
}

void net_shutdown(void) {
    if (!net_modules_loaded) {
        return;
    }
    /* Best-effort — never block exit path; Disconnect can hang if half-torn-down */
    sceNetApctlDisconnect();
    sceKernelDelayThread(50000);
    pspSdkInetTerm();
    sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
    sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
    net_modules_loaded = 0;
}

int net_is_connected(void) {
    int state = 0;
    if (!net_modules_loaded) {
        return 0;
    }
    if (sceNetApctlGetState(&state) < 0) {
        return 0;
    }
    return state == PSP_NET_APCTL_STATE_GOT_IP;
}

int net_wlan_on(void) {
    return sceWlanGetSwitchState() != 0;
}

static void configure_dialog(pspUtilityDialogCommon *dialog, size_t dialog_size) {
    memset(dialog, 0, sizeof(*dialog));
    dialog->size = (unsigned int)dialog_size;
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &dialog->language);
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_UNKNOWN, &dialog->buttonSwap);
    /* Official netdialog sample values */
    dialog->graphicsThread = 17;
    dialog->accessThread = 19;
    dialog->fontThread = 18;
    dialog->soundThread = 16;
}

/*
 * GU setup from pspdev SDK samples/utility/netdialog.
 * Draw/disp/depth buffers MUST be offsets from VRAM start — absolute
 * 0x40000000|edram pointers corrupt the LCD (especially at high clocks).
 */
static void gu_init_for_dialog(void) {
    sceGuInit();

    sceGuStart(GU_DIRECT, gu_list);
    sceGuDrawBuffer(GU_PSM_8888, (void *)0, NET_BUF_WIDTH);
    sceGuDispBuffer(NET_SCR_W, NET_SCR_H, (void *)NET_FRAME_SIZE, NET_BUF_WIDTH);
    sceGuDepthBuffer((void *)(NET_FRAME_SIZE * 2), NET_BUF_WIDTH);
    sceGuOffset(2048 - (NET_SCR_W / 2), 2048 - (NET_SCR_H / 2));
    sceGuViewport(2048, 2048, NET_SCR_W, NET_SCR_H);
    sceGuDepthRange(0xc350, 0x2710);
    sceGuScissor(0, 0, NET_SCR_W, NET_SCR_H);
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
    gu_ready = 1;
}

static void gu_term_dialog(void) {
    if (!gu_ready) {
        return;
    }
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
    gu_ready = 0;
}

static void draw_dialog_bg(void) {
    sceGuStart(GU_DIRECT, gu_list);
    sceGuClearColor(0xff554433);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
}

int net_connect_dialog(void) {
    int ret;
    int done = 0;
    int status = 0;
    int wait;
    int running = 1;
    pspUtilityNetconfData data;
    struct pspUtilityNetconfAdhoc adhocparam;

    /* #region agent log */
    dbg_log("B", "net.c:net_connect_dialog", "enter", "{}");
    /* #endregion */

    g_gu_was_used = 0;

    /* Always show the official connection list — do not skip when already online. */
    if (net_modules_loaded && net_is_connected()) {
        dbg_log("B", "net.c:net_connect_dialog", "disconnect_for_repick", "{}");
        sceNetApctlDisconnect();
        sceKernelDelayThread(300000);
    }

    ret = net_init();
    /* #region agent log */
    {
        char d[48];
        snprintf(d, sizeof(d), "{\"net_init\":%d}", ret);
        dbg_log("B", "net.c:net_connect_dialog", "after_net_init", d);
    }
    /* #endregion */
    if (ret < 0) {
        return 0;
    }

    memset(&adhocparam, 0, sizeof(adhocparam));
    memset(&data, 0, sizeof(data));
    configure_dialog(&data.base, sizeof(data));
    data.action = PSP_NETCONF_ACTION_CONNECTAP;
    data.hotspot = 0;
    data.wifisp = 0;
    data.adhocparam = &adhocparam;

    /* #region agent log */
    dbg_log("D", "net.c:net_connect_dialog", "before_gu_init", "{}");
    /* #endregion */
    gu_init_for_dialog();
    g_gu_was_used = 1;
    /* #region agent log */
    dbg_log("D", "net.c:net_connect_dialog", "after_gu_init", "{}");
    /* #endregion */

    ret = sceUtilityNetconfInitStart(&data);
    /* #region agent log */
    {
        char d[64];
        snprintf(d, sizeof(d), "{\"InitStart\":%d}", ret);
        dbg_log("B", "net.c:net_connect_dialog", "after_InitStart", d);
    }
    /* #endregion */
    if (ret < 0) {
        gu_term_dialog();
        return 0;
    }

    /* Loop matches SDK samples/utility/netdialog */
    while (running) {
        int prev = status;
        draw_dialog_bg();

        status = sceUtilityNetconfGetStatus();
        /* #region agent log */
        if (status != prev) {
            char d[48];
            snprintf(d, sizeof(d), "{\"status\":%d}", status);
            dbg_log("D", "net.c:net_connect_dialog", "dialog_status", d);
        }
        /* #endregion */

        switch (status) {
            case PSP_UTILITY_DIALOG_NONE:
                done = 1;
                break;
            case PSP_UTILITY_DIALOG_VISIBLE:
                sceUtilityNetconfUpdate(1);
                break;
            case PSP_UTILITY_DIALOG_QUIT:
                sceUtilityNetconfShutdownStart();
                break;
            case PSP_UTILITY_DIALOG_FINISHED:
                /* Keep pumping until NONE — GuTerm during FINISHED blacks the LCD */
                break;
            default:
                break;
        }

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();

        if (done || http_should_abort()) {
            break;
        }
    }

    /* #region agent log */
    dbg_log("D", "net.c:net_connect_dialog", "dialog_loop_done", "{}");
    /* #endregion */

    sceDisplayWaitVblankStart();
    gu_term_dialog();
    sceDisplayWaitVblankStart();

    if (http_should_abort()) {
        return 0;
    }

    for (wait = 0; wait < 50 && !net_is_connected(); wait++) {
        if (http_should_abort()) {
            break;
        }
        sceKernelDelayThread(100000);
    }

    /* #region agent log */
    {
        char d[48];
        snprintf(d, sizeof(d), "{\"connected\":%d}", net_is_connected());
        dbg_log("B", "net.c:net_connect_dialog", "exit", d);
    }
    /* #endregion */

    return net_is_connected() ? 1 : 0;
}
