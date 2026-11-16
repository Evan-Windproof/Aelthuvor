/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE
#include "console.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); abort(); } } while (0)
struct ael_runtime ael_runtime;
static struct { unsigned owned; uint32_t generation; uintptr_t va; } pages[4];
static unsigned v_owned, v_mapped, k_owned, grants, supported;
static uint32_t vg=0x80000000u, kg=0x90000000u, v_rights;
static int v_error, k_error, map_error, claim_error;
static uint32_t scan_format=KBD_SCANSET1;
static uint8_t queue[KBD_QUEUE_CAPACITY];
static unsigned count, overflow, lost, calls;
_Noreturn void ael_exit(uint32_t status) { fprintf(stderr,"unexpected exit %u\n",status); abort(); }

int32_t ael_call(uint32_t n, uint32_t bx, uint32_t cx, uint32_t dx, uint32_t si, uint32_t *extra)
{
    ++calls;
    if (extra) { *extra=0; }
    if (n==SYS_REVOKE_POLL) { return RES_ENOENT; }
    if (n==SYS_VGA_INFO) {
        if (!supported) { return -38; }
        if (extra) { *extra=VGA_MAPPING_BYTES; }
        return VGA_COLUMNS | VGA_ROWS<<16;
    }
    if (n==SYS_KBD_INFO) {
        if (!supported) { return -38; }
        *extra=scan_format; return KBD_QUEUE_CAPACITY;
    }
    if (n==SYS_VGA_ACQUIRE) {
        if (!(grants&1)) { return RES_EPERM; }
        if (v_owned) { return RES_EBUSY; }
        v_owned=1; v_rights=cx; *extra=++vg; return VGA_DEVICE_HANDLE;
    }
    if (n==SYS_KBD_ACQUIRE) {
        if (!(grants&2)) { return RES_EPERM; }
        if (k_owned) { return RES_EBUSY; }
        k_owned=1; *extra=++kg; count=overflow=lost=0; return KBD_HANDLE;
    }
    if (n>=SYS_VGA_MAP && n<=SYS_VGA_CURSOR) {
        if (v_error) { return v_error; }
        if (!v_owned || bx!=VGA_DEVICE_HANDLE || cx!=vg) { return RES_ESTALE; }
        if (n==SYS_VGA_MAP) {
            if (dx!=VGA_USER_ADDRESS || (si!=1 && si!=3)) { return RES_EINVAL; }
            if ((si&v_rights)!=si) { return RES_EPERM; }
            if (v_mapped) { return RES_EBUSY; }
            if (map_error) { return map_error; }
            CHECK(mmap((void *)(uintptr_t)dx,4096,PROT_READ|PROT_WRITE,
                       MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)!=(void *)-1);
            v_mapped=1; return 0;
        }
        if (n==SYS_VGA_CURSOR) {
            if (!(v_rights&RESOURCE_WRITE)) { return RES_EPERM; }
            return dx<2000 && v_mapped ? 0 : RES_EINVAL;
        }
        if (v_mapped) { CHECK(munmap((void *)(uintptr_t)VGA_USER_ADDRESS,4096)==0); }
        v_mapped=v_owned=0; return 0;
    }
    if (n>=SYS_KBD_POLL && n<=SYS_KBD_STATS) {
        if (k_error) { return k_error; }
        if (!k_owned || bx!=KBD_HANDLE || cx!=kg) { return RES_ESTALE; }
        if (n==SYS_KBD_STATS) { *extra=lost; return (int32_t)count; }
        if (n==SYS_KBD_RELEASE) { k_owned=count=overflow=0; return 0; }
        if (overflow) { *extra=lost; count=overflow=0; return RES_EOVERFLOW; }
        if (!count) { return RES_EAGAIN; }
        *extra=queue[0]; --count; memmove(queue,queue+1,count); return 0;
    }
    if (n==SYS_PAGE_CLAIM) {
        if (claim_error) { return claim_error; }
        for (unsigned i=0;i<4;++i) {
            if (!pages[i].owned) { pages[i].owned=1; *extra=++pages[i].generation; return (int32_t)i+1; }
        }
        return RES_ENOSPC;
    }
    if (!bx || bx>4 || !pages[bx-1].owned || pages[bx-1].generation!=cx) { return RES_ESTALE; }
    unsigned i=bx-1;
    if (n==SYS_PAGE_QUERY) { return (int32_t)i+100; }
    if (n==SYS_PAGE_MAP) {
        CHECK(dx>=RESOURCE_WINDOW && dx<RESOURCE_WINDOW_END && dx!=VGA_USER_ADDRESS);
        void *p=mmap((void *)(uintptr_t)dx,4096,PROT_READ|PROT_WRITE,
                     MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
        if (p==MAP_FAILED) { return RES_EBUSY; }
        pages[i].va=dx; return 0;
    }
    if (n==SYS_PAGE_RELEASE) {
        if (pages[i].va) { CHECK(munmap((void *)pages[i].va,4096)==0); }
        pages[i].owned=pages[i].va=0; return 0;
    }
    return -38;
}

static void enqueue(uint8_t byte)
{
    if (count==KBD_QUEUE_CAPACITY) { overflow=1; ++lost; }
    else { queue[count++]=byte; }
}

static void reset(enum ael_policy policy)
{
    for (unsigned i=0;i<4;++i) { CHECK(!pages[i].owned); }
    CHECK(!v_owned && !k_owned && !v_mapped);
    supported=1; grants=3; v_error=k_error=map_error=claim_error=0;
    scan_format=KBD_SCANSET1; count=overflow=lost=0;
    ael_runtime.version=0x10002; ael_runtime.features=0x77;
    CHECK(ael_pages_init(&ael_runtime.pages,4)==0);
    ael_runtime.pages.revocation=1;
    CHECK(ael_heap_init(&ael_runtime.heap,&ael_runtime.pages,policy)==0);
}

static int contains(const char *text)
{
    char screen[2026]; unsigned j=0;
    volatile uint16_t *cells=(void *)(uintptr_t)VGA_USER_ADDRESS;
    for (unsigned r=0;r<25;++r) {
        for (unsigned c=0;c<80;++c) { screen[j++]=(char)cells[r*80+c]; }
        screen[j++]='\n';
    }
    screen[j]=0; return strstr(screen,text)!=NULL;
}

static void devices(void)
{
    reset(AEL_FIRST_FIT);
    struct ael_vga v={0}; struct ael_keyboard k={0};
    supported=0; CHECK(ael_vga_probe()==-38 && ael_keyboard_probe()==-38);
    CHECK(ael_vga_acquire(&v,3)==-38 && ael_keyboard_acquire(&k)==-38);
    supported=1; scan_format=2; CHECK(ael_keyboard_probe()==RES_EPROTONOSUPPORT); scan_format=1;
    grants=0; CHECK(ael_vga_probe()==0 && ael_keyboard_probe()==0);
    CHECK(ael_vga_acquire(&v,3)==RES_EPERM && ael_keyboard_acquire(&k)==RES_EPERM);
    grants=3;
    CHECK(ael_vga_acquire(&v,2)==RES_EINVAL);
    CHECK(ael_vga_acquire(&v,1)==0 && v.handle.generation&0x80000000u);
    CHECK(ael_vga_map(&v,VGA_USER_ADDRESS,3)==RES_EPERM);
    CHECK(ael_vga_map(&v,RESOURCE_WINDOW,1)==RES_EINVAL);
    CHECK(ael_vga_map(&v,VGA_USER_ADDRESS,1)==0);
    CHECK(ael_vga_cell(&v,0,'X',0x1f)==RES_EPERM);
    CHECK(ael_vga_release(&v)==0 && ael_vga_release(&v)==RES_ENOENT);
    CHECK(ael_vga_acquire(&v,3)==0 && ael_vga_map(&v,VGA_USER_ADDRESS,3)==0);
    CHECK(ael_vga_cell(&v,1999,'Z',0x4e)==0 && v.cells[1999]==0x4e5a);
    v.cells[2000]=0xabcd; /* Padding canary: renderer must not touch it. */
    CHECK(ael_vga_cell(&v,2000,'X',0)==RES_EINVAL);
    CHECK(ael_vga_fill(&v,1999,2,0)==RES_EINVAL && ael_vga_fill(&v,0,~0u,0)==RES_EINVAL);
    struct ael_text t; ael_text_init(&t,&v,0x1f); CHECK(ael_text_clear(&t)==0);
    for (unsigned i=0;i<27;++i) { CHECK(ael_text_write(&t,"line\n")==0); }
    CHECK(t.row==24 && t.column==0 && v.cells[2000]==0xabcd);
    char wide[82]; memset(wide,'w',81); wide[81]=0;
    CHECK(ael_text_clear(&t)==0 && ael_text_write(&t,wide)==0 && t.row==1 && t.column==1);
    CHECK(ael_text_backspace(&t)==0 && t.column==0);
    CHECK(ael_text_backspace(&t)==0 && t.row==0 && t.column==79);
    struct ael_vga old=v;
    CHECK(ael_vga_release(&v)==0);
    CHECK(ael_vga_acquire(&v,3)==0 && ael_vga_map(&v,VGA_USER_ADDRESS,3)==0);
    CHECK(ael_vga_cell(&old,0,'?',0)==RES_ESTALE && old.cells==NULL && v.mapped);
    v_error=RES_ESTALE;
    CHECK(mprotect((void *)(uintptr_t)VGA_USER_ADDRESS,4096,PROT_NONE)==0);
    CHECK(ael_vga_cell(&v,0,'?',0)==RES_ESTALE && v.cells==NULL);
    CHECK(ael_vga_fill(&v,0,2000,0)==RES_ESTALE); /* No access to protected memory. */
    CHECK(munmap((void *)(uintptr_t)VGA_USER_ADDRESS,4096)==0); v_owned=v_mapped=0; v_error=0;
    CHECK(ael_keyboard_acquire(&k)==0);
    uint32_t raw, n, l;
    CHECK(ael_keyboard_poll(&k,&raw)==RES_EAGAIN && raw==0);
    enqueue(0x1e); CHECK(ael_keyboard_poll(&k,&raw)==0 && raw==0x1e);
    k.decoder.shifts=1; k.decoder.extended=1;
    for (unsigned i=0;i<65;++i) { enqueue(0x2a); }
    CHECK(ael_keyboard_poll(&k,&raw)==RES_EOVERFLOW && raw>0 && !k.decoder.shifts && !k.decoder.extended);
    CHECK(ael_keyboard_stats(&k,&n,&l)==0 && n==0 && l>0);
    struct ael_keyboard oldk=k;
    CHECK(ael_keyboard_release(&k)==0 && ael_keyboard_release(&k)==RES_ENOENT);
    CHECK(ael_keyboard_acquire(&k)==0);
    CHECK(ael_keyboard_poll(&oldk,&raw)==RES_ESTALE && !oldk.owned && k.owned);
    k_error=RES_EPERM; CHECK(ael_keyboard_poll(&k,&raw)==RES_EPERM && !k.owned);
    k_owned=0; k_error=0;
}

static void decode(void)
{
    struct ael_key_decoder d={0};
    CHECK(ael_key_decode(&d,0x1e)=='a');
    CHECK(ael_key_decode(&d,42)==0 && ael_key_decode(&d,0x1e)=='A');
    ael_key_decode(&d,54); ael_key_decode(&d,42|0x80);
    CHECK(ael_key_decode(&d,0x1e)=='A'); ael_key_decode(&d,54|0x80);
    ael_key_decode(&d,58); ael_key_decode(&d,58); /* held caps does not retoggle */
    CHECK(ael_key_decode(&d,0x1e)=='A'); ael_key_decode(&d,58|0x80);
    ael_key_decode(&d,42); CHECK(ael_key_decode(&d,0x1e)=='a' && ael_key_decode(&d,2)=='!');
    ael_key_decode(&d,42|0x80);
    ael_key_decode(&d,0xe0); CHECK(ael_key_decode(&d,42)==0 && !d.shifts);
    const uint8_t pause[]={0xe1,0x1d,0x45,0xe1,0x9d,0xc5};
    for (unsigned i=0;i<sizeof pause;++i) { CHECK(ael_key_decode(&d,pause[i])==0); }
    CHECK(ael_key_decode(&d,14)=='\b' && ael_key_decode(&d,28)=='\n');
    CHECK(ael_key_decode(&d,0x9e)==0);
    /* Decoder handles all byte values without indexing outside its tables. */
    for (unsigned i=0;i<10000;++i) { (void)ael_key_decode(&d,(uint8_t)(i*73)); }
    ael_key_reset(&d); CHECK(!d.shifts && !d.caps && !d.pause && !d.extended);
}

static void byte(struct ael_console *c, uint8_t raw)
{
    uint32_t actual; enqueue(raw); CHECK(ael_console_step(c,&actual)==0 && actual==raw);
}
static void type(struct ael_console *c, const char *text)
{
    static const char map[59]="\0\0" "1234567890-=\b\0qwertyuiop[]\n\0asdfghjkl;'`\0\\zxcvbnm,./\0\0\0 ";
    for (;*text;++text) {
        unsigned code=0; for (unsigned i=1;i<sizeof map;++i) { if (map[i]==*text) { code=i; break; } }
        CHECK(code); byte(c,(uint8_t)code); byte(c,(uint8_t)(code|0x80));
    }
}
static void application(enum ael_policy policy)
{
    reset(policy); struct ael_console c;
    CHECK(ael_console_open(&c,&ael_runtime.heap)==0 && c.line && c.heap->live==1);
    CHECK((uintptr_t)c.line>=RESOURCE_WINDOW && (uintptr_t)c.line<RESOURCE_WINDOW_END);
    CHECK(contains("Aelthuvor Phase 3 Console"));
    type(&c,"help\n"); CHECK(contains("help clear about status memtest exit"));
    type(&c,"clx\b ear"); /* invalid command is intentional */
    type(&c,"\n"); CHECK(contains("Unknown command"));
    type(&c,"clear\n"); CHECK(c.text.row==0 && c.text.column==5 && contains("ael> "));
    type(&c,"about\nstatus\n"); CHECK(contains("independent Ring 3") && contains("ABI 1.2") && contains("live=1"));
    type(&c,"memtest\n"); CHECK(c.memtests==1 && c.heap->live==1 && c.heap->pool->owned==1);
    claim_error=RES_ENOMEM; type(&c,"memtest\n"); CHECK(contains("memtest: FAIL") && c.memtests==1 && c.heap->live==1); claim_error=0;
    for (unsigned i=0;i<80;++i) { byte(&c,0x1e); }
    CHECK(c.length==63 && c.line[63]==0);
    for (unsigned i=0;i<65;++i) { byte(&c,14); }
    CHECK(c.length==0 && c.text.column==5);
    byte(&c,42); byte(&c,0x1e); CHECK(c.line[0]=='A');
    for (unsigned i=0;i<65;++i) { enqueue(0xe0); }
    uint32_t raw; CHECK(ael_console_step(&c,&raw)==RES_EOVERFLOW && c.length==0 && c.line[0]==0 && !c.keyboard.decoder.shifts);
    type(&c,"exit\n"); CHECK(!c.running);
    CHECK(ael_console_close(&c)==0 && c.heap->live==0 && c.heap->pool->owned==0);
    CHECK(ael_heap_destroy(c.heap)==0);

    reset(policy);
    CHECK(ael_console_open(&c,&ael_runtime.heap)==0 && c.running);
    CHECK(ael_console_close(&c)==0 && !c.running && c.line==NULL && c.length==0);
    CHECK(c.heap->live==0 && c.heap->pool->owned==0);

    reset(policy); grants=1;
    CHECK(ael_console_open(&c,&ael_runtime.heap)==RES_EPERM && !v_owned && !v_mapped);
    reset(policy); map_error=RES_ENOMEM;
    CHECK(ael_console_open(&c,&ael_runtime.heap)==RES_ENOMEM && !v_owned);
    reset(policy); claim_error=RES_ENOMEM;
    CHECK(ael_console_open(&c,&ael_runtime.heap)==RES_ENOMEM && !v_owned && !k_owned);
}
int main(void)
{
    devices(); decode(); application(AEL_FIRST_FIT); application(AEL_BEST_FIT);
    printf("PASS: %u device/decoder/text/Console assertions; real allocator, ASan/UBSan\n",checks);
    return 0;
}
