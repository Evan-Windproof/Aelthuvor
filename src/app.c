/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "app_internal.h"

int ael_app_result(struct ael_app_context *context, int error)
{
    if (context != NULL) { context->error = error; }
    return error;
}

int ael_app_bind(struct ael_app_context *context, struct ael_runtime *runtime)
{
    if (context == NULL || runtime == NULL || runtime->heap.pool != &runtime->pages) {
        return RES_EINVAL;
    }
    if (context->active) { return RES_EBUSY; }
    *context = (struct ael_app_context){.runtime = runtime, .active = 1};
    return 0;
}

int ael_app_check(struct ael_app_context *context)
{
    if (context == NULL || !context->active || context->runtime == NULL) {
        return ael_app_result(context, RES_EINVAL);
    }
    int error = ael_heap_check(&context->runtime->heap);
    if (error != 0) {
        context->error = error;
        ael_exit(96); /* An opaque API does not make a lost pointer safe. */
    }
    return ael_app_result(context, 0);
}

int ael_app_get_info(struct ael_app_context *context, struct ael_app_info *info)
{
    int error = ael_app_check(context);
    if (error) { return error; }
    if (info == NULL) { return ael_app_result(context, RES_EINVAL); }
    struct ael_runtime *runtime = context->runtime;
    *info = (struct ael_app_info){
        AEL_APP_API_VERSION, runtime->version, runtime->features,
        (uint32_t)runtime->heap.policy, runtime->quota, runtime->bindings,
        runtime->pages.owned, runtime->heap.live
    };
    return ael_app_result(context, 0);
}

void *ael_app_alloc(struct ael_app_context *context, size_t bytes)
{
    if (ael_app_check(context)) { return NULL; }
    void *pointer = ael_alloc(&context->runtime->heap, bytes);
    context->error = context->runtime->heap.error;
    return pointer;
}

int ael_app_free(struct ael_app_context *context, void *pointer)
{
    int error = ael_app_check(context);
    return error ? error : ael_app_result(context, ael_free(&context->runtime->heap, pointer));
}

int ael_app_trim(struct ael_app_context *context)
{
    int error = ael_app_check(context);
    return error ? error : ael_app_result(context, ael_heap_trim(&context->runtime->heap));
}

int ael_app_error(const struct ael_app_context *context)
{
    return context == NULL ? RES_EINVAL : context->error;
}

int ael_app_log(struct ael_app_context *context, const char *message)
{
    int error = ael_app_check(context);
    if (error) { return error; }
    if (message == NULL) { return ael_app_result(context, RES_EINVAL); }
    while (*message) {
        error = ael_call(SYS_LOG_CHAR, (uint8_t)*message++, 0, 0, 0, NULL);
        if (error) { break; } /* Propagate the real diagnostic output quota. */
    }
    return ael_app_result(context, error);
}

int ael_app_yield(struct ael_app_context *context)
{
    int error = ael_app_check(context);
    return error ? error : ael_app_result(context, ael_call(SYS_YIELD, 0, 0, 0, 0, NULL));
}

int ael_app_finish(struct ael_app_context *context)
{
    int error = ael_app_check(context);
    if (error) { return error; }
    if (context->close_service) { error = context->close_service(context); }
    int heap_error = ael_heap_destroy(&context->runtime->heap);
    /* A failed map + failed rollback can leave a claimed page without an
     * arena. The original pool deliberately retains that identity. Do not
     * report successful application cleanup while it is still owned. */
    if (!heap_error && context->runtime->pages.owned) { heap_error = RES_EBUSY; }
    if (!error) { error = heap_error; }
    /* A terminal context never hands control back to the application. Live
     * blocks are left for kernel teardown, not falsely reported as freed. */
    context->active = 0;
    return ael_app_result(context, error);
}

_Noreturn void ael_app_exit(struct ael_app_context *context, uint32_t status)
{
    int error = ael_app_finish(context);
    ael_exit(status ? status : (error ? 95u : 0u));
}
