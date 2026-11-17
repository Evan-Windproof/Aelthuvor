# SPDX-License-Identifier: GPL-3.0-or-later
SOURCES := tests/fixtures/app_exit.c
include $(AEL_SDK)/app.mk
APP_FLAGS += $(if $(filter 1,$(FAULT)),-DAPP_FAULT)
