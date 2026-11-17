/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE
#include "../src/app_internal.h" /* White-box SDK tests, never an application dependency. */
#include <aelthuvor/app_console.h>
#include <aelthuvor/vga.h>
#include <aelthuvor/keyboard.h>
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); abort(); } } while (0)
static struct { unsigned owned; uint32_t generation; uintptr_t va; } pages[4];
static struct ael_runtime runtime;
static struct ael_app_context context;
static int claim_error, map_error, release_error, log_error, vgrant, kgrant, exit_expected, exit_status;
static unsigned vowned, vmapped, kowned, calls;
static uint8_t input[64];
static unsigned input_count, input_offset;
static int input_error;
static jmp_buf exit_target;
_Noreturn void ael_exit(uint32_t status)
{
    CHECK(exit_expected); exit_status=(int)status; longjmp(exit_target,1);
}
int32_t ael_call(uint32_t n,uint32_t bx,uint32_t cx,uint32_t dx,uint32_t si,uint32_t *extra)
{
    ++calls;
    if (extra) { *extra=0; }
    if (n==SYS_LOG_CHAR) { CHECK(bx<256); return log_error; }
    if (n==SYS_YIELD) { return 0; }
    if (n==SYS_REVOKE_POLL) { return RES_ENOENT; }
    if (n==SYS_VGA_INFO) { *extra=4096; return 80|25<<16; }
    if (n==SYS_KBD_INFO) { *extra=KBD_SCANSET1; return KBD_QUEUE_CAPACITY; }
    if (n==SYS_VGA_ACQUIRE) { if (!vgrant) { return RES_EPERM; } vowned=1;*extra=0x80000001;return VGA_DEVICE_HANDLE; }
    if (n==SYS_KBD_ACQUIRE) { if (!kgrant) { return RES_EPERM; } kowned=1;*extra=0x80000002;return KBD_HANDLE; }
    if (n==SYS_VGA_MAP) {
        CHECK(vowned && bx==VGA_DEVICE_HANDLE && cx==0x80000001 && dx==VGA_USER_ADDRESS && si==3);
        CHECK(mmap((void *)(uintptr_t)dx,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)!=MAP_FAILED);
        vmapped=1;return 0;
    }
    if (n==SYS_VGA_CURSOR) { CHECK(vowned && vmapped && dx<2000);return 0; }
    if (n==SYS_VGA_RELEASE) {
        if (vmapped) { CHECK(munmap((void *)(uintptr_t)VGA_USER_ADDRESS,4096)==0); }
        vowned=vmapped=0;return 0;
    }
    if (n==SYS_KBD_RELEASE) { kowned=0;return 0; }
    if (n==SYS_KBD_POLL) {
        if (input_error) { int error=input_error;input_error=0;if (error==RES_ESTALE) { kowned=0; }if (extra) { *extra=7; }return error; }
        if (input_offset==input_count) { return RES_EAGAIN; }
        *extra=input[input_offset++];return 0;
    }
    if (n==SYS_KBD_STATS) { CHECK(kowned);*extra=7;return 0; }
    if (n==SYS_PAGE_CLAIM) {
        if (claim_error) { return claim_error; }
        for (unsigned i=0;i<4;++i) { if (!pages[i].owned) { pages[i].owned=1;*extra=++pages[i].generation;return (int)i+1; } }
        return RES_ENOSPC;
    }
    if (bx==0 || bx>4 || !pages[bx-1].owned || pages[bx-1].generation!=cx) { return RES_ESTALE; }
    unsigned i=bx-1;
    if (n==SYS_PAGE_QUERY) { return 100+(int)i; }
    if (n==SYS_PAGE_MAP) {
        if (map_error) { return map_error; }
        CHECK(dx>=RESOURCE_WINDOW && dx<RESOURCE_WINDOW_END);
        if (mmap((void *)(uintptr_t)dx,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)==MAP_FAILED) { return RES_EBUSY; }
        pages[i].va=dx;return 0;
    }
    if (n==SYS_PAGE_RELEASE) {
        if (release_error) { return release_error; }
        if (pages[i].va) { CHECK(munmap((void *)pages[i].va,4096)==0); }
        pages[i].owned=0;pages[i].va=0;return 0;
    }
    return -38;
}
static void setup(enum ael_policy policy)
{
    CHECK(!vowned && !kowned);
    for (unsigned i=0;i<4;++i) {
        if (pages[i].va) { CHECK(munmap((void *)pages[i].va,4096)==0); }
        pages[i].owned=0;pages[i].va=0;
    }
    runtime=(struct ael_runtime){.version=0x10002,.features=0x77,.quota=4,.bindings=4};
    CHECK(ael_pages_init(&runtime.pages,4)==0);
    runtime.pages.revocation=1;
    CHECK(ael_heap_init(&runtime.heap,&runtime.pages,policy)==0);
    context=(struct ael_app_context){0};
    claim_error=map_error=release_error=log_error=exit_expected=input_error=0;vgrant=kgrant=0;input_count=input_offset=0;
    CHECK(ael_app_bind(&context,&runtime)==0);
}
static void core(enum ael_policy policy)
{
    setup(policy);
    struct ael_app_info info;
    CHECK(ael_app_bind(&context,&runtime)==RES_EBUSY);
    CHECK(ael_app_get_info(&context,&info)==0 && info.api_version==0x10000 && info.kernel_version==0x10002 && info.policy==(uint32_t)policy);
    CHECK(ael_app_alloc(&context,0)==NULL && ael_app_error(&context)==RES_EINVAL);
    claim_error=RES_ENOMEM;
    CHECK(ael_app_alloc(&context,16)==NULL && ael_app_error(&context)==RES_ENOMEM);claim_error=0;
    unsigned char *p=ael_app_alloc(&context,80);CHECK(p && ((uintptr_t)p&15)==0);
    for (unsigned i=0;i<80;++i) { p[i]=(unsigned char)i; }
    for (unsigned i=0;i<80;++i) { CHECK(p[i]==(unsigned char)i); }
    CHECK(ael_app_free(&context,p+1)==RES_EINVAL);
    CHECK(ael_app_free(&context,p)==0 && ael_app_free(&context,p)==RES_EINVAL);
    CHECK(ael_app_trim(&context)==0);
    CHECK(ael_app_get_info(&context,&info)==0 && !info.live_allocations && !info.pages_owned);
    CHECK(ael_app_log(&context,NULL)==RES_EINVAL);
    log_error=RES_EAGAIN;CHECK(ael_app_log(&context,"bounded log")==RES_EAGAIN && ael_app_error(&context)==RES_EAGAIN);log_error=0;
    CHECK(ael_app_log(&context,"OK")==0 && ael_app_yield(&context)==0);
    CHECK(ael_app_finish(&context)==0 && !context.active);
    CHECK(ael_app_alloc(&context,16)==NULL && ael_app_error(&context)==RES_EINVAL);
    CHECK(ael_app_finish(&context)==RES_EINVAL);

    setup(policy);
    p=ael_app_alloc(&context,16);CHECK(p);
    CHECK(ael_app_finish(&context)==RES_EBUSY && runtime.heap.live==1); /* Leak is not hidden. */
    setup(policy);map_error=RES_ENOMEM;release_error=RES_EAGAIN;
    CHECK(ael_app_alloc(&context,16)==NULL && ael_app_error(&context)==RES_EAGAIN);
    CHECK(runtime.heap.live==0 && runtime.pages.owned==1);
    CHECK(ael_app_finish(&context)==RES_EAGAIN && runtime.pages.owned==1 && !context.active);
    setup(policy);map_error=RES_ENOMEM;release_error=RES_EAGAIN;
    CHECK(ael_app_alloc(&context,16)==NULL && runtime.pages.owned==1);
    map_error=release_error=0;
    CHECK(ael_app_trim(&context)==0 && runtime.pages.owned==0);
    CHECK(ael_app_finish(&context)==0);
    setup(policy);map_error=RES_ENOMEM;release_error=RES_EAGAIN;
    CHECK(ael_app_alloc(&context,16)==NULL && runtime.pages.owned==1);
    map_error=release_error=0;
    CHECK(ael_app_finish(&context)==0 && runtime.pages.owned==0);
    setup(policy);runtime.heap.failure=RES_ESTALE;exit_expected=1;
    if (setjmp(exit_target)==0) { (void)ael_app_alloc(&context,16);CHECK(0); }
    CHECK(exit_status==96 && context.error==RES_ESTALE);
    setup(policy);exit_expected=1;
    if (setjmp(exit_target)==0) { ael_app_exit(&context,7); }
    CHECK(exit_status==7 && !context.active);
    setup(policy);CHECK(ael_app_alloc(&context,16));exit_expected=1;
    if (setjmp(exit_target)==0) { ael_app_exit(&context,0); }
    CHECK(exit_status==95); /* Successful application exit with a leak. */
}
static void devices(void)
{
    setup(AEL_FIRST_FIT);
    unsigned before=calls;
    CHECK(ael_app_log(&context,"")==0 && calls==before+1); /* Only RAM-revocation POLL, no device request. */
    CHECK(ael_app_write(&context,"no service")==RES_ENOENT);
    CHECK(ael_app_console_open(&context)==RES_EPERM && !context.service && !vowned);
    vgrant=1;
    CHECK(ael_app_console_open(&context)==RES_EPERM && !vowned && !vmapped && !context.service);
    kgrant=1;
    CHECK(ael_app_console_open(&context)==0 && context.service);
    CHECK(ael_app_console_open(&context)==RES_EBUSY);
    struct ael_app_console_info info;
    CHECK(ael_app_console_info(&context,&info)==0 && info.columns==80 && info.lost==7);
    CHECK(ael_app_readline(&context,NULL,64)==RES_EINVAL);
    CHECK(ael_app_write(&context,"SDK output\n")==0 && ael_app_number(&context,0xffffffffu)==0);
    char line[8];
    const uint8_t edits[]={58,0xba,35,0xa3,42,18,0x92,0xaa,14,28};
    for (unsigned i=0;i<sizeof edits;++i) { input[i]=edits[i]; }
    input_count=sizeof edits;
    CHECK(ael_app_readline(&context,line,sizeof line)==1 && line[0]=='H' && line[1]==0);
    input_offset=0;input_count=9;
    for (unsigned i=0;i<8;++i) { input[i]=30; } input[8]=28;
    CHECK(ael_app_readline(&context,line,4)==3 && line[3]==0); /* Bound is capacity-1. */
    input_error=RES_EOVERFLOW;
    CHECK(ael_app_readline(&context,line,sizeof line)==RES_EOVERFLOW && line[0]==0);
    input_error=RES_ESTALE;
    CHECK(ael_app_readline(&context,line,sizeof line)==RES_ESTALE);
    unsigned previous=calls;
    CHECK(ael_app_write(&context,"after loss")==RES_ESTALE && calls==previous+1 && ael_app_error(&context)==RES_ESTALE);
    CHECK(ael_app_finish(&context)==RES_ESTALE && !vowned && !kowned && !context.service);
}

static void lifetimes(enum ael_policy policy)
{
    uint32_t seed=0x5a17c0de;
    for (unsigned cycle=0;cycle<128;++cycle) {
        setup(policy);
        seed=seed*1664525u+1013904223u;
        if (cycle%3==0) {
            map_error=RES_ENOMEM;release_error=RES_EAGAIN;
            CHECK(ael_app_alloc(&context,16)==NULL && ael_app_error(&context)==RES_EAGAIN);
            CHECK(runtime.pages.owned==1 && runtime.heap.live==0);
            map_error=release_error=0;
        }
        unsigned char *p=ael_app_alloc(&context,1+seed%4096);CHECK(p);
        p[0]=(unsigned char)cycle;
        struct ael_app_info info;
        CHECK(ael_app_get_info(&context,&info)==0 && info.live_allocations==1);
        CHECK(p[0]==(unsigned char)cycle && ael_app_free(&context,p)==0);
        CHECK(ael_app_free(&context,p)==RES_EINVAL);
        CHECK(ael_app_finish(&context)==0 && runtime.pages.owned==0 && runtime.heap.live==0);
        unsigned before=calls;
        CHECK(ael_app_alloc(&context,16)==NULL);
        CHECK(ael_app_free(&context,NULL)==RES_EINVAL && ael_app_trim(&context)==RES_EINVAL);
        CHECK(ael_app_check(&context)==RES_EINVAL && ael_app_get_info(&context,&info)==RES_EINVAL);
        CHECK(ael_app_finish(&context)==RES_EINVAL && calls==before);
        for (unsigned j=0;j<4;++j) { CHECK(!pages[j].owned && !pages[j].va); }
    }
    setup(policy);CHECK(ael_app_alloc(&context,64));
    runtime.heap.failure=RES_ESTALE;exit_expected=1;
    if (setjmp(exit_target)==0) { (void)ael_app_check(&context);CHECK(0); }
    if (setjmp(exit_target)==0) { (void)ael_app_free(&context,NULL);CHECK(0); }
    if (setjmp(exit_target)==0) { (void)ael_app_trim(&context);CHECK(0); }
    if (setjmp(exit_target)==0) { struct ael_app_info info;(void)ael_app_get_info(&context,&info);CHECK(0); }
    if (setjmp(exit_target)==0) { (void)ael_app_finish(&context);CHECK(0); }
    if (setjmp(exit_target)==0) { ael_app_exit(&context,0); }
    CHECK(exit_status==96 && runtime.heap.live==1 && context.error==RES_ESTALE);
}
int main(void)
{
    CHECK(ael_app_bind(NULL,NULL)==RES_EINVAL && ael_app_error(NULL)==RES_EINVAL);
    core(AEL_FIRST_FIT);core(AEL_BEST_FIT);devices();
    lifetimes(AEL_FIRST_FIT);lifetimes(AEL_BEST_FIT);
    setup(AEL_FIRST_FIT); /* Host-only teardown of the last fail-stop fixture. */
    puts("PASS: 256 seeded Application Context lifetimes and closed/unsafe API gates");
    printf("PASS: %u Application SDK assertions (ASan/UBSan)\n",checks);
    return 0;
}
