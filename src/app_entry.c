/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Explicitly selected entry adapter; never stored in either static library. */
#include "app_internal.h"
static struct ael_app_context application;
int ael_main(struct ael_heap *heap)
{
    if (heap != &ael_runtime.heap || ael_app_bind(&application, &ael_runtime)) { return 94; }
    int status = ael_app_main(&application);
    int error = ael_app_finish(&application);
    return status ? status : (error ? 95 : 0);
}
