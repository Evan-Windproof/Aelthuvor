/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aelthuvor/app.h>
int ael_app_main(struct ael_app_context *context)
{
    struct ael_app_info info;
    if (ael_app_get_info(context, &info)) { return 10; }
    if (info.api_version != AEL_APP_API_VERSION || (info.kernel_version >> 16) != 1) { return 11; }
    return ael_app_log(context, "Hello from the Aelthuvor Application SDK\n") ? 12 : 0;
}
