#ifndef PPSSPP_QA_DEMO_H
#define PPSSPP_QA_DEMO_H

#include "ui.h"

/* Active only when both ppsspp_qa.txt and ppsspp_qa_demo.txt exist (bench only). */
int ppsspp_qa_demo_active(void);

void ppsspp_qa_demo_patch_home(void);

int ppsspp_qa_demo_library(
    const char ***labels,
    const char ***rights,
    const char ***sublines,
    int *count,
    int *cursor,
    UiMiniPlayer *mini
);

void ppsspp_qa_demo_np(UiNowPlaying *np);

void ppsspp_qa_demo_setup(int octets[4], int *selected, int *port, int *focus_port);

const char *ppsspp_qa_demo_cf_screen_title(void);
const char *ppsspp_qa_demo_cf_track(void);
const char *ppsspp_qa_demo_cf_artist(void);
const char *ppsspp_qa_demo_cf_album(void);

#endif
