/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef AELTHUVOR_APP_H
#define AELTHUVOR_APP_H
#include <stddef.h>
#include <stdint.h>
/* LibOS application API, NOT the Liberums kernel ABI version. */
#define AEL_APP_API_VERSION 0x00010000u
#define AEL_APP_FIRST_FIT 0u
#define AEL_APP_BEST_FIT 1u
/* Public LibOS error names; numeric values preserve the documented ABI
 * convention. They do not add kernel calls or grant authority. */
enum ael_app_error_code {
    AEL_APP_EPERM=-1, AEL_APP_ENOENT=-2, AEL_APP_EIO=-5,
    AEL_APP_EAGAIN=-11, AEL_APP_ENOMEM=-12, AEL_APP_EBUSY=-16,
    AEL_APP_EINVAL=-22, AEL_APP_ENOSPC=-28, AEL_APP_ENOSYS=-38,
    AEL_APP_EOVERFLOW=-75, AEL_APP_EPROTONOSUPPORT=-93, AEL_APP_ESTALE=-116
};
struct ael_app_context; /* Borrowed only for the duration of ael_app_main. */
struct ael_app_info {
    uint32_t api_version, kernel_version, kernel_features;
    uint32_t policy, page_quota, page_bindings, pages_owned, live_allocations;
};
int ael_app_main(struct ael_app_context *context);
int ael_app_get_info(struct ael_app_context *context, struct ael_app_info *info);
void *ael_app_alloc(struct ael_app_context *context, size_t bytes);
int ael_app_free(struct ael_app_context *context, void *pointer);
int ael_app_trim(struct ael_app_context *context);
int ael_app_check(struct ael_app_context *context);
int ael_app_error(const struct ael_app_context *context);
int ael_app_log(struct ael_app_context *context, const char *message);
int ael_app_yield(struct ael_app_context *context);
/* Cleans up services/heap. Successful return from main follows the same path.
 * Leaked allocations make a successful exit status 95; unsafe heap -> exit 96. */
_Noreturn void ael_app_exit(struct ael_app_context *context, uint32_t status);
#endif
