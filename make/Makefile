# ==============================================================================
#  BMS_demo - standalone command-line build
#
#  Mirrors the S32DS "Debug_FLASH" managed build (compiler flags, defines,
#  include paths, linker script) but depends only on the GNU Arm toolchain
#  bundled with S32DS - no Eclipse, no headless launcher.
#
#  Usage (from anywhere):
#     make -f make/Makefile                 # build Debug_FLASH
#     make -f make/Makefile CONFIG=Release_FLASH
#     make -f make/Makefile -j              # parallel
#     make -f make/Makefile clean
#     make -f make/Makefile flash-hex       # just (re)emit .hex/.bin/.srec
#
#  Or:  cd make && make
#
#  Override the toolchain / SDK location on the command line or in the
#  environment if S32DS is installed elsewhere, e.g.
#     make -f make/Makefile S32DS=/opt/nxp/S32DS.3.6.10/S32DS
# ==============================================================================

# ---- project root (this file lives in <root>/make/) --------------------------
TOP := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)

# ---- build configuration ----------------------------------------------------
CONFIG ?= Debug_FLASH

ifeq ($(CONFIG),Debug_FLASH)
  LDSCRIPT := $(TOP)/Project_Settings/Linker_Files/linker_flash_s32k344.ld
  OPT      := -Os -ggdb3
else ifeq ($(CONFIG),Release_FLASH)
  LDSCRIPT := $(TOP)/Project_Settings/Linker_Files/linker_flash_s32k344.ld
  OPT      := -Os
else ifeq ($(CONFIG),Debug_RAM)
  LDSCRIPT := $(TOP)/Project_Settings/Linker_Files/linker_ram_s32k344.ld
  OPT      := -Os -ggdb3
else ifeq ($(CONFIG),Release_RAM)
  LDSCRIPT := $(TOP)/Project_Settings/Linker_Files/linker_ram_s32k344.ld
  OPT      := -Os
else
  $(error Unknown CONFIG '$(CONFIG)'  (Debug_FLASH|Release_FLASH|Debug_RAM|Release_RAM))
endif

TARGET := BMS_demo
BUILD  ?= $(TOP)/make/build/$(CONFIG)

# ---- toolchain -------------------------------------------------------------
S32DS      ?= C:/NXP/S32DS.3.6.10/S32DS
BUILD_TOOLS ?= $(S32DS)/build_tools
TOOLCHAIN  ?= $(BUILD_TOOLS)/gcc_v11.4/gcc-11.4-arm32-eabi
CROSS      ?= $(TOOLCHAIN)/bin/arm-none-eabi-
SYSROOT    ?= $(TOOLCHAIN)/arm-none-eabi/lib

CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
OBJDUMP := $(CROSS)objdump
SIZE    := $(CROSS)size

# ---- Platform SDK (RTD BaseNXP + Platform headers) -------------------------
SDK ?= $(S32DS)/software/PlatformSDK_S32K3/RTD

# ==============================================================================
#  Sources
# ==============================================================================
C_SRCS := \
  $(wildcard $(TOP)/src/*.c) \
  $(wildcard $(TOP)/src/app/*.c) \
  $(wildcard $(TOP)/src/battery/*.c) \
  $(wildcard $(TOP)/src/battery/vAFE/*.c) \
  $(wildcard $(TOP)/src/battery/vPACK/*.c) \
  $(wildcard $(TOP)/src/communication/*.c) \
  $(wildcard $(TOP)/src/control/*.c) \
  $(wildcard $(TOP)/src/drivers/*.c) \
  $(wildcard $(TOP)/src/safety/*.c) \
  $(wildcard $(TOP)/src/storage/*.c) \
  $(wildcard $(TOP)/generate/src/*.c) \
  $(wildcard $(TOP)/board/*.c) \
  $(wildcard $(TOP)/RTD/src/*.c) \
  $(TOP)/Project_Settings/Startup_Code/exceptions.c \
  $(TOP)/Project_Settings/Startup_Code/nvic.c \
  $(TOP)/Project_Settings/Startup_Code/startup.c \
  $(TOP)/Project_Settings/Startup_Code/system.c

S_SRCS := \
  $(TOP)/Project_Settings/Startup_Code/Vector_Table.s \
  $(TOP)/Project_Settings/Startup_Code/startup_cm7.s

OBJS := \
  $(patsubst $(TOP)/%.c,$(BUILD)/%.o,$(C_SRCS)) \
  $(patsubst $(TOP)/%.s,$(BUILD)/%.o,$(S_SRCS))

DEPS := $(OBJS:.o=.d)

# ==============================================================================
#  Flags  (kept in lock-step with the S32DS *.args files)
# ==============================================================================
INCLUDES := \
  -I$(TOP)/RTD/include \
  -I$(TOP)/generate/include \
  -I$(TOP)/board \
  -I$(TOP)/src \
  -I$(TOP)/src/app \
  -I$(TOP)/src/battery \
  -I$(TOP)/src/battery/vAFE \
  -I$(TOP)/src/battery/vPACK \
  -I$(TOP)/src/communication \
  -I$(TOP)/src/control \
  -I$(TOP)/src/drivers \
  -I$(TOP)/src/safety \
  -I$(TOP)/src/storage \
  -I"$(SDK)/BaseNXP_TS_T40D34M70I1R0/header" \
  -I"$(SDK)/BaseNXP_TS_T40D34M70I1R0/include" \
  -I"$(SDK)/Platform_TS_T40D34M70I1R0/include" \
  -I"$(SDK)/Platform_TS_T40D34M70I1R0/startup/include"

DEFS := \
  -DD_CACHE_ENABLE -DI_CACHE_ENABLE -DENABLE_FPU -DMPU_ENABLE \
  -DGCC -DCPU_S32K344 -DCPU_CORTEX_M7

ARCH := \
  -mcpu=cortex-m7 -mthumb -mlittle-endian \
  -mfloat-abi=hard -mfpu=fpv5-sp-d16 \
  -specs=nano.specs -specs=nosys.specs

WARN := \
  -pedantic -Wall -Wextra -Wunused -Wstrict-prototypes -Wsign-compare \
  -Werror=implicit-function-declaration -Wundef -Wdouble-promotion

CFLAGS := \
  -std=c99 $(OPT) $(ARCH) $(DEFS) $(WARN) \
  -funsigned-char -fomit-frame-pointer -fno-short-enums \
  -funsigned-bitfields -fno-common \
  --sysroot="$(SYSROOT)"

ASFLAGS := \
  -x assembler-with-cpp -ggdb3 $(ARCH) \
  -I$(TOP)/RTD/include -I$(TOP)/board \
  --sysroot="$(SYSROOT)"

LDFLAGS := \
  -nostartfiles --entry=Reset_Handler -ggdb3 $(ARCH) \
  -T "$(LDSCRIPT)" \
  -Wl,-Map,"$(BUILD)/$(TARGET).map" \
  --sysroot="$(SYSROOT)"

LDLIBS := -lc -lm -lgcc

# ==============================================================================
#  Rules
# ==============================================================================
.PHONY: all clean size disasm flash-hex
.DEFAULT_GOAL := all

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).hex $(BUILD)/$(TARGET).bin size

$(BUILD)/$(TARGET).elf: $(OBJS) $(LDSCRIPT)
	@echo '  LD    $@'
	@$(CC) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@ \
	 || $(CC) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@

# The '|| retry' guards against an intermittent MSYS2/Windows CreateProcess
# race ("cannot execute cc1.exe" / "/bin/sh: Permission denied") that shows up
# under -j on machines with real-time AV scanning. Harmless on a clean spawn.
$(BUILD)/%.o: $(TOP)/%.c
	@mkdir -p $(dir $@)
	@echo '  CC    $<'
	@$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c -o $@ $< \
	 || $(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c -o $@ $<

$(BUILD)/%.o: $(TOP)/%.s
	@mkdir -p $(dir $@)
	@echo '  AS    $<'
	@$(CC) $(ASFLAGS) -MMD -MP -c -o $@ $< \
	 || $(CC) $(ASFLAGS) -MMD -MP -c -o $@ $<

$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).elf
	@echo '  HEX   $@'
	@$(OBJCOPY) -O ihex $< $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	@echo '  BIN   $@'
	@$(OBJCOPY) -O binary $< $@

flash-hex: $(BUILD)/$(TARGET).hex $(BUILD)/$(TARGET).bin
	@$(OBJCOPY) -O srec --srec-len=32 $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).srec
	@echo '  SREC  $(BUILD)/$(TARGET).srec'

size: $(BUILD)/$(TARGET).elf
	@echo ''
	@$(SIZE) --format=berkeley $<

disasm: $(BUILD)/$(TARGET).elf
	@$(OBJDUMP) -d -S $< > $(BUILD)/$(TARGET).lst
	@echo '  LST   $(BUILD)/$(TARGET).lst'

clean:
	@rm -rf $(TOP)/make/build
	@echo '  CLEAN make/build'

-include $(DEPS)
