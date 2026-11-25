
BASIN_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))..)
TEST_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

CC     ?= gcc
BASIN  ?= basin
CFLAGS ?= -std=c11 -O2 -g
SILENT ?= @

ifeq ($(OS),Windows_NT)
    SHELL := cmd.exe
    .SHELLFLAGS := /C

    RUNNER := $(BASIN_ROOT)/bin/runner.exe
    OBJDIR ?= $(BASIN_ROOT)/obj/windows
    PATH := $(BASIN_ROOT)/bin;$(PATH)
    export PATH

    define makedirs =
        mkdir $(subst /,\,$(1)) 2>nul || exit /b 0
    endef
else
    RUNNER := $(BASIN_ROOT)/bin/runner
    OBJDIR ?= $(BASIN_ROOT)/obj/linux
    PATH := $(BASIN_ROOT)/bin:$(PATH)
    export PATH
    define makedirs =
        mkdir -p $(1)
    endef
endif

ifeq ($(OS),Windows_NT)
    EXIT_CMD := exit /b 0
else
    EXIT_CMD := exit 0
endif

RUNNER_ARGS := -testutil $(OBJDIR)/testutil.o -importdir $(TEST_ROOT) -objdir $(OBJDIR)

ifneq ($(strip $(VERBOSE)),)
    RUNNER_ARGS += -verbose
endif


.PHONY: test
test: $(RUNNER) basin_compiler $(OBJDIR)/testutil.o
	$(SILENT) $(RUNNER) $(RUNNER_ARGS) || $(EXIT_CMD)


$(RUNNER): $(TEST_ROOT)/runner.c | $(BASIN_ROOT)/bin
	$(SILENT) $(CC) $(CFLAGS) -o $@ $^


$(OBJDIR)/testutil.o: $(TEST_ROOT)/testutil.c | $(OBJDIR)
	$(SILENT) $(CC) -c $(CFLAGS) -o $@ $^


$(BASIN_ROOT)/bin $(OBJDIR):
	$(SILENT) $(call makedirs,$@)


include $(BASIN_ROOT)/Makefile

basin_compiler: $(BASIN_OUTPUT_EXE)
