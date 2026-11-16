/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Public-ABI negative fixture. No grants or kernel-private data are available. */
#include <aelthuvor/runtime.h>
#include <aelthuvor/vga.h>
#include <aelthuvor/keyboard.h>
#define EXPECT(condition, code) do { if (!(condition)) { return (code); } } while (0)
int ael_main(struct ael_heap *heap)
{
    struct ael_vga vga = {0};
    struct ael_keyboard keyboard = {0};
    EXPECT(ael_vga_acquire(&vga, RESOURCE_READ | RESOURCE_WRITE) == 0, 40);
    EXPECT(ael_call(SYS_VGA_MAP, vga.handle.index, vga.handle.generation,
                    RESOURCE_WINDOW, RESOURCE_READ | RESOURCE_WRITE, NULL) == RES_EINVAL, 41);
    EXPECT(ael_call(SYS_PAGE_QUERY, vga.handle.index, vga.handle.generation, 0, 0, NULL) == RES_ESTALE, 42);
    EXPECT(ael_vga_map(&vga, VGA_USER_ADDRESS, RESOURCE_READ | RESOURCE_WRITE) == 0, 43);
    EXPECT(ael_vga_cell(&vga, 1999, 'Z', 0x4e) == 0 && ael_vga_cursor(&vga, 1999) == 0, 44);
    struct resource_handle old = vga.handle;
    EXPECT(ael_vga_release(&vga) == 0 && ael_vga_release(&vga) == RES_ENOENT, 45);
    EXPECT(ael_vga_acquire(&vga, RESOURCE_READ | RESOURCE_WRITE) == 0, 46);
    EXPECT(vga.handle.generation != old.generation, 47);
    EXPECT(ael_call(SYS_VGA_MAP, old.index, old.generation, VGA_USER_ADDRESS, 3, NULL) == RES_ESTALE, 48);
    EXPECT(ael_vga_release(&vga) == 0, 49);
    EXPECT(ael_keyboard_acquire(&keyboard) == 0, 50);
    old = keyboard.handle;
    EXPECT(ael_keyboard_release(&keyboard) == 0 && ael_keyboard_release(&keyboard) == RES_ENOENT, 51);
    EXPECT(ael_keyboard_acquire(&keyboard) == 0 && keyboard.handle.generation != old.generation, 52);
    uint32_t value;
    EXPECT(ael_call(SYS_KBD_POLL, old.index, old.generation, 0, 0, &value) == RES_ESTALE, 53);
    EXPECT(ael_keyboard_poll(&keyboard, &value) == RES_EAGAIN, 54);
    EXPECT(ael_keyboard_release(&keyboard) == 0, 55);
    EXPECT(heap->live == 0 && heap->pool->owned == 0, 56);
    return 0;
}
