
# @TODO Compilers to support gcc, clang, MSVC, TinyCC

BASIN_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

ifeq ($(OS),Windows_NT)
  SHELL := cmd.exe
  .SHELLFLAGS := /C
endif

ifeq ($(OS),Windows_NT)
  BASIN_OUTPUT_EXE ?= $(BASIN_ROOT)/bin/basin.exe
  BASIN_OUTPUT_LIB ?= $(BASIN_ROOT)/bin/basin.lib
  BASIN_OUTPUT_DYN ?= $(BASIN_ROOT)/bin/basin.dll
  BASIN_OBJDIR ?= obj/windows
else
  BASIN_OUTPUT_EXE ?= $(BASIN_ROOT)/bin/basin
  BASIN_OUTPUT_LIB ?= $(BASIN_ROOT)/bin/libbasin.a
  BASIN_OUTPUT_DYN ?= $(BASIN_ROOT)/bin/libbasin.so
  BASIN_OBJDIR ?= obj/linux
endif

BASIN_BINDIR        ?= bin
CC            ?= gcc
BASIN_CXX           ?= g++
AR            ?= ar
SILENT        ?= @
ENABLE_TRACY  ?= 1


BASIN_CFLAGS ?=
BASIN_CFLAGS += -Wall -Werror -Wextra
BASIN_CFLAGS += -Wno-unused-value -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter -Wno-cast-function-type # @TODO Remove these and fix errors for final release?
BASIN_CFLAGS += -fPIC # needed for shared library
BASIN_CFLAGS += -I $(BASIN_ROOT)/include -I $(BASIN_ROOT)/src
BASIN_CFLAGS += -g -DDEBUG_BUILD
# DEBUG_BUILD macro will use enums instead of 8-bit integers in some structs for debuggability.
# Ideally C has 8-bit enums but it does not. ISO C11 doesn't at least.
# BASIN_CFLAGS += -O3
BASIN_LFLAGS += -g

# Below is needed for debugging on windows with GDB (from mingw)
ifeq ($(OS),Windows_NT)
  BASIN_LFLAGS += -lws2_32 -lwinmm -ldbghelp -static-libstdc++ -static-libgcc
endif

BASIN_SRCDIRS := \
  $(BASIN_ROOT)/src \
  $(BASIN_ROOT)/src/basin \
  $(BASIN_ROOT)/src/basin/backend \
  $(BASIN_ROOT)/src/basin/core \
  $(BASIN_ROOT)/src/basin/extra \
  $(BASIN_ROOT)/src/basin/frontend \
  $(BASIN_ROOT)/src/basin/util \
  $(BASIN_ROOT)/src/platform \
  #

BASIN_SRCFILES := $(foreach DIR,$(BASIN_SRCDIRS),\
  $(wildcard $(DIR)/*.c) \
)

ifneq ($(ENABLE_TRACY),)
  BASIN_SRCFILES += $(BASIN_ROOT)/src/tracy/0.12.2/public/TracyClient.cpp
  BASIN_CFLAGS   += -I $(BASIN_ROOT)/src/tracy/0.12.2/public
endif

BASIN_OBJFILES := $(patsubst $(BASIN_ROOT)/src/%.c,$(BASIN_OBJDIR)/%.o,$(BASIN_SRCFILES))
BASIN_OBJFILES := $(patsubst $(BASIN_ROOT)/src/%.cpp,$(BASIN_OBJDIR)/%.o,$(BASIN_OBJFILES))
BASIN_DEPFILES := $(patsubst %.o,%.d,$(BASIN_OBJFILES))

.PHONY: all
# @TODO Compile libs again (for now it's unnecessary, i don't use them).
# all: $(BASIN_OUTPUT_EXE) $(BASIN_OUTPUT_LIB) $(BASIN_OUTPUT_DYN)
all: $(BASIN_OUTPUT_EXE)

ifeq (0, $(words $(findstring $(MAKECMDGOALS), clean)))
    -include $(BASIN_DEPFILES)
endif

ifeq ($(OS),Windows_NT)
  define makedirs =
    mkdir $(subst /,\,$(1)) 2>nul || exit /b 0
  endef
else
  define makedirs =
    mkdir -p $(1)
  endef
endif


$(BASIN_OBJDIR) $(BASIN_BINDIR):
	$(SILENT) $(call makedirs,$@)

BASIN_VERSION    := 0.0.1-dev
BASIN_CFLAGS += "-DBASIN_VERSION=\"$(BASIN_VERSION)\""


ifeq ($(OS),Windows_NT)
  BASIN_GIT_COMMIT := $(shell git rev-parse --short=10 HEAD 2>nul || echo unknown)
  BASIN_BUILD_DATE := $(shell powershell -NoProfile -Command "Get-Date -Format 'yyyy-MM-dd HH:mm:ss'")
  BASIN_CFLAGS += "-DBASIN_COMMIT=\"$(BASIN_GIT_COMMIT)\"" "-DBASIN_DATE=\"$(BASIN_BUILD_DATE)\""
else
  BASIN_GIT_COMMIT := $(shell git rev-parse --short=10 HEAD 2>/dev/null || echo unknown)
  BASIN_BUILD_DATE := $(shell date '+%Y-%m-%d %H:%M:%S')
  BASIN_CFLAGS += "-DBASIN_COMMIT=\"$(BASIN_GIT_COMMIT)\"" "-DBASIN_DATE=\"$(BASIN_BUILD_DATE)\""
endif

$(BASIN_OBJDIR)/%.o: $(BASIN_ROOT)/src/%.c | $(BASIN_OBJDIR)
	$(SILENT) $(call makedirs,$(@D))
	$(SILENT) $(CC) -c -MD $(BASIN_CFLAGS) -o $@ $<

$(BASIN_OBJDIR)/%.o: $(BASIN_ROOT)/src/%.cpp | $(BASIN_OBJDIR)
	$(SILENT) $(call makedirs,$(@D))
	$(SILENT) $(BASIN_CXX) -c -MD $(BASIN_CFLAGS) -o $@ $<

# @TODO For libraries we don't want to include main.o
# $(BASIN_OUTPUT_LIB): $(BASIN_OBJFILES) | $(BASIN_BINDIR)
# 	$(SILENT) $(AR) rcs $@ $^

$(BASIN_OUTPUT_EXE): $(BASIN_OBJFILES) | $(BASIN_BINDIR)
	$(SILENT) $(BASIN_CXX) $(BASIN_LFLAGS) -o $@ $^

# $(BASIN_OUTPUT_DYN): $(BASIN_OBJFILES) | $(BASIN_BINDIR)
# 	$(SILENT) $(BASIN_CXX) $(BASIN_LFLAGS) -shared -o $@ $^


.PHONY: clean
clean:
ifeq ($(OS),Windows_NT)
	rd /S /Q obj bin 2>nul || exit /b 0
else
	rm -rf obj bin
endif
