/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aelthuvor/app.h>
int ael_app_main(struct ael_app_context *context)
{
    volatile unsigned char *p=ael_app_alloc(context,4096);
    if (!p) { return 50; }
    p[0]=0x5a;
#ifdef APP_FAULT
    __asm__ volatile("ud2");
#endif
    return 0; /* Deliberately leave a live allocation: successful exit becomes 95. */
}
