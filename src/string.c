/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Freestanding aggregate initialization may generate these compiler calls. */
#include <stddef.h>
void *memset(void *destination, int value, size_t length)
{
    unsigned char *bytes = destination;
    for (size_t i = 0; i < length; ++i) {
        bytes[i] = (unsigned char)value;
    }
    return destination;
}
void *memcpy(void *destination, const void *source, size_t length)
{
    unsigned char *out = destination;
    const unsigned char *in = source;
    for (size_t i = 0; i < length; ++i) {
        out[i] = in[i];
    }
    return destination;
}
