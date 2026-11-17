/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aelthuvor/app.h>
int ael_app_main(struct ael_app_context *context)
{
    struct ael_app_info info;
    if (ael_app_get_info(context,&info)) { return 1; }
    unsigned char *bytes=ael_app_alloc(context,80);
    if (!bytes) { return 2; }
    for (unsigned i=0;i<80;++i) { bytes[i]=(unsigned char)(i+7); }
    for (unsigned i=0;i<80;++i) { if (bytes[i]!=(unsigned char)(i+7)) { return 3; } }
    if (ael_app_free(context,bytes) || ael_app_trim(context)) { return 4; }
    return ael_app_log(context,"Standalone Aelthuvor application template passed\n") ? 5 : 0;
}
