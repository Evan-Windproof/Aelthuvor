/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aelthuvor/app.h>
int ael_app_main(struct ael_app_context *ctx)
{
    return ael_app_log(ctx,"This application links only the installed SDK\n") ? 1 : 0;
}
