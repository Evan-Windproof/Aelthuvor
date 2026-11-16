/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aelthuvor/keyboard.h>
#include <stddef.h>

void ael_key_reset(struct ael_key_decoder *decoder)
{
    *decoder = (struct ael_key_decoder){0};
}

static int result(struct ael_keyboard *keyboard, int error)
{
    if (error == RES_ESTALE || error == RES_EPERM || error == -38) {
        keyboard->failure = error;
        keyboard->owned = 0;
        ael_key_reset(&keyboard->decoder);
    }
    return error;
}

int ael_keyboard_probe(void)
{
    uint32_t format = 0;
    int32_t capacity = ael_call(SYS_KBD_INFO, KBD_DEVICE_PS2, 0, 0, 0, &format);
    if (capacity < 0) { return capacity; }
    return capacity == KBD_QUEUE_CAPACITY && format == KBD_SCANSET1 ? 0 : RES_EPROTONOSUPPORT;
}

int ael_keyboard_acquire(struct ael_keyboard *keyboard)
{
    if (keyboard == NULL) { return RES_EINVAL; }
    if (keyboard->failure) { return keyboard->failure; }
    if (keyboard->owned) { return RES_EBUSY; }
    int error = ael_keyboard_probe();
    if (error) { return error; }
    uint32_t generation = 0;
    int32_t index = ael_call(SYS_KBD_ACQUIRE, KBD_DEVICE_PS2, 0, 0, 0, &generation);
    if (index < 0) { return index; }
    keyboard->handle = (struct resource_handle){(uint32_t)index, generation};
    keyboard->owned = 1;
    ael_key_reset(&keyboard->decoder);
    return 0;
}

int ael_keyboard_poll(struct ael_keyboard *keyboard, uint32_t *raw)
{
    if (keyboard == NULL || raw == NULL) { return RES_EINVAL; }
    *raw = 0;
    if (!keyboard->owned) { return keyboard->failure ? keyboard->failure : RES_ESTALE; }
    int error = ael_call(SYS_KBD_POLL, keyboard->handle.index, keyboard->handle.generation, 0, 0, raw);
    if (error == RES_EOVERFLOW) { ael_key_reset(&keyboard->decoder); }
    if (error == 0 && *raw > 255) { error = RES_EINVAL; }
    return result(keyboard, error);
}

int ael_keyboard_stats(struct ael_keyboard *keyboard, uint32_t *count, uint32_t *lost)
{
    if (keyboard == NULL || count == NULL || lost == NULL) { return RES_EINVAL; }
    *count = *lost = 0;
    if (!keyboard->owned) { return keyboard->failure ? keyboard->failure : RES_ESTALE; }
    int32_t available = ael_call(SYS_KBD_STATS, keyboard->handle.index, keyboard->handle.generation, 0, 0, lost);
    if (available < 0) { return result(keyboard, available); }
    if ((uint32_t)available > KBD_QUEUE_CAPACITY) { return RES_EINVAL; }
    *count = (uint32_t)available;
    return 0;
}

int ael_keyboard_release(struct ael_keyboard *keyboard)
{
    if (keyboard == NULL) { return RES_EINVAL; }
    if (!keyboard->owned) { return keyboard->failure ? keyboard->failure : RES_ENOENT; }
    int error = ael_call(SYS_KBD_RELEASE, keyboard->handle.index, keyboard->handle.generation, 0, 0, NULL);
    if (error == 0) { keyboard->owned = 0; ael_key_reset(&keyboard->decoder); }
    return result(keyboard, error);
}

char ael_key_decode(struct ael_key_decoder *d, uint8_t raw)
{
    static const char plain[128] = {
        [2]='1',[3]='2',[4]='3',[5]='4',[6]='5',[7]='6',[8]='7',[9]='8',[10]='9',[11]='0',
        [12]='-',[13]='=',[14]='\b',[16]='q',[17]='w',[18]='e',[19]='r',[20]='t',[21]='y',
        [22]='u',[23]='i',[24]='o',[25]='p',[26]='[',[27]=']',[28]='\n',[30]='a',[31]='s',
        [32]='d',[33]='f',[34]='g',[35]='h',[36]='j',[37]='k',[38]='l',[39]=';',[40]='\'',
        [41]='`',[43]='\\',[44]='z',[45]='x',[46]='c',[47]='v',[48]='b',[49]='n',[50]='m',
        [51]=',',[52]='.',[53]='/',[57]=' '
    };
    static const char shifted[128] = {
        [2]='!',[3]='@',[4]='#',[5]='$',[6]='%',[7]='^',[8]='&',[9]='*',[10]='(',[11]=')',
        [12]='_',[13]='+',[26]='{',[27]='}',[39]=':',[40]='"',[41]='~',[43]='|',[51]='<',[52]='>',[53]='?'
    };
    if (d->pause) { --d->pause; return 0; }
    if (raw == 0xe1) { d->pause = 5; d->extended = 0; return 0; }
    if (raw == 0xe0) { d->extended = 1; return 0; }
    if (d->extended) { d->extended = 0; return 0; }
    unsigned code = raw & 0x7f, released = raw & 0x80;
    if (code == 42 || code == 54) {
        unsigned bit = code == 42 ? 1u : 2u;
        if (released) { d->shifts &= ~bit; } else { d->shifts |= bit; }
        return 0;
    }
    if (code == 58) {
        if (!released && !d->caps_down) { d->caps ^= 1; }
        d->caps_down = !released;
        return 0;
    }
    if (released) { return 0; }
    char ch = plain[code];
    if (ch >= 'a' && ch <= 'z') {
        if ((d->shifts != 0) != (d->caps != 0)) { ch = (char)(ch - 'a' + 'A'); }
    } else if (d->shifts && shifted[code]) { ch = shifted[code]; }
    return ch;
}
