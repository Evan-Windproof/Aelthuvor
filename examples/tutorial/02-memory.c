/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aelthuvor/app.h>
int ael_app_main(struct ael_app_context *ctx)
{
    unsigned char *p=ael_app_alloc(ctx,128);
    if (!p) { return ael_app_error(ctx)==AEL_APP_ENOMEM ? 2 : 3; }
    if ((uintptr_t)p%16) { return 4; }
    for (unsigned i=0;i<128;++i) { p[i]=(unsigned char)i; }
    for (unsigned i=0;i<128;++i) { if (p[i]!=(unsigned char)i) { return 5; } }
    if (ael_app_free(ctx,p) || ael_app_trim(ctx)) { return 6; }
    /* Bounded quota experiment. Every successful allocation is cleaned up. */
    void *pages[16]={0};unsigned count=0;
    while (count<16 && (pages[count]=ael_app_alloc(ctx,4096))!=0) { ++count; }
    void *extra=count==16 ? ael_app_alloc(ctx,4096) : 0;
    int error=ael_app_error(ctx);
    if (extra) { (void)ael_app_free(ctx,extra);return 7; }
    if (error!=AEL_APP_ENOSPC && error!=AEL_APP_ENOMEM) { return 8; }
    for (unsigned i=0;i<count;++i) { if (ael_app_free(ctx,pages[i])) { return 9; } }
    return ael_app_trim(ctx) ? 10 : 0;
}
