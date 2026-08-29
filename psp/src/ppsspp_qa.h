#ifndef PPSSPP_QA_H
#define PPSSPP_QA_H

/*
 * PPSSPP bench visual QA — active only when data/ppsspp_qa.txt exists.
 * Never deploy ppsspp_qa.txt to a real PSP Memory Stick.
 */

int net_want_ppsspp_qa(void);

void ppsspp_qa_init(void);

/* 1 while QA script is running (not done). */
int ppsspp_qa_active(void);

/*
 * Advance QA state machine once per frame after the screen is drawn.
 * Returns 1 when the app should exit (script reached "done").
 */
int ppsspp_qa_tick(void);

/* Action requests for main.c to execute (connect, navigate, play). */
typedef enum {
    PPSSPP_QA_ACT_NONE = 0,
    PPSSPP_QA_ACT_CONNECT,
    PPSSPP_QA_ACT_HOME,
    PPSSPP_QA_ACT_LIBRARY,
    PPSSPP_QA_ACT_ALBUMS,
    PPSSPP_QA_ACT_SETUP,
    PPSSPP_QA_ACT_PICK_ALBUM,
    PPSSPP_QA_ACT_PICK_TRACK,
    PPSSPP_QA_ACT_PLAY,
    PPSSPP_QA_ACT_NOW_PLAYING,
    PPSSPP_QA_ACT_SKIN,
    PPSSPP_QA_ACT_TOGGLE_REPEAT,
    PPSSPP_QA_ACT_TOGGLE_SHUFFLE,
    PPSSPP_QA_ACT_EQ_NEXT
} PpssppQaAction;

typedef struct {
    PpssppQaAction action;
    int index;
    int skin_id;
} PpssppQaCmd;

/* Pop pending navigation command (main executes, then calls ppsspp_qa_cmd_done). */
int ppsspp_qa_poll_cmd(PpssppQaCmd *out);

void ppsspp_qa_cmd_done(void);

#endif
