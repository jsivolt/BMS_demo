################################################################################
# make/config.mk -- repository layout, configuration, toolchain and output paths
#
# Included by make/Makefile, which defines REPO and MAKE_DIR beforehand.
#
# Everything here can be overridden from the command line or the environment:
#
#   make CONFIG=Release_RAM
#   make ARMGCC_DIR=/opt/arm-gnu-toolchain-10.3-2021.10-x86_64-arm-none-eabi
#   make SYSROOT=/path/to/sysroot
#   make OUT=/tmp/bms-build
################################################################################

# ---------------------------------------------------------------------------
# Configuration: must be a S32DS configuration directory of this project.
# ---------------------------------------------------------------------------
CONFIG ?= Debug_FLASH
VALID_CONFIGS := Debug_FLASH Release_RAM

ifeq ($(filter $(CONFIG),$(VALID_CONFIGS)),)
  $(error CONFIG='$(CONFIG)' is not a known configuration. Expected one of: $(VALID_CONFIGS))
endif

CONFIG_DIR := $(REPO)/$(CONFIG)
ifeq ($(wildcard $(CONFIG_DIR)/sources.mk),)
  $(error $(CONFIG_DIR)/sources.mk not found - is '$(CONFIG)' a S32DS configuration of this project?)
endif

# Linker script per configuration, mirroring the S32DS project settings. The
# value is cross-checked against the link response file by gen_sources.py.
ifeq ($(CONFIG),Debug_FLASH)
  LDSCRIPT := linker_flash_s32k344.ld
else
  LDSCRIPT := linker_ram_s32k344.ld
endif
LDFILE := $(REPO)/Project_Settings/Linker_Files/$(LDSCRIPT)
ifeq ($(wildcard $(LDFILE)),)
  $(error linker script not found: $(LDFILE))
endif

# ---------------------------------------------------------------------------
# Vendored NXP SDK headers (see sdk/README.md).
# ---------------------------------------------------------------------------
SDK_DIR      ?= $(REPO)/sdk
SDK_BASE     ?= $(SDK_DIR)/base
SDK_PLATFORM ?= $(SDK_DIR)/platform

# ---------------------------------------------------------------------------
# Toolchain. ARMGCC_DIR is the toolchain install prefix, i.e. the directory that
# contains bin/arm-none-eabi-gcc. Leave it empty to use arm-none-eabi-* from PATH.
# ---------------------------------------------------------------------------
ARMGCC_DIR ?=

ifneq ($(strip $(ARMGCC_DIR)),)
  TOOLCHAIN_PREFIX := $(ARMGCC_DIR)/bin/arm-none-eabi-
else
  TOOLCHAIN_PREFIX := arm-none-eabi-
endif

CC      := $(TOOLCHAIN_PREFIX)gcc
SIZE    := $(TOOLCHAIN_PREFIX)size
NM      := $(TOOLCHAIN_PREFIX)nm
OBJDUMP := $(TOOLCHAIN_PREFIX)objdump

# Python is used only to derive sources and flags from the S32DS files. On Linux
# 'python3' is normally enough; under the S32DS MSYS shell (Windows) the user PATH
# is not inherited, so pass PYTHON=<path to python.exe> there.
PYTHON ?= $(or $(shell command -v python3 2>/dev/null),$(shell command -v python 2>/dev/null),$(shell command -v py 2>/dev/null))

# Toolchain release this build is verified against. A mismatch is a warning, not
# an error, so a different 10.x or newer toolchain can be tried without editing.
EXPECTED_TOOLCHAIN_VERSION := 10.3

# ---------------------------------------------------------------------------
# Output. make/build/ is pure runtime output and is never committed.
# ---------------------------------------------------------------------------
OUT      ?= $(MAKE_DIR)/build/$(CONFIG)
OBJ_DIR  := $(OUT)/obj
GEN_DIR  := $(OUT)/.gen
ELF      := $(OUT)/BMS_demo.elf
MAP      := $(OUT)/BMS_demo.map
SRC_FRAG := $(GEN_DIR)/sources-$(CONFIG).mk

# The S32DS response files pass --sysroot=<gcc>/arm-none-eabi/lib, a Windows path
# that cannot be migrated. It is opt-in here; see 'make check-toolchain'.
SYSROOT_FLAG := $(if $(strip $(SYSROOT)),--sysroot=$(SYSROOT),)
