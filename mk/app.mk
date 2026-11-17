# SPDX-License-Identifier: GPL-3.0-or-later
# Installed with the SDK. No source-tree or kernel dependency.
.DEFAULT_GOAL := all
OUT ?= build
SOURCES ?= main.c
OBJECTS := $(patsubst %.c,$(OUT)/%.o,$(SOURCES))
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
APP_PROFILE ?= preempt
APP_POLICY ?= first
APP_DEVICES ?= 0
APP_FLAGS := -m32 -march=i686 -mno-mmx -mno-sse -mno-sse2 -msoft-float \
  -std=c11 -ffreestanding -fno-builtin -fno-pic -fno-pie -fno-stack-protector \
  -fno-common -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-omit-frame-pointer \
  -g3 -O2 -Wall -Wextra -Werror -Wpedantic
RUNTIME := $(AEL_SDK)/lib/runtime-$(APP_PROFILE)-$(APP_POLICY).o
LIBS := $(if $(filter 1,$(APP_DEVICES)),$(AEL_SDK)/lib/libaeldevices.a) $(AEL_SDK)/lib/libaelthuvor.a
.PHONY: all clean app-config-check
all: $(OUT)/app.elf
ifeq ($(filter $(APP_PROFILE),coop preempt),)
$(error APP_PROFILE must be coop or preempt)
endif
ifeq ($(filter $(APP_POLICY),first best),)
$(error APP_POLICY must be first or best)
endif
ifeq ($(filter $(APP_DEVICES),0 1),)
$(error APP_DEVICES must be 0 or 1)
endif
app-config-check:
$(OUT)/.config: app-config-check
	@mkdir -p $(OUT)
	@printf '%s\n' '$(APP_PROFILE) $(APP_POLICY) $(APP_DEVICES) $(SOURCES) $(APP_FLAGS)' > $(OUT)/.config.next
	@if ! cmp -s $(OUT)/.config.next $@; then mv $(OUT)/.config.next $@; else rm -f $(OUT)/.config.next; fi
$(OUT)/%.o: %.c $(OUT)/.config
	mkdir -p $(dir $@)
	$(CC) -I$(AEL_SDK)/include $(APP_FLAGS) -MMD -MP -c $< -o $@
$(OUT)/app.elf: $(OUT)/.config $(OBJECTS) $(AEL_SDK)/lib/crt0.o $(RUNTIME) $(AEL_SDK)/lib/app-entry.o $(LIBS) $(AEL_SDK)/linker.ld
	$(LD) -m elf_i386 -nostdlib --build-id=none -T $(AEL_SDK)/linker.ld -Map=$(OUT)/app.map \
	  $(OBJECTS) $(AEL_SDK)/lib/crt0.o $(RUNTIME) $(AEL_SDK)/lib/app-entry.o $(LIBS) -o $@
clean:
	rm -rf $(OUT)
-include $(OBJECTS:.o=.d)
