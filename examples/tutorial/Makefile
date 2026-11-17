# SPDX-License-Identifier: GPL-3.0-or-later
AEL_SDK ?= ../../build/app-sdk
LESSON ?= 01-hello
SOURCES := $(LESSON).c
APP_DEVICES ?= $(if $(filter 05-console,$(LESSON)),1,0)
include $(AEL_SDK)/app.mk
ifeq ($(LESSON):$(APP_DEVICES),10-project:1)
APP_FLAGS += -DPROJECT_CONSOLE
endif
