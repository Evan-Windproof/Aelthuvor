/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aelthuvor/runtime.h>
int ael_runtime_probe(struct ael_runtime *runtime, uint32_t major, uint32_t minor, uint32_t required)
{
    if (runtime == NULL) {
        return RES_EINVAL;
    }
    uint32_t features = 0, bindings = 0;
    int32_t version = ael_call(SYS_ABI_QUERY, 0, 0, 0, 0, &features);
    if (version < 0 || !liberums_abi_compatible((uint32_t)version, features, major, minor, required)
        || !(features & LIBERUMS_FEATURE_LIMITS)) {
        return RES_EPROTONOSUPPORT;
    }
    int32_t quota = ael_call(SYS_RESOURCE_LIMITS, 0, 0, 0, 0, &bindings);
    if (quota <= 0 || bindings == 0) {
        return RES_EPROTONOSUPPORT;
    }
    runtime->version = (uint32_t)version;
    runtime->features = features;
    runtime->quota = (uint32_t)quota;
    runtime->bindings = bindings;
    /* Never exceed either our metadata/window capacity or the kernel quota. */
    int error = ael_pages_init(&runtime->pages, (unsigned)quota < AEL_PAGE_SLOTS ? (unsigned)quota : AEL_PAGE_SLOTS);
    runtime->pages.revocation = (required & LIBERUMS_FEATURE_REVOCATION) != 0;
    return error;
}
