/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aelthuvor/app.h>
int ael_app_main(struct ael_app_context *ctx)
{
    struct ael_app_info info;
    void *p=ael_app_alloc(ctx,80);if (!p) { return 1; }
    if (ael_app_get_info(ctx,&info) || info.pages_owned!=1 || info.live_allocations!=1) { return 2; }
    if (ael_app_free(ctx,p)) { return 3; }
    if (ael_app_get_info(ctx,&info) || info.pages_owned!=1 || info.live_allocations) { return 4; }
    if (ael_app_trim(ctx)) { return 5; }
    if (ael_app_get_info(ctx,&info) || info.pages_owned || info.live_allocations) { return 6; }
    return 0;
}
