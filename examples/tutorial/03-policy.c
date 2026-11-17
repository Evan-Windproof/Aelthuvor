/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aelthuvor/app.h>
int ael_app_main(struct ael_app_context *ctx)
{
    const unsigned sizes[5]={64,256,64,128,64};void *blocks[5]={0};
    for (unsigned i=0;i<5;++i) { blocks[i]=ael_app_alloc(ctx,sizes[i]);if (!blocks[i]) { return 1; } }
    uintptr_t first=(uintptr_t)blocks[1], best=(uintptr_t)blocks[3];
    if (ael_app_free(ctx,blocks[1]) || ael_app_free(ctx,blocks[3])) { return 2; }
    blocks[1]=blocks[3]=0;
    void *p=ael_app_alloc(ctx,96);if (!p) { return 3; }
    struct ael_app_info info;if (ael_app_get_info(ctx,&info)) { return 4; }
    if ((uintptr_t)p!=(info.policy==AEL_APP_FIRST_FIT ? first : best)) { return 5; }
    if (ael_app_free(ctx,p)) { return 6; }
    for (unsigned i=0;i<5;++i) { if (ael_app_free(ctx,blocks[i])) { return 7; } }
    return ael_app_trim(ctx) ? 8 : 0;
}
