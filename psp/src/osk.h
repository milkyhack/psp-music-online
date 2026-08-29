#ifndef OSK_H
#define OSK_H

/*
 * Show the official PSP on-screen keyboard.
 * Returns 1 if user confirmed, 0 on cancel.
 * out receives a Latin-1 copy of the entered text (may be empty).
 */
int osk_prompt(const char *title, const char *desc, const char *initial, char *out, int out_sz);

/* 1 if GU was started for the OSK dialog (call ui_init after). */
int osk_gu_was_used(void);

#endif
