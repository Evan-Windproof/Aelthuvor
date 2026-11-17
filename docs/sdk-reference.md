# Application SDK 1.0 reference

Release v1.0.0 targets Liberums v1.0.0 / Kernel ABI 1.2. `AEL_APP_API_VERSION`
is `0x00010000`; it is not the kernel's `0x00010002`. Headers retain their
existing declarations and error numbers. Source compatibility covers the two
ordinary application headers, not internal structure layouts.

## API and module review

| Surface / implementation | Contract and review outcome |
| --- | --- |
| app.h / app.c | Stable ordinary application interface; borrowed opaque Context; no handles or heap layout required |
| app_console.h / app_console.c | Optional stable service; requires existing device grants; one context/service per ELF |
| startup.S / runtime.c / app_entry.c | crt0 calls ael_start; explicit adapter selects ael_app_main; legacy ael_main retained |
| syscall.h / syscall.c / sdk/include/abi | Advanced scalar syscall helpers; public kernel headers have frozen historical provenance |
| page.h / page.c | Advanced capability ownership; full generation, release rollback records, explicit map/unmap/release |
| memory.h / memory.c | Advanced allocator interface; first/best, 16-byte alignment, split/coalesce; metadata is outside payload |
| revoke.h / revoke.c | Advanced raw protocol; heap coordination through sticky check; raw ACK must not bypass active allocations |
| vga.h / keyboard.h / text.h | Advanced device/decoder implementation; never ordinary RAM; same kernel grant requirements |
| src/app_internal.h | Private bind/finish/service machinery; not exported, not an application API |
| linker.ld / app.mk / export_sdk.py | Static ELF32, explicit startup/runtime/entry objects, no archive mains, no libc |
| independent-libos | Separate startup/syscalls/object pool; uses public kernel ABI only, no ael library |

## Context and core functions

Implement `int ael_app_main(struct ael_app_context *ctx)`. SDK startup supplies a
fresh borrowed context; do not instantiate, copy, free or keep it after main.
Return 0 after cleanup; the adapter finishes the context and exits the domain.
There is no public bind/finish constructor/destructor pair. `ael_app_exit` ends
execution; internal finish is intentionally absent from app.h.

| Function | Return / behavior |
| --- | --- |
| ael_app_get_info(ctx, &info) | 0 or negative error; checks heap first, fills complete snapshot on success |
| ael_app_alloc(ctx, bytes) | 16-byte aligned pointer for 1..4096 bytes; NULL on recoverable failure, last-error set |
| ael_app_free(ctx, pointer) | 0 or negative error; NULL is accepted after safety check; only exact live block starts |
| ael_app_trim(ctx) | 0 or actual release error; releases idle arenas and retries failed-acquire records |
| ael_app_check(ctx) | 0 if current checkpoint is safe; invalid context EINVAL; unsafe heap terminates with 96 |
| ael_app_error(ctx) | Last recorded result, no checkpoint/syscall; NULL context EINVAL |
| ael_app_log(ctx, text) | 0 or first syscall error; NUL-terminated ASCII diagnostic output, bounded by kernel quota (128 chars/domain) |
| ael_app_yield(ctx) | 0 or syscall error after checkpoint; kernel retains scheduling authority |
| ael_app_exit(ctx, status) | Never returns; ordinary cleanup failure turns status 0 into 95; nonzero status retained; unsafe heap always 96 |

Info fields: `api_version`, `kernel_version`, `kernel_features`, `policy`
(`AEL_APP_FIRST_FIT`=0 / `AEL_APP_BEST_FIT`=1), `page_quota`, `page_bindings`,
`pages_owned`, `live_allocations`. A snapshot grants no lease or pin.

Read last-error immediately after failure: a later successful checked operation
sets it to zero. Names in app.h are negative errno-style values: EPERM (-1),
ENOENT (-2), EIO (-5), EAGAIN (-11), ENOMEM (-12), EBUSY (-16), EINVAL (-22),
ENOSPC (-28), ENOSYS (-38), EOVERFLOW (-75), EPROTONOSUPPORT (-93), ESTALE (-116).
Not every function produces every code. No thread-local errno or host libc exists.

Pointers remain usable only while their allocation is live **and** its resource
has not been revoked. Free/trim/ACK/forced loss invalidate ownership. A stale raw
pointer may later equal a new allocation's address; same-domain C has no pointer
tagging or memory-safety sandbox. Check does not protect the next load from a PIT
deadline. Active pending or lost arenas fail-stop instead of erasing live metadata.

Return-from-main cleanup closes devices and attempts heap destroy. A live
allocation or remaining standalone page prevents false success. Transient
MAP+rollback failures can be retried by Trim before returning. A cleanup error
on successful main yields 95; unsafe heap yields 96. Kernel EXIT/fault teardown
then removes bindings, cancels requests, zeros and returns resource/image RAM.

## Optional Console

Include app_console.h, build with `APP_DEVICES=1`, and request trusted boot
grants. INFO discovery does not grant ACQUIRE authority. Open requests VGA
READ|WRITE and keyboard; partial initialization tries to release acquired parts.

| Function | Result |
| --- | --- |
| ael_app_console_open | 0, grant/ABI/device error, or EBUSY for an existing service |
| ael_app_console_close | Attempts both device releases; 0 or cleanup/previous sticky service error; absent service ENOENT |
| ael_app_console_info | 0 and columns/rows/queued/lost, or error |
| ael_app_write / ael_app_number / ael_app_clear | 0 or service/device error; number prints unsigned decimal |
| ael_app_readline | Length or negative error; NUL-terminated buffer of capacity 2..4096; blocks in this single-threaded domain |

US scan-set-1 translation supports printable ASCII, Shift/Caps, Backspace and
Enter. A full caller buffer retains capacity-1 characters and ignores further
printable input; **keyboard queue loss** returns EOVERFLOW and clears the line
and decoder. These are distinct conditions. EOVERFLOW permits an explicit new
read attempt; stale/permission/unsupported-device failures stop the service.
Close may return the prior service failure even after devices were released.
The application's buffers and strings must be valid C objects; APIs do not
validate arbitrary pointers inside the same domain.
