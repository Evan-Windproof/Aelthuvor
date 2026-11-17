/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE
#include "../reference/independent-libos/pool.h"
#include "../reference/independent-libos/syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <sys/mman.h>
static unsigned checks,owned,mapped,releases;
static int claim_error,map_error,release_error,stale,expect_exit;
static uint32_t generation=0x80000000;
static jmp_buf terminal;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);abort(); } } while (0)
_Noreturn void reference_exit(uint32_t status) { CHECK(expect_exit && status==86);longjmp(terminal,1); }
int32_t reference_call(uint32_t n,uint32_t bx,uint32_t cx,uint32_t dx,uint32_t si,uint32_t *extra)
{
    if (n==SYS_PAGE_CLAIM) { if (claim_error) { return claim_error; } owned=1;*extra=++generation;return 7; }
    CHECK(bx==7 && cx==generation);
    if (n==SYS_PAGE_QUERY) { return stale ? RES_ESTALE : 123; }
    if (n==SYS_PAGE_MAP) {
        if (map_error) { return map_error; }
        CHECK(dx==RESOURCE_WINDOW_END-4096 && si==3);
        CHECK(mmap((void *)(uintptr_t)dx,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)!=MAP_FAILED);
        mapped=1;return 0;
    }
    CHECK(n==SYS_PAGE_RELEASE);
    ++releases;
    if (release_error) { return release_error; }
    if (mapped) { CHECK(munmap((void *)(uintptr_t)(RESOURCE_WINDOW_END-4096),4096)==0); }
    owned=mapped=0;return 0;
}
int main(void)
{
    struct reference_pool p={0};
    claim_error=RES_ENOMEM;CHECK(reference_pool_open(&p)==RES_ENOMEM && !p.owned);claim_error=0;
    map_error=RES_ENOMEM;CHECK(reference_pool_open(&p)==RES_ENOMEM && !p.owned && releases==1);
    release_error=RES_EAGAIN;CHECK(reference_pool_open(&p)==RES_EAGAIN && p.owned && owned);
    map_error=release_error=0;CHECK(reference_pool_close(&p)==0);
    CHECK(reference_pool_open(&p)==0 && p.handle.generation&0x80000000u);
    void *all[64];
    for (unsigned i=0;i<64;++i) { all[i]=reference_pool_take(&p);CHECK((uintptr_t)all[i]==p.address+(63-i)*64); }
    CHECK(reference_pool_take(&p)==NULL && reference_pool_close(&p)==RES_EBUSY);
    CHECK(reference_pool_give(&p,(char *)all[0]+1)==RES_EINVAL);
    for (unsigned i=0;i<64;++i) { CHECK(reference_pool_give(&p,all[i])==0); }
    CHECK(reference_pool_give(&p,all[0])==RES_EINVAL);
    CHECK(reference_pool_take(&p)==all[63]);CHECK(reference_pool_give(&p,all[63])==0);
    stale=1;expect_exit=1;
    if (setjmp(terminal)==0) { (void)reference_pool_take(&p);CHECK(0); }
    stale=0;CHECK(reference_pool_close(&p)==0 && reference_pool_close(&p)==RES_ENOENT);
    printf("PASS: %u independent reference pool assertions (ASan/UBSan)\n",checks);
    return 0;
}
