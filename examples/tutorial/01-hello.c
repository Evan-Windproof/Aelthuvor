/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aelthuvor/app.h>
int ael_app_main(struct ael_app_context *ctx)
{
    struct ael_app_info info;
    if (ael_app_get_info(ctx,&info)) { return 1; }
    if (info.api_version!=AEL_APP_API_VERSION || info.kernel_version!=0x00010002u) { return 2; }
    return ael_app_log(ctx,"Hello Aelthuvor: Application API 1.0, Kernel ABI 1.2\n") ? 3 : 0;
}
