################################################################################
# Repository-root entry point, so a fresh clone builds with a bare 'make'.
#
# The real makefile is make/Makefile (portable, non-S32DS build). This file only
# forwards the supported goals to it; 'make -C make <goal>' is equivalent.
################################################################################

REPO_DIR := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
MAKE_DIR := $(REPO_DIR)/make

GOALS := all clean size check-toolchain print-config vendor-sdk help

.PHONY: $(GOALS)

all clean size check-toolchain print-config vendor-sdk help:
	@$(MAKE) --no-print-directory -C "$(MAKE_DIR)" $@
