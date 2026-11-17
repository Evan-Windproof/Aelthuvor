/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef AELTHUVOR_APP_INTERNAL_H
#define AELTHUVOR_APP_INTERNAL_H
#include <aelthuvor/app.h>
#include <aelthuvor/runtime.h>
struct ael_app_context {
    struct ael_runtime *runtime;
    int error;
    unsigned active;
    void *service;
    int (*close_service)(struct ael_app_context *);
};
int ael_app_bind(struct ael_app_context *context, struct ael_runtime *runtime);
int ael_app_finish(struct ael_app_context *context);
int ael_app_result(struct ael_app_context *context, int error);
#endif
