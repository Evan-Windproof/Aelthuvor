/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <abi/elf.h>
#include "syscall.h"
#include "pool.h"
#define REFERENCE_FEATURES (LIBERUMS_FEATURE_PAGES|LIBERUMS_FEATURE_ELF32|LIBERUMS_FEATURE_LIMITS|LIBERUMS_FEATURE_PREEMPTION)
const struct liberums_abi_note reference_requirement
    __attribute__((section(".note.liberums"),aligned(4),used))={
        9,12,LIBERUMS_NOTE_TYPE,"LIBERUMS",1,2,REFERENCE_FEATURES
    };
struct reference_pool reference_pool;
_Noreturn void reference_start(void)
{
    uint32_t features=0,bindings=0;
    int32_t version=reference_call(SYS_ABI_QUERY,0,0,0,0,&features);
    if (version<0 || !liberums_abi_compatible((uint32_t)version,features,1,2,REFERENCE_FEATURES)) { reference_exit(83); }
    int32_t quota=reference_call(SYS_RESOURCE_LIMITS,0,0,0,0,&bindings);
    if (quota<1 || bindings<1) { reference_exit(84); }
    if (reference_pool_open(&reference_pool)) { reference_exit(85); }
    int status=reference_application(&reference_pool);
    int cleanup=reference_pool_close(&reference_pool);
    reference_exit(status ? (uint32_t)status : (cleanup ? 87u : 0u));
}
