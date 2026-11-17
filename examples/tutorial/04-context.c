/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aelthuvor/app.h>
int ael_app_main(struct ael_app_context *ctx)
{
    struct ael_app_info info;
    if (ael_app_get_info(ctx,&info) || info.live_allocations) { return 1; }
    if (ael_app_alloc(ctx,0)!=0 || ael_app_error(ctx)!=AEL_APP_EINVAL) { return 2; }
    /* Read the error immediately: a successful API call clears last-error. */
    if (ael_app_check(ctx) || ael_app_error(ctx)) { return 3; }
    return 0; /* Adapter ends this borrowed context; do not retain ctx. */
}
