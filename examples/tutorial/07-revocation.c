/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aelthuvor/app.h>
int ael_app_main(struct ael_app_context *ctx)
{
    unsigned char *p=ael_app_alloc(ctx,64);if (!p) { return 1; }
    if (ael_app_check(ctx)) { return 2; }
    /* The check is not a pin. A later PIT revoke can still make this load fault. */
    p[0]=42;
    if (p[0]!=42 || ael_app_free(ctx,p) || ael_app_trim(ctx)) { return 3; }
    return 0;
}
