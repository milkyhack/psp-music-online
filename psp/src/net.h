#ifndef NET_H
#define NET_H

/* Load net modules + sceNet/Inet/Apctl. Safe to call more than once. */
int net_init(void);
void net_shutdown(void);

/*
 * Show the official PSP Wi-Fi connection dialog (Network Settings list).
 * Returns 1 if connected with IP, 0 on cancel/fail.
 * Uses GU while the dialog is open; call ui_init() afterwards to restore UI.
 *
 * If ms0:/.../data/ppsspp_auto.txt exists (PPSSPP bench), skips the dialog and
 * uses sceNetApctlConnect(1) — PPSSPP auto-connects when EnableWlan is ON.
 */
int net_want_auto_connect(void);
/* 1 if ppsspp_auto.txt contains "albums" (bench opens album list after connect). */
int net_ppsspp_auto_open_albums(void);
int net_connect_auto(void);
int net_connect_dialog(void);

/* 1 if we currently have an IP from APCTL. */
int net_is_connected(void);

/* 1 if WLAN slider reports ON (sceWlanGetSwitchState != 0). */
int net_wlan_on(void);

/* Last error from net_init / dialog (0 if ok). */
int net_last_error(void);

/* Short name of failing step, e.g. "NetInit", or "". */
const char *net_last_step(void);

/* 1 if GU was started for the netconf dialog (need ui_init after). */
int net_gu_was_used(void);

#endif
