/* SPDX-License-Identifier: GPL-3.0-or-later */
/* External application using only the exported, opaque Aelthuvor App SDK and
 * public kernel ABI. No LibOS metadata is inspected or patched by this test. */
/* Commands 1..11 adapted from Liberums v1.0.0 tests/fixtures/security9/ael_ram.c;
 * Phase 5 extends the same public-API consumer, under GPL-3.0-or-later. */
#include <aelthuvor/app.h>
#include <abi/syscall.h>
volatile uint32_t ram_command, ram_stage, ram_sequence, ram_address;
volatile uint32_t ram_index, ram_generation, ram_request, ram_pfn;
volatile int32_t ram_result;
volatile uint32_t cycle_index, cycle_generation, cycle_pfn;
volatile uint32_t ram_pressure_oom;

static int pressure(struct ael_app_context *context)
{
    unsigned char *objects[24]={0};unsigned sizes[24]={0};
    uint32_t seed=0x5a17c0de;
    for (unsigned step=0;step<512;++step) {
        seed=seed*1664525u+1013904223u;
        unsigned slot=(seed>>16)%24;
        if (objects[slot]) {
            for (unsigned j=0;j<sizes[slot];++j) {
                if (objects[slot][j]!=(unsigned char)(slot^j)) { return 82; }
            }
            if (ael_app_free(context,objects[slot])) { return 83; }
            objects[slot]=0;
        } else {
            sizes[slot]=1+(seed>>5)%4000;
            objects[slot]=ael_app_alloc(context,sizes[slot]);
            if (!objects[slot]) {
                ++ram_pressure_oom;
                int e=ael_app_error(context);
                if (e!=AEL_APP_ENOSPC && e!=AEL_APP_ENOMEM) { return 84; }
            } else {
                for (unsigned j=0;j<sizes[slot];++j) { objects[slot][j]=(unsigned char)(slot^j); }
            }
        }
        if (step%17==0 && ael_app_trim(context)) { return 85; }
    }
    for (unsigned slot=0;slot<24;++slot) {
        if (!objects[slot]) { continue; }
        for (unsigned j=0;j<sizes[slot];++j) {
            if (objects[slot][j]!=(unsigned char)(slot^j)) { return 86; }
        }
        if (ael_app_free(context,objects[slot])) { return 87; }
    }
    if (!ram_pressure_oom || ael_app_trim(context)) { return 88; }
    struct ael_app_info info;
    if (ael_app_get_info(context,&info) || info.pages_owned!=1 || info.live_allocations!=1) { return 89; }
    return 0;
}
__attribute__((noinline)) static void observed(uint32_t stage)
{
    ram_stage=stage; ++ram_sequence;
    __asm__ volatile(".global ram_observed; ram_observed: nop" : : : "memory");
}
__attribute__((noinline)) static void idle(void)
{
    __asm__ volatile(".global ram_idle; ram_idle: nop" : : : "memory");
    liberums_call(SYS_YIELD,0,0,0,0,0);
}
int ael_app_main(struct ael_app_context *context)
{
    unsigned char *allocation=ael_app_alloc(context,128);
    if (!allocation) { return 70; }
    ram_address=(uintptr_t)allocation;
    for (unsigned i=0;i<128;++i) { allocation[i]=(unsigned char)(i^0x9a); }
    observed(1);
    for (;;) {
        uint32_t command=ram_command; ram_command=0;
        if (!command) { idle();continue; }
        if (command==1) { /* Empty arena retained, so the LibOS can ACK it. */
            ram_result=ael_app_free(context,allocation);allocation=0;
            if (ram_result) { return 71; }
            observed(2);
        } else if (command==2) {
            ram_result=ael_app_check(context); /* Unmodified LibOS POLL/INFO/ACK. */
            observed(3); /* Unsafe active/lost memory must exit 96 before here. */
        } else if (command==3) {
            __asm__ volatile(".global ram_fault_read; ram_fault_read: movb (%0),%%al"
                : : "r"(ram_address) : "eax","memory");
            return 72;
        } else if (command==4) {
            ram_result=liberums_call(SYS_PAGE_MAP,ram_index,ram_generation,RESOURCE_WINDOW,3,0);
            if (ram_result!=RES_ESTALE) { return 73; }
            observed(4);
        } else if (command==5) { /* Raw RELEASE supersedes a pending request. */
            ram_result=liberums_call(SYS_PAGE_RELEASE,ram_index,ram_generation,0,0,0);
            observed(5);
        } else if (command==6) {
            ram_result=liberums_call(SYS_REVOKE_ACK,ram_index,ram_generation,ram_request,0,0);
            observed(6);
        } else if (command==7) {
            if (allocation && ael_app_free(context,allocation)) { return 74; }
            return 0;
        } else if (command==8) { __asm__ volatile("ud2"); }
        else if (command==9) { liberums_exit(0); } /* Deliberate EXIT with live RAM. */
        else if (command==10) { /* A fresh claim after ACK must return zero RAM. */
            uint32_t gen=0; int32_t index=liberums_call(SYS_PAGE_SPECIFIC,ram_pfn,RESOURCE_ALL,0,0,&gen);
            if (index<1 || liberums_call(SYS_PAGE_MAP,(uint32_t)index,gen,RESOURCE_WINDOW,3,0)) { return 75; }
            volatile uint32_t *page=(void *)RESOURCE_WINDOW;
            for (unsigned i=0;i<1024;++i) { if (page[i]) { return 76; } }
            if (liberums_call(SYS_PAGE_RELEASE,(uint32_t)index,gen,0,0,0)) { return 77; }
            observed(7);
        } else if (command==11) {
            ram_result=liberums_call(SYS_PAGE_MAP,ram_index,ram_generation,RESOURCE_WINDOW+4096,3,0);
            if (ram_result) { return 81; }
            observed(8);
        } else if (command==12) {
            ram_result=pressure(context);
            if (ram_result) { return ram_result; }
            observed(9);
        } else if (command==13) {
            uint32_t generation=0;
            int32_t index=liberums_call(SYS_PAGE_CLAIM,47,0,0,0,&generation);
            if (index<1) { return 90; }
            cycle_index=(uint32_t)index;cycle_generation=generation;
            if (liberums_call(SYS_PAGE_MAP,cycle_index,generation,RESOURCE_WINDOW+8192,3,0)) { return 91; }
            cycle_pfn=(uint32_t)liberums_call(SYS_PAGE_QUERY,cycle_index,generation,0,0,0);
            volatile uint32_t *page=(void *)(RESOURCE_WINDOW+8192);
            for (unsigned j=0;j<1024;++j) { page[j]=0xa5e10000u|j; }
            observed(10);
        } else if (command==14) {
            ram_result=liberums_call(SYS_PAGE_UNMAP,RESOURCE_WINDOW+8192,0,0,0,0);
            if (ram_result) { return 92; }
            observed(11);
        } else if (command==15) {
            if (liberums_call(SYS_PAGE_MAP,cycle_index,cycle_generation,RESOURCE_WINDOW+8192,3,0)) { return 93; }
            volatile uint32_t *page=(void *)(RESOURCE_WINDOW+8192);
            for (unsigned j=0;j<1024;++j) { if (page[j]!=(0xa5e10000u|j)) { return 94; } }
            if (liberums_call(SYS_PAGE_RELEASE,cycle_index,cycle_generation,0,0,0)) { return 97; }
            ram_result=liberums_call(SYS_PAGE_QUERY,cycle_index,cycle_generation,0,0,0);
            if (ram_result!=RES_ESTALE) { return 98; }
            observed(12);
        } else if (command==16) {
            struct ael_app_info info;
            if (ael_app_get_info(context,&info) || info.pages_owned || info.live_allocations) { return 99; }
            unsigned char *p=ael_app_alloc(context,4096);
            if (!p) { return 100; }
            for (unsigned j=0;j<4096;++j) { if (p[j]) { return 101; }p[j]=(unsigned char)j; }
            if (ael_app_free(context,p) || ael_app_trim(context)) { return 102; }
            if (ael_app_get_info(context,&info) || info.pages_owned || info.live_allocations) { return 103; }
            observed(13);
        } else { return 78; }
    }
}
