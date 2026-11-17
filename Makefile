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
CORE := src/syscall.c src/page.c src/memory.c src/compat.c src/string.c src/revoke.c src/app.c
OBJECTS := $(CORE:%.c=build/%.o)
HEADERS := $(wildcard include/aelthuvor/*.h sdk/include/abi/*.h) tests/memory_checks.h
VARIANTS := first best fault readonly exit-live runtime-abi preempt-first preempt-best
.PHONY: all clean test check-sdk integration profiles abi11
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
build/preempt-first/runtime.o build/preempt-first/demo.o: VARIANT_FLAGS = -DAEL_PREEMPTIVE_BUILD
build/preempt-best/runtime.o build/preempt-best/demo.o: VARIANT_FLAGS = -DAEL_PREEMPTIVE_BUILD -DAEL_BEST_FIT_BUILD
build/best/runtime.o: VARIANT_FLAGS = -DAEL_BEST_FIT_BUILD
build/fault/demo.o: VARIANT_FLAGS = -DAEL_FAULT_BUILD
build/readonly/demo.o: VARIANT_FLAGS = -DAEL_READONLY_BUILD
build/exit-live/demo.o: VARIANT_FLAGS = -DAEL_EXIT_LIVE_BUILD
build/runtime-abi/runtime.o: VARIANT_FLAGS = -DAEL_REQUIRED_MAJOR=2
build/%.elf: build/%/runtime.o build/%/demo.o build/startup.o build/tests/memory_checks.o build/libaelthuvor.a linker.ld
	$(LD) -m elf_i386 -nostdlib --build-id=none -T linker.ld -Map=$(@:.elf=.map) $(filter %.o %.a,$^) -o $@
build/aelthuvor.elf: build/first.elf
	cp $< $@
build/memory-tests: tests/memory_tests.c tests/memory_checks.c src/memory.c src/page.c src/compat.c src/revoke.c $(HEADERS) Makefile
	mkdir -p build
	$(HOST_CC) -std=c11 -g -O1 $(WARN) -fsanitize=address,undefined -fno-omit-frame-pointer \
		$(CPPFLAGS) $(filter %.c,$^) -o $@
build/revoke-tests: tests/revoke_tests.c src/memory.c src/page.c src/compat.c src/revoke.c $(HEADERS) Makefile
	mkdir -p build
	$(HOST_CC) -std=c11 -g -O1 $(WARN) -fsanitize=address,undefined -fno-omit-frame-pointer \
		$(CPPFLAGS) $(filter %.c,$^) -o $@
profiles: all build/best.elf build/preempt-first.elf build/preempt-best.elf
abi11:
	$(PYTHON) tests/legacy_build.py

test: profiles console build/memory-tests build/revoke-tests build/device-tests build/app-tests build/reference-tests
	$(PYTHON) tests/sdk_tests.py
	./build/memory-tests
	./build/revoke-tests
	./build/device-tests
	./build/app-tests
	./build/reference-tests
integration: abi11 all $(addprefix build/,$(addsuffix .elf,$(VARIANTS)))
	$(PYTHON) tests/integration.py --liberums "$(LIBERUMS)" --qemu "$(QEMU)" --gdb "$(GDB)"
clean:
	rm -rf build
.SECONDARY:
-include $(OBJECTS:.o=.d) build/tests/memory_checks.d

DEVICE_SOURCES := src/vga.c src/keyboard.c src/text.c
DEVICE_OBJECTS := $(DEVICE_SOURCES:%.c=build/%.o)
CONSOLE_HEADERS := $(wildcard include/aelthuvor/*.h sdk/include/abi/*.h) apps/console/console.h
.PHONY: console console-integration
console: check-sdk build/ael-console.elf
build/ael-console.elf: build/console.elf
	cp $< $@
build/console/runtime.o build/console-best/runtime.o: VARIANT_FLAGS = -DAEL_PREEMPTIVE_BUILD
build/console-best/runtime.o: VARIANT_FLAGS += -DAEL_BEST_FIT_BUILD
build/console/main.o: apps/console/main.c $(CONSOLE_HEADERS) Makefile
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/console-test/main.o: apps/console/main.c $(CONSOLE_HEADERS) Makefile
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DAEL_CONSOLE_DIAGNOSTIC -c $< -o $@
build/console/app.o: apps/console/console.c $(CONSOLE_HEADERS) Makefile
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/console.elf build/console-best.elf: build/console/main.o build/console/app.o $(DEVICE_OBJECTS) build/startup.o build/libaelthuvor.a linker.ld
build/console.elf: build/console/runtime.o
build/console-best.elf: build/console-best/runtime.o
build/console-test.elf: build/console-test/main.o build/console/app.o $(DEVICE_OBJECTS) build/console/runtime.o build/startup.o build/libaelthuvor.a linker.ld
build/console.elf build/console-best.elf build/console-test.elf:
	$(LD) -m elf_i386 -nostdlib --build-id=none -T linker.ld -Map=$(@:.elf=.map) $(filter %.o,$^) $(filter %.a,$^) -o $@
build/device-tests: tests/device_tests.c apps/console/console.c $(DEVICE_SOURCES) src/memory.c src/page.c src/compat.c src/revoke.c $(CONSOLE_HEADERS) Makefile
	mkdir -p build
	$(HOST_CC) -std=c11 -g -O1 $(WARN) -fsanitize=address,undefined -fno-omit-frame-pointer $(CPPFLAGS) -Iapps/console $(filter %.c,$^) -o $@
console-integration: console build/console-best.elf build/console-test.elf build/device-probe.elf
	$(PYTHON) tests/console_integration.py --liberums "$(LIBERUMS)" --qemu "$(QEMU)" --gdb "$(GDB)"

build/console/device-probe.o: apps/console/device_probe.c $(CONSOLE_HEADERS) Makefile
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/device-probe.elf: build/console/device-probe.o $(DEVICE_OBJECTS) build/console/runtime.o build/startup.o build/libaelthuvor.a linker.ld
	$(LD) -m elf_i386 -nostdlib --build-id=none -T linker.ld -Map=$(@:.elf=.map) $(filter %.o,$^) $(filter %.a,$^) -o $@

# Application SDK: entry adapters are explicit objects, never archive mains.
APP_RUNTIMES := $(foreach p,coop preempt,$(foreach s,first best,build/api-$(p)-$(s)/runtime.o))
build/api-preempt-first/runtime.o: VARIANT_FLAGS = -DAEL_PREEMPTIVE_BUILD
build/api-preempt-best/runtime.o: VARIANT_FLAGS = -DAEL_PREEMPTIVE_BUILD -DAEL_BEST_FIT_BUILD
build/api-coop-best/runtime.o: VARIANT_FLAGS = -DAEL_BEST_FIT_BUILD
build/libaeldevices.a: $(DEVICE_OBJECTS) build/src/app_console.o
	$(AR) rcs $@ $^
build/src/app.o build/src/app_entry.o build/src/app_console.o: src/app_internal.h
build/app-sdk/.stamp: build/libaelthuvor.a build/libaeldevices.a build/startup.o build/src/app_entry.o $(APP_RUNTIMES) $(HEADERS) linker.ld mk/app.mk sdk/manifest.json scripts/export_sdk.py
	$(PYTHON) scripts/export_sdk.py
.PHONY: app-sdk apps app-hello app-memory app-console app-template
app-sdk: check-sdk build/app-sdk/.stamp
apps: app-hello app-memory app-console app-template
app-hello: app-sdk
	$(MAKE) -C apps/hello AEL_SDK="$(abspath build/app-sdk)" OUT="$(abspath build/apps/hello)"
app-memory: app-sdk
	$(MAKE) -C apps/memory AEL_SDK="$(abspath build/app-sdk)" OUT="$(abspath build/apps/memory)"
	$(MAKE) -C apps/memory AEL_SDK="$(abspath build/app-sdk)" OUT="$(abspath build/apps/memory-best)" APP_POLICY=best
app-console: app-sdk
	$(MAKE) -C apps/sdk-console AEL_SDK="$(abspath build/app-sdk)" OUT="$(abspath build/apps/console)"
app-template: app-sdk
	$(MAKE) -C examples/application-template AEL_SDK="$(abspath build/app-sdk)" OUT="$(abspath build/apps/template)"
.PHONY: independent-libos
independent-libos: check-sdk
	$(MAKE) -C reference/independent-libos SDK="$(abspath sdk/include)" OUT="$(abspath build/independent-libos)"
build/app-tests: tests/app_tests.c src/app.c src/app_console.c $(DEVICE_SOURCES) src/compat.c src/memory.c src/page.c src/revoke.c $(HEADERS) src/app_internal.h Makefile
	mkdir -p build
	$(HOST_CC) -std=c11 -g -O1 $(WARN) -fsanitize=address,undefined -fno-omit-frame-pointer $(CPPFLAGS) $(filter %.c,$^) -o $@
build/reference-tests: tests/reference_tests.c reference/independent-libos/pool.c reference/independent-libos/pool.h reference/independent-libos/syscall.h Makefile
	mkdir -p build
	$(HOST_CC) -std=c11 -g -O1 $(WARN) -fsanitize=address,undefined -fno-omit-frame-pointer -Isdk/include $(filter %.c,$^) -o $@
.PHONY: sdk-tests
sdk-tests: apps independent-libos
	$(PYTHON) tests/sdk_builds.py
.PHONY: phase4-fixtures
phase4-fixtures: app-sdk
	$(MAKE) -C apps/hello AEL_SDK="$(abspath build/app-sdk)" OUT="$(abspath build/apps/hello-coop)" APP_PROFILE=coop
	$(MAKE) -f mk/fixture.mk AEL_SDK="$(abspath build/app-sdk)" OUT="$(abspath build/apps/leak)"
	$(MAKE) -f mk/fixture.mk AEL_SDK="$(abspath build/app-sdk)" OUT="$(abspath build/apps/fault)" FAULT=1
	$(MAKE) -C reference/independent-libos SDK="$(abspath sdk/include)" OUT="$(abspath build/independent-diagnostic)" APP_SOURCE=diagnostic-app.c
.PHONY: phase4-integration
phase4-integration: apps independent-libos phase4-fixtures
	$(PYTHON) tests/phase4_integration.py --liberums "$(LIBERUMS)" --qemu "$(QEMU)" --gdb "$(GDB)"

.PHONY: phase5-fixtures phase5-integration
phase5-fixtures: app-sdk independent-libos
	$(MAKE) -f mk/reliability.mk AEL_SDK="$(abspath build/app-sdk)" OUT="$(abspath build/phase9/ael-first)" APP_PROFILE=preempt APP_POLICY=first
	$(MAKE) -f mk/reliability.mk AEL_SDK="$(abspath build/app-sdk)" OUT="$(abspath build/phase9/ael-best)" APP_PROFILE=preempt APP_POLICY=best
phase5-integration: phase5-fixtures
	$(PYTHON) tests/phase5_integration.py --liberums "$(LIBERUMS)" --qemu "$(QEMU)" --gdb "$(GDB)"
