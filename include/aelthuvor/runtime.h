/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef AELTHUVOR_RUNTIME_H
#define AELTHUVOR_RUNTIME_H
#include <aelthuvor/memory.h>
#define AEL_REQUIRED_FEATURES (LIBERUMS_FEATURE_PAGES | LIBERUMS_FEATURE_ELF32 | LIBERUMS_FEATURE_COOPERATIVE | LIBERUMS_FEATURE_LIMITS)
struct ael_runtime {
    uint32_t version, features, quota, bindings;
    struct ael_page_pool pages;
    struct ael_heap heap;
};
extern struct ael_runtime ael_runtime;
int ael_runtime_probe(struct ael_runtime *runtime, uint32_t major, uint32_t minor, uint32_t required);
int ael_main(struct ael_heap *heap);
void ael_checkpoint(uint32_t stage);
_Noreturn void ael_start(void);
#endif
