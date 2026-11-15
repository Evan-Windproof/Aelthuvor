# SPDX-License-Identifier: GPL-3.0-or-later
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
CROSS_COMPILE ?=
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
AR = $(CROSS_COMPILE)ar
HOST_CC ?= gcc
PYTHON ?= python3
LIBERUMS ?= ../Liberums
QEMU ?= qemu-system-i386
GDB ?= gdb
ARCH := -m32 -march=i686 -mno-mmx -mno-sse -mno-sse2 -msoft-float
WARN := -Wall -Wextra -Werror -Wpedantic
CFLAGS := $(ARCH) -std=c11 -ffreestanding -fno-builtin -fno-pic -fno-pie \
          -fno-stack-protector -fno-common -fno-asynchronous-unwind-tables \
          -fno-unwind-tables -fno-omit-frame-pointer -g3 -O2 $(WARN)
CPPFLAGS := -Iinclude -Isdk/include -Itests
CORE := src/syscall.c src/page.c src/memory.c src/compat.c src/string.c
OBJECTS := $(CORE:%.c=build/%.o)
HEADERS := $(wildcard include/aelthuvor/*.h sdk/include/abi/*.h) tests/memory_checks.h
VARIANTS := first best fault readonly exit-live runtime-abi
.PHONY: all clean test check-sdk integration
all: check-sdk build/aelthuvor.elf
check-sdk:
	$(PYTHON) tests/check_sdk.py $(if $(SDK_SOURCE),--source "$(SDK_SOURCE)")
build/%.o: %.c $(HEADERS) Makefile
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@
build/startup.o: arch/x86/startup.S Makefile
	mkdir -p build
	$(CC) $(ARCH) -g3 -c $< -o $@
build/libaelthuvor.a: $(OBJECTS)
	$(AR) rcs $@ $^
build/%/runtime.o: src/runtime.c $(HEADERS) Makefile
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(VARIANT_FLAGS) -c $< -o $@
build/%/demo.o: examples/memory_demo.c $(HEADERS) Makefile
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(VARIANT_FLAGS) -c $< -o $@
build/best/runtime.o: VARIANT_FLAGS = -DAEL_BEST_FIT_BUILD
build/fault/demo.o: VARIANT_FLAGS = -DAEL_FAULT_BUILD
build/readonly/demo.o: VARIANT_FLAGS = -DAEL_READONLY_BUILD
build/exit-live/demo.o: VARIANT_FLAGS = -DAEL_EXIT_LIVE_BUILD
build/runtime-abi/runtime.o: VARIANT_FLAGS = -DAEL_REQUIRED_MAJOR=2
build/%.elf: build/%/runtime.o build/%/demo.o build/startup.o build/tests/memory_checks.o build/libaelthuvor.a linker.ld
	$(LD) -m elf_i386 -nostdlib --build-id=none -T linker.ld -Map=$(@:.elf=.map) $(filter %.o %.a,$^) -o $@
build/aelthuvor.elf: build/first.elf
	cp $< $@
build/memory-tests: tests/memory_tests.c tests/memory_checks.c src/memory.c src/page.c src/compat.c $(HEADERS) Makefile
	mkdir -p build
	$(HOST_CC) -std=c11 -g -O1 $(WARN) -fsanitize=address,undefined -fno-omit-frame-pointer \
		$(CPPFLAGS) $(filter %.c,$^) -o $@
test: all build/memory-tests
	$(PYTHON) tests/sdk_tests.py
	./build/memory-tests
integration: all $(addprefix build/,$(addsuffix .elf,$(VARIANTS)))
	$(PYTHON) tests/integration.py --liberums "$(LIBERUMS)" --qemu "$(QEMU)" --gdb "$(GDB)"
clean:
	rm -rf build
.SECONDARY:
-include $(OBJECTS:.o=.d) build/tests/memory_checks.d
