/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aelthuvor/vga.h>
#include <stddef.h>

static int result(struct ael_vga *vga, int error)
{
    if (error == RES_ESTALE || error == RES_EPERM || error == -38) {
        vga->failure = error;
        vga->owned = vga->mapped = 0;
        vga->cells = NULL;
    }
    return error;
}

int ael_vga_probe(void)
{
    uint32_t bytes = 0;
    int32_t geometry = ael_call(SYS_VGA_INFO, VGA_DEVICE_TEXT, 0, 0, 0, &bytes);
    if (geometry < 0) { return geometry; }
    return (uint32_t)geometry == (VGA_COLUMNS | VGA_ROWS << 16) && bytes == VGA_MAPPING_BYTES
        ? 0 : RES_EPROTONOSUPPORT;
}

int ael_vga_acquire(struct ael_vga *vga, uint32_t rights)
{
    if (vga == NULL || (rights != RESOURCE_READ && rights != (RESOURCE_READ | RESOURCE_WRITE))) {
        return RES_EINVAL;
    }
    if (vga->owned) { return RES_EBUSY; }
    if (vga->failure) { return vga->failure; }
    int error = ael_vga_probe();
    if (error) { return error; }
    uint32_t generation = 0;
    int32_t index = ael_call(SYS_VGA_ACQUIRE, VGA_DEVICE_TEXT, rights, 0, 0, &generation);
    if (index < 0) { return index; }
    vga->handle = (struct resource_handle){(uint32_t)index, generation};
    vga->rights = rights;
    vga->owned = 1;
    return 0;
}

int ael_vga_map(struct ael_vga *vga, uintptr_t address, uint32_t rights)
{
    if (vga == NULL || address != VGA_USER_ADDRESS
        || (rights != RESOURCE_READ && rights != (RESOURCE_READ | RESOURCE_WRITE))) { return RES_EINVAL; }
    if (!vga->owned || vga->failure) { return vga->failure ? vga->failure : RES_ESTALE; }
    if ((rights & vga->rights) != rights) { return RES_EPERM; }
    if (vga->mapped) { return RES_EBUSY; }
    int error = ael_call(SYS_VGA_MAP, vga->handle.index, vga->handle.generation,
                         (uint32_t)address, rights, NULL);
    if (error == 0) {
        vga->rights = rights;
        vga->mapped = 1;
        vga->cells = (volatile uint16_t *)address;
    }
    return result(vga, error);
}

int ael_vga_release(struct ael_vga *vga)
{
    if (vga == NULL) { return RES_EINVAL; }
    if (!vga->owned) { return vga->failure ? vga->failure : RES_ENOENT; }
    int error = ael_call(SYS_VGA_RELEASE, vga->handle.index, vga->handle.generation, 0, 0, NULL);
    if (error == 0) {
        vga->owned = vga->mapped = 0;
        vga->cells = NULL;
    }
    return result(vga, error);
}

int ael_vga_cursor(struct ael_vga *vga, unsigned cell)
{
    if (vga == NULL || cell >= VGA_COLUMNS * VGA_ROWS) { return RES_EINVAL; }
    if (vga->failure) { return vga->failure; }
    if (!vga->owned || !vga->mapped) { return RES_ESTALE; }
    if (!(vga->rights & RESOURCE_WRITE)) { return RES_EPERM; }
    int error = ael_call(SYS_VGA_CURSOR, vga->handle.index, vga->handle.generation, cell, 0, NULL);
    if (error == 0) { vga->cursor = cell; }
    return result(vga, error);
}

int ael_vga_check(struct ael_vga *vga)
{
    /* INFO does not validate authority. CURSOR resolves this WRITE capability.
     * A later forced revoke may still fault a direct store: there is no pin ABI. */
    return vga == NULL ? RES_EINVAL : ael_vga_cursor(vga, vga->cursor);
}

int ael_vga_cell(struct ael_vga *vga, unsigned cell, char ch, uint8_t attribute)
{
    if (cell >= VGA_COLUMNS * VGA_ROWS) { return RES_EINVAL; }
    int error = ael_vga_check(vga);
    if (error == 0) { vga->cells[cell] = (uint16_t)((uint16_t)attribute << 8 | (uint8_t)ch); }
    return error;
}

int ael_vga_fill(struct ael_vga *vga, unsigned first, unsigned count, uint8_t attribute)
{
    const unsigned limit = VGA_COLUMNS * VGA_ROWS;
    if (first > limit || count > limit - first) { return RES_EINVAL; }
    int error = ael_vga_check(vga);
    if (error) { return error; }
    for (unsigned i = first; i < first + count; ++i) { vga->cells[i] = (uint16_t)((uint16_t)attribute << 8 | ' '); }
    return 0;
}

int ael_vga_scroll(struct ael_vga *vga, uint8_t attribute)
{
    int error = ael_vga_check(vga);
    if (error) { return error; }
    for (unsigned i = 0; i < VGA_COLUMNS * (VGA_ROWS - 1); ++i) { vga->cells[i] = vga->cells[i + VGA_COLUMNS]; }
    return ael_vga_fill(vga, VGA_COLUMNS * (VGA_ROWS - 1), VGA_COLUMNS, attribute);
}
