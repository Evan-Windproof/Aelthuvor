# SPDX-License-Identifier: GPL-3.0-or-later
.DEFAULT_GOAL := all
# Only public Liberums ABI headers. May be a completely separate SDK directory.
SDK ?= ../../sdk/include
OUT ?= build
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
FLAGS := -m32 -march=i686 -mno-mmx -mno-sse -mno-sse2 -msoft-float \
  -std=c11 -ffreestanding -fno-builtin -fno-pic -fno-pie -fno-stack-protector \
  -fno-common -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-omit-frame-pointer \
  -g3 -O2 -Wall -Wextra -Werror -Wpedantic
APP_SOURCE ?= application.c
SOURCES := runtime.c syscall.c pool.c
OBJECTS := $(SOURCES:%.c=$(OUT)/%.o) $(OUT)/application.o $(OUT)/start.o
.PHONY: all clean
all: $(OUT)/reference.elf
$(OUT)/%.o: %.c pool.h syscall.h
	mkdir -p $(dir $@)
	$(CC) -I$(SDK) $(FLAGS) -MMD -MP -c $< -o $@
$(OUT)/application.o: $(APP_SOURCE) pool.h syscall.h
	mkdir -p $(dir $@)
	$(CC) -I$(SDK) $(FLAGS) -MMD -MP -c $< -o $@
$(OUT)/start.o: start.S
	mkdir -p $(dir $@)
	$(CC) -m32 -march=i686 -g3 -c $< -o $@
$(OUT)/reference.elf: $(OBJECTS) link.ld
	$(LD) -m elf_i386 -nostdlib --build-id=none -T link.ld -Map=$(OUT)/reference.map $(OBJECTS) -o $@
clean:
	rm -rf $(OUT)
-include $(OBJECTS:.o=.d)
