################################################################################
# Portable (non-S32DS) build for BMS_demo -- S32K344, Cortex-M7.
#
# This build is a peer of the S32DS project, not a replacement for it:
#
#   * S32DS owns the project definition. The source list, compiler flags,
#     assembler flags and link line are *derived* from the CDT-generated files
#     of the selected configuration by make/gen_sources.py, so the two builds
#     cannot drift apart.
#   * Nothing under <CONFIG>/ (.args, .mk, .cproject) is written by this build.
#   * Output goes to make/build/<CONFIG>/, so it never mixes with the objects
#     the S32DS IDE produces in <CONFIG>/.
#
# Usage (from the repository root, or with -C make):
#
#   make                       # CONFIG=Debug_FLASH (default)
#   make CONFIG=Release_RAM
#   make -j$(nproc)
#   make clean                 # removes make/build/<CONFIG> only
#   make check-toolchain       # toolchain identity, specs and libraries
#   make print-config          # configuration, paths and flags in use
#   make vendor-sdk            # re-copy sdk/ from an S32DS install
#   make help
#
# Overridable variables (see make/config.mk): CONFIG, ARMGCC_DIR, SYSROOT, OUT,
# SDK_BASE, SDK_PLATFORM, PYTHON.
################################################################################

MKFILE   := $(lastword $(MAKEFILE_LIST))
MAKE_DIR := $(patsubst %/,%,$(dir $(abspath $(MKFILE))))
REPO     := $(patsubst %/,%,$(dir $(MAKE_DIR)))

include $(MAKE_DIR)/config.mk

# SUBDIRS (the folders that contribute sources) comes from the configuration's
# own sources.mk, so the generated fragment is remade whenever S32DS changes it.
# Note that sources.mk also (re)sets an empty C_SRCS/S_SRCS; the fragment is
# included afterwards and therefore wins.
include $(CONFIG_DIR)/sources.mk

SUBDIR_MKS := $(wildcard $(addsuffix /subdir.mk,$(addprefix $(CONFIG_DIR)/,$(SUBDIRS))))

# 'make clean' must work even when the generated fragment cannot be produced
# (no python, or the vendored SDK headers are absent): those are only needed to
# build, and clean has to stay usable as the recovery path.
ifneq ($(MAKECMDGOALS),clean)
-include $(SRC_FRAG)
endif

.DEFAULT_GOAL := all

# ---------------------------------------------------------------------------
# Sources, flags and link order (generated)
# ---------------------------------------------------------------------------
$(SRC_FRAG): $(CONFIG_DIR)/sources.mk $(SUBDIR_MKS) $(CONFIG_DIR)/BMS_demo.args $(MAKE_DIR)/gen_sources.py
	@command -v "$(firstword $(PYTHON))" >/dev/null 2>&1 || { \
		echo "Python 3 not found (PYTHON='$(PYTHON)')."; \
		echo 'The source list and compiler flags are derived from the S32DS configuration, so a'; \
		echo 'Python 3 interpreter is required. Retry with: make PYTHON=/path/to/python3'; \
		exit 1; \
	}
	@mkdir -p $(@D)
	@$(PYTHON) "$(MAKE_DIR)/gen_sources.py" --repo "$(REPO)" --config "$(CONFIG)" --out "$@"

# ---------------------------------------------------------------------------
# Compile. The recipes mirror the CDT-generated ones: same flag text (from the
# configuration's response files), same -MMD/-MP dependency generation, same
# assembler invocation.
# ---------------------------------------------------------------------------
$(OBJ_DIR)/%.o: $(REPO)/%.c
	@mkdir -p $(@D)
	@echo '  CC      $<'
	@$(CC) $(CFLAGS) -MMD -MP -MF "$(@:.o=.d)" -MT "$@" -o "$@" "$<"

$(OBJ_DIR)/%.o: $(REPO)/%.s
	@mkdir -p $(@D)
	@echo '  AS      $<'
	@$(CC) $(ASFLAGS) -o "$@" "$<"

# ---------------------------------------------------------------------------
# Link
# ---------------------------------------------------------------------------
all: $(ELF)

$(ELF): $(LINK_OBJS) $(LDFILE) $(SRC_FRAG)
	@mkdir -p $(@D)
	@echo '  LD      $@'
	@$(CC) -o "$@" $(LDFLAGS_OPT) $(LINK_OBJS) $(LDFLAGS_LIB)
	@$(SIZE) --format=berkeley "$@"

size: $(ELF)
	@$(SIZE) --format=berkeley "$(ELF)"

# Nothing here can be built if a header moved, so keep the generated
# dependencies of the previous build (they are rewritten by -MMD).
ifneq ($(MAKECMDGOALS),clean)
-include $(LINK_OBJS:.o=.d)
endif

# ---------------------------------------------------------------------------
# Housekeeping
# ---------------------------------------------------------------------------
clean:
	@rm -rf "$(OUT)"
	@echo '  RM      $(OUT)'

check-toolchain:
	@missing=''; for d in "$(SDK_BASE)/header" "$(SDK_BASE)/include" "$(SDK_PLATFORM)/include" \
			"$(SDK_PLATFORM)/startup/include"; do \
		[ -d "$$d" ] || missing="$$missing $$d"; \
	done; \
	if [ -n "$$missing" ]; then \
		echo "MISSING   : vendored SDK include root(s):$$missing"; \
		echo 'Run: make vendor-sdk S32DS_SDK_ROOT=<...>/software/PlatformSDK_S32K3/RTD'; \
		exit 1; \
	fi; \
	echo 'sdk       : all four vendored include roots present'
	@echo 'CC        : $(CC)'
	@$(CC) --version
	@echo "sysroot   : $$($(CC) -print-sysroot)"
	@ok=1; for f in nano.specs libc_nano.a libm.a libgcc.a; do \
		p="$$($(CC) -print-file-name=$$f)"; \
		if [ "$$p" = "$$f" ]; then echo "MISSING   : $$f"; ok=0; else echo "resolved  : $$p"; fi; \
	done; \
	if [ "$$ok" = 0 ]; then \
		echo ''; \
		echo 'The toolchain cannot resolve the bare-metal specs/libraries above.'; \
		echo 'With the S32DS toolchain, retry with SYSROOT=<gcc>/arm-none-eabi/lib'; \
		exit 1; \
	fi
	@case "$$($(CC) -dumpversion)" in \
		$(EXPECTED_TOOLCHAIN_VERSION)*) echo 'version   : ok (expected $(EXPECTED_TOOLCHAIN_VERSION).x)';; \
		*) echo "WARNING   : expected GCC $(EXPECTED_TOOLCHAIN_VERSION).x, found $$($(CC) -dumpversion) - continuing";; \
	esac

print-config:
	@echo 'repo        : $(REPO)'
	@echo 'config      : $(CONFIG)'
	@echo 'linker      : $(LDFILE)'
	@echo 'toolchain   : $(CC)'
	@echo 'python      : $(PYTHON)'
	@echo 'sdk base    : $(SDK_BASE)'
	@echo 'sdk platform: $(SDK_PLATFORM)'
	@echo 'sources     : $(words $(C_SRCS)) C, $(words $(S_SRCS)) assembler'
	@echo 'objects     : $(words $(LINK_OBJS)) linked'
	@echo 'output      : $(OUT)'
	@echo 'CFLAGS      : $(CFLAGS)'
	@echo 'ASFLAGS     : $(ASFLAGS)'
	@echo 'LDFLAGS_OPT : $(LDFLAGS_OPT)'
	@echo 'LDFLAGS_LIB : $(LDFLAGS_LIB)'

# Re-copy the vendored SDK subset from a local S32DS installation. The scope and
# rationale are documented in sdk/README.md; keep both in sync if this changes.
vendor-sdk:
	@if [ -z "$(S32DS_SDK_ROOT)" ]; then \
		echo 'S32DS_SDK_ROOT is not set.'; \
		echo 'Usage: make vendor-sdk S32DS_SDK_ROOT=/path/to/software/PlatformSDK_S32K3/RTD'; \
		exit 1; \
	fi
	@set -e; src="$(S32DS_SDK_ROOT)"; dst="$(REPO)/sdk"; \
	base="$$src/BaseNXP_TS_T40D34M70I1R0"; plat="$$src/Platform_TS_T40D34M70I1R0"; \
	test -d "$$base/header" || { echo "not found: $$base/header"; exit 1; }; \
	mkdir -p "$$dst/base/header" "$$dst/base/include" "$$dst/platform/include" "$$dst/platform/startup/include"; \
	cp "$$base"/header/S32K344*.h "$$dst/base/header/"; \
	cp "$$base"/header/SCR.txt "$$dst/base/header/"; \
	cp "$$base"/include/. "$$dst/base/include/" -R; \
	cp "$$plat"/include/. "$$dst/platform/include/" -R; \
	cp "$$plat"/startup/include/. "$$dst/platform/startup/include/" -R; \
	echo "vendored $$(find "$$dst" -type f | wc -l) files into $$dst"; \
	echo 'review the diff, then update the counts in sdk/README.md'

help:
	@echo 'Portable BMS_demo build (see the comment at the top of make/Makefile)'
	@echo ''
	@echo '  make [CONFIG=Debug_FLASH|Release_RAM]   build (default CONFIG=Debug_FLASH)'
	@echo '  make -j$$(nproc)                        parallel build'
	@echo '  make clean                              remove make/build/<CONFIG>'
	@echo '  make size                               ELF section sizes'
	@echo '  make check-toolchain                    toolchain identity and libraries'
	@echo '  make print-config                       configuration, paths, flags'
	@echo '  make vendor-sdk S32DS_SDK_ROOT=<path>   re-copy the vendored SDK headers'
	@echo ''
	@echo 'Variables: CONFIG ARMGCC_DIR SYSROOT OUT SDK_BASE SDK_PLATFORM PYTHON'

.PHONY: all clean size check-toolchain print-config vendor-sdk help
