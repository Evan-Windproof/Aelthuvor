/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef AELTHUVOR_APP_CONSOLE_H
#define AELTHUVOR_APP_CONSOLE_H
#include <aelthuvor/app.h>
struct ael_app_console_info { uint32_t columns, rows, queued, lost; };
/* Optional library. OPEN explicitly requests existing grants, never creates
 * authority. No device code is linked into applications that omit it. */
int ael_app_console_open(struct ael_app_context *context);
int ael_app_console_close(struct ael_app_context *context);
int ael_app_console_info(struct ael_app_context *context, struct ael_app_console_info *info);
int ael_app_write(struct ael_app_context *context, const char *message);
int ael_app_number(struct ael_app_context *context, uint32_t number);
int ael_app_clear(struct ael_app_context *context);
/* Blocking single-threaded line edit: returns length or negative error.
 * Overflow discards the current line/decoder and returns -75. */
int ael_app_readline(struct ael_app_context *context, char *buffer, size_t capacity);
#endif
